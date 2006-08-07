/* Copyright (C) 2003-2004 MySQL AB

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

/***************************************************************************
 This is a test sample to test the new features in MySQL client-server
 protocol

 Main author: venu ( venu@mysql.com )
***************************************************************************/

/*
  XXX: PLEASE RUN THIS PROGRAM UNDER VALGRIND AND VERIFY THAT YOUR TEST
  DOESN'T CONTAIN WARNINGS/ERRORS BEFORE YOU PUSH.
*/


#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <errmsg.h>
#include <my_getopt.h>
#include <m_string.h>

#define VER "2.1"
#define MAX_TEST_QUERY_LENGTH 300 /* MAX QUERY BUFFER LENGTH */
#define MAX_KEY MAX_INDEXES
#define MAX_SERVER_ARGS 64

/* set default options */
static int   opt_testcase = 0;
static char *opt_db= 0;
static char *opt_user= 0;
static char *opt_password= 0;
static char *opt_host= 0;
static char *opt_unix_socket= 0;
static unsigned int  opt_port;
static my_bool tty_password= 0, opt_silent= 0;

static MYSQL *mysql= 0;
static char current_db[]= "client_test_db";
static unsigned int test_count= 0;
static unsigned int opt_count= 0;
static unsigned int iter_count= 0;
static my_bool have_innodb= FALSE;

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

#define myheader(str) \
if (opt_silent < 2) \
{ \
  fprintf(stdout, "\n\n#####################################\n"); \
  fprintf(stdout, "%d of (%d/%d): %s", test_count++, iter_count, \
                                     opt_count, str); \
  fprintf(stdout, "  \n#####################################\n"); \
}
#define myheader_r(str) \
if (!opt_silent) \
{ \
  fprintf(stdout, "\n\n#####################################\n"); \
  fprintf(stdout, "%s", str); \
  fprintf(stdout, "  \n#####################################\n"); \
}

static void print_error(const char *msg);
static void print_st_error(MYSQL_STMT *stmt, const char *msg);
static void client_disconnect();


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
#define DIE(expr) \
        die(__FILE__, __LINE__, #expr)

void die(const char *file, int line, const char *expr)
{
  fprintf(stderr, "%s:%d: check failed: '%s'\n", file, line, expr);
  fflush(NULL);
  abort();
}


#define myerror(msg) print_error(msg)
#define mysterror(stmt, msg) print_st_error(stmt, msg)

#define myquery(RES) \
{ \
  int r= (RES);                                \
  if (r) \
    myerror(NULL); \
  DIE_UNLESS(r == 0); \
}

#define myquery_r(r) \
{ \
if (r) \
  myerror(NULL); \
DIE_UNLESS(r != 0); \
}

#define check_execute(stmt, r) \
{ \
if (r) \
  mysterror(stmt, NULL); \
DIE_UNLESS(r == 0);\
}

#define check_execute_r(stmt, r) \
{ \
if (r) \
  mysterror(stmt, NULL); \
DIE_UNLESS(r != 0);\
}

#define check_stmt(stmt) \
{ \
if ( stmt == 0) \
  myerror(NULL); \
DIE_UNLESS(stmt != 0); \
}

#define check_stmt_r(stmt) \
{ \
if (stmt == 0) \
  myerror(NULL);\
DIE_UNLESS(stmt == 0);\
}

#define mytest(x) if (!x) {myerror(NULL);DIE_UNLESS(FALSE);}
#define mytest_r(x) if (x) {myerror(NULL);DIE_UNLESS(FALSE);}


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

MYSQL_STMT *STDCALL
mysql_simple_prepare(MYSQL  *mysql, const char *query)
{
  MYSQL_STMT *stmt= mysql_stmt_init(mysql);
  if (stmt && mysql_stmt_prepare(stmt, query, strlen(query)))
  {
    mysql_stmt_close(stmt);
    return 0;
  }
  return stmt;
}


/* Connect to the server */

static void client_connect(ulong flag)
{
  int  rc;
  static char query[MAX_TEST_QUERY_LENGTH];
  myheader_r("client_connect");

  if (!opt_silent)
    fprintf(stdout, "\n Establishing a connection to '%s' ...",
            opt_host ? opt_host : "");

  if (!(mysql= mysql_init(NULL)))
  {
    opt_silent= 0;
    myerror("mysql_init() failed");
    exit(1);
  }

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
  mysql->reconnect= 1;

  if (!opt_silent)
    fprintf(stdout, " OK");

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
    fprintf(stdout, " OK");
}


/* Close the connection */

static void client_disconnect()
{
  static char query[MAX_TEST_QUERY_LENGTH];

  myheader_r("client_disconnect");

  if (mysql)
  {
    if (!opt_silent)
      fprintf(stdout, "\n dropping the test database '%s' ...", current_db);
    strxmov(query, "DROP DATABASE IF EXISTS ", current_db, NullS);

    mysql_query(mysql, query);
    if (!opt_silent)
      fprintf(stdout, " OK");

    if (!opt_silent)
      fprintf(stdout, "\n closing the connection ...");
    mysql_close(mysql);
    fprintf(stdout, " OK\n");
  }
}


/* Query processing */

static void client_query()
{
  int rc;

  myheader("client_query");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1("
                         "id int primary key auto_increment, "
                         "name varchar(20))");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1(id int, name varchar(20))");
  myquery_r(rc);

  rc= mysql_query(mysql, "INSERT INTO t1(name) VALUES('mysql')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO t1(name) VALUES('monty')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO t1(name) VALUES('venu')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO t1(name) VALUES('deleted')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO t1(name) VALUES('deleted')");
  myquery(rc);

  rc= mysql_query(mysql, "UPDATE t1 SET name= 'updated' "
                          "WHERE name= 'deleted'");
  myquery(rc);

  rc= mysql_query(mysql, "UPDATE t1 SET id= 3 WHERE name= 'updated'");
  myquery_r(rc);

  myquery(mysql_query(mysql, "drop table t1"));
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

int my_process_result_set(MYSQL_RES *result)
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


int my_process_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  int       row_count;

  if (!(result= mysql_store_result(mysql)))
    return 0;

  row_count= my_process_result_set(result);

  mysql_free_result(result);
  return row_count;
}


/* Process the statement result set */

#define MAX_RES_FIELDS 50
#define MAX_FIELD_DATA_SIZE 255

int my_process_stmt_result(MYSQL_STMT *stmt)
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

  if (!(field= mysql_fetch_field_direct(result, no)))
  {
    fprintf(stdout, "\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  cs= get_charset(field->charsetnr, 0);
  DIE_UNLESS(cs);
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
    fprintf(stdout, "\n    length   :`%lu`\t(expected: `%lu`)",
            field->length, length * cs->mbmaxlen);
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
  if (length && field->length != length * cs->mbmaxlen)
  {
    fprintf(stderr, "Expected field length: %d,  got length: %d\n",
            (int) (length * cs->mbmaxlen), (int) field->length);
    DIE_UNLESS(field->length == length * cs->mbmaxlen);
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

/* Store result processing */

static void client_store_result()
{
  MYSQL_RES *result;
  int       rc;

  myheader("client_store_result");

  rc= mysql_query(mysql, "SELECT * FROM t1");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  (void) my_process_result_set(result);
  mysql_free_result(result);
}


/* Fetch the results */

static void client_use_result()
{
  MYSQL_RES *result;
  int       rc;
  myheader("client_use_result");

  rc= mysql_query(mysql, "SELECT * FROM t1");
  myquery(rc);

  /* get the result */
  result= mysql_use_result(mysql);
  mytest(result);

  (void) my_process_result_set(result);
  mysql_free_result(result);
}


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

static my_bool thread_query(char *query)
{
  MYSQL *l_mysql;
  my_bool error;

  error= 0;
  if (!opt_silent)
    fprintf(stdout, "\n in thread_query(%s)", query);
  if (!(l_mysql= mysql_init(NULL)))
  {
    myerror("mysql_init() failed");
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
  if (mysql_query(l_mysql, (char *)query))
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


/* Query processing */

static void test_debug_example()
{
  int rc;
  MYSQL_RES *result;

  myheader("test_debug_example");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_debug_example");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_debug_example("
                         "id INT PRIMARY KEY AUTO_INCREMENT, "
                         "name VARCHAR(20), xxx INT)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_debug_example (name) "
                         "VALUES ('mysql')");
  myquery(rc);

  rc= mysql_query(mysql, "UPDATE test_debug_example SET name='updated' "
                         "WHERE name='deleted'");
  myquery(rc);

  rc= mysql_query(mysql, "SELECT * FROM test_debug_example where name='mysql'");
  myquery(rc);

  result= mysql_use_result(mysql);
  mytest(result);

  (void) my_process_result_set(result);
  mysql_free_result(result);

  rc= mysql_query(mysql, "DROP TABLE test_debug_example");
  myquery(rc);
}


/* Test autocommit feature for BDB tables */

static void test_tran_bdb()
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_bdb");

  /* set AUTOCOMMIT to OFF */
  rc= mysql_autocommit(mysql, FALSE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS my_demo_transaction");
  myquery(rc);


  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc= mysql_query(mysql, "CREATE TABLE my_demo_transaction( "
                         "col1 int , col2 varchar(30)) TYPE= BDB");
  myquery(rc);

  /* insert a row and commit the transaction */
  rc= mysql_query(mysql, "INSERT INTO my_demo_transaction VALUES(10, 'venu')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* now insert the second row, and roll back the transaction */
  rc= mysql_query(mysql, "INSERT INTO my_demo_transaction VALUES(20, 'mysql')");
  myquery(rc);

  rc= mysql_rollback(mysql);
  myquery(rc);

  /* delete first row, and roll it back */
  rc= mysql_query(mysql, "DELETE FROM my_demo_transaction WHERE col1= 10");
  myquery(rc);

  rc= mysql_rollback(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  (void) my_process_result_set(result);
  mysql_free_result(result);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result= mysql_use_result(mysql);
  mytest(result);

  row= mysql_fetch_row(result);
  mytest(row);

  row= mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
  mysql_autocommit(mysql, TRUE);
}


/* Test autocommit feature for InnoDB tables */

static void test_tran_innodb()
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_innodb");

  /* set AUTOCOMMIT to OFF */
  rc= mysql_autocommit(mysql, FALSE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS my_demo_transaction");
  myquery(rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc= mysql_query(mysql, "CREATE TABLE my_demo_transaction(col1 int, "
                         "col2 varchar(30)) TYPE= InnoDB");
  myquery(rc);

  /* insert a row and commit the transaction */
  rc= mysql_query(mysql, "INSERT INTO my_demo_transaction VALUES(10, 'venu')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* now insert the second row, and roll back the transaction */
  rc= mysql_query(mysql, "INSERT INTO my_demo_transaction VALUES(20, 'mysql')");
  myquery(rc);

  rc= mysql_rollback(mysql);
  myquery(rc);

  /* delete first row, and roll it back */
  rc= mysql_query(mysql, "DELETE FROM my_demo_transaction WHERE col1= 10");
  myquery(rc);

  rc= mysql_rollback(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  (void) my_process_result_set(result);
  mysql_free_result(result);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result= mysql_use_result(mysql);
  mytest(result);

  row= mysql_fetch_row(result);
  mytest(row);

  row= mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
  mysql_autocommit(mysql, TRUE);
}


/* Test for BUG#7242 */

static void test_prepare_insert_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        i;
  const char *testcase[]= {
    "CREATE TABLE t1 (a INT, b INT, c INT, UNIQUE (A), UNIQUE(B))",
    "INSERT t1 VALUES (1,2,10), (3,4,20)",
    "INSERT t1 VALUES (5,6,30), (7,4,40), (8,9,60) ON DUPLICATE KEY UPDATE c=c+100",
    "SELECT * FROM t1",
    "INSERT t1 SET a=5 ON DUPLICATE KEY UPDATE b=0",
    "SELECT * FROM t1",
    "INSERT t1 VALUES (2,1,11), (7,4,40) ON DUPLICATE KEY UPDATE c=c+VALUES(a)",
    NULL};
  const char **cur_query;

  myheader("test_prepare_insert_update");
  
  for (cur_query= testcase; *cur_query; cur_query++)
  {
    char query[MAX_TEST_QUERY_LENGTH];
    printf("\nRunning query: %s", *cur_query);
    strmov(query, *cur_query);
    stmt= mysql_simple_prepare(mysql, query);
    check_stmt(stmt);

    verify_param_count(stmt, 0);
    rc= mysql_stmt_execute(stmt);

    check_execute(stmt, rc);
    /* try the last query several times */
    if (!cur_query[1])
    {
      for (i=0; i < 3;i++)
      {
        printf("\nExecuting last statement again");
        rc= mysql_stmt_execute(stmt);
        check_execute(stmt, rc);
        rc= mysql_stmt_execute(stmt);
        check_execute(stmt, rc);
      }
    }
    mysql_stmt_close(stmt);
  }

  rc= mysql_commit(mysql);
  myquery(rc);
}

/* Test simple prepares of all DML statements */

static void test_prepare_simple()
{
  MYSQL_STMT *stmt;
  int        rc;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_prepare_simple");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prepare_simple");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prepare_simple("
                         "id int, name varchar(50))");
  myquery(rc);

  /* insert */
  strmov(query, "INSERT INTO test_prepare_simple VALUES(?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);
  mysql_stmt_close(stmt);

  /* update */
  strmov(query, "UPDATE test_prepare_simple SET id=? "
                "WHERE id=? AND CONVERT(name USING utf8)= ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 3);
  mysql_stmt_close(stmt);

  /* delete */
  strmov(query, "DELETE FROM test_prepare_simple WHERE id=10");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  mysql_stmt_close(stmt);

  /* delete */
  strmov(query, "DELETE FROM test_prepare_simple WHERE id=?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 1);

  mysql_stmt_close(stmt);

  /* select */
  strmov(query, "SELECT * FROM test_prepare_simple WHERE id=? "
                "AND CONVERT(name USING utf8)= ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);
}


/* Test simple prepare field results */

static void test_prepare_field_result()
{
  MYSQL_STMT *stmt;
  MYSQL_RES  *result;
  int        rc;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_prepare_field_result");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prepare_field_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prepare_field_result(int_c int, "
                         "var_c varchar(50), ts_c timestamp(14), "
                         "char_c char(4), date_c date, extra tinyint)");
  myquery(rc);

  /* insert */
  strmov(query, "SELECT int_c, var_c, date_c as date, ts_c, char_c FROM "
                " test_prepare_field_result as t1 WHERE int_c=?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 1);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);

  my_print_result_metadata(result);

  if (!opt_silent)
    fprintf(stdout, "\n\n field attributes:\n");
  verify_prepare_field(result, 0, "int_c", "int_c", MYSQL_TYPE_LONG,
                       "t1", "test_prepare_field_result", current_db, 11, 0);
  verify_prepare_field(result, 1, "var_c", "var_c", MYSQL_TYPE_VAR_STRING,
                       "t1", "test_prepare_field_result", current_db, 50, 0);
  verify_prepare_field(result, 2, "date", "date_c", MYSQL_TYPE_DATE,
                       "t1", "test_prepare_field_result", current_db, 10, 0);
  verify_prepare_field(result, 3, "ts_c", "ts_c", MYSQL_TYPE_TIMESTAMP,
                       "t1", "test_prepare_field_result", current_db, 19, 0);
  verify_prepare_field(result, 4, "char_c", "char_c",
                       (mysql_get_server_version(mysql) <= 50000 ?
                        MYSQL_TYPE_VAR_STRING : MYSQL_TYPE_STRING),
                       "t1", "test_prepare_field_result", current_db, 4, 0);

  verify_field_count(result, 5);
  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/* Test simple prepare field results */

static void test_prepare_syntax()
{
  MYSQL_STMT *stmt;
  int        rc;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_prepare_syntax");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prepare_syntax");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prepare_syntax("
                         "id int, name varchar(50), extra int)");
  myquery(rc);

  strmov(query, "INSERT INTO test_prepare_syntax VALUES(?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  strmov(query, "SELECT id, name FROM test_prepare_syntax WHERE id=? AND WHERE");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);
}


/* Test a simple prepare */

static void test_prepare()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  int        int_data, o_int_data;
  char       str_data[50], data[50];
  char       tiny_data, o_tiny_data;
  short      small_data, o_small_data;
  longlong   big_data, o_big_data;
  float      real_data, o_real_data;
  double     double_data, o_double_data;
  ulong      length[7], len;
  my_bool    is_null[7];
  char	     llbuf[22];
  MYSQL_BIND bind[7];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_prepare");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS my_prepare");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE my_prepare(col1 tinyint, "
                         "col2 varchar(15), col3 int, "
                         "col4 smallint, col5 bigint, "
                         "col6 float, col7 double )");
  myquery(rc);

  /* insert by prepare */
  strxmov(query, "INSERT INTO my_prepare VALUES(?, ?, ?, ?, ?, ?, ?)", NullS);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 7);

  bzero((char*) bind, sizeof(bind));

  /* tinyint */
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[0].buffer= (void *)&tiny_data;
  /* string */
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)str_data;
  bind[1].buffer_length= 1000;                  /* Max string length */
  /* integer */
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[2].buffer= (void *)&int_data;
  /* short */
  bind[3].buffer_type= MYSQL_TYPE_SHORT;
  bind[3].buffer= (void *)&small_data;
  /* bigint */
  bind[4].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[4].buffer= (void *)&big_data;
  /* float */
  bind[5].buffer_type= MYSQL_TYPE_FLOAT;
  bind[5].buffer= (void *)&real_data;
  /* double */
  bind[6].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[6].buffer= (void *)&double_data;

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].length= &length[i];
    bind[i].is_null= &is_null[i];
    is_null[i]= 0;
  }

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  int_data= 320;
  small_data= 1867;
  big_data= 1000;
  real_data= 2;
  double_data= 6578.001;

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data= 0; tiny_data < 100; tiny_data++)
  {
    length[1]= my_sprintf(str_data, (str_data, "MySQL%d", int_data));
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    int_data += 25;
    small_data += 10;
    big_data += 100;
    real_data += 1;
    double_data += 10.09;
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= my_stmt_result("SELECT * FROM my_prepare");
  DIE_UNLESS(tiny_data == (char) rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM my_prepare");
  check_stmt(stmt);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  /* get the result */
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  o_int_data= 320;
  o_small_data= 1867;
  o_big_data= 1000;
  o_real_data= 2;
  o_double_data= 6578.001;

  /* now, execute the prepared statement to insert 10 records.. */
  for (o_tiny_data= 0; o_tiny_data < 100; o_tiny_data++)
  {
    len= my_sprintf(data, (data, "MySQL%d", o_int_data));

    rc= mysql_stmt_fetch(stmt);
    check_execute(stmt, rc);

    if (!opt_silent)
    {
      fprintf(stdout, "\n");
      fprintf(stdout, "\n\t tiny   : %d (%lu)", tiny_data, length[0]);
      fprintf(stdout, "\n\t short  : %d (%lu)", small_data, length[3]);
      fprintf(stdout, "\n\t int    : %d (%lu)", int_data, length[2]);
      fprintf(stdout, "\n\t big    : %s (%lu)", llstr(big_data, llbuf),
              length[4]);

      fprintf(stdout, "\n\t float  : %f (%lu)", real_data, length[5]);
      fprintf(stdout, "\n\t double : %f (%lu)", double_data, length[6]);

      fprintf(stdout, "\n\t str    : %s (%lu)", str_data, length[1]);
    }

    DIE_UNLESS(tiny_data == o_tiny_data);
    DIE_UNLESS(is_null[0] == 0);
    DIE_UNLESS(length[0] == 1);

    DIE_UNLESS(int_data == o_int_data);
    DIE_UNLESS(length[2] == 4);

    DIE_UNLESS(small_data == o_small_data);
    DIE_UNLESS(length[3] == 2);

    DIE_UNLESS(big_data == o_big_data);
    DIE_UNLESS(length[4] == 8);

    DIE_UNLESS(real_data == o_real_data);
    DIE_UNLESS(length[5] == 4);

    DIE_UNLESS(cmp_double(&double_data, &o_double_data));
    DIE_UNLESS(length[6] == 8);

    DIE_UNLESS(strcmp(data, str_data) == 0);
    DIE_UNLESS(length[1] == len);

    o_int_data += 25;
    o_small_data += 10;
    o_big_data += 100;
    o_real_data += 1;
    o_double_data += 10.09;
  }

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

}


/* Test double comparision */

static void test_double_compare()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       real_data[10], tiny_data;
  double     double_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];
  ulong      length[3];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_double_compare");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_double_compare");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_double_compare(col1 tinyint, "
                         " col2 float, col3 double )");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_double_compare "
                         "VALUES (1, 10.2, 34.5)");
  myquery(rc);

  strmov(query, "UPDATE test_double_compare SET col1=100 "
                "WHERE col1 = ? AND col2 = ? AND COL3 = ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 3);

  /* Always bzero bind array because there can be internal members */
  bzero((char*) bind, sizeof(bind));

  /* tinyint */
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[0].buffer= (void *)&tiny_data;

  /* string->float */
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)&real_data;
  bind[1].buffer_length= sizeof(real_data);
  bind[1].length= &length[1];
  length[1]= 10;

  /* double */
  bind[2].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[2].buffer= (void *)&double_data;

  tiny_data= 1;
  strmov(real_data, "10.2");
  double_data= 34.5;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  verify_affected_rows(0);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM test_double_compare");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS((int)tiny_data == rc);
  mysql_free_result(result);
}


/* Test simple null */

static void test_null()
{
  MYSQL_STMT *stmt;
  int        rc;
  uint       nData;
  MYSQL_BIND bind[2];
  my_bool    is_null[2];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_null");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_null");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_null(col1 int, col2 varchar(50))");
  myquery(rc);

  /* insert by prepare, wrong column name */
  strmov(query, "INSERT INTO test_null(col3, col2) VALUES(?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  strmov(query, "INSERT INTO test_null(col1, col2) VALUES(?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].is_null= &is_null[0];
  is_null[0]= 1;
  bind[1]= bind[0];

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (nData= 0; nData<10; nData++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }

  /* Re-bind with MYSQL_TYPE_NULL */
  bind[0].buffer_type= MYSQL_TYPE_NULL;
  is_null[0]= 0; /* reset */
  bind[1]= bind[0];

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  for (nData= 0; nData<10; nData++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);

  nData*= 2;
  rc= my_stmt_result("SELECT * FROM test_null");;
  DIE_UNLESS((int) nData == rc);

  /* Fetch results */
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&nData; /* this buffer won't be altered */
  bind[0].length= 0;
  bind[1]= bind[0];
  bind[0].is_null= &is_null[0];
  bind[1].is_null= &is_null[1];

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_null");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= 0;
  is_null[0]= is_null[1]= 0;
  while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
  {
    DIE_UNLESS(is_null[0]);
    DIE_UNLESS(is_null[1]);
    rc++;
    is_null[0]= is_null[1]= 0;
  }
  DIE_UNLESS(rc == (int) nData);
  mysql_stmt_close(stmt);
}


/* Test for NULL as PS parameter (BUG#3367, BUG#3371) */

static void test_ps_null_param()
{
  MYSQL_STMT *stmt;
  int        rc;

  MYSQL_BIND in_bind;
  my_bool    in_is_null;
  long int   in_long;

  MYSQL_BIND out_bind;
  ulong      out_length;
  my_bool    out_is_null;
  char       out_str_data[20];

  const char *queries[]= {"select ?", "select ?+1",
                    "select col1 from test_ps_nulls where col1 <=> ?",
                    NULL
                    };
  const char **cur_query= queries;

  myheader("test_null_ps_param_in_result");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_ps_nulls");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_ps_nulls(col1 int)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_ps_nulls values (1), (null)");
  myquery(rc);

  /* Always bzero all members of bind parameter */
  bzero((char*) &in_bind, sizeof(in_bind));
  bzero((char*) &out_bind, sizeof(out_bind));

  in_bind.buffer_type= MYSQL_TYPE_LONG;
  in_bind.is_null= &in_is_null;
  in_bind.length= 0;
  in_bind.buffer= (void *)&in_long;
  in_is_null= 1;
  in_long= 1;

  out_bind.buffer_type= MYSQL_TYPE_STRING;
  out_bind.is_null= &out_is_null;
  out_bind.length= &out_length;
  out_bind.buffer= out_str_data;
  out_bind.buffer_length= array_elements(out_str_data);

  /* Execute several queries, all returning NULL in result. */
  for(cur_query= queries; *cur_query; cur_query++)
  {
    char query[MAX_TEST_QUERY_LENGTH];
    strmov(query, *cur_query);
    stmt= mysql_simple_prepare(mysql, query);
    check_stmt(stmt);
    verify_param_count(stmt, 1);

    rc= mysql_stmt_bind_param(stmt, &in_bind);
    check_execute(stmt, rc);
    rc= mysql_stmt_bind_result(stmt, &out_bind);
    check_execute(stmt, rc);
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= mysql_stmt_fetch(stmt);
    DIE_UNLESS(rc != MYSQL_NO_DATA);
    DIE_UNLESS(out_is_null);
    rc= mysql_stmt_fetch(stmt);
    DIE_UNLESS(rc == MYSQL_NO_DATA);
    mysql_stmt_close(stmt);
  }
}


/* Test fetch null */

static void test_fetch_null()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        i, nData;
  MYSQL_BIND bind[11];
  ulong      length[11];
  my_bool    is_null[11];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_fetch_null");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_fetch_null");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_fetch_null("
                         " col1 tinyint, col2 smallint, "
                         " col3 int, col4 bigint, "
                         " col5 float, col6 double, "
                         " col7 date, col8 time, "
                         " col9 varbinary(10), "
                         " col10 varchar(50), "
                         " col11 char(20))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_fetch_null (col11) "
                         "VALUES (1000), (88), (389789)");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  bzero((char*) bind, sizeof(bind));
  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }
  bind[i-1].buffer= (void *)&nData;              /* Last column is not null */

  strmov((char *)query , "SELECT * FROM test_fetch_null");

  rc= my_stmt_result(query);
  DIE_UNLESS(rc == 3);

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= 0;
  while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
  {
    rc++;
    for (i= 0; i < 10; i++)
    {
      if (!opt_silent)
        fprintf(stdout, "\n data[%d] : %s", i,
                is_null[i] ? "NULL" : "NOT NULL");
      DIE_UNLESS(is_null[i]);
    }
    if (!opt_silent)
      fprintf(stdout, "\n data[%d]: %d", i, nData);
    DIE_UNLESS(nData == 1000 || nData == 88 || nData == 389789);
    DIE_UNLESS(is_null[i] == 0);
    DIE_UNLESS(length[i] == 4);
  }
  DIE_UNLESS(rc == 3);
  mysql_stmt_close(stmt);
}


/* Test simple select */

static void test_select_version()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_select_version");

  stmt= mysql_simple_prepare(mysql, "SELECT @@version");
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}


/* Test simple show */

static void test_select_show_table()
{
  MYSQL_STMT *stmt;
  int        rc, i;

  myheader("test_select_show_table");

  stmt= mysql_simple_prepare(mysql, "SHOW TABLES FROM mysql");
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  for (i= 1; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}


/* Test simple select to debug */

static void test_select_direct()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_select_direct");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_select(id int, id1 tinyint, "
                                                 " id2 float, "
                                                 " id3 double, "
                                                 " name varchar(50))");
  myquery(rc);

  /* insert a row and commit the transaction */
  rc= mysql_query(mysql, "INSERT INTO test_select VALUES(10, 5, 2.3, 4.5, 'venu')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  rc= mysql_query(mysql, "SELECT * FROM test_select");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  (void) my_process_result_set(result);
  mysql_free_result(result);
}


/* Test simple select with prepare */

static void test_select_prepare()
{
  int        rc;
  MYSQL_STMT *stmt;

  myheader("test_select_prepare");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_select(id int, name varchar(50))");
  myquery(rc);

  /* insert a row and commit the transaction */
  rc= mysql_query(mysql, "INSERT INTO test_select VALUES(10, 'venu')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_select");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE test_select");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_select(id tinyint, id1 int, "
                                                "  id2 float, id3 float, "
                                                "  name varchar(50))");
  myquery(rc);

  /* insert a row and commit the transaction */
  rc= mysql_query(mysql, "INSERT INTO test_select(id, id1, id2, name) VALUES(10, 5, 2.3, 'venu')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_select");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);
  mysql_stmt_close(stmt);
}


/* Test simple select */

static void test_select()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData= 1;
  MYSQL_BIND bind[2];
  ulong length[2];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_select");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_select(id int, name varchar(50))");
  myquery(rc);

  /* insert a row and commit the transaction */
  rc= mysql_query(mysql, "INSERT INTO test_select VALUES(10, 'venu')");
  myquery(rc);

  /* now insert the second row, and roll back the transaction */
  rc= mysql_query(mysql, "INSERT INTO test_select VALUES(20, 'mysql')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  strmov(query, "SELECT * FROM test_select WHERE id= ? "
                "AND CONVERT(name USING utf8) =?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  /* string data */
  nData= 10;
  strmov(szData, (char *)"venu");
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)szData;
  bind[1].buffer_length= 4;
  bind[1].length= &length[1];
  length[1]= 4;

  bind[0].buffer= (void *)&nData;
  bind[0].buffer_type= MYSQL_TYPE_LONG;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  mysql_stmt_close(stmt);
}


/*
  Test for BUG#3420 ("select id1, value1 from t where id= ? or value= ?"
  returns all rows in the table)
*/

static void test_ps_conj_select()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_BIND bind[2];
  int32      int_data;
  char       str_data[32];
  unsigned long str_length;
  char query[MAX_TEST_QUERY_LENGTH];
  myheader("test_ps_conj_select");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1 (id1 int(11) NOT NULL default '0', "
                         "value2 varchar(100), value1 varchar(100))");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1 values (1, 'hh', 'hh'), "
                          "(2, 'hh', 'hh'), (1, 'ii', 'ii'), (2, 'ii', 'ii')");
  myquery(rc);

  strmov(query, "select id1, value1 from t1 where id1= ? or "
                "CONVERT(value1 USING utf8)= ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&int_data;

  bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[1].buffer= (void *)str_data;
  bind[1].buffer_length= array_elements(str_data);
  bind[1].length= &str_length;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  int_data= 1;
  strmov(str_data, "hh");
  str_length= strlen(str_data);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 3);

  mysql_stmt_close(stmt);
}


/* Test BUG#1115 (incorrect string parameter value allocation) */

static void test_bug1115()
{
  MYSQL_STMT *stmt;
  int rc;
  MYSQL_BIND bind[1];
  ulong length[1];
  char szData[11];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_bug1115");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_select(\
session_id  char(9) NOT NULL, \
    a       int(8) unsigned NOT NULL, \
    b        int(5) NOT NULL, \
    c      int(5) NOT NULL, \
    d  datetime NOT NULL)");
  myquery(rc);
  rc= mysql_query(mysql, "INSERT INTO test_select VALUES "
                         "(\"abc\", 1, 2, 3, 2003-08-30), "
                         "(\"abd\", 1, 2, 3, 2003-08-30), "
                         "(\"abf\", 1, 2, 3, 2003-08-30), "
                         "(\"abg\", 1, 2, 3, 2003-08-30), "
                         "(\"abh\", 1, 2, 3, 2003-08-30), "
                         "(\"abj\", 1, 2, 3, 2003-08-30), "
                         "(\"abk\", 1, 2, 3, 2003-08-30), "
                         "(\"abl\", 1, 2, 3, 2003-08-30), "
                         "(\"abq\", 1, 2, 3, 2003-08-30) ");
  myquery(rc);
  rc= mysql_query(mysql, "INSERT INTO test_select VALUES "
                         "(\"abw\", 1, 2, 3, 2003-08-30), "
                         "(\"abe\", 1, 2, 3, 2003-08-30), "
                         "(\"abr\", 1, 2, 3, 2003-08-30), "
                         "(\"abt\", 1, 2, 3, 2003-08-30), "
                         "(\"aby\", 1, 2, 3, 2003-08-30), "
                         "(\"abu\", 1, 2, 3, 2003-08-30), "
                         "(\"abi\", 1, 2, 3, 2003-08-30), "
                         "(\"abo\", 1, 2, 3, 2003-08-30), "
                         "(\"abp\", 1, 2, 3, 2003-08-30), "
                         "(\"abz\", 1, 2, 3, 2003-08-30), "
                         "(\"abx\", 1, 2, 3, 2003-08-30)");
  myquery(rc);

  strmov(query, "SELECT * FROM test_select WHERE "
                "CONVERT(session_id USING utf8)= ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 1);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  strmov(szData, (char *)"abc");
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 3;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  strmov(szData, (char *)"venu");
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 4;
  bind[0].is_null= 0;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 0);

  strmov(szData, (char *)"abc");
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 3;
  bind[0].is_null= 0;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  mysql_stmt_close(stmt);
}


/* Test BUG#1180 (optimized away part of WHERE clause) */

static void test_bug1180()
{
  MYSQL_STMT *stmt;
  int rc;
  MYSQL_BIND bind[1];
  ulong length[1];
  char szData[11];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_select_bug");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_select(session_id  char(9) NOT NULL)");
  myquery(rc);
  rc= mysql_query(mysql, "INSERT INTO test_select VALUES (\"abc\")");
  myquery(rc);

  strmov(query, "SELECT * FROM test_select WHERE ?= \"1111\" and "
                "session_id= \"abc\"");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 1);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  strmov(szData, (char *)"abc");
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 3;
  bind[0].is_null= 0;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 0);

  strmov(szData, (char *)"1111");
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 4;
  bind[0].is_null= 0;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  strmov(szData, (char *)"abc");
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)szData;
  bind[0].buffer_length= 10;
  bind[0].length= &length[0];
  length[0]= 3;
  bind[0].is_null= 0;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 0);

  mysql_stmt_close(stmt);
}


/*
  Test BUG#1644 (Insertion of more than 3 NULL columns with parameter
  binding fails)
*/

static void test_bug1644()
{
  MYSQL_STMT *stmt;
  MYSQL_RES *result;
  MYSQL_ROW row;
  MYSQL_BIND bind[4];
  int num;
  my_bool isnull;
  int rc, i;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_bug1644");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS foo_dfr");
  myquery(rc);

  rc= mysql_query(mysql,
           "CREATE TABLE foo_dfr(col1 int, col2 int, col3 int, col4 int);");
  myquery(rc);

  strmov(query, "INSERT INTO foo_dfr VALUES (?, ?, ?, ? )");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 4);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  num= 22;
  isnull= 0;
  for (i= 0 ; i < 4 ; i++)
  {
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *)&num;
    bind[i].is_null= &isnull;
  }

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  isnull= 1;
  for (i= 0 ; i < 4 ; i++)
    bind[i].is_null= &isnull;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  isnull= 0;
  num= 88;
  for (i= 0 ; i < 4 ; i++)
    bind[i].is_null= &isnull;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "SELECT * FROM foo_dfr");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 3);

  mysql_data_seek(result, 0);

  row= mysql_fetch_row(result);
  mytest(row);
  for (i= 0 ; i < 4 ; i++)
  {
    DIE_UNLESS(strcmp(row[i], "22") == 0);
  }
  row= mysql_fetch_row(result);
  mytest(row);
  for (i= 0 ; i < 4 ; i++)
  {
    DIE_UNLESS(row[i] == 0);
  }
  row= mysql_fetch_row(result);
  mytest(row);
  for (i= 0 ; i < 4 ; i++)
  {
    DIE_UNLESS(strcmp(row[i], "88") == 0);
  }
  row= mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
}


/* Test simple select show */

static void test_select_show()
{
  MYSQL_STMT *stmt;
  int        rc;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_select_show");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_show");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_show(id int(4) NOT NULL primary "
                         " key, name char(2))");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "show columns from test_show");
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "show tables from mysql like ?");
  check_stmt_r(stmt);

  strxmov(query, "show tables from ", current_db, " like \'test_show\'", NullS);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "describe test_show");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "show keys from test_show");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);
  mysql_stmt_close(stmt);
}


/* Test simple update */

static void test_simple_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData= 1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong      length[2];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_simple_update");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_update");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_update(col1 int, "
                         " col2 varchar(50), col3 int )");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_update VALUES(1, 'MySQL', 100)");
  myquery(rc);

  verify_affected_rows(1);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* insert by prepare */
  strmov(query, "UPDATE test_update SET col2= ? WHERE col1= ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  nData= 1;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= szData;                /* string data */
  bind[0].buffer_length= sizeof(szData);
  bind[0].length= &length[0];
  length[0]= my_sprintf(szData, (szData, "updated-data"));

  bind[1].buffer= (void *) &nData;
  bind[1].buffer_type= MYSQL_TYPE_LONG;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  verify_affected_rows(1);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM test_update");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);
}


/* Test simple long data handling */

static void test_long_data()
{
  MYSQL_STMT *stmt;
  int        rc, int_data;
  char       *data= NullS;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_long_data");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_long_data");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_long_data(col1 int, "
                         "      col2 long varchar, col3 long varbinary)");
  myquery(rc);

  strmov(query, "INSERT INTO test_long_data(col1, col2) VALUES(?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  strmov(query, "INSERT INTO test_long_data(col1, col2, col3) VALUES(?, ?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 3);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer= (void *)&int_data;
  bind[0].buffer_type= MYSQL_TYPE_LONG;

  bind[1].buffer_type= MYSQL_TYPE_STRING;

  bind[2]= bind[1];
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  int_data= 999;
  data= (char *)"Michael";

  /* supply data in pieces */
  rc= mysql_stmt_send_long_data(stmt, 1, data, strlen(data));
  data= (char *)" 'Monty' Widenius";
  rc= mysql_stmt_send_long_data(stmt, 1, data, strlen(data));
  check_execute(stmt, rc);
  rc= mysql_stmt_send_long_data(stmt, 2, "Venu (venu@mysql.com)", 4);
  check_execute(stmt, rc);

  /* execute */
  rc= mysql_stmt_execute(stmt);
  if (!opt_silent)
    fprintf(stdout, " mysql_stmt_execute() returned %d\n", rc);
  check_execute(stmt, rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc= mysql_query(mysql, "SELECT * FROM test_long_data");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);

  verify_col_data("test_long_data", "col1", "999");
  verify_col_data("test_long_data", "col2", "Michael 'Monty' Widenius");
  verify_col_data("test_long_data", "col3", "Venu");
  mysql_stmt_close(stmt);
}


/* Test long data (string) handling */

static void test_long_data_str()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       data[255];
  long       length;
  ulong      length1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  my_bool    is_null[2];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_long_data_str");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_long_data_str");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_long_data_str(id int, longstr long varchar)");
  myquery(rc);

  strmov(query, "INSERT INTO test_long_data_str VALUES(?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer= (void *)&length;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].is_null= &is_null[0];
  is_null[0]= 0;
  length= 0;

  bind[1].buffer= data;                          /* string data */
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].length= &length1;
  bind[1].is_null= &is_null[1];
  is_null[1]= 0;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  length= 40;
  strmov(data, "MySQL AB");

  /* supply data in pieces */
  for(i= 0; i < 4; i++)
  {
    rc= mysql_stmt_send_long_data(stmt, 1, (char *)data, 5);
    check_execute(stmt, rc);
  }
  /* execute */
  rc= mysql_stmt_execute(stmt);
  if (!opt_silent)
    fprintf(stdout, " mysql_stmt_execute() returned %d\n", rc);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc= mysql_query(mysql, "SELECT LENGTH(longstr), longstr FROM test_long_data_str");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);

  my_sprintf(data, (data, "%d", i*5));
  verify_col_data("test_long_data_str", "LENGTH(longstr)", data);
  data[0]= '\0';
  while (i--)
   strxmov(data, data, "MySQL", NullS);
  verify_col_data("test_long_data_str", "longstr", data);

  rc= mysql_query(mysql, "DROP TABLE test_long_data_str");
  myquery(rc);
}


/* Test long data (string) handling */

static void test_long_data_str1()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       data[255];
  long       length;
  ulong      max_blob_length, blob_length, length1;
  my_bool    true_value;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  MYSQL_FIELD *field;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_long_data_str1");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_long_data_str");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_long_data_str(longstr long varchar, blb long varbinary)");
  myquery(rc);

  strmov(query, "INSERT INTO test_long_data_str VALUES(?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer= data;            /* string data */
  bind[0].buffer_length= sizeof(data);
  bind[0].length= &length1;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  length1= 0;

  bind[1]= bind[0];
  bind[1].buffer_type= MYSQL_TYPE_BLOB;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);
  length= my_sprintf(data, (data, "MySQL AB"));

  /* supply data in pieces */
  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_send_long_data(stmt, 0, data, length);
    check_execute(stmt, rc);

    rc= mysql_stmt_send_long_data(stmt, 1, data, 2);
    check_execute(stmt, rc);
  }

  /* execute */
  rc= mysql_stmt_execute(stmt);
  if (!opt_silent)
    fprintf(stdout, " mysql_stmt_execute() returned %d\n", rc);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc= mysql_query(mysql, "SELECT LENGTH(longstr), longstr, LENGTH(blb), blb FROM test_long_data_str");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);

  mysql_field_seek(result, 1);
  field= mysql_fetch_field(result);
  max_blob_length= field->max_length;

  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);

  my_sprintf(data, (data, "%ld", (long)i*length));
  verify_col_data("test_long_data_str", "length(longstr)", data);

  my_sprintf(data, (data, "%d", i*2));
  verify_col_data("test_long_data_str", "length(blb)", data);

  /* Test length of field->max_length */
  stmt= mysql_simple_prepare(mysql, "SELECT * from test_long_data_str");
  check_stmt(stmt);
  verify_param_count(stmt, 0);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  result= mysql_stmt_result_metadata(stmt);
  field= mysql_fetch_fields(result);

  /* First test what happens if STMT_ATTR_UPDATE_MAX_LENGTH is not used */
  DIE_UNLESS(field->max_length == 0);
  mysql_free_result(result);

  /* Enable updating of field->max_length */
  true_value= 1;
  mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (void*) &true_value);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  result= mysql_stmt_result_metadata(stmt);
  field= mysql_fetch_fields(result);

  DIE_UNLESS(field->max_length == max_blob_length);

  /* Fetch results into a data buffer that is smaller than data */
  bzero((char*) bind, sizeof(*bind));
  bind[0].buffer_type= MYSQL_TYPE_BLOB;
  bind[0].buffer= (void *) &data; /* this buffer won't be altered */
  bind[0].buffer_length= 16;
  bind[0].length= &blob_length;
  bind[0].error= &bind[0].error_value;
  rc= mysql_stmt_bind_result(stmt, bind);
  data[16]= 0;

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_DATA_TRUNCATED);
  DIE_UNLESS(bind[0].error_value);
  DIE_UNLESS(strlen(data) == 16);
  DIE_UNLESS(blob_length == max_blob_length);

  /* Fetch all data */
  bzero((char*) (bind+1), sizeof(*bind));
  bind[1].buffer_type= MYSQL_TYPE_BLOB;
  bind[1].buffer= (void *) &data; /* this buffer won't be altered */
  bind[1].buffer_length= sizeof(data);
  bind[1].length= &blob_length;
  bzero(data, sizeof(data));
  mysql_stmt_fetch_column(stmt, bind+1, 0, 0);
  DIE_UNLESS(strlen(data) == max_blob_length);

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  /* Drop created table */
  rc= mysql_query(mysql, "DROP TABLE test_long_data_str");
  myquery(rc);
}


/* Test long data (binary) handling */

static void test_long_data_bin()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       data[255];
  long       length;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  char query[MAX_TEST_QUERY_LENGTH];


  myheader("test_long_data_bin");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_long_data_bin");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_long_data_bin(id int, longbin long varbinary)");
  myquery(rc);

  strmov(query, "INSERT INTO test_long_data_bin VALUES(?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer= (void *)&length;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  length= 0;

  bind[1].buffer= data;           /* string data */
  bind[1].buffer_type= MYSQL_TYPE_LONG_BLOB;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  length= 10;
  strmov(data, "MySQL AB");

  /* supply data in pieces */
  {
    int i;
    for (i= 0; i < 100; i++)
    {
      rc= mysql_stmt_send_long_data(stmt, 1, (char *)data, 4);
      check_execute(stmt, rc);
    }
  }
  /* execute */
  rc= mysql_stmt_execute(stmt);
  if (!opt_silent)
    fprintf(stdout, " mysql_stmt_execute() returned %d\n", rc);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc= mysql_query(mysql, "SELECT LENGTH(longbin), longbin FROM test_long_data_bin");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);
}


/* Test simple delete */

static void test_simple_delete()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[30]= {0};
  int        nData= 1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong length[2];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_simple_delete");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_simple_delete");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_simple_delete(col1 int, \
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_simple_delete VALUES(1, 'MySQL', 100)");
  myquery(rc);

  verify_affected_rows(1);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* insert by prepare */
  strmov(query, "DELETE FROM test_simple_delete WHERE col1= ? AND "
                "CONVERT(col2 USING utf8)= ? AND col3= 100");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  nData= 1;
  strmov(szData, "MySQL");
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= szData;               /* string data */
  bind[1].buffer_length= sizeof(szData);
  bind[1].length= &length[1];
  length[1]= 5;

  bind[0].buffer= (void *)&nData;
  bind[0].buffer_type= MYSQL_TYPE_LONG;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  verify_affected_rows(1);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM test_simple_delete");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 0);
  mysql_free_result(result);
}


/* Test simple update */

static void test_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData= 1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong length[2];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_update");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_update");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_update("
                               "col1 int primary key auto_increment, "
                               "col2 varchar(50), col3 int )");
  myquery(rc);

  strmov(query, "INSERT INTO test_update(col2, col3) VALUES(?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  /* string data */
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= szData;
  bind[0].buffer_length= sizeof(szData);
  bind[0].length= &length[0];
  length[0]= my_sprintf(szData, (szData, "inserted-data"));

  bind[1].buffer= (void *)&nData;
  bind[1].buffer_type= MYSQL_TYPE_LONG;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  nData= 100;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  verify_affected_rows(1);
  mysql_stmt_close(stmt);

  strmov(query, "UPDATE test_update SET col2= ? WHERE col3= ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);
  nData= 100;

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= szData;
  bind[0].buffer_length= sizeof(szData);
  bind[0].length= &length[0];
  length[0]= my_sprintf(szData, (szData, "updated-data"));

  bind[1].buffer= (void *)&nData;
  bind[1].buffer_type= MYSQL_TYPE_LONG;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  verify_affected_rows(1);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM test_update");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);
}


/* Test prepare without parameters */

static void test_prepare_noparam()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_prepare_noparam");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS my_prepare");
  myquery(rc);


  rc= mysql_query(mysql, "CREATE TABLE my_prepare(col1 int, col2 varchar(50))");
  myquery(rc);

  /* insert by prepare */
  strmov(query, "INSERT INTO my_prepare VALUES(10, 'venu')");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM my_prepare");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);
}


/* Test simple bind result */

static void test_bind_result()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        nData;
  ulong      length1;
  char       szData[100];
  MYSQL_BIND bind[2];
  my_bool    is_null[2];

  myheader("test_bind_result");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_result(col1 int , col2 varchar(50))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_bind_result VALUES(10, 'venu')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_bind_result VALUES(20, 'MySQL')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_bind_result(col2) VALUES('monty')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* fetch */

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *) &nData;      /* integer data */
  bind[0].is_null= &is_null[0];

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= szData;                /* string data */
  bind[1].buffer_length= sizeof(szData);
  bind[1].length= &length1;
  bind[1].is_null= &is_null[1];

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_bind_result");
  check_stmt(stmt);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 1: %d, %s(%lu)", nData, szData, length1);
  DIE_UNLESS(nData == 10);
  DIE_UNLESS(strcmp(szData, "venu") == 0);
  DIE_UNLESS(length1 == 4);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 2: %d, %s(%lu)", nData, szData, length1);
  DIE_UNLESS(nData == 20);
  DIE_UNLESS(strcmp(szData, "MySQL") == 0);
  DIE_UNLESS(length1 == 5);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent && is_null[0])
    fprintf(stdout, "\n row 3: NULL, %s(%lu)", szData, length1);
  DIE_UNLESS(is_null[0]);
  DIE_UNLESS(strcmp(szData, "monty") == 0);
  DIE_UNLESS(length1 == 5);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test ext bind result */

static void test_bind_result_ext()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  uchar      t_data;
  short      s_data;
  int        i_data;
  longlong   b_data;
  float      f_data;
  double     d_data;
  char       szData[20], bData[20];
  ulong       szLength, bLength;
  MYSQL_BIND bind[8];
  ulong      length[8];
  my_bool    is_null[8];
  char	     llbuf[22];
  myheader("test_bind_result_ext");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_result(c1 tinyint, "
                                                      " c2 smallint, "
                                                      " c3 int, c4 bigint, "
                                                      " c5 float, c6 double, "
                                                      " c7 varbinary(10), "
                                                      " c8 varchar(50))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_bind_result "
                         "VALUES (19, 2999, 3999, 4999999, "
                         " 2345.6, 5678.89563, 'venu', 'mysql')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  bzero((char*) bind, sizeof(bind));
  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].length=  &length[i];
    bind[i].is_null= &is_null[i];
  }

  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[0].buffer= (void *)&t_data;

  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;

  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[1].buffer= (void *)&s_data;

  bind[2].buffer= (void *)&i_data;
  bind[3].buffer= (void *)&b_data;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (void *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)szData;
  bind[6].buffer_length= sizeof(szData);
  bind[6].length= &szLength;

  bind[7].buffer_type= MYSQL_TYPE_TINY_BLOB;
  bind[7].buffer= (void *)&bData;
  bind[7].length= &bLength;
  bind[7].buffer_length= sizeof(bData);

  stmt= mysql_simple_prepare(mysql, "select * from test_bind_result");
  check_stmt(stmt);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n data (tiny)   : %d", t_data);
    fprintf(stdout, "\n data (short)  : %d", s_data);
    fprintf(stdout, "\n data (int)    : %d", i_data);
    fprintf(stdout, "\n data (big)    : %s", llstr(b_data, llbuf));

    fprintf(stdout, "\n data (float)  : %f", f_data);
    fprintf(stdout, "\n data (double) : %f", d_data);

    fprintf(stdout, "\n data (str)    : %s(%lu)", szData, szLength);

    bData[bLength]= '\0';                         /* bData is binary */
    fprintf(stdout, "\n data (bin)    : %s(%lu)", bData, bLength);
  }

  DIE_UNLESS(t_data == 19);
  DIE_UNLESS(s_data == 2999);
  DIE_UNLESS(i_data == 3999);
  DIE_UNLESS(b_data == 4999999);
  /*DIE_UNLESS(f_data == 2345.60);*/
  /*DIE_UNLESS(d_data == 5678.89563);*/
  DIE_UNLESS(strcmp(szData, "venu") == 0);
  DIE_UNLESS(strncmp(bData, "mysql", 5) == 0);
  DIE_UNLESS(szLength == 4);
  DIE_UNLESS(bLength == 5);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test ext bind result */

static void test_bind_result_ext1()
{
  MYSQL_STMT *stmt;
  uint       i;
  int        rc;
  char       t_data[20];
  float      s_data;
  short      i_data;
  uchar      b_data;
  int        f_data;
  long       bData;
  char       d_data[20];
  double     szData;
  MYSQL_BIND bind[8];
  ulong      length[8];
  my_bool    is_null[8];
  myheader("test_bind_result_ext1");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_result(c1 tinyint, c2 smallint, \
                                                        c3 int, c4 bigint, \
                                                        c5 float, c6 double, \
                                                        c7 varbinary(10), \
                                                        c8 varchar(10))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_bind_result VALUES(120, 2999, 3999, 54, \
                                                              2.6, 58.89, \
                                                              '206', '6.7')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *) t_data;
  bind[0].buffer_length= sizeof(t_data);
  bind[0].error= &bind[0].error_value;

  bind[1].buffer_type= MYSQL_TYPE_FLOAT;
  bind[1].buffer= (void *)&s_data;
  bind[1].buffer_length= 0;
  bind[1].error= &bind[1].error_value;

  bind[2].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer= (void *)&i_data;
  bind[2].buffer_length= 0;
  bind[2].error= &bind[2].error_value;

  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (void *)&b_data;
  bind[3].buffer_length= 0;
  bind[3].error= &bind[3].error_value;

  bind[4].buffer_type= MYSQL_TYPE_LONG;
  bind[4].buffer= (void *)&f_data;
  bind[4].buffer_length= 0;
  bind[4].error= &bind[4].error_value;

  bind[5].buffer_type= MYSQL_TYPE_STRING;
  bind[5].buffer= (void *)d_data;
  bind[5].buffer_length= sizeof(d_data);
  bind[5].error= &bind[5].error_value;

  bind[6].buffer_type= MYSQL_TYPE_LONG;
  bind[6].buffer= (void *)&bData;
  bind[6].buffer_length= 0;
  bind[6].error= &bind[6].error_value;

  bind[7].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[7].buffer= (void *)&szData;
  bind[7].buffer_length= 0;
  bind[7].error= &bind[7].error_value;

  for (i= 0; i < array_elements(bind); i++)
  {
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }

  stmt= mysql_simple_prepare(mysql, "select * from test_bind_result");
  check_stmt(stmt);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  printf("rc=%d\n", rc);
  DIE_UNLESS(rc == 0);

  if (!opt_silent)
  {
    fprintf(stdout, "\n data (tiny)   : %s(%lu)", t_data, length[0]);
    fprintf(stdout, "\n data (short)  : %f(%lu)", s_data, length[1]);
    fprintf(stdout, "\n data (int)    : %d(%lu)", i_data, length[2]);
    fprintf(stdout, "\n data (big)    : %d(%lu)", b_data, length[3]);

    fprintf(stdout, "\n data (float)  : %d(%lu)", f_data, length[4]);
    fprintf(stdout, "\n data (double) : %s(%lu)", d_data, length[5]);

    fprintf(stdout, "\n data (bin)    : %ld(%lu)", bData, length[6]);
    fprintf(stdout, "\n data (str)    : %g(%lu)", szData, length[7]);
  }

  DIE_UNLESS(strcmp(t_data, "120") == 0);
  DIE_UNLESS(i_data == 3999);
  DIE_UNLESS(f_data == 2);
  DIE_UNLESS(strcmp(d_data, "58.89") == 0);
  DIE_UNLESS(b_data == 54);

  DIE_UNLESS(length[0] == 3);
  DIE_UNLESS(length[1] == 4);
  DIE_UNLESS(length[2] == 2);
  DIE_UNLESS(length[3] == 1);
  DIE_UNLESS(length[4] == 4);
  DIE_UNLESS(length[5] == 5);
  DIE_UNLESS(length[6] == 4);
  DIE_UNLESS(length[7] == 8);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Generalized fetch conversion routine for all basic types */

static void bind_fetch(int row_count)
{
  MYSQL_STMT   *stmt;
  int          rc, i, count= row_count;
  int32        data[10];
  int8         i8_data;
  int16        i16_data;
  int32        i32_data;
  longlong     i64_data;
  float        f_data;
  double       d_data;
  char         s_data[10];
  ulong        length[10];
  MYSQL_BIND   bind[7];
  my_bool      is_null[7];

  stmt= mysql_simple_prepare(mysql, "INSERT INTO test_bind_fetch VALUES "
                                    "(?, ?, ?, ?, ?, ?, ?)");
  check_stmt(stmt);

  verify_param_count(stmt, 7);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void *) &data[i];
  }
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  while (count--)
  {
    rc= 10+count;
    for (i= 0; i < (int) array_elements(bind); i++)
    {
      data[i]= rc+i;
      rc+= 12;
    }
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }

  rc= mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  rc= my_stmt_result("SELECT * FROM test_bind_fetch");
  DIE_UNLESS(row_count == rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_bind_fetch");
  check_stmt(stmt);

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].buffer= (void *) &data[i];
    bind[i].length= &length[i];
    bind[i].is_null= &is_null[i];
  }

  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[0].buffer= (void *)&i8_data;

  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[1].buffer= (void *)&i16_data;

  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[2].buffer= (void *)&i32_data;

  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[3].buffer= (void *)&i64_data;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (void *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)&s_data;
  bind[6].buffer_length= sizeof(s_data);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  while (row_count--)
  {
    rc= mysql_stmt_fetch(stmt);
    check_execute(stmt, rc);

    if (!opt_silent)
    {
      fprintf(stdout, "\n");
      fprintf(stdout, "\n tiny     : %ld(%lu)", (ulong) i8_data, length[0]);
      fprintf(stdout, "\n short    : %ld(%lu)", (ulong) i16_data, length[1]);
      fprintf(stdout, "\n int      : %ld(%lu)", (ulong) i32_data, length[2]);
      fprintf(stdout, "\n longlong : %ld(%lu)", (ulong) i64_data, length[3]);
      fprintf(stdout, "\n float    : %f(%lu)",  f_data,  length[4]);
      fprintf(stdout, "\n double   : %g(%lu)",  d_data,  length[5]);
      fprintf(stdout, "\n char     : %s(%lu)",  s_data,  length[6]);
    }
    rc= 10+row_count;

    /* TINY */
    DIE_UNLESS((int) i8_data == rc);
    DIE_UNLESS(length[0] == 1);
    rc+= 13;

    /* SHORT */
    DIE_UNLESS((int) i16_data == rc);
    DIE_UNLESS(length[1] == 2);
    rc+= 13;

    /* LONG */
    DIE_UNLESS((int) i32_data == rc);
    DIE_UNLESS(length[2] == 4);
    rc+= 13;

    /* LONGLONG */
    DIE_UNLESS((int) i64_data == rc);
    DIE_UNLESS(length[3] == 8);
    rc+= 13;

    /* FLOAT */
    DIE_UNLESS((int)f_data == rc);
    DIE_UNLESS(length[4] == 4);
    rc+= 13;

    /* DOUBLE */
    DIE_UNLESS((int)d_data == rc);
    DIE_UNLESS(length[5] == 8);
    rc+= 13;

    /* CHAR */
    {
      char buff[20];
      long len= my_sprintf(buff, (buff, "%d", rc));
      DIE_UNLESS(strcmp(s_data, buff) == 0);
      DIE_UNLESS(length[6] == (ulong) len);
    }
  }
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test fetching of date, time and ts */

static void test_fetch_date()
{
  MYSQL_STMT *stmt;
  uint       i;
  int        rc, year;
  char       date[25], time[25], ts[25], ts_4[25], ts_6[20], dt[20];
  ulong      d_length, t_length, ts_length, ts4_length, ts6_length,
             dt_length, y_length;
  MYSQL_BIND bind[8];
  my_bool    is_null[8];
  ulong      length[8];

  myheader("test_fetch_date");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_result(c1 date, c2 time, \
                                                        c3 timestamp(14), \
                                                        c4 year, \
                                                        c5 datetime, \
                                                        c6 timestamp(4), \
                                                        c7 timestamp(6))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_bind_result VALUES('2002-01-02', \
                                                              '12:49:00', \
                                                              '2002-01-02 17:46:59', \
                                                              2010, \
                                                              '2010-07-10', \
                                                              '2020', '1999-12-29')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  bzero((char*) bind, sizeof(bind));
  for (i= 0; i < array_elements(bind); i++)
  {
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[1]= bind[2]= bind[0];

  bind[0].buffer= (void *)&date;
  bind[0].buffer_length= sizeof(date);
  bind[0].length= &d_length;

  bind[1].buffer= (void *)&time;
  bind[1].buffer_length= sizeof(time);
  bind[1].length= &t_length;

  bind[2].buffer= (void *)&ts;
  bind[2].buffer_length= sizeof(ts);
  bind[2].length= &ts_length;

  bind[3].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer= (void *)&year;
  bind[3].length= &y_length;

  bind[4].buffer_type= MYSQL_TYPE_STRING;
  bind[4].buffer= (void *)&dt;
  bind[4].buffer_length= sizeof(dt);
  bind[4].length= &dt_length;

  bind[5].buffer_type= MYSQL_TYPE_STRING;
  bind[5].buffer= (void *)&ts_4;
  bind[5].buffer_length= sizeof(ts_4);
  bind[5].length= &ts4_length;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (void *)&ts_6;
  bind[6].buffer_length= sizeof(ts_6);
  bind[6].length= &ts6_length;

  rc= my_stmt_result("SELECT * FROM test_bind_result");
  DIE_UNLESS(rc == 1);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_bind_result");
  check_stmt(stmt);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  ts_4[0]= '\0';
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n date   : %s(%lu)", date, d_length);
    fprintf(stdout, "\n time   : %s(%lu)", time, t_length);
    fprintf(stdout, "\n ts     : %s(%lu)", ts, ts_length);
    fprintf(stdout, "\n year   : %d(%lu)", year, y_length);
    fprintf(stdout, "\n dt     : %s(%lu)", dt,  dt_length);
    fprintf(stdout, "\n ts(4)  : %s(%lu)", ts_4, ts4_length);
    fprintf(stdout, "\n ts(6)  : %s(%lu)", ts_6, ts6_length);
  }

  DIE_UNLESS(strcmp(date, "2002-01-02") == 0);
  DIE_UNLESS(d_length == 10);

  DIE_UNLESS(strcmp(time, "12:49:00") == 0);
  DIE_UNLESS(t_length == 8);

  DIE_UNLESS(strcmp(ts, "2002-01-02 17:46:59") == 0);
  DIE_UNLESS(ts_length == 19);

  DIE_UNLESS(year == 2010);
  DIE_UNLESS(y_length == 4);

  DIE_UNLESS(strcmp(dt, "2010-07-10 00:00:00") == 0);
  DIE_UNLESS(dt_length == 19);

  DIE_UNLESS(strcmp(ts_4, "0000-00-00 00:00:00") == 0);
  DIE_UNLESS(ts4_length == strlen("0000-00-00 00:00:00"));

  DIE_UNLESS(strcmp(ts_6, "1999-12-29 00:00:00") == 0);
  DIE_UNLESS(ts6_length == 19);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test fetching of str to all types */

static void test_fetch_str()
{
  int rc;

  myheader("test_fetch_str");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_fetch(c1 char(10), \
                                                     c2 char(10), \
                                                     c3 char(20), \
                                                     c4 char(20), \
                                                     c5 char(30), \
                                                     c6 char(40), \
                                                     c7 char(20))");
  myquery(rc);

  bind_fetch(3);
}


/* Test fetching of long to all types */

static void test_fetch_long()
{
  int rc;

  myheader("test_fetch_long");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_fetch(c1 int unsigned, \
                                                     c2 int unsigned, \
                                                     c3 int, \
                                                     c4 int, \
                                                     c5 int, \
                                                     c6 int unsigned, \
                                                     c7 int)");
  myquery(rc);

  bind_fetch(4);
}


/* Test fetching of short to all types */

static void test_fetch_short()
{
  int rc;

  myheader("test_fetch_short");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_fetch(c1 smallint unsigned, \
                                                     c2 smallint, \
                                                     c3 smallint unsigned, \
                                                     c4 smallint, \
                                                     c5 smallint, \
                                                     c6 smallint, \
                                                     c7 smallint unsigned)");
  myquery(rc);

  bind_fetch(5);
}


/* Test fetching of tiny to all types */

static void test_fetch_tiny()
{
  int rc;

  myheader("test_fetch_tiny");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_fetch(c1 tinyint unsigned, \
                                                     c2 tinyint, \
                                                     c3 tinyint unsigned, \
                                                     c4 tinyint, \
                                                     c5 tinyint, \
                                                     c6 tinyint, \
                                                     c7 tinyint unsigned)");
  myquery(rc);

  bind_fetch(3);

}


/* Test fetching of longlong to all types */

static void test_fetch_bigint()
{
  int rc;

  myheader("test_fetch_bigint");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_fetch(c1 bigint, \
                                                     c2 bigint, \
                                                     c3 bigint unsigned, \
                                                     c4 bigint unsigned, \
                                                     c5 bigint unsigned, \
                                                     c6 bigint unsigned, \
                                                     c7 bigint unsigned)");
  myquery(rc);

  bind_fetch(2);

}


/* Test fetching of float to all types */

static void test_fetch_float()
{
  int rc;

  myheader("test_fetch_float");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_fetch(c1 float(3), \
                                                     c2 float, \
                                                     c3 float unsigned, \
                                                     c4 float, \
                                                     c5 float, \
                                                     c6 float, \
                                                     c7 float(10) unsigned)");
  myquery(rc);

  bind_fetch(2);

}


/* Test fetching of double to all types */

static void test_fetch_double()
{
  int rc;

  myheader("test_fetch_double");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bind_fetch(c1 double(5, 2), "
                         "c2 double unsigned, c3 double unsigned, "
                         "c4 double unsigned, c5 double unsigned, "
                         "c6 double unsigned, c7 double unsigned)");
  myquery(rc);

  bind_fetch(3);

}


/* Test simple prepare with all possible types */

static void test_prepare_ext()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       *sql;
  int        nData= 1;
  char       tData= 1;
  short      sData= 10;
  longlong   bData= 20;
  MYSQL_BIND bind[6];
  char query[MAX_TEST_QUERY_LENGTH];
  myheader("test_prepare_ext");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prepare_ext");
  myquery(rc);

  sql= (char *)"CREATE TABLE test_prepare_ext"
               "("
               " c1  tinyint,"
               " c2  smallint,"
               " c3  mediumint,"
               " c4  int,"
               " c5  integer,"
               " c6  bigint,"
               " c7  float,"
               " c8  double,"
               " c9  double precision,"
               " c10 real,"
               " c11 decimal(7, 4),"
               " c12 numeric(8, 4),"
               " c13 date,"
               " c14 datetime,"
               " c15 timestamp(14),"
               " c16 time,"
               " c17 year,"
               " c18 bit,"
               " c19 bool,"
               " c20 char,"
               " c21 char(10),"
               " c22 varchar(30),"
               " c23 tinyblob,"
               " c24 tinytext,"
               " c25 blob,"
               " c26 text,"
               " c27 mediumblob,"
               " c28 mediumtext,"
               " c29 longblob,"
               " c30 longtext,"
               " c31 enum('one', 'two', 'three'),"
               " c32 set('monday', 'tuesday', 'wednesday'))";

  rc= mysql_query(mysql, sql);
  myquery(rc);

  /* insert by prepare - all integers */
  strmov(query, (char *)"INSERT INTO test_prepare_ext(c1, c2, c3, c4, c5, c6) VALUES(?, ?, ?, ?, ?, ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 6);

  /* Always bzero all members of bind parameter */
  bzero((char*) bind, sizeof(bind));

  /*tinyint*/
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[0].buffer= (void *)&tData;

  /*smallint*/
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[1].buffer= (void *)&sData;

  /*mediumint*/
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[2].buffer= (void *)&nData;

  /*int*/
  bind[3].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer= (void *)&nData;

  /*integer*/
  bind[4].buffer_type= MYSQL_TYPE_LONG;
  bind[4].buffer= (void *)&nData;

  /*bigint*/
  bind[5].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[5].buffer= (void *)&bData;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  /*
  *  integer to integer
  */
  for (nData= 0; nData<10; nData++, tData++, sData++, bData++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/

  stmt= mysql_simple_prepare(mysql, "SELECT c1, c2, c3, c4, c5, c6 "
                                    "FROM test_prepare_ext");
  check_stmt(stmt);

  /* get the result */
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(nData == rc);

  mysql_stmt_close(stmt);
}


/* Test real and alias names */

static void test_field_names()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_field_names");

  if (!opt_silent)
    fprintf(stdout, "\n %d, %d, %d", MYSQL_TYPE_DECIMAL, MYSQL_TYPE_NEWDATE, MYSQL_TYPE_ENUM);
  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_field_names1");
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_field_names2");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_field_names1(id int, name varchar(50))");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_field_names2(id int, name varchar(50))");
  myquery(rc);

  /* with table name included with TRUE column name */
  rc= mysql_query(mysql, "SELECT id as 'id-alias' FROM test_field_names1");
  myquery(rc);

  result= mysql_use_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 0);
  mysql_free_result(result);

  /* with table name included with TRUE column name */
  rc= mysql_query(mysql, "SELECT t1.id as 'id-alias', test_field_names2.name FROM test_field_names1 t1, test_field_names2");
  myquery(rc);

  result= mysql_use_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 0);
  mysql_free_result(result);
}


/* Test warnings */

static void test_warnings()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_warnings");

  mysql_query(mysql, "DROP TABLE if exists test_non_exists");

  rc= mysql_query(mysql, "DROP TABLE if exists test_non_exists");
  myquery(rc);

  if (!opt_silent)
    fprintf(stdout, "\n total warnings: %d", mysql_warning_count(mysql));
  rc= mysql_query(mysql, "SHOW WARNINGS");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);
}


/* Test errors */

static void test_errors()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_errors");

  mysql_query(mysql, "DROP TABLE if exists test_non_exists");

  rc= mysql_query(mysql, "DROP TABLE test_non_exists");
  myquery_r(rc);

  rc= mysql_query(mysql, "SHOW ERRORS");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  (void) my_process_result_set(result);
  mysql_free_result(result);
}


/* Test simple prepare-insert */

static void test_insert()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       str_data[50];
  char       tiny_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong      length;

  myheader("test_insert");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prep_insert");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prep_insert(col1 tinyint, \
                                col2 varchar(50))");
  myquery(rc);

  /* insert by prepare */
  stmt= mysql_simple_prepare(mysql,
                             "INSERT INTO test_prep_insert VALUES(?, ?)");
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  /* tinyint */
  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[0].buffer= (void *)&tiny_data;

  /* string */
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= str_data;
  bind[1].buffer_length= sizeof(str_data);;
  bind[1].length= &length;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data= 0; tiny_data < 3; tiny_data++)
  {
    length= my_sprintf(str_data, (str_data, "MySQL%d", tiny_data));
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc= mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exist */
  rc= mysql_query(mysql, "SELECT * FROM test_prep_insert");
  myquery(rc);

  /* get the result */
  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS((int) tiny_data == rc);
  mysql_free_result(result);

}


/* Test simple prepare-resultset info */

static void test_prepare_resultset()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;

  myheader("test_prepare_resultset");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prepare_resultset");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prepare_resultset(id int, \
                                name varchar(50), extra double)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_prepare_resultset");
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);
  my_print_result_metadata(result);
  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/* Test field flags (verify .NET provider) */

static void test_field_flags()
{
  int          rc;
  MYSQL_RES    *result;
  MYSQL_FIELD  *field;
  unsigned int i;


  myheader("test_field_flags");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_field_flags");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_field_flags(id int NOT NULL AUTO_INCREMENT PRIMARY KEY, \
                                                        id1 int NOT NULL, \
                                                        id2 int UNIQUE, \
                                                        id3 int, \
                                                        id4 int NOT NULL, \
                                                        id5 int, \
                                                        KEY(id3, id4))");
  myquery(rc);

  /* with table name included with TRUE column name */
  rc= mysql_query(mysql, "SELECT * FROM test_field_flags");
  myquery(rc);

  result= mysql_use_result(mysql);
  mytest(result);

  mysql_field_seek(result, 0);
  if (!opt_silent)
    fputc('\n', stdout);

  for(i= 0; i< mysql_num_fields(result); i++)
  {
    field= mysql_fetch_field(result);
    if (!opt_silent)
    {
      fprintf(stdout, "\n field:%d", i);
      if (field->flags & NOT_NULL_FLAG)
        fprintf(stdout, "\n  NOT_NULL_FLAG");
      if (field->flags & PRI_KEY_FLAG)
        fprintf(stdout, "\n  PRI_KEY_FLAG");
      if (field->flags & UNIQUE_KEY_FLAG)
        fprintf(stdout, "\n  UNIQUE_KEY_FLAG");
      if (field->flags & MULTIPLE_KEY_FLAG)
        fprintf(stdout, "\n  MULTIPLE_KEY_FLAG");
      if (field->flags & AUTO_INCREMENT_FLAG)
        fprintf(stdout, "\n  AUTO_INCREMENT_FLAG");

    }
  }
  mysql_free_result(result);
}


/* Test mysql_stmt_close for open stmts */

static void test_stmt_close()
{
  MYSQL *lmysql;
  MYSQL_STMT *stmt1, *stmt2, *stmt3, *stmt_x;
  MYSQL_BIND  bind[1];
  MYSQL_RES   *result;
  unsigned int  count;
  int   rc;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_stmt_close");

  if (!opt_silent)
    fprintf(stdout, "\n Establishing a test connection ...");
  if (!(lmysql= mysql_init(NULL)))
  {
    myerror("mysql_init() failed");
    exit(1);
  }
  if (!(mysql_real_connect(lmysql, opt_host, opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket, 0)))
  {
    myerror("connection failed");
    exit(1);
  }
  lmysql->reconnect= 1;
  if (!opt_silent)
    fprintf(stdout, " OK");


  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(lmysql, TRUE);

  rc= mysql_query(lmysql, "DROP TABLE IF EXISTS test_stmt_close");
  myquery(rc);

  rc= mysql_query(lmysql, "CREATE TABLE test_stmt_close(id int)");
  myquery(rc);

  strmov(query, "DO \"nothing\"");
  stmt1= mysql_simple_prepare(lmysql, query);
  check_stmt(stmt1);

  verify_param_count(stmt1, 0);

  strmov(query, "INSERT INTO test_stmt_close(id) VALUES(?)");
  stmt_x= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_x);

  verify_param_count(stmt_x, 1);

  strmov(query, "UPDATE test_stmt_close SET id= ? WHERE id= ?");
  stmt3= mysql_simple_prepare(lmysql, query);
  check_stmt(stmt3);

  verify_param_count(stmt3, 2);

  strmov(query, "SELECT * FROM test_stmt_close WHERE id= ?");
  stmt2= mysql_simple_prepare(lmysql, query);
  check_stmt(stmt2);

  verify_param_count(stmt2, 1);

  rc= mysql_stmt_close(stmt1);
  if (!opt_silent)
    fprintf(stdout, "\n mysql_close_stmt(1) returned: %d", rc);
  DIE_UNLESS(rc == 0);

  /*
    Originally we were going to close all statements automatically in
    mysql_close(). This proved to not work well - users weren't able to
    close statements by hand once mysql_close() had been called.
    Now mysql_close() doesn't free any statements, so this test doesn't
    serve its original designation any more.
    Here we free stmt2 and stmt3 by hande to avoid memory leaks.
  */
  mysql_stmt_close(stmt2);
  mysql_stmt_close(stmt3);
  mysql_close(lmysql);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer= (void *)&count;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  count= 100;

  rc= mysql_stmt_bind_param(stmt_x, bind);
  check_execute(stmt_x, rc);

  rc= mysql_stmt_execute(stmt_x);
  check_execute(stmt_x, rc);

  verify_st_affected_rows(stmt_x, 1);

  rc= mysql_stmt_close(stmt_x);
  if (!opt_silent)
    fprintf(stdout, "\n mysql_close_stmt(x) returned: %d", rc);
  DIE_UNLESS( rc == 0);

  rc= mysql_query(mysql, "SELECT id FROM test_stmt_close");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);
}


/* Test simple set-variable prepare */

static void test_set_variable()
{
  MYSQL_STMT *stmt, *stmt1;
  int        rc;
  int        set_count, def_count, get_count;
  ulong      length;
  char       var[NAME_LEN+1];
  MYSQL_BIND set_bind[1], get_bind[2];

  myheader("test_set_variable");

  mysql_autocommit(mysql, TRUE);

  stmt1= mysql_simple_prepare(mysql, "show variables like 'max_error_count'");
  check_stmt(stmt1);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) get_bind, sizeof(get_bind));

  get_bind[0].buffer_type= MYSQL_TYPE_STRING;
  get_bind[0].buffer= (void *)var;
  get_bind[0].length= &length;
  get_bind[0].buffer_length= (int)NAME_LEN;
  length= NAME_LEN;

  get_bind[1].buffer_type= MYSQL_TYPE_LONG;
  get_bind[1].buffer= (void *)&get_count;

  rc= mysql_stmt_execute(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_bind_result(stmt1, get_bind);
  check_execute(stmt1, rc);

  rc= mysql_stmt_fetch(stmt1);
  check_execute(stmt1, rc);

  if (!opt_silent)
    fprintf(stdout, "\n max_error_count(default): %d", get_count);
  def_count= get_count;

  DIE_UNLESS(strcmp(var, "max_error_count") == 0);
  rc= mysql_stmt_fetch(stmt1);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  stmt= mysql_simple_prepare(mysql, "set max_error_count= ?");
  check_stmt(stmt);

  bzero((char*) set_bind, sizeof(set_bind));

  set_bind[0].buffer_type= MYSQL_TYPE_LONG;
  set_bind[0].buffer= (void *)&set_count;

  rc= mysql_stmt_bind_param(stmt, set_bind);
  check_execute(stmt, rc);

  set_count= 31;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_commit(mysql);

  rc= mysql_stmt_execute(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_fetch(stmt1);
  check_execute(stmt1, rc);

  if (!opt_silent)
    fprintf(stdout, "\n max_error_count         : %d", get_count);
  DIE_UNLESS(get_count == set_count);

  rc= mysql_stmt_fetch(stmt1);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  /* restore back to default */
  set_count= def_count;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_fetch(stmt1);
  check_execute(stmt1, rc);

  if (!opt_silent)
    fprintf(stdout, "\n max_error_count(default): %d", get_count);
  DIE_UNLESS(get_count == set_count);

  rc= mysql_stmt_fetch(stmt1);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
  mysql_stmt_close(stmt1);
}

#if NOT_USED

/* Insert meta info .. */

static void test_insert_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_insert_meta");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prep_insert");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prep_insert(col1 tinyint, \
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query, "INSERT INTO test_prep_insert VALUES(10, 'venu1', 'test')");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  mysql_stmt_close(stmt);

  strmov(query, "INSERT INTO test_prep_insert VALUES(?, 'venu', ?)");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
    fprintf(stdout, "\n obtained: `%s` (expected: `%s`)", field->name, "col1");
  DIE_UNLESS(strcmp(field->name, "col1") == 0);

  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
    fprintf(stdout, "\n obtained: `%s` (expected: `%s`)", field->name, "col3");
  DIE_UNLESS(strcmp(field->name, "col3") == 0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/* Update meta info .. */

static void test_update_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_update_meta");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prep_update");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prep_update(col1 tinyint, \
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query, "UPDATE test_prep_update SET col1=10, col2='venu1' WHERE col3='test'");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  mysql_stmt_close(stmt);

  strmov(query, "UPDATE test_prep_update SET col1=?, col2='venu' WHERE col3=?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
  {
    fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col1");
    fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_update");
  }
  DIE_UNLESS(strcmp(field->name, "col1") == 0);
  DIE_UNLESS(strcmp(field->table, "test_prep_update") == 0);

  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
  {
    fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col3");
    fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_update");
  }
  DIE_UNLESS(strcmp(field->name, "col3") == 0);
  DIE_UNLESS(strcmp(field->table, "test_prep_update") == 0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/* Select meta info .. */

static void test_select_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_select_meta");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prep_select");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prep_select(col1 tinyint, \
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query, "SELECT * FROM test_prep_select WHERE col1=10");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  strmov(query, "SELECT col1, col3 from test_prep_select WHERE col1=? AND col3='test' AND col2= ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  verify_param_count(stmt, 2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
  {
    fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col1");
    fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_select");
  }
  DIE_UNLESS(strcmp(field->name, "col1") == 0);
  DIE_UNLESS(strcmp(field->table, "test_prep_select") == 0);

  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
  {
    fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col2");
    fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_select");
  }
  DIE_UNLESS(strcmp(field->name, "col2") == 0);
  DIE_UNLESS(strcmp(field->table, "test_prep_select") == 0);

  field= mysql_fetch_field(result);
  mytest_r(field);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}
#endif


/* Test FUNCTION field info / DATE_FORMAT() table_name . */

static void test_func_fields()
{
  int        rc;
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_func_fields");

  rc= mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_dateformat");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_dateformat(id int, \
                                                       ts timestamp)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_dateformat(id) values(10)");
  myquery(rc);

  rc= mysql_query(mysql, "SELECT ts FROM test_dateformat");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
    fprintf(stdout, "\n table name: `%s` (expected: `%s`)", field->table,
            "test_dateformat");
  DIE_UNLESS(strcmp(field->table, "test_dateformat") == 0);

  field= mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);

  /* DATE_FORMAT */
  rc= mysql_query(mysql, "SELECT DATE_FORMAT(ts, '%Y') AS 'venu' FROM test_dateformat");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
    fprintf(stdout, "\n table name: `%s` (expected: `%s`)", field->table, "");
  DIE_UNLESS(field->table[0] == '\0');

  field= mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);

  /* FIELD ALIAS TEST */
  rc= mysql_query(mysql, "SELECT DATE_FORMAT(ts, '%Y')  AS 'YEAR' FROM test_dateformat");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  field= mysql_fetch_field(result);
  mytest(field);
  if (!opt_silent)
  {
    printf("\n field name: `%s` (expected: `%s`)", field->name, "YEAR");
    printf("\n field org name: `%s` (expected: `%s`)", field->org_name, "");
  }
  DIE_UNLESS(strcmp(field->name, "YEAR") == 0);
  DIE_UNLESS(field->org_name[0] == '\0');

  field= mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);
}


/* Multiple stmts .. */

static void test_multi_stmt()
{

  MYSQL_STMT  *stmt, *stmt1, *stmt2;
  int         rc;
  uint32      id;
  char        name[50];
  MYSQL_BIND  bind[2];
  ulong       length[2];
  my_bool     is_null[2];
  myheader("test_multi_stmt");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_multi_table");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_multi_table(id int, name char(20))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_multi_table values(10, 'mysql')");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_multi_table "
                                    "WHERE id= ?");
  check_stmt(stmt);

  stmt2= mysql_simple_prepare(mysql, "UPDATE test_multi_table "
                                     "SET name='updated' WHERE id=10");
  check_stmt(stmt2);

  verify_param_count(stmt, 1);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&id;
  bind[0].is_null= &is_null[0];
  bind[0].length= &length[0];
  is_null[0]= 0;
  length[0]= 0;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)name;
  bind[1].buffer_length= sizeof(name);
  bind[1].length= &length[1];
  bind[1].is_null= &is_null[1];

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  id= 10;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  id= 999;
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n int_data: %lu(%lu)", (ulong) id, length[0]);
    fprintf(stdout, "\n str_data: %s(%lu)", name, length[1]);
  }
  DIE_UNLESS(id == 10);
  DIE_UNLESS(strcmp(name, "mysql") == 0);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  /* alter the table schema now */
  stmt1= mysql_simple_prepare(mysql, "DELETE FROM test_multi_table "
                                     "WHERE id= ? AND "
                                     "CONVERT(name USING utf8)=?");
  check_stmt(stmt1);

  verify_param_count(stmt1, 2);

  rc= mysql_stmt_bind_param(stmt1, bind);
  check_execute(stmt1, rc);

  rc= mysql_stmt_execute(stmt2);
  check_execute(stmt2, rc);

  verify_st_affected_rows(stmt2, 1);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n int_data: %lu(%lu)", (ulong) id, length[0]);
    fprintf(stdout, "\n str_data: %s(%lu)", name, length[1]);
  }
  DIE_UNLESS(id == 10);
  DIE_UNLESS(strcmp(name, "updated") == 0);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  rc= mysql_stmt_execute(stmt1);
  check_execute(stmt1, rc);

  verify_st_affected_rows(stmt1, 1);

  mysql_stmt_close(stmt1);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  rc= my_stmt_result("SELECT * FROM test_multi_table");
  DIE_UNLESS(rc == 0);

  mysql_stmt_close(stmt);
  mysql_stmt_close(stmt2);

}


/* Test simple sample - manual */

static void test_manual_sample()
{
  unsigned int param_count;
  MYSQL_STMT   *stmt;
  short        small_data;
  int          int_data;
  int          rc;
  char         str_data[50];
  ulonglong    affected_rows;
  MYSQL_BIND   bind[3];
  my_bool      is_null;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_manual_sample");

  /*
    Sample which is incorporated directly in the manual under Prepared
    statements section (Example from mysql_stmt_execute()
  */

  mysql_autocommit(mysql, 1);
  if (mysql_query(mysql, "DROP TABLE IF EXISTS test_table"))
  {
    fprintf(stderr, "\n drop table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(1);
  }
  if (mysql_query(mysql, "CREATE TABLE test_table(col1 int, col2 varchar(50), \
                                                 col3 smallint, \
                                                 col4 timestamp(14))"))
  {
    fprintf(stderr, "\n create table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(1);
  }

  /* Prepare a insert query with 3 parameters */
  strmov(query, "INSERT INTO test_table(col1, col2, col3) values(?, ?, ?)");
  if (!(stmt= mysql_simple_prepare(mysql, query)))
  {
    fprintf(stderr, "\n prepare, insert failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(1);
  }
  if (!opt_silent)
    fprintf(stdout, "\n prepare, insert successful");

  /* Get the parameter count from the statement */
  param_count= mysql_stmt_param_count(stmt);

  if (!opt_silent)
    fprintf(stdout, "\n total parameters in insert: %d", param_count);
  if (param_count != 3) /* validate parameter count */
  {
    fprintf(stderr, "\n invalid parameter count returned by MySQL");
    exit(1);
  }

  /* Bind the data for the parameters */

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  /* INTEGER PART */
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&int_data;

  /* STRING PART */
  bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[1].buffer= (void *)str_data;
  bind[1].buffer_length= sizeof(str_data);

  /* SMALLINT PART */
  bind[2].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer= (void *)&small_data;
  bind[2].is_null= &is_null;
  is_null= 0;

  /* Bind the buffers */
  if (mysql_stmt_bind_param(stmt, bind))
  {
    fprintf(stderr, "\n param bind failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(1);
  }

  /* Specify the data */
  int_data= 10;             /* integer */
  strmov(str_data, "MySQL"); /* string  */

  /* INSERT SMALLINT data as NULL */
  is_null= 1;

  /* Execute the insert statement - 1*/
  if (mysql_stmt_execute(stmt))
  {
    fprintf(stderr, "\n execute 1 failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(1);
  }

  /* Get the total rows affected */
  affected_rows= mysql_stmt_affected_rows(stmt);

  if (!opt_silent)
    fprintf(stdout, "\n total affected rows: %ld", (ulong) affected_rows);
  if (affected_rows != 1) /* validate affected rows */
  {
    fprintf(stderr, "\n invalid affected rows by MySQL");
    exit(1);
  }

  /* Re-execute the insert, by changing the values */
  int_data= 1000;
  strmov(str_data, "The most popular open source database");
  small_data= 1000;         /* smallint */
  is_null= 0;               /* reset */

  /* Execute the insert statement - 2*/
  if (mysql_stmt_execute(stmt))
  {
    fprintf(stderr, "\n execute 2 failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(1);
  }

  /* Get the total rows affected */
  affected_rows= mysql_stmt_affected_rows(stmt);

  if (!opt_silent)
    fprintf(stdout, "\n total affected rows: %ld", (ulong) affected_rows);
  if (affected_rows != 1) /* validate affected rows */
  {
    fprintf(stderr, "\n invalid affected rows by MySQL");
    exit(1);
  }

  /* Close the statement */
  if (mysql_stmt_close(stmt))
  {
    fprintf(stderr, "\n failed while closing the statement");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(1);
  }
  rc= my_stmt_result("SELECT * FROM test_table");
  DIE_UNLESS(rc == 2);

  /* DROP THE TABLE */
  if (mysql_query(mysql, "DROP TABLE test_table"))
  {
    fprintf(stderr, "\n drop table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(1);
  }
  if (!opt_silent)
    fprintf(stdout, "Success !!!");
}


/* Test alter table scenario in the middle of prepare */

static void test_prepare_alter()
{
  MYSQL_STMT  *stmt;
  int         rc, id;
  MYSQL_BIND  bind[1];
  my_bool     is_null;

  myheader("test_prepare_alter");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prep_alter");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prep_alter(id int, name char(20))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_prep_alter values(10, 'venu'), (20, 'mysql')");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "INSERT INTO test_prep_alter VALUES(?, 'monty')");
  check_stmt(stmt);

  verify_param_count(stmt, 1);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  is_null= 0;
  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (void *)&id;
  bind[0].is_null= &is_null;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  id= 30;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  if (thread_query((char *)"ALTER TABLE test_prep_alter change id id_new varchar(20)"))
    exit(1);

  is_null= 1;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_stmt_result("SELECT * FROM test_prep_alter");
  DIE_UNLESS(rc == 4);

  mysql_stmt_close(stmt);
}


/* Test the support of multi-statement executions */

static void test_multi_statements()
{
  MYSQL *mysql_local;
  MYSQL_RES *result;
  int    rc;

  const char *query= "\
DROP TABLE IF EXISTS test_multi_tab;\
CREATE TABLE test_multi_tab(id int, name char(20));\
INSERT INTO test_multi_tab(id) VALUES(10), (20);\
INSERT INTO test_multi_tab VALUES(20, 'insert;comma');\
SELECT * FROM test_multi_tab;\
UPDATE test_multi_tab SET name='new;name' WHERE id=20;\
DELETE FROM test_multi_tab WHERE name='new;name';\
SELECT * FROM test_multi_tab;\
DELETE FROM test_multi_tab WHERE id=10;\
SELECT * FROM test_multi_tab;\
DROP TABLE test_multi_tab;\
select 1;\
DROP TABLE IF EXISTS test_multi_tab";
  uint count, exp_value;
  uint rows[]= {0, 0, 2, 1, 3, 2, 2, 1, 1, 0, 0, 1, 0};

  myheader("test_multi_statements");

  /*
    First test that we get an error for multi statements
    (Because default connection is not opened with CLIENT_MULTI_STATEMENTS)
  */
  rc= mysql_query(mysql, query); /* syntax error */
  myquery_r(rc);

  rc= mysql_next_result(mysql);
  DIE_UNLESS(rc == -1);
  rc= mysql_more_results(mysql);
  DIE_UNLESS(rc == 0);

  if (!(mysql_local= mysql_init(NULL)))
  {
    fprintf(stdout, "\n mysql_init() failed");
    exit(1);
  }

  /* Create connection that supports multi statements */
  if (!(mysql_real_connect(mysql_local, opt_host, opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket, CLIENT_MULTI_STATEMENTS)))
  {
    fprintf(stdout, "\n connection failed(%s)", mysql_error(mysql_local));
    exit(1);
  }
  mysql_local->reconnect= 1;

  rc= mysql_query(mysql_local, query);
  myquery(rc);

  for (count= 0 ; count < array_elements(rows) ; count++)
  {
    if (!opt_silent)
      fprintf(stdout, "\n Query %d: ", count);
    if ((result= mysql_store_result(mysql_local)))
    {
      (void) my_process_result_set(result);
      mysql_free_result(result);
    }
    else if (!opt_silent)
      fprintf(stdout, "OK, %ld row(s) affected, %ld warning(s)\n",
              (ulong) mysql_affected_rows(mysql_local),
              (ulong) mysql_warning_count(mysql_local));

    exp_value= (uint) mysql_affected_rows(mysql_local);
    if (rows[count] !=  exp_value)
    {
      fprintf(stderr, "row %d  had affected rows: %d, should be %d\n",
              count, exp_value, rows[count]);
      exit(1);
    }
    if (count != array_elements(rows) -1)
    {
      if (!(rc= mysql_more_results(mysql_local)))
      {
        fprintf(stdout,
                "mysql_more_result returned wrong value: %d for row %d\n",
                rc, count);
        exit(1);
      }
      if ((rc= mysql_next_result(mysql_local)))
      {
        exp_value= mysql_errno(mysql_local);

        exit(1);
      }
    }
    else
    {
      rc= mysql_more_results(mysql_local);
      DIE_UNLESS(rc == 0);
      rc= mysql_next_result(mysql_local);
      DIE_UNLESS(rc == -1);
    }
  }

  /* check that errors abort multi statements */

  rc= mysql_query(mysql_local, "select 1+1+a;select 1+1");
  myquery_r(rc);
  rc= mysql_more_results(mysql_local);
  DIE_UNLESS(rc == 0);
  rc= mysql_next_result(mysql_local);
  DIE_UNLESS(rc == -1);

  rc= mysql_query(mysql_local, "select 1+1;select 1+1+a;select 1");
  myquery(rc);
  result= mysql_store_result(mysql_local);
  mytest(result);
  mysql_free_result(result);
  rc= mysql_more_results(mysql_local);
  DIE_UNLESS(rc == 1);
  rc= mysql_next_result(mysql_local);
  DIE_UNLESS(rc > 0);

  /*
    Ensure that we can now do a simple query (this checks that the server is
    not trying to send us the results for the last 'select 1'
  */
  rc= mysql_query(mysql_local, "select 1+1+1");
  myquery(rc);
  result= mysql_store_result(mysql_local);
  mytest(result);
  (void) my_process_result_set(result);
  mysql_free_result(result);

  mysql_close(mysql_local);
}


/*
  Check that Prepared statement cannot contain several
  SQL statements
*/

static void test_prepare_multi_statements()
{
  MYSQL *mysql_local;
  MYSQL_STMT *stmt;
  char query[MAX_TEST_QUERY_LENGTH];
  myheader("test_prepare_multi_statements");

  if (!(mysql_local= mysql_init(NULL)))
  {
    fprintf(stderr, "\n mysql_init() failed");
    exit(1);
  }

  if (!(mysql_real_connect(mysql_local, opt_host, opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket, CLIENT_MULTI_STATEMENTS)))
  {
    fprintf(stderr, "\n connection failed(%s)", mysql_error(mysql_local));
    exit(1);
  }
  mysql_local->reconnect= 1;
  strmov(query, "select 1; select 'another value'");
  stmt= mysql_simple_prepare(mysql_local, query);
  check_stmt_r(stmt);
  mysql_close(mysql_local);
}


/* Test simple bind store result */

static void test_store_result()
{
  MYSQL_STMT *stmt;
  int        rc;
  int32      nData;
  char       szData[100];
  MYSQL_BIND bind[2];
  ulong      length, length1;
  my_bool    is_null[2];

  myheader("test_store_result");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_store_result(col1 int , col2 varchar(50))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_store_result VALUES(10, 'venu'), (20, 'mysql')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *) &nData;       /* integer data */
  bind[0].length= &length;
  bind[0].is_null= &is_null[0];

  length= 0;
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= szData;                /* string data */
  bind[1].buffer_length= sizeof(szData);
  bind[1].length= &length1;
  bind[1].is_null= &is_null[1];
  length1= 0;

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_store_result");
  check_stmt(stmt);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 1: %ld, %s(%lu)", (long) nData, szData, length1);
  DIE_UNLESS(nData == 10);
  DIE_UNLESS(strcmp(szData, "venu") == 0);
  DIE_UNLESS(length1 == 4);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 2: %ld, %s(%lu)", (long) nData, szData, length1);
  DIE_UNLESS(nData == 20);
  DIE_UNLESS(strcmp(szData, "mysql") == 0);
  DIE_UNLESS(length1 == 5);

  length= 99;
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent && is_null[0])
    fprintf(stdout, "\n row 3: NULL, %s(%lu)", szData, length1);
  DIE_UNLESS(is_null[0]);
  DIE_UNLESS(strcmp(szData, "monty") == 0);
  DIE_UNLESS(length1 == 5);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 1: %ld, %s(%lu)", (long) nData, szData, length1);
  DIE_UNLESS(nData == 10);
  DIE_UNLESS(strcmp(szData, "venu") == 0);
  DIE_UNLESS(length1 == 4);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 2: %ld, %s(%lu)", (long) nData, szData, length1);
  DIE_UNLESS(nData == 20);
  DIE_UNLESS(strcmp(szData, "mysql") == 0);
  DIE_UNLESS(length1 == 5);

  length= 99;
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent && is_null[0])
    fprintf(stdout, "\n row 3: NULL, %s(%lu)", szData, length1);
  DIE_UNLESS(is_null[0]);
  DIE_UNLESS(strcmp(szData, "monty") == 0);
  DIE_UNLESS(length1 == 5);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test simple bind store result */

static void test_store_result1()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_store_result1");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_store_result(col1 int , col2 varchar(50))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_store_result VALUES(10, 'venu'), (20, 'mysql')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_store_result");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= 0;
  while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
    rc++;
  if (!opt_silent)
    fprintf(stdout, "\n total rows: %d", rc);
  DIE_UNLESS(rc == 3);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= 0;
  while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
    rc++;
  if (!opt_silent)
    fprintf(stdout, "\n total rows: %d", rc);
  DIE_UNLESS(rc == 3);

  mysql_stmt_close(stmt);
}


/* Another test for bind and store result */

static void test_store_result2()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        nData;
  ulong      length;
  MYSQL_BIND bind[1];
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_store_result2");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_store_result(col1 int , col2 varchar(50))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_store_result VALUES(10, 'venu'), (20, 'mysql')");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *) &nData;      /* integer data */
  bind[0].length= &length;
  bind[0].is_null= 0;

  strmov((char *)query , "SELECT col1 FROM test_store_result where col1= ?");
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  nData= 10; length= 0;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  nData= 0;
  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 1: %d", nData);
  DIE_UNLESS(nData == 10);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  nData= 20;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  nData= 0;
  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 1: %d", nData);
  DIE_UNLESS(nData == 20);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);
  mysql_stmt_close(stmt);
}


/* Test simple subselect prepare */

static void test_subselect()
{

  MYSQL_STMT *stmt;
  int        rc, id;
  MYSQL_BIND bind[1];
  DBUG_ENTER("test_subselect");

  myheader("test_subselect");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_sub1");
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_sub2");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_sub1(id int)");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_sub2(id int, id1 int)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_sub1 values(2)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_sub2 VALUES(1, 7), (2, 7)");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *) &id;
  bind[0].length= 0;
  bind[0].is_null= 0;

  stmt= mysql_simple_prepare(mysql, "INSERT INTO test_sub2(id) SELECT * FROM test_sub1 WHERE id= ?");
  check_stmt(stmt);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  id= 2;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  verify_st_affected_rows(stmt, 1);

  id= 9;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  verify_st_affected_rows(stmt, 0);

  mysql_stmt_close(stmt);

  rc= my_stmt_result("SELECT * FROM test_sub2");
  DIE_UNLESS(rc == 3);

  rc= my_stmt_result("SELECT ROW(1, 7) IN (select id, id1 "
                     "from test_sub2 WHERE id1= 8)");
  DIE_UNLESS(rc == 1);
  rc= my_stmt_result("SELECT ROW(1, 7) IN (select id, id1 "
                     "from test_sub2 WHERE id1= 7)");
  DIE_UNLESS(rc == 1);

  stmt= mysql_simple_prepare(mysql, ("SELECT ROW(1, 7) IN (select id, id1 "
                                     "from test_sub2 WHERE id1= ?)"));
  check_stmt(stmt);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  id= 7;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 1: %d", id);
  DIE_UNLESS(id == 1);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  id= 8;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 1: %d", id);
  DIE_UNLESS(id == 0);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
  DBUG_VOID_RETURN;
}


/*
  Generalized conversion routine to handle DATE, TIME and DATETIME
  conversion using MYSQL_TIME structure
*/

static void test_bind_date_conv(uint row_count)
{
  MYSQL_STMT   *stmt= 0;
  uint         rc, i, count= row_count;
  ulong        length[4];
  MYSQL_BIND   bind[4];
  my_bool      is_null[4]= {0};
  MYSQL_TIME   tm[4];
  ulong        second_part;
  uint         year, month, day, hour, minute, sec;

  stmt= mysql_simple_prepare(mysql, "INSERT INTO test_date VALUES(?, ?, ?, ?)");
  check_stmt(stmt);

  verify_param_count(stmt, 4);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_TIMESTAMP;
  bind[1].buffer_type= MYSQL_TYPE_TIME;
  bind[2].buffer_type= MYSQL_TYPE_DATETIME;
  bind[3].buffer_type= MYSQL_TYPE_DATE;

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].buffer= (void *) &tm[i];
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
    bind[i].buffer_length= 30;
    length[i]= 20;
  }

  second_part= 0;

  year= 2000;
  month= 01;
  day= 10;

  hour= 11;
  minute= 16;
  sec= 20;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  for (count= 0; count < row_count; count++)
  {
    for (i= 0; i < (int) array_elements(bind); i++)
    {
      tm[i].neg= 0;
      tm[i].second_part= second_part+count;
      if (bind[i].buffer_type != MYSQL_TYPE_TIME)
      {
        tm[i].year= year+count;
        tm[i].month= month+count;
        tm[i].day= day+count;
      }
      else
        tm[i].year= tm[i].month= tm[i].day= 0;
      if (bind[i].buffer_type != MYSQL_TYPE_DATE)
      {
        tm[i].hour= hour+count;
        tm[i].minute= minute+count;
        tm[i].second= sec+count;
      }
      else
        tm[i].hour= tm[i].minute= tm[i].second= 0;
    }
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }

  rc= mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  rc= my_stmt_result("SELECT * FROM test_date");
  DIE_UNLESS(row_count == rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_date");
  check_stmt(stmt);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  for (count= 0; count < row_count; count++)
  {
    rc= mysql_stmt_fetch(stmt);
    DIE_UNLESS(rc == 0 || rc == MYSQL_DATA_TRUNCATED);

    if (!opt_silent)
      fprintf(stdout, "\n");
    for (i= 0; i < array_elements(bind); i++)
    {
      if (!opt_silent)
        fprintf(stdout, "\ntime[%d]: %02d-%02d-%02d %02d:%02d:%02d.%02lu",
                i, tm[i].year, tm[i].month, tm[i].day,
                tm[i].hour, tm[i].minute, tm[i].second,
                tm[i].second_part);
      DIE_UNLESS(tm[i].year == 0 || tm[i].year == year+count);
      DIE_UNLESS(tm[i].month == 0 || tm[i].month == month+count);
      DIE_UNLESS(tm[i].day == 0 || tm[i].day == day+count);

      DIE_UNLESS(tm[i].hour == 0 || tm[i].hour == hour+count);
      DIE_UNLESS(tm[i].minute == 0 || tm[i].minute == minute+count);
      DIE_UNLESS(tm[i].second == 0 || tm[i].second == sec+count);
      DIE_UNLESS(tm[i].second_part == 0 ||
                 tm[i].second_part == second_part+count);
    }
  }
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test DATE, TIME, DATETIME and TS with MYSQL_TIME conversion */

static void test_date()
{
  int        rc;

  myheader("test_date");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_date");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_date(c1 TIMESTAMP(14), \
                                                 c2 TIME, \
                                                 c3 DATETIME, \
                                                 c4 DATE)");

  myquery(rc);

  test_bind_date_conv(5);
}


/* Test all time types to DATE and DATE to all types */

static void test_date_date()
{
  int        rc;

  myheader("test_date_date");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_date");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_date(c1 DATE, \
                                                 c2 DATE, \
                                                 c3 DATE, \
                                                 c4 DATE)");

  myquery(rc);

  test_bind_date_conv(3);
}


/* Test all time types to TIME and TIME to all types */

static void test_date_time()
{
  int        rc;

  myheader("test_date_time");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_date");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_date(c1 TIME, \
                                                 c2 TIME, \
                                                 c3 TIME, \
                                                 c4 TIME)");

  myquery(rc);

  test_bind_date_conv(3);
}


/* Test all time types to TIMESTAMP and TIMESTAMP to all types */

static void test_date_ts()
{
  int        rc;

  myheader("test_date_ts");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_date");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_date(c1 TIMESTAMP(10), \
                                                 c2 TIMESTAMP(14), \
                                                 c3 TIMESTAMP, \
                                                 c4 TIMESTAMP(6))");

  myquery(rc);

  test_bind_date_conv(2);
}


/* Test all time types to DATETIME and DATETIME to all types */

static void test_date_dt()
{
  int rc;

  myheader("test_date_dt");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_date");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_date(c1 datetime, "
                         " c2 datetime, c3 datetime, c4 date)");
  myquery(rc);

  test_bind_date_conv(2);
}


/* Misc tests to keep pure coverage happy */

static void test_pure_coverage()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  int        rc;
  ulong      length;

  myheader("test_pure_coverage");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_pure");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_pure(c1 int, c2 varchar(20))");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "insert into test_pure(c67788) values(10)");
  check_stmt_r(stmt);

  /* Query without params and result should allow to bind 0 arrays */
  stmt= mysql_simple_prepare(mysql, "insert into test_pure(c2) values(10)");
  check_stmt(stmt);

  rc= mysql_stmt_bind_param(stmt, (MYSQL_BIND*)0);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, (MYSQL_BIND*)0);
  DIE_UNLESS(rc == 1);

  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "insert into test_pure(c2) values(?)");
  check_stmt(stmt);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].length= &length;
  bind[0].is_null= 0;
  bind[0].buffer_length= 0;

  bind[0].buffer_type= MYSQL_TYPE_GEOMETRY;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute_r(stmt, rc); /* unsupported buffer type */

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "select * from test_pure");
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer_type= MYSQL_TYPE_GEOMETRY;
  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute_r(stmt, rc); /* unsupported buffer type */

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute_r(stmt, rc); /* commands out of sync */

  mysql_stmt_close(stmt);

  mysql_query(mysql, "DROP TABLE test_pure");
}


/* Test for string buffer fetch */

static void test_buffers()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  int        rc;
  ulong      length;
  my_bool    is_null;
  char       buffer[20];

  myheader("test_buffers");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_buffer");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_buffer(str varchar(20))");
  myquery(rc);

  rc= mysql_query(mysql, "insert into test_buffer values('MySQL')\
                          , ('Database'), ('Open-Source'), ('Popular')");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "select str from test_buffer");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero(buffer, sizeof(buffer));              /* Avoid overruns in printf() */

  bzero((char*) bind, sizeof(bind));
  bind[0].length= &length;
  bind[0].is_null= &is_null;
  bind[0].buffer_length= 1;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)buffer;
  bind[0].error= &bind[0].error_value;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  buffer[1]= 'X';
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_DATA_TRUNCATED);
  DIE_UNLESS(bind[0].error_value);
  if (!opt_silent)
    fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  DIE_UNLESS(buffer[0] == 'M');
  DIE_UNLESS(buffer[1] == 'X');
  DIE_UNLESS(length == 5);

  bind[0].buffer_length= 8;
  rc= mysql_stmt_bind_result(stmt, bind);/* re-bind */
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  DIE_UNLESS(strncmp(buffer, "Database", 8) == 0);
  DIE_UNLESS(length == 8);

  bind[0].buffer_length= 12;
  rc= mysql_stmt_bind_result(stmt, bind);/* re-bind */
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  DIE_UNLESS(strcmp(buffer, "Open-Source") == 0);
  DIE_UNLESS(length == 11);

  bind[0].buffer_length= 6;
  rc= mysql_stmt_bind_result(stmt, bind);/* re-bind */
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_DATA_TRUNCATED);
  DIE_UNLESS(bind[0].error_value);
  if (!opt_silent)
    fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  DIE_UNLESS(strncmp(buffer, "Popula", 6) == 0);
  DIE_UNLESS(length == 7);

  mysql_stmt_close(stmt);
}


/* Test the direct query execution in the middle of open stmts */

static void test_open_direct()
{
  MYSQL_STMT  *stmt;
  MYSQL_RES   *result;
  int         rc;

  myheader("test_open_direct");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_open_direct");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_open_direct(id int, name char(6))");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "INSERT INTO test_open_direct values(10, 'mysql')");
  check_stmt(stmt);

  rc= mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 0);
  mysql_free_result(result);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  verify_st_affected_rows(stmt, 1);

  rc= mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);
  mysql_free_result(result);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  verify_st_affected_rows(stmt, 1);

  rc= mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 2);
  mysql_free_result(result);

  mysql_stmt_close(stmt);

  /* run a direct query in the middle of a fetch */
  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_open_direct");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_query(mysql, "INSERT INTO test_open_direct(id) VALUES(20)");
  myquery_r(rc);

  rc= mysql_stmt_close(stmt);
  check_execute(stmt, rc);

  rc= mysql_query(mysql, "INSERT INTO test_open_direct(id) VALUES(20)");
  myquery(rc);

  /* run a direct query with store result */
  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_open_direct");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_query(mysql, "drop table test_open_direct");
  myquery(rc);

  rc= mysql_stmt_close(stmt);
  check_execute(stmt, rc);
}


/* Test fetch without prior bound buffers */

static void test_fetch_nobuffs()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  char       str[4][50];
  int        rc;

  myheader("test_fetch_nobuffs");

  stmt= mysql_simple_prepare(mysql, "SELECT DATABASE(), CURRENT_USER(), \
                              CURRENT_DATE(), CURRENT_TIME()");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= 0;
  while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
    rc++;

  if (!opt_silent)
    fprintf(stdout, "\n total rows        : %d", rc);
  DIE_UNLESS(rc == 1);

  bzero((char*) bind, sizeof(MYSQL_BIND));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)str[0];
  bind[0].buffer_length= sizeof(str[0]);
  bind[1]= bind[2]= bind[3]= bind[0];
  bind[1].buffer= (void *)str[1];
  bind[2].buffer= (void *)str[2];
  bind[3].buffer= (void *)str[3];

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= 0;
  while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
  {
    rc++;
    if (!opt_silent)
    {
      fprintf(stdout, "\n CURRENT_DATABASE(): %s", str[0]);
      fprintf(stdout, "\n CURRENT_USER()    : %s", str[1]);
      fprintf(stdout, "\n CURRENT_DATE()    : %s", str[2]);
      fprintf(stdout, "\n CURRENT_TIME()    : %s", str[3]);
    }
  }
  if (!opt_silent)
    fprintf(stdout, "\n total rows        : %d", rc);
  DIE_UNLESS(rc == 1);

  mysql_stmt_close(stmt);
}


/* Test a misc bug */

static void test_ushort_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  ushort     short_value;
  uint32     long_value;
  ulong      s_length, l_length, ll_length, t_length;
  ulonglong  longlong_value;
  int        rc;
  uchar      tiny_value;
  char       llbuf[22];
  myheader("test_ushort_bug");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_ushort");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_ushort(a smallint unsigned, \
                                                  b smallint unsigned, \
                                                  c smallint unsigned, \
                                                  d smallint unsigned)");
  myquery(rc);

  rc= mysql_query(mysql,
                  "INSERT INTO test_ushort VALUES(35999, 35999, 35999, 200)");
  myquery(rc);


  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_ushort");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (void *)&short_value;
  bind[0].is_unsigned= TRUE;
  bind[0].length= &s_length;

  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (void *)&long_value;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (void *)&longlong_value;
  bind[2].length= &ll_length;

  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (void *)&tiny_value;
  bind[3].is_unsigned= TRUE;
  bind[3].length= &t_length;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n ushort   : %d (%ld)", short_value, s_length);
    fprintf(stdout, "\n ulong    : %lu (%ld)", (ulong) long_value, l_length);
    fprintf(stdout, "\n longlong : %s (%ld)", llstr(longlong_value, llbuf),
            ll_length);
    fprintf(stdout, "\n tinyint  : %d   (%ld)", tiny_value, t_length);
  }

  DIE_UNLESS(short_value == 35999);
  DIE_UNLESS(s_length == 2);

  DIE_UNLESS(long_value == 35999);
  DIE_UNLESS(l_length == 4);

  DIE_UNLESS(longlong_value == 35999);
  DIE_UNLESS(ll_length == 8);

  DIE_UNLESS(tiny_value == 200);
  DIE_UNLESS(t_length == 1);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test a misc smallint-signed conversion bug */

static void test_sshort_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  short      short_value;
  int32      long_value;
  ulong      s_length, l_length, ll_length, t_length;
  ulonglong  longlong_value;
  int        rc;
  uchar      tiny_value;
  char       llbuf[22];

  myheader("test_sshort_bug");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_sshort");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_sshort(a smallint signed, \
                                                  b smallint signed, \
                                                  c smallint unsigned, \
                                                  d smallint unsigned)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_sshort VALUES(-5999, -5999, 35999, 200)");
  myquery(rc);


  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_sshort");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (void *)&short_value;
  bind[0].length= &s_length;

  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (void *)&long_value;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (void *)&longlong_value;
  bind[2].length= &ll_length;

  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (void *)&tiny_value;
  bind[3].is_unsigned= TRUE;
  bind[3].length= &t_length;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n sshort   : %d (%ld)", short_value, s_length);
    fprintf(stdout, "\n slong    : %ld (%ld)", (long) long_value, l_length);
    fprintf(stdout, "\n longlong : %s (%ld)", llstr(longlong_value, llbuf),
            ll_length);
    fprintf(stdout, "\n tinyint  : %d   (%ld)", tiny_value, t_length);
  }

  DIE_UNLESS(short_value == -5999);
  DIE_UNLESS(s_length == 2);

  DIE_UNLESS(long_value == -5999);
  DIE_UNLESS(l_length == 4);

  DIE_UNLESS(longlong_value == 35999);
  DIE_UNLESS(ll_length == 8);

  DIE_UNLESS(tiny_value == 200);
  DIE_UNLESS(t_length == 1);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test a misc tinyint-signed conversion bug */

static void test_stiny_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  short      short_value;
  int32      long_value;
  ulong      s_length, l_length, ll_length, t_length;
  ulonglong  longlong_value;
  int        rc;
  uchar      tiny_value;
  char       llbuf[22];

  myheader("test_stiny_bug");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_stiny");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_stiny(a tinyint signed, \
                                                  b tinyint signed, \
                                                  c tinyint unsigned, \
                                                  d tinyint unsigned)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_stiny VALUES(-128, -127, 255, 0)");
  myquery(rc);


  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_stiny");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (void *)&short_value;
  bind[0].length= &s_length;

  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (void *)&long_value;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (void *)&longlong_value;
  bind[2].length= &ll_length;

  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (void *)&tiny_value;
  bind[3].length= &t_length;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n sshort   : %d (%ld)", short_value, s_length);
    fprintf(stdout, "\n slong    : %ld (%ld)", (long) long_value, l_length);
    fprintf(stdout, "\n longlong : %s  (%ld)", llstr(longlong_value, llbuf),
            ll_length);
    fprintf(stdout, "\n tinyint  : %d    (%ld)", tiny_value, t_length);
  }

  DIE_UNLESS(short_value == -128);
  DIE_UNLESS(s_length == 2);

  DIE_UNLESS(long_value == -127);
  DIE_UNLESS(l_length == 4);

  DIE_UNLESS(longlong_value == 255);
  DIE_UNLESS(ll_length == 8);

  DIE_UNLESS(tiny_value == 0);
  DIE_UNLESS(t_length == 1);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test misc field information, bug: #74 */

static void test_field_misc()
{
  MYSQL_STMT  *stmt;
  MYSQL_RES   *result;
  MYSQL_BIND  bind[1];
  char        table_type[NAME_LEN];
  ulong       type_length;
  int         rc;

  myheader("test_field_misc");

  rc= mysql_query(mysql, "SELECT @@autocommit");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);

  verify_prepare_field(result, 0,
                       "@@autocommit", "",  /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "", 1, 0);           /* db name, length(its bool flag)*/

  mysql_free_result(result);

  stmt= mysql_simple_prepare(mysql, "SELECT @@autocommit");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  verify_prepare_field(result, 0,
                       "@@autocommit", "",  /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "", 1, 0);           /* db name, length(its bool flag)*/

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "SELECT @@table_type");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= table_type;
  bind[0].length= &type_length;
  bind[0].buffer_length= NAME_LEN;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n default table type: %s(%ld)", table_type, type_length);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "SELECT @@table_type");
  check_stmt(stmt);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);
  DIE_UNLESS(mysql_stmt_field_count(stmt) == mysql_num_fields(result));

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  DIE_UNLESS(1 == my_process_stmt_result(stmt));

  verify_prepare_field(result, 0,
                       "@@table_type", "",   /* field and its org name */
                       mysql_get_server_version(mysql) <= 50000 ?
                       MYSQL_TYPE_STRING : MYSQL_TYPE_VAR_STRING,
                       "", "",              /* table and its org name */
                       "", type_length, 0);   /* db name, length */

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "SELECT @@max_error_count");
  check_stmt(stmt);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  verify_prepare_field(result, 0,
                       "@@max_error_count", "",   /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "", 10, 0);            /* db name, length */

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "SELECT @@max_allowed_packet");
  check_stmt(stmt);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  DIE_UNLESS(1 == my_process_stmt_result(stmt));

  verify_prepare_field(result, 0,
                       "@@max_allowed_packet", "", /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "", 10, 0);          /* db name, length */

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "SELECT @@sql_warnings");
  check_stmt(stmt);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  verify_prepare_field(result, 0,
                       "@@sql_warnings", "",  /* field and its org name */
                       MYSQL_TYPE_LONGLONG,   /* field type */
                       "", "",                /* table and its org name */
                       "", 1, 0);             /* db name, length */

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/*
  Test SET OPTION feature with prepare stmts
  bug #85 (reported by mark@mysql.com)
*/

static void test_set_option()
{
  MYSQL_STMT *stmt;
  MYSQL_RES  *result;
  int        rc;

  myheader("test_set_option");

  mysql_autocommit(mysql, TRUE);

  /* LIMIT the rows count to 2 */
  rc= mysql_query(mysql, "SET OPTION SQL_SELECT_LIMIT= 2");
  myquery(rc);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_limit");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_limit(a tinyint)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_limit VALUES(10), (20), (30), (40)");
  myquery(rc);

  if (!opt_silent)
    fprintf(stdout, "\n with SQL_SELECT_LIMIT= 2 (direct)");
  rc= mysql_query(mysql, "SELECT * FROM test_limit");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 2);

  mysql_free_result(result);

  if (!opt_silent)
    fprintf(stdout, "\n with SQL_SELECT_LIMIT=2 (prepare)");
  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_limit");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 2);

  mysql_stmt_close(stmt);

  /* RESET the LIMIT the rows count to 0 */
  if (!opt_silent)
    fprintf(stdout, "\n with SQL_SELECT_LIMIT=DEFAULT (prepare)");
  rc= mysql_query(mysql, "SET OPTION SQL_SELECT_LIMIT=DEFAULT");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_limit");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 4);

  mysql_stmt_close(stmt);
}


/*
  Test a misc GRANT option
  bug #89 (reported by mark@mysql.com)
*/

#ifndef EMBEDDED_LIBRARY
static void test_prepare_grant()
{
  int rc;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_prepare_grant");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_grant");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_grant(a tinyint primary key auto_increment)");
  myquery(rc);

  strxmov(query, "GRANT INSERT, UPDATE, SELECT ON ", current_db,
                ".test_grant TO 'test_grant'@",
                opt_host ? opt_host : "'localhost'", NullS);

  if (mysql_query(mysql, query))
  {
    myerror("GRANT failed");

    /*
       If server started with --skip-grant-tables, skip this test, else
       exit to indicate an error

       ER_UNKNOWN_COM_ERROR= 1047
     */
    if (mysql_errno(mysql) != 1047)
      exit(1);
  }
  else
  {
    MYSQL *org_mysql= mysql, *lmysql;
    MYSQL_STMT *stmt;

    if (!opt_silent)
      fprintf(stdout, "\n Establishing a test connection ...");
    if (!(lmysql= mysql_init(NULL)))
    {
      myerror("mysql_init() failed");
      exit(1);
    }
    if (!(mysql_real_connect(lmysql, opt_host, "test_grant",
                             "", current_db, opt_port,
                             opt_unix_socket, 0)))
    {
      myerror("connection failed");
      mysql_close(lmysql);
      exit(1);
    }
    lmysql->reconnect= 1;
    if (!opt_silent)
      fprintf(stdout, " OK");

    mysql= lmysql;
    rc= mysql_query(mysql, "INSERT INTO test_grant VALUES(NULL)");
    myquery(rc);

    rc= mysql_query(mysql, "INSERT INTO test_grant(a) VALUES(NULL)");
    myquery(rc);

    execute_prepare_query("INSERT INTO test_grant(a) VALUES(NULL)", 1);
    execute_prepare_query("INSERT INTO test_grant VALUES(NULL)", 1);
    execute_prepare_query("UPDATE test_grant SET a=9 WHERE a=1", 1);
    rc= my_stmt_result("SELECT a FROM test_grant");
    DIE_UNLESS(rc == 4);

    /* Both DELETE expected to fail as user does not have DELETE privs */

    rc= mysql_query(mysql, "DELETE FROM test_grant");
    myquery_r(rc);

    stmt= mysql_simple_prepare(mysql, "DELETE FROM test_grant");
    check_stmt_r(stmt);

    rc= my_stmt_result("SELECT * FROM test_grant");
    DIE_UNLESS(rc == 4);

    mysql_close(lmysql);
    mysql= org_mysql;

    rc= mysql_query(mysql, "delete from mysql.user where User='test_grant'");
    myquery(rc);
    DIE_UNLESS(1 == mysql_affected_rows(mysql));

    rc= mysql_query(mysql, "delete from mysql.tables_priv where User='test_grant'");
    myquery(rc);
    DIE_UNLESS(1 == mysql_affected_rows(mysql));

  }
}
#endif /* EMBEDDED_LIBRARY */

/*
  Test a crash when invalid/corrupted .frm is used in the
  SHOW TABLE STATUS
  bug #93 (reported by serg@mysql.com).
*/

static void test_frm_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  MYSQL_RES  *result;
  MYSQL_ROW  row;
  FILE       *test_file;
  char       data_dir[FN_REFLEN];
  char       test_frm[FN_REFLEN];
  int        rc;

  myheader("test_frm_bug");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql, "drop table if exists test_frm_bug");
  myquery(rc);

  rc= mysql_query(mysql, "flush tables");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "show variables like 'datadir'");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= data_dir;
  bind[0].buffer_length= FN_REFLEN;
  bind[1]= bind[0];

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n data directory: %s", data_dir);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  strxmov(test_frm, data_dir, "/", current_db, "/", "test_frm_bug.frm", NullS);

  if (!opt_silent)
    fprintf(stdout, "\n test_frm: %s", test_frm);

  if (!(test_file= my_fopen(test_frm, (int) (O_RDWR | O_CREAT), MYF(MY_WME))))
  {
    fprintf(stdout, "\n ERROR: my_fopen failed for '%s'", test_frm);
    fprintf(stdout, "\n test cancelled");
    exit(1);
  }
  if (!opt_silent)
    fprintf(test_file, "this is a junk file for test");

  rc= mysql_query(mysql, "SHOW TABLE STATUS like 'test_frm_bug'");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);/* It can't be NULL */

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 1);

  mysql_data_seek(result, 0);

  row= mysql_fetch_row(result);
  mytest(row);

  if (!opt_silent)
    fprintf(stdout, "\n Comment: %s", row[17]);
  DIE_UNLESS(row[17] != 0);

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  my_fclose(test_file, MYF(0));
  mysql_query(mysql, "drop table if exists test_frm_bug");
}


/* Test DECIMAL conversion */

static void test_decimal_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char       data[30];
  int        rc;
  my_bool    is_null;

  myheader("test_decimal_bug");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql, "drop table if exists test_decimal_bug");
  myquery(rc);

  rc= mysql_query(mysql, "create table test_decimal_bug(c1 decimal(10, 2))");
  myquery(rc);

  rc= mysql_query(mysql, "insert into test_decimal_bug value(8), (10.22), (5.61)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "select c1 from test_decimal_bug where c1= ?");
  check_stmt(stmt);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_NEWDECIMAL;
  bind[0].buffer= (void *)data;
  bind[0].buffer_length= 25;
  bind[0].is_null= &is_null;

  is_null= 0;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  strmov(data, "8.0");
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  data[0]= 0;
  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n data: %s", data);
  DIE_UNLESS(strcmp(data, "8.00") == 0);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  strmov(data, "5.61");
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  data[0]= 0;
  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n data: %s", data);
  DIE_UNLESS(strcmp(data, "5.61") == 0);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  is_null= 1;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  strmov(data, "10.22"); is_null= 0;
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  data[0]= 0;
  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n data: %s", data);
  DIE_UNLESS(strcmp(data, "10.22") == 0);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/* Test EXPLAIN bug (#115, reported by mark@mysql.com & georg@php.net). */

static void test_explain_bug()
{
  MYSQL_STMT *stmt;
  MYSQL_RES  *result;
  int        rc;

  myheader("test_explain_bug");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_explain");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_explain(id int, name char(2))");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "explain test_explain");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 2);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);

  if (!opt_silent)
    fprintf(stdout, "\n total fields in the result: %d",
            mysql_num_fields(result));
  DIE_UNLESS(6 == mysql_num_fields(result));

  verify_prepare_field(result, 0, "Field", "COLUMN_NAME",
                       mysql_get_server_version(mysql) <= 50000 ?
                       MYSQL_TYPE_STRING : MYSQL_TYPE_VAR_STRING,
                       0, 0, "", 64, 0);

  verify_prepare_field(result, 1, "Type", "COLUMN_TYPE", MYSQL_TYPE_BLOB,
                       0, 0, "", 0, 0);

  verify_prepare_field(result, 2, "Null", "IS_NULLABLE",
                       mysql_get_server_version(mysql) <= 50000 ?
                       MYSQL_TYPE_STRING : MYSQL_TYPE_VAR_STRING,
                       0, 0, "", 3, 0);

  verify_prepare_field(result, 3, "Key", "COLUMN_KEY",
                       mysql_get_server_version(mysql) <= 50000 ?
                       MYSQL_TYPE_STRING : MYSQL_TYPE_VAR_STRING,
                       0, 0, "", 3, 0);

  verify_prepare_field(result, 4, "Default", "COLUMN_DEFAULT",
                       mysql_get_server_version(mysql) <= 50000 ?
                       MYSQL_TYPE_STRING : MYSQL_TYPE_VAR_STRING,
                       0, 0, "", 64, 0);

  verify_prepare_field(result, 5, "Extra", "EXTRA",
                       mysql_get_server_version(mysql) <= 50000 ?
                       MYSQL_TYPE_STRING : MYSQL_TYPE_VAR_STRING,
                       0, 0, "", 20, 0);

  mysql_free_result(result);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, "explain select id, name FROM test_explain");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  result= mysql_stmt_result_metadata(stmt);
  mytest(result);

  if (!opt_silent)
    fprintf(stdout, "\n total fields in the result: %d",
            mysql_num_fields(result));
  DIE_UNLESS(10 == mysql_num_fields(result));

  verify_prepare_field(result, 0, "id", "", MYSQL_TYPE_LONGLONG,
                       "", "", "", 3, 0);

  verify_prepare_field(result, 1, "select_type", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", 19, 0);

  verify_prepare_field(result, 2, "table", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", NAME_LEN, 0);

  verify_prepare_field(result, 3, "type", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", 10, 0);

  verify_prepare_field(result, 4, "possible_keys", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", NAME_LEN*MAX_KEY, 0);

  verify_prepare_field(result, 5, "key", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", NAME_LEN, 0);

  if (mysql_get_server_version(mysql) <= 50000)
  {
    verify_prepare_field(result, 6, "key_len", "", MYSQL_TYPE_LONGLONG, "",
                         "", "", 3, 0);
  }
  else
  {
    verify_prepare_field(result, 6, "key_len", "", MYSQL_TYPE_VAR_STRING, "", 
                         "", "", NAME_LEN*MAX_KEY, 0);
  }

  verify_prepare_field(result, 7, "ref", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", NAME_LEN*16, 0);

  verify_prepare_field(result, 8, "rows", "", MYSQL_TYPE_LONGLONG,
                       "", "", "", 10, 0);

  verify_prepare_field(result, 9, "Extra", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", 255, 0);

  mysql_free_result(result);
  mysql_stmt_close(stmt);
}

#ifdef NOT_YET_WORKING

/*
  Test math functions.
  Bug #148 (reported by salle@mysql.com).
*/

#define myerrno(n) check_errcode(n)

static void check_errcode(const unsigned int err)
{
  if (!opt_silent || mysql_errno(mysql) != err)
  {
    if (mysql->server_version)
      fprintf(stdout, "\n [MySQL-%s]", mysql->server_version);
    else
      fprintf(stdout, "\n [MySQL]");
    fprintf(stdout, "[%d] %s\n", mysql_errno(mysql), mysql_error(mysql));
  }
  DIE_UNLESS(mysql_errno(mysql) == err);
}


static void test_drop_temp()
{
  int rc;

  myheader("test_drop_temp");

  rc= mysql_query(mysql, "DROP DATABASE IF EXISTS test_drop_temp_db");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE DATABASE test_drop_temp_db");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_drop_temp_db.t1(c1 int, c2 char(1))");
  myquery(rc);

  rc= mysql_query(mysql, "delete from mysql.db where Db='test_drop_temp_db'");
  myquery(rc);

  rc= mysql_query(mysql, "delete from mysql.db where Db='test_drop_temp_db'");
  myquery(rc);

  strxmov(query, "GRANT SELECT, USAGE, DROP ON test_drop_temp_db.* TO test_temp@",
                opt_host ? opt_host : "localhost", NullS);

  if (mysql_query(mysql, query))
  {
    myerror("GRANT failed");

    /*
       If server started with --skip-grant-tables, skip this test, else
       exit to indicate an error

       ER_UNKNOWN_COM_ERROR= 1047
     */
    if (mysql_errno(mysql) != 1047)
      exit(1);
  }
  else
  {
    MYSQL *org_mysql= mysql, *lmysql;

    if (!opt_silent)
      fprintf(stdout, "\n Establishing a test connection ...");
    if (!(lmysql= mysql_init(NULL)))
    {
      myerror("mysql_init() failed");
      exit(1);
    }

    rc= mysql_query(mysql, "flush privileges");
    myquery(rc);

    if (!(mysql_real_connect(lmysql, opt_host ? opt_host : "localhost", "test_temp",
                             "", "test_drop_temp_db", opt_port,
                             opt_unix_socket, 0)))
    {
      mysql= lmysql;
      myerror("connection failed");
      mysql_close(lmysql);
      exit(1);
    }
    lmysql->reconnect= 1;
    if (!opt_silent)
      fprintf(stdout, " OK");

    mysql= lmysql;
    rc= mysql_query(mysql, "INSERT INTO t1 VALUES(10, 'C')");
    myerrno((uint)1142);

    rc= mysql_query(mysql, "DROP TABLE t1");
    myerrno((uint)1142);

    mysql= org_mysql;
    rc= mysql_query(mysql, "CREATE TEMPORARY TABLE test_drop_temp_db.t1(c1 int)");
    myquery(rc);

    rc= mysql_query(mysql, "CREATE TEMPORARY TABLE test_drop_temp_db.t2 LIKE test_drop_temp_db.t1");
    myquery(rc);

    mysql= lmysql;

    rc= mysql_query(mysql, "DROP TABLE t1, t2");
    myquery_r(rc);

    rc= mysql_query(mysql, "DROP TEMPORARY TABLE t1");
    myquery_r(rc);

    rc= mysql_query(mysql, "DROP TEMPORARY TABLE t2");
    myquery_r(rc);

    mysql_close(lmysql);
    mysql= org_mysql;

    rc= mysql_query(mysql, "drop database test_drop_temp_db");
    myquery(rc);
    DIE_UNLESS(1 == mysql_affected_rows(mysql));

    rc= mysql_query(mysql, "delete from mysql.user where User='test_temp'");
    myquery(rc);
    DIE_UNLESS(1 == mysql_affected_rows(mysql));


    rc= mysql_query(mysql, "delete from mysql.tables_priv where User='test_temp'");
    myquery(rc);
    DIE_UNLESS(1 == mysql_affected_rows(mysql));
  }
}
#endif


/* Test warnings for cuted rows */

static void test_cuted_rows()
{
  int        rc, count;
  MYSQL_RES  *result;

  myheader("test_cuted_rows");

  mysql_query(mysql, "DROP TABLE if exists t1");
  mysql_query(mysql, "DROP TABLE if exists t2");

  rc= mysql_query(mysql, "CREATE TABLE t1(c1 tinyint)");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t2(c1 int not null)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO t1 values(10), (NULL), (NULL)");
  myquery(rc);

  count= mysql_warning_count(mysql);
  if (!opt_silent)
    fprintf(stdout, "\n total warnings: %d", count);
  DIE_UNLESS(count == 0);

  rc= mysql_query(mysql, "INSERT INTO t2 SELECT * FROM t1");
  myquery(rc);

  count= mysql_warning_count(mysql);
  if (!opt_silent)
    fprintf(stdout, "\n total warnings: %d", count);
  DIE_UNLESS(count == 2);

  rc= mysql_query(mysql, "SHOW WARNINGS");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 2);
  mysql_free_result(result);

  rc= mysql_query(mysql, "INSERT INTO t1 VALUES('junk'), (876789)");
  myquery(rc);

  count= mysql_warning_count(mysql);
  if (!opt_silent)
    fprintf(stdout, "\n total warnings: %d", count);
  DIE_UNLESS(count == 2);

  rc= mysql_query(mysql, "SHOW WARNINGS");
  myquery(rc);

  result= mysql_store_result(mysql);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 2);
  mysql_free_result(result);
}


/* Test update/binary logs */

static void test_logs()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  char       data[255];
  ulong      length;
  int        rc;
  short      id;

  myheader("test_logs");


  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_logs");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_logs(id smallint, name varchar(20))");
  myquery(rc);

  strmov((char *)data, "INSERT INTO test_logs VALUES(?, ?)");
  stmt= mysql_simple_prepare(mysql, data);
  check_stmt(stmt);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (void *)&id;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)&data;
  bind[1].buffer_length= 255;
  bind[1].length= &length;

  id= 9876;
  length= (ulong)(strmov((char *)data, "MySQL - Open Source Database")- data);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  strmov((char *)data, "'");
  length= 1;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  strmov((char *)data, "\"");
  length= 1;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  length= (ulong)(strmov((char *)data, "my\'sql\'")-data);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  length= (ulong)(strmov((char *)data, "my\"sql\"")-data);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  strmov((char *)data, "INSERT INTO test_logs VALUES(20, 'mysql')");
  stmt= mysql_simple_prepare(mysql, data);
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  strmov((char *)data, "SELECT * FROM test_logs WHERE id=?");
  stmt= mysql_simple_prepare(mysql, data);
  check_stmt(stmt);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind[1].buffer_length= 255;
  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "id    : %d\n", id);
    fprintf(stdout, "name  : %s(%ld)\n", data, length);
  }

  DIE_UNLESS(id == 9876);
  DIE_UNLESS(length == 19 || length == 20); /* Due to VARCHAR(20) */
  DIE_UNLESS(is_prefix(data, "MySQL - Open Source") == 1);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n name  : %s(%ld)", data, length);

  DIE_UNLESS(length == 1);
  DIE_UNLESS(strcmp(data, "'") == 0);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n name  : %s(%ld)", data, length);

  DIE_UNLESS(length == 1);
  DIE_UNLESS(strcmp(data, "\"") == 0);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n name  : %s(%ld)", data, length);

  DIE_UNLESS(length == 7);
  DIE_UNLESS(strcmp(data, "my\'sql\'") == 0);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n name  : %s(%ld)", data, length);

  DIE_UNLESS(length == 7);
  /*DIE_UNLESS(strcmp(data, "my\"sql\"") == 0); */

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE test_logs");
  myquery(rc);
}


/* Test 'n' statements create and close */

static void test_nstmts()
{
  MYSQL_STMT  *stmt;
  char        query[255];
  int         rc;
  static uint i, total_stmts= 2000;
  MYSQL_BIND  bind[1];

  myheader("test_nstmts");

  mysql_autocommit(mysql, TRUE);

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_nstmts");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_nstmts(id int)");
  myquery(rc);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer= (void *)&i;
  bind[0].buffer_type= MYSQL_TYPE_LONG;

  for (i= 0; i < total_stmts; i++)
  {
    if (!opt_silent)
      fprintf(stdout, "\r stmt: %d", i);

    strmov(query, "insert into test_nstmts values(?)");
    stmt= mysql_simple_prepare(mysql, query);
    check_stmt(stmt);

    rc= mysql_stmt_bind_param(stmt, bind);
    check_execute(stmt, rc);

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    mysql_stmt_close(stmt);
  }

  stmt= mysql_simple_prepare(mysql, " select count(*) from test_nstmts");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  i= 0;
  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n total rows: %d", i);
  DIE_UNLESS( i == total_stmts);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE test_nstmts");
  myquery(rc);
}


/* Test stmt seek() functions */

static void test_fetch_seek()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[3];
  MYSQL_ROW_OFFSET row;
  int        rc;
  int32      c1;
  char       c2[11], c3[20];

  myheader("test_fetch_seek");
  rc= mysql_query(mysql, "drop table if exists t1");

  myquery(rc);

  rc= mysql_query(mysql, "create table t1(c1 int primary key auto_increment, c2 char(10), c3 timestamp(14))");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1(c2) values('venu'), ('mysql'), ('open'), ('source')");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "select * from t1");
  check_stmt(stmt);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&c1;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)c2;
  bind[1].buffer_length= sizeof(c2);

  bind[2]= bind[1];
  bind[2].buffer= (void *)c3;
  bind[2].buffer_length= sizeof(c3);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 0: %ld, %s, %s", (long) c1, c2, c3);

  row= mysql_stmt_row_tell(stmt);

  row= mysql_stmt_row_seek(stmt, row);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 2: %ld, %s, %s", (long) c1, c2, c3);

  row= mysql_stmt_row_seek(stmt, row);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 2: %ld, %s, %s", (long) c1, c2, c3);

  mysql_stmt_data_seek(stmt, 0);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 0: %ld, %s, %s", (long) c1, c2, c3);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
  myquery(mysql_query(mysql, "drop table t1"));
}


/* Test mysql_stmt_fetch_column() with offset */

static void test_fetch_offset()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char       data[11];
  ulong      length;
  int        rc;
  my_bool    is_null;


  myheader("test_fetch_offset");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1(a char(10))");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1 values('abcdefghij'), (null)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "select * from t1");
  check_stmt(stmt);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)data;
  bind[0].buffer_length= 11;
  bind[0].is_null= &is_null;
  bind[0].length= &length;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute_r(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  data[0]= '\0';
  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 1: %s (%ld)", data, length);
  DIE_UNLESS(strncmp(data, "abcd", 4) == 0 && length == 10);

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 5);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 1: %s (%ld)", data, length);
  DIE_UNLESS(strncmp(data, "fg", 2) == 0 && length == 10);

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 9);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 0: %s (%ld)", data, length);
  DIE_UNLESS(strncmp(data, "j", 1) == 0 && length == 10);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  is_null= 0;

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute(stmt, rc);

  DIE_UNLESS(is_null == 1);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  rc= mysql_stmt_fetch_column(stmt, bind, 1, 0);
  check_execute_r(stmt, rc);

  mysql_stmt_close(stmt);

  myquery(mysql_query(mysql, "drop table t1"));
}


/* Test mysql_stmt_fetch_column() */

static void test_fetch_column()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  char       c2[20], bc2[20];
  ulong      l1, l2, bl1, bl2;
  int        rc, c1, bc1;

  myheader("test_fetch_column");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1(c1 int primary key auto_increment, c2 char(10))");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1(c2) values('venu'), ('mysql')");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "select * from t1 order by c2 desc");
  check_stmt(stmt);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&bc1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &bl1;
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)bc2;
  bind[1].buffer_length= 7;
  bind[1].is_null= 0;
  bind[1].length= &bl2;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch_column(stmt, bind, 1, 0); /* No-op at this point */
  check_execute_r(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 0: %d, %s", bc1, bc2);

  c2[0]= '\0'; l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)c2;
  bind[0].buffer_length= 7;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc= mysql_stmt_fetch_column(stmt, bind, 1, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 1: %s(%ld)", c2, l2);
  DIE_UNLESS(strncmp(c2, "venu", 4) == 0 && l2 == 4);

  c2[0]= '\0'; l2= 0;
  rc= mysql_stmt_fetch_column(stmt, bind, 1, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 1: %s(%ld)", c2, l2);
  DIE_UNLESS(strcmp(c2, "venu") == 0 && l2 == 4);

  c1= 0;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &l1;

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 0: %d(%ld)", c1, l1);
  DIE_UNLESS(c1 == 1 && l1 == 4);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
    fprintf(stdout, "\n row 1: %d, %s", bc1, bc2);

  c2[0]= '\0'; l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)c2;
  bind[0].buffer_length= 7;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc= mysql_stmt_fetch_column(stmt, bind, 1, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 1: %s(%ld)", c2, l2);
  DIE_UNLESS(strncmp(c2, "mysq", 4) == 0 && l2 == 5);

  c2[0]= '\0'; l2= 0;
  rc= mysql_stmt_fetch_column(stmt, bind, 1, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 1: %si(%ld)", c2, l2);
  DIE_UNLESS(strcmp(c2, "mysql") == 0 && l2 == 5);

  c1= 0;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &l1;

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 0: %d(%ld)", c1, l1);
  DIE_UNLESS(c1 == 2 && l1 == 4);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  rc= mysql_stmt_fetch_column(stmt, bind, 1, 0);
  check_execute_r(stmt, rc);

  mysql_stmt_close(stmt);
  myquery(mysql_query(mysql, "drop table t1"));
}


/* Test mysql_list_fields() */

static void test_list_fields()
{
  MYSQL_RES *result;
  int rc;
  myheader("test_list_fields");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1(c1 int primary key auto_increment, c2 char(10) default 'mysql')");
  myquery(rc);

  result= mysql_list_fields(mysql, "t1", NULL);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 0);

  verify_prepare_field(result, 0, "c1", "c1", MYSQL_TYPE_LONG,
                       "t1", "t1",
                       current_db, 11, "0");

  verify_prepare_field(result, 1, "c2", "c2", MYSQL_TYPE_STRING,
                       "t1", "t1",
                       current_db, 10, "mysql");

  mysql_free_result(result);
  myquery(mysql_query(mysql, "drop table t1"));
}


static void test_bug19671()
{
  MYSQL_RES *result;
  int rc;
  myheader("test_bug19671");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);

  rc= mysql_query(mysql, "drop view if exists v1");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1(f1 int)");
  myquery(rc);

  rc= mysql_query(mysql, "create view v1 as select va.* from t1 va");
  myquery(rc);

  result= mysql_list_fields(mysql, "v1", NULL);
  mytest(result);

  rc= my_process_result_set(result);
  DIE_UNLESS(rc == 0);

  verify_prepare_field(result, 0, "f1", "f1", MYSQL_TYPE_LONG,
                       "v1", "v1", current_db, 11, "0");

  mysql_free_result(result);
  myquery(mysql_query(mysql, "drop view v1"));
  myquery(mysql_query(mysql, "drop table t1"));
}


/* Test a memory ovverun bug */

static void test_mem_overun()
{
  char       buffer[10000], field[10];
  MYSQL_STMT *stmt;
  MYSQL_RES  *field_res;
  int        rc, i, length;

  myheader("test_mem_overun");

  /*
    Test a memory ovverun bug when a table had 1000 fields with
    a row of data
  */
  rc= mysql_query(mysql, "drop table if exists t_mem_overun");
  myquery(rc);

  strxmov(buffer, "create table t_mem_overun(", NullS);
  for (i= 0; i < 1000; i++)
  {
    sprintf(field, "c%d int", i);
    strxmov(buffer, buffer, field, ", ", NullS);
  }
  length= strlen(buffer);
  buffer[length-2]= ')';
  buffer[--length]= '\0';

  rc= mysql_real_query(mysql, buffer, length);
  myquery(rc);

  strxmov(buffer, "insert into t_mem_overun values(", NullS);
  for (i= 0; i < 1000; i++)
  {
    strxmov(buffer, buffer, "1, ", NullS);
  }
  length= strlen(buffer);
  buffer[length-2]= ')';
  buffer[--length]= '\0';

  rc= mysql_real_query(mysql, buffer, length);
  myquery(rc);

  rc= mysql_query(mysql, "select * from t_mem_overun");
  myquery(rc);

  rc= my_process_result(mysql);
  DIE_UNLESS(rc == 1);

  stmt= mysql_simple_prepare(mysql, "select * from t_mem_overun");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  field_res= mysql_stmt_result_metadata(stmt);
  mytest(field_res);

  if (!opt_silent)
    fprintf(stdout, "\n total fields : %d", mysql_num_fields(field_res));
  DIE_UNLESS( 1000 == mysql_num_fields(field_res));

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_free_result(field_res);

  mysql_stmt_close(stmt);
}


/* Test mysql_stmt_free_result() */

static void test_free_result()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char       c2[5];
  ulong      bl1, l2;
  int        rc, c1, bc1;

  myheader("test_free_result");

  rc= mysql_query(mysql, "drop table if exists test_free_result");
  myquery(rc);

  rc= mysql_query(mysql, "create table test_free_result("
                         "c1 int primary key auto_increment)");
  myquery(rc);

  rc= mysql_query(mysql, "insert into test_free_result values(), (), ()");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "select * from test_free_result");
  check_stmt(stmt);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&bc1;
  bind[0].length= &bl1;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  c2[0]= '\0'; l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)c2;
  bind[0].buffer_length= 7;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 0: %s(%ld)", c2, l2);
  DIE_UNLESS(strncmp(c2, "1", 1) == 0 && l2 == 1);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  c1= 0, l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 0: %d(%ld)", c1, l2);
  DIE_UNLESS(c1 == 2 && l2 == 4);

  rc= mysql_query(mysql, "drop table test_free_result");
  myquery_r(rc); /* error should be, COMMANDS OUT OF SYNC */

  rc= mysql_stmt_free_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_query(mysql, "drop table test_free_result");
  myquery(rc);  /* should be successful */

  mysql_stmt_close(stmt);
}


/* Test mysql_stmt_store_result() */

static void test_free_store_result()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char       c2[5];
  ulong      bl1, l2;
  int        rc, c1, bc1;

  myheader("test_free_store_result");

  rc= mysql_query(mysql, "drop table if exists test_free_result");
  myquery(rc);

  rc= mysql_query(mysql, "create table test_free_result(c1 int primary key auto_increment)");
  myquery(rc);

  rc= mysql_query(mysql, "insert into test_free_result values(), (), ()");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "select * from test_free_result");
  check_stmt(stmt);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&bc1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &bl1;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  c2[0]= '\0'; l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)c2;
  bind[0].buffer_length= 7;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 1: %s(%ld)", c2, l2);
  DIE_UNLESS(strncmp(c2, "1", 1) == 0 && l2 == 1);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  c1= 0, l2= 0;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= &l2;

  rc= mysql_stmt_fetch_column(stmt, bind, 0, 0);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "\n col 0: %d(%ld)", c1, l2);
  DIE_UNLESS(c1 == 2 && l2 == 4);

  rc= mysql_stmt_free_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_query(mysql, "drop table test_free_result");
  myquery(rc);

  mysql_stmt_close(stmt);
}


/* Test SQLmode */

static void test_sqlmode()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  char       c1[5], c2[5];
  int        rc;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_sqlmode");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_piping");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_piping(name varchar(10))");
  myquery(rc);

  /* PIPES_AS_CONCAT */
  strmov(query, "SET SQL_MODE= \"PIPES_AS_CONCAT\"");
  if (!opt_silent)
    fprintf(stdout, "\n With %s", query);
  rc= mysql_query(mysql, query);
  myquery(rc);

  strmov(query, "INSERT INTO test_piping VALUES(?||?)");
  if (!opt_silent)
    fprintf(stdout, "\n  query: %s", query);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  if (!opt_silent)
    fprintf(stdout, "\n  total parameters: %ld", mysql_stmt_param_count(stmt));

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)c1;
  bind[0].buffer_length= 2;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)c2;
  bind[1].buffer_length= 3;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  strmov(c1, "My"); strmov(c2, "SQL");
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);
  verify_col_data("test_piping", "name", "MySQL");

  rc= mysql_query(mysql, "DELETE FROM test_piping");
  myquery(rc);

  strmov(query, "SELECT connection_id    ()");
  if (!opt_silent)
    fprintf(stdout, "\n  query: %s", query);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  /* ANSI */
  strmov(query, "SET SQL_MODE= \"ANSI\"");
  if (!opt_silent)
    fprintf(stdout, "\n With %s", query);
  rc= mysql_query(mysql, query);
  myquery(rc);

  strmov(query, "INSERT INTO test_piping VALUES(?||?)");
  if (!opt_silent)
    fprintf(stdout, "\n  query: %s", query);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  if (!opt_silent)
    fprintf(stdout, "\n  total parameters: %ld", mysql_stmt_param_count(stmt));

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  strmov(c1, "My"); strmov(c2, "SQL");
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);
  verify_col_data("test_piping", "name", "MySQL");

  /* ANSI mode spaces ... */
  strmov(query, "SELECT connection_id    ()");
  if (!opt_silent)
    fprintf(stdout, "\n  query: %s", query);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);
  if (!opt_silent)
    fprintf(stdout, "\n  returned 1 row\n");

  mysql_stmt_close(stmt);

  /* IGNORE SPACE MODE */
  strmov(query, "SET SQL_MODE= \"IGNORE_SPACE\"");
  if (!opt_silent)
    fprintf(stdout, "\n With %s", query);
  rc= mysql_query(mysql, query);
  myquery(rc);

  strmov(query, "SELECT connection_id    ()");
  if (!opt_silent)
    fprintf(stdout, "\n  query: %s", query);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);
  if (!opt_silent)
    fprintf(stdout, "\n  returned 1 row");

  mysql_stmt_close(stmt);
}


/* Test for timestamp handling */

static void test_ts()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[6];
  MYSQL_TIME ts;
  MYSQL_RES  *prep_res;
  char       strts[30];
  ulong      length;
  int        rc, field_count;
  char       name;
  char query[MAX_TEST_QUERY_LENGTH];

  myheader("test_ts");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_ts");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_ts(a DATE, b TIME, c TIMESTAMP)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "INSERT INTO test_ts VALUES(?, ?, ?), (?, ?, ?)");
  check_stmt(stmt);

  ts.year= 2003;
  ts.month= 07;
  ts.day= 12;
  ts.hour= 21;
  ts.minute= 07;
  ts.second= 46;
  ts.second_part= 0;
  length= (long)(strmov(strts, "2003-07-12 21:07:46") - strts);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_TIMESTAMP;
  bind[0].buffer= (void *)&ts;
  bind[0].buffer_length= sizeof(ts);

  bind[2]= bind[1]= bind[0];

  bind[3].buffer_type= MYSQL_TYPE_STRING;
  bind[3].buffer= (void *)strts;
  bind[3].buffer_length= sizeof(strts);
  bind[3].length= &length;

  bind[5]= bind[4]= bind[3];

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  verify_col_data("test_ts", "a", "2003-07-12");
  verify_col_data("test_ts", "b", "21:07:46");
  verify_col_data("test_ts", "c", "2003-07-12 21:07:46");

  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_ts");
  check_stmt(stmt);

  prep_res= mysql_stmt_result_metadata(stmt);
  mytest(prep_res);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 2);
  field_count= mysql_num_fields(prep_res);

  mysql_free_result(prep_res);
  mysql_stmt_close(stmt);

  for (name= 'a'; field_count--; name++)
  {
    int row_count= 0;

    sprintf(query, "SELECT a, b, c FROM test_ts WHERE %c=?", name);

    if (!opt_silent)
      fprintf(stdout, "\n  %s", query);
    stmt= mysql_simple_prepare(mysql, query);
    check_stmt(stmt);

    rc= mysql_stmt_bind_param(stmt, bind);
    check_execute(stmt, rc);

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    while (mysql_stmt_fetch(stmt) == 0)
      row_count++;

    if (!opt_silent)
      fprintf(stdout, "\n   returned '%d' rows", row_count);
    DIE_UNLESS(row_count == 2);
    mysql_stmt_close(stmt);
  }
}


/* Test for bug #1500. */

static void test_bug1500()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[3];
  int        rc;
  int32 int_data[3]= {2, 3, 4};
  const char *data;

  myheader("test_bug1500");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bg1500");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bg1500 (i INT)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_bg1500 VALUES (1), (2)");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "SELECT i FROM test_bg1500 WHERE i IN (?, ?, ?)");
  check_stmt(stmt);
  verify_param_count(stmt, 3);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer= (void *)int_data;
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[2]= bind[1]= bind[0];
  bind[1].buffer= (void *)(int_data + 1);
  bind[2].buffer= (void *)(int_data + 2);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE test_bg1500");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bg1500 (s VARCHAR(25), FULLTEXT(s))");
  myquery(rc);

  rc= mysql_query(mysql,
        "INSERT INTO test_bg1500 VALUES ('Gravedigger'), ('Greed'), ('Hollow Dogs')");
  myquery(rc);

  rc= mysql_commit(mysql);
  myquery(rc);

  stmt= mysql_simple_prepare(mysql,
          "SELECT s FROM test_bg1500 WHERE MATCH (s) AGAINST (?)");
  check_stmt(stmt);

  verify_param_count(stmt, 1);

  data= "Dogs";
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *) data;
  bind[0].buffer_length= strlen(data);
  bind[0].is_null= 0;
  bind[0].length= 0;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  mysql_stmt_close(stmt);

  /* This should work too */
  stmt= mysql_simple_prepare(mysql,
          "SELECT s FROM test_bg1500 WHERE MATCH (s) AGAINST (CONCAT(?, 'digger'))");
  check_stmt(stmt);

  verify_param_count(stmt, 1);

  data= "Grave";
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *) data;
  bind[0].buffer_length= strlen(data);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 1);

  mysql_stmt_close(stmt);
}


static void test_bug1946()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *query= "INSERT INTO prepare_command VALUES (?)";

  myheader("test_bug1946");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS prepare_command");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE prepare_command(ID INT)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  rc= mysql_real_query(mysql, query, strlen(query));
  DIE_UNLESS(rc != 0);
  if (!opt_silent)
    fprintf(stdout, "Got error (as expected):\n");
  myerror(NULL);

  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "DROP TABLE prepare_command");
}


static void test_parse_error_and_bad_length()
{
  MYSQL_STMT *stmt;
  int rc;

  /* check that we get 4 syntax errors over the 4 calls */
  myheader("test_parse_error_and_bad_length");

  rc= mysql_query(mysql, "SHOW DATABAAAA");
  DIE_UNLESS(rc);
  if (!opt_silent)
    fprintf(stdout, "Got error (as expected): '%s'\n", mysql_error(mysql));
  rc= mysql_real_query(mysql, "SHOW DATABASES", 100);
  DIE_UNLESS(rc);
  if (!opt_silent)
    fprintf(stdout, "Got error (as expected): '%s'\n", mysql_error(mysql));

  stmt= mysql_simple_prepare(mysql, "SHOW DATABAAAA");
  DIE_UNLESS(!stmt);
  if (!opt_silent)
    fprintf(stdout, "Got error (as expected): '%s'\n", mysql_error(mysql));
  stmt= mysql_stmt_init(mysql);
  DIE_UNLESS(stmt);
  rc= mysql_stmt_prepare(stmt, "SHOW DATABASES", 100);
  DIE_UNLESS(rc != 0);
  if (!opt_silent)
    fprintf(stdout, "Got error (as expected): '%s'\n", mysql_stmt_error(stmt));
  mysql_stmt_close(stmt);
}


static void test_bug2247()
{
  MYSQL_STMT *stmt;
  MYSQL_RES *res;
  int rc;
  int i;
  const char *create= "CREATE TABLE bug2247(id INT UNIQUE AUTO_INCREMENT)";
  const char *insert= "INSERT INTO bug2247 VALUES (NULL)";
  const char *select= "SELECT id FROM bug2247";
  const char *update= "UPDATE bug2247 SET id=id+10";
  const char *drop= "DROP TABLE IF EXISTS bug2247";
  ulonglong exp_count;
  enum { NUM_ROWS= 5 };

  myheader("test_bug2247");

  if (!opt_silent)
    fprintf(stdout, "\nChecking if stmt_affected_rows is not affected by\n"
                  "mysql_query ... ");
  /* create table and insert few rows */
  rc= mysql_query(mysql, drop);
  myquery(rc);

  rc= mysql_query(mysql, create);
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, insert);
  check_stmt(stmt);
  for (i= 0; i < NUM_ROWS; ++i)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }
  exp_count= mysql_stmt_affected_rows(stmt);
  DIE_UNLESS(exp_count == 1);

  rc= mysql_query(mysql, select);
  myquery(rc);
  /*
    mysql_store_result overwrites mysql->affected_rows. Check that
    mysql_stmt_affected_rows() returns the same value, whereas
    mysql_affected_rows() value is correct.
  */
  res= mysql_store_result(mysql);
  mytest(res);

  DIE_UNLESS(mysql_affected_rows(mysql) == NUM_ROWS);
  DIE_UNLESS(exp_count == mysql_stmt_affected_rows(stmt));

  rc= mysql_query(mysql, update);
  myquery(rc);
  DIE_UNLESS(mysql_affected_rows(mysql) == NUM_ROWS);
  DIE_UNLESS(exp_count == mysql_stmt_affected_rows(stmt));

  mysql_free_result(res);
  mysql_stmt_close(stmt);

  /* check that mysql_stmt_store_result modifies mysql_stmt_affected_rows */
  stmt= mysql_simple_prepare(mysql, select);
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);
  exp_count= mysql_stmt_affected_rows(stmt);
  DIE_UNLESS(exp_count == NUM_ROWS);

  rc= mysql_query(mysql, insert);
  myquery(rc);
  DIE_UNLESS(mysql_affected_rows(mysql) == 1);
  DIE_UNLESS(mysql_stmt_affected_rows(stmt) == exp_count);

  mysql_stmt_close(stmt);
  if (!opt_silent)
    fprintf(stdout, "OK");
}


static void test_subqueries()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query= "SELECT (SELECT SUM(a+b) FROM t2 where t1.b=t2.b GROUP BY t1.a LIMIT 1) as scalar_s, exists (select 1 from t2 where t2.a/2=t1.a) as exists_s, a in (select a+3 from t2) as in_s, (a-1, b-1) in (select a, b from t2) as in_row_s FROM t1, (select a x, b y from t2) tt WHERE x=a";

  myheader("test_subqueries");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1 (a int , b int);");
  myquery(rc);

  rc= mysql_query(mysql,
                  "insert into t1 values (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);");
  myquery(rc);

  rc= mysql_query(mysql, "create table t2 select * from t1;");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= my_process_stmt_result(stmt);
    DIE_UNLESS(rc == 5);
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1, t2");
  myquery(rc);
}


static void test_bad_union()
{
  MYSQL_STMT *stmt;
  const char *query= "SELECT 1, 2 union SELECT 1";

  myheader("test_bad_union");

  stmt= mysql_simple_prepare(mysql, query);
  DIE_UNLESS(stmt == 0);
  myerror(NULL);
}


static void test_distinct()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query=
    "SELECT 2+count(distinct b), group_concat(a) FROM t1 group by a";

  myheader("test_distinct");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1 (a int , b int);");
  myquery(rc);

  rc= mysql_query(mysql,
                  "insert into t1 values (1, 1), (2, 2), (3, 3), (4, 4), (5, 5), \
(1, 10), (2, 20), (3, 30), (4, 40), (5, 50);");
  myquery(rc);

  for (i= 0; i < 3; i++)
  {
    stmt= mysql_simple_prepare(mysql, query);
    check_stmt(stmt);
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= my_process_stmt_result(stmt);
    DIE_UNLESS(rc == 5);
    mysql_stmt_close(stmt);
  }

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


/*
  Test for bug#2248 "mysql_fetch without prior mysql_stmt_execute hangs"
*/

static void test_bug2248()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *query1= "SELECT DATABASE()";
  const char *query2= "INSERT INTO test_bug2248 VALUES (10)";

  myheader("test_bug2248");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_bug2248");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_bug2248 (id int)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, query1);
  check_stmt(stmt);

  /* This should not hang */
  rc= mysql_stmt_fetch(stmt);
  check_execute_r(stmt, rc);

  /* And this too */
  rc= mysql_stmt_store_result(stmt);
  check_execute_r(stmt, rc);

  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql, query2);
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  /* This too should not hang but should return proper error */
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 1);

  /* This too should not hang but should not bark */
  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  /* This should return proper error */
  rc= mysql_stmt_fetch(stmt);
  check_execute_r(stmt, rc);
  DIE_UNLESS(rc == 1);

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE test_bug2248");
  myquery(rc);
}


static void test_subqueries_ref()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query= "SELECT a as ccc from t1 where a+1=(SELECT 1+ccc from t1 where ccc+1=a+1 and a=1)";

  myheader("test_subqueries_ref");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1 (a int);");
  myquery(rc);

  rc= mysql_query(mysql,
                  "insert into t1 values (1), (2), (3), (4), (5);");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= my_process_stmt_result(stmt);
    DIE_UNLESS(rc == 1);
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_union()
{
  MYSQL_STMT *stmt;
  int rc;

  myheader("test_union");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql,
                  "CREATE TABLE t1 "
                  "(id INTEGER NOT NULL PRIMARY KEY, "
                  " name VARCHAR(20) NOT NULL)");
  myquery(rc);
  rc= mysql_query(mysql,
                  "INSERT INTO t1 (id, name) VALUES "
                  "(2, 'Ja'), (3, 'Ede'), "
                  "(4, 'Haag'), (5, 'Kabul'), "
                  "(6, 'Almere'), (7, 'Utrecht'), "
                  "(8, 'Qandahar'), (9, 'Amsterdam'), "
                  "(10, 'Amersfoort'), (11, 'Constantine')");
  myquery(rc);
  rc= mysql_query(mysql,
                  "CREATE TABLE t2 "
                  "(id INTEGER NOT NULL PRIMARY KEY, "
                  " name VARCHAR(20) NOT NULL)");
  myquery(rc);
  rc= mysql_query(mysql,
                  "INSERT INTO t2 (id, name) VALUES "
                  "(4, 'Guam'), (5, 'Aruba'), "
                  "(6, 'Angola'), (7, 'Albania'), "
                  "(8, 'Anguilla'), (9, 'Argentina'), "
                  "(10, 'Azerbaijan'), (11, 'Afghanistan'), "
                  "(12, 'Burkina Faso'), (13, 'Faroe Islands')");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql,
                             "SELECT t1.name FROM t1 UNION "
                             "SELECT t2.name FROM t2");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  rc= my_process_stmt_result(stmt);
  DIE_UNLESS(rc == 20);
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1, t2");
  myquery(rc);
}


static void test_bug3117()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND buffer;
  longlong lii;
  ulong length;
  my_bool is_null;
  int rc;

  myheader("test_bug3117");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1 (id int auto_increment primary key)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "SELECT LAST_INSERT_ID()");
  check_stmt(stmt);

  rc= mysql_query(mysql, "INSERT INTO t1 VALUES (NULL)");
  myquery(rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) &buffer, sizeof(buffer));
  buffer.buffer_type= MYSQL_TYPE_LONGLONG;
  buffer.buffer_length= sizeof(lii);
  buffer.buffer= (void *)&lii;
  buffer.length= &length;
  buffer.is_null= &is_null;

  rc= mysql_stmt_bind_result(stmt, &buffer);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  DIE_UNLESS(is_null == 0 && lii == 1);
  if (!opt_silent)
    fprintf(stdout, "\n\tLAST_INSERT_ID()= 1 ok\n");

  rc= mysql_query(mysql, "INSERT INTO t1 VALUES (NULL)");
  myquery(rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  DIE_UNLESS(is_null == 0 && lii == 2);
  if (!opt_silent)
    fprintf(stdout, "\tLAST_INSERT_ID()= 2 ok\n");

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_join()
{
  MYSQL_STMT *stmt;
  int rc, i, j;
  const char *query[]= {"SELECT * FROM t2 join t1 on (t1.a=t2.a)",
                        "SELECT * FROM t2 natural join t1",
                        "SELECT * FROM t2 join t1 using(a)",
                        "SELECT * FROM t2 left join t1 on(t1.a=t2.a)",
                        "SELECT * FROM t2 natural left join t1",
                        "SELECT * FROM t2 left join t1 using(a)",
                        "SELECT * FROM t2 right join t1 on(t1.a=t2.a)",
                        "SELECT * FROM t2 natural right join t1",
                        "SELECT * FROM t2 right join t1 using(a)"};

  myheader("test_join");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1 (a int , b int);");
  myquery(rc);

  rc= mysql_query(mysql,
                  "insert into t1 values (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t2 (a int , c int);");
  myquery(rc);

  rc= mysql_query(mysql,
                  "insert into t2 values (1, 1), (2, 2), (3, 3), (4, 4), (5, 5);");
  myquery(rc);

  for (j= 0; j < 9; j++)
  {
    stmt= mysql_simple_prepare(mysql, query[j]);
    check_stmt(stmt);
    for (i= 0; i < 3; i++)
    {
      rc= mysql_stmt_execute(stmt);
      check_execute(stmt, rc);
      rc= my_process_stmt_result(stmt);
      DIE_UNLESS(rc == 5);
    }
    mysql_stmt_close(stmt);
  }

  rc= mysql_query(mysql, "DROP TABLE t1, t2");
  myquery(rc);
}


static void test_selecttmp()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query= "select a, (select count(distinct t1.b) as sum from t1, t2 where t1.a=t2.a and t2.b > 0 and t1.a <= t3.b group by t1.a order by sum limit 1) from t3";

  myheader("test_select_tmp");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2, t3");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1 (a int , b int);");
  myquery(rc);

  rc= mysql_query(mysql, "create table t2 (a int, b int);");
  myquery(rc);

  rc= mysql_query(mysql, "create table t3 (a int, b int);");
  myquery(rc);

  rc= mysql_query(mysql,
                  "insert into t1 values (0, 100), (1, 2), (1, 3), (2, 2), (2, 7), \
(2, -1), (3, 10);");
  myquery(rc);
  rc= mysql_query(mysql,
                  "insert into t2 values (0, 0), (1, 1), (2, 1), (3, 1), (4, 1);");
  myquery(rc);
  rc= mysql_query(mysql,
                  "insert into t3 values (3, 3), (2, 2), (1, 1);");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= my_process_stmt_result(stmt);
    DIE_UNLESS(rc == 3);
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1, t2, t3");
  myquery(rc);
}


static void test_create_drop()
{
  MYSQL_STMT *stmt_create, *stmt_drop, *stmt_select, *stmt_create_select;
  char *query;
  int rc, i;
  myheader("test_table_manipulation");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql, "create table t2 (a int);");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1 (a int);");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t2 values (3), (2), (1);");
  myquery(rc);

  query= (char*)"create table t1 (a int)";
  stmt_create= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_create);

  query= (char*)"drop table t1";
  stmt_drop= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_drop);

  query= (char*)"select a in (select a from t2) from t1";
  stmt_select= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_select);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);

  query= (char*)"create table t1 select a from t2";
  stmt_create_select= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_create_select);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt_create);
    check_execute(stmt_create, rc);
    if (!opt_silent)
      fprintf(stdout, "created %i\n", i);

    rc= mysql_stmt_execute(stmt_select);
    check_execute(stmt_select, rc);
    rc= my_process_stmt_result(stmt_select);
    DIE_UNLESS(rc == 0);

    rc= mysql_stmt_execute(stmt_drop);
    check_execute(stmt_drop, rc);
    if (!opt_silent)
      fprintf(stdout, "dropped %i\n", i);

    rc= mysql_stmt_execute(stmt_create_select);
    check_execute(stmt_create, rc);
    if (!opt_silent)
      fprintf(stdout, "created select %i\n", i);

    rc= mysql_stmt_execute(stmt_select);
    check_execute(stmt_select, rc);
    rc= my_process_stmt_result(stmt_select);
    DIE_UNLESS(rc == 3);

    rc= mysql_stmt_execute(stmt_drop);
    check_execute(stmt_drop, rc);
    if (!opt_silent)
      fprintf(stdout, "dropped %i\n", i);
  }

  mysql_stmt_close(stmt_create);
  mysql_stmt_close(stmt_drop);
  mysql_stmt_close(stmt_select);
  mysql_stmt_close(stmt_create_select);

  rc= mysql_query(mysql, "DROP TABLE t2");
  myquery(rc);
}


static void test_rename()
{
  MYSQL_STMT *stmt;
  const char *query= "rename table t1 to t2, t3 to t4";
  int rc;
  myheader("test_table_manipulation");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2, t3, t4");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  rc= mysql_query(mysql, "create table t1 (a int)");
  myquery(rc);

  rc= mysql_stmt_execute(stmt);
  check_execute_r(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "rename without t3\n");

  rc= mysql_query(mysql, "create table t3 (a int)");
  myquery(rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "rename with t3\n");

  rc= mysql_stmt_execute(stmt);
  check_execute_r(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "rename renamed\n");

  rc= mysql_query(mysql, "rename table t2 to t1, t4 to t3");
  myquery(rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  if (!opt_silent)
    fprintf(stdout, "rename reverted\n");

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t2, t4");
  myquery(rc);
}


static void test_do_set()
{
  MYSQL_STMT *stmt_do, *stmt_set;
  char *query;
  int rc, i;
  myheader("test_do_set");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1 (a int)");
  myquery(rc);

  query= (char*)"do @var:=(1 in (select * from t1))";
  stmt_do= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_do);

  query= (char*)"set @var=(1 in (select * from t1))";
  stmt_set= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_set);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt_do);
    check_execute(stmt_do, rc);
    if (!opt_silent)
      fprintf(stdout, "do %i\n", i);
    rc= mysql_stmt_execute(stmt_set);
    check_execute(stmt_set, rc);
    if (!opt_silent)
      fprintf(stdout, "set %i\n", i);
  }

  mysql_stmt_close(stmt_do);
  mysql_stmt_close(stmt_set);
}


static void test_multi()
{
  MYSQL_STMT *stmt_delete, *stmt_update, *stmt_select1, *stmt_select2;
  char *query;
  MYSQL_BIND bind[1];
  int rc, i;
  int32 param= 1;
  ulong length= 1;
  myheader("test_multi");

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&param;
  bind[0].length= &length;

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1 (a int, b int)");
  myquery(rc);

  rc= mysql_query(mysql, "create table t2 (a int, b int)");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1 values (3, 3), (2, 2), (1, 1)");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t2 values (3, 3), (2, 2), (1, 1)");
  myquery(rc);

  query= (char*)"delete t1, t2 from t1, t2 where t1.a=t2.a and t1.b=10";
  stmt_delete= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_delete);

  query= (char*)"update t1, t2 set t1.b=10, t2.b=10 where t1.a=t2.a and t1.b=?";
  stmt_update= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_update);

  query= (char*)"select * from t1";
  stmt_select1= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_select1);

  query= (char*)"select * from t2";
  stmt_select2= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_select2);

  for(i= 0; i < 3; i++)
  {
    rc= mysql_stmt_bind_param(stmt_update, bind);
    check_execute(stmt_update, rc);

    rc= mysql_stmt_execute(stmt_update);
    check_execute(stmt_update, rc);
    if (!opt_silent)
      fprintf(stdout, "update %ld\n", (long) param);

    rc= mysql_stmt_execute(stmt_delete);
    check_execute(stmt_delete, rc);
    if (!opt_silent)
      fprintf(stdout, "delete %ld\n", (long) param);

    rc= mysql_stmt_execute(stmt_select1);
    check_execute(stmt_select1, rc);
    rc= my_process_stmt_result(stmt_select1);
    DIE_UNLESS(rc == 3-param);

    rc= mysql_stmt_execute(stmt_select2);
    check_execute(stmt_select2, rc);
    rc= my_process_stmt_result(stmt_select2);
    DIE_UNLESS(rc == 3-param);

    param++;
  }

  mysql_stmt_close(stmt_delete);
  mysql_stmt_close(stmt_update);
  mysql_stmt_close(stmt_select1);
  mysql_stmt_close(stmt_select2);
  rc= mysql_query(mysql, "drop table t1, t2");
  myquery(rc);
}


static void test_insert_select()
{
  MYSQL_STMT *stmt_insert, *stmt_select;
  char *query;
  int rc;
  uint i;
  myheader("test_insert_select");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1 (a int)");
  myquery(rc);

  rc= mysql_query(mysql, "create table t2 (a int)");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t2 values (1)");
  myquery(rc);

  query= (char*)"insert into t1 select a from t2";
  stmt_insert= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_insert);

  query= (char*)"select * from t1";
  stmt_select= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_select);

  for(i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt_insert);
    check_execute(stmt_insert, rc);
    if (!opt_silent)
      fprintf(stdout, "insert %u\n", i);

    rc= mysql_stmt_execute(stmt_select);
    check_execute(stmt_select, rc);
    rc= my_process_stmt_result(stmt_select);
    DIE_UNLESS(rc == (int)(i+1));
  }

  mysql_stmt_close(stmt_insert);
  mysql_stmt_close(stmt_select);
  rc= mysql_query(mysql, "drop table t1, t2");
  myquery(rc);
}


static void test_bind_nagative()
{
  MYSQL_STMT *stmt_insert;
  char *query;
  int rc;
  MYSQL_BIND      bind[1];
  int32           my_val= 0;
  ulong           my_length= 0L;
  my_bool         my_null= FALSE;
  myheader("test_insert_select");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "create temporary table t1 (c1 int unsigned)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO t1 VALUES (1), (-1)");
  myquery(rc);

  query= (char*)"INSERT INTO t1 VALUES (?)";
  stmt_insert= mysql_simple_prepare(mysql, query);
  check_stmt(stmt_insert);

  /* bind parameters */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&my_val;
  bind[0].length= &my_length;
  bind[0].is_null= (char*)&my_null;

  rc= mysql_stmt_bind_param(stmt_insert, bind);
  check_execute(stmt_insert, rc);

  my_val= -1;
  rc= mysql_stmt_execute(stmt_insert);
  check_execute(stmt_insert, rc);

  mysql_stmt_close(stmt_insert);
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


static void test_derived()
{
  MYSQL_STMT *stmt;
  int rc, i;
  MYSQL_BIND      bind[1];
  int32           my_val= 0;
  ulong           my_length= 0L;
  my_bool         my_null= FALSE;
  const char *query=
    "select count(1) from (select f.id from t1 f where f.id=?) as x";

  myheader("test_derived");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1 (id  int(8), primary key (id)) \
TYPE=InnoDB DEFAULT CHARSET=utf8");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1 values (1)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&my_val;
  bind[0].length= &my_length;
  bind[0].is_null= (char*)&my_null;
  my_val= 1;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= my_process_stmt_result(stmt);
    DIE_UNLESS(rc == 1);
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_xjoin()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query=
    "select t.id, p1.value, n1.value, p2.value, n2.value from t3 t LEFT JOIN t1 p1 ON (p1.id=t.param1_id) LEFT JOIN t2 p2 ON (p2.id=t.param2_id) LEFT JOIN t4 n1 ON (n1.id=p1.name_id) LEFT JOIN t4 n2 ON (n2.id=p2.name_id) where t.id=1";

  myheader("test_xjoin");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, t2, t3, t4");
  myquery(rc);

  rc= mysql_query(mysql, "create table t3 (id int(8), param1_id int(8), param2_id int(8)) TYPE=InnoDB DEFAULT CHARSET=utf8");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1 ( id int(8), name_id int(8), value varchar(10)) TYPE=InnoDB DEFAULT CHARSET=utf8");
  myquery(rc);

  rc= mysql_query(mysql, "create table t2 (id int(8), name_id int(8), value varchar(10)) TYPE=InnoDB DEFAULT CHARSET=utf8;");
  myquery(rc);

  rc= mysql_query(mysql, "create table t4(id int(8), value varchar(10)) TYPE=InnoDB DEFAULT CHARSET=utf8");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t3 values (1, 1, 1), (2, 2, null)");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1 values (1, 1, 'aaa'), (2, null, 'bbb')");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t2 values (1, 2, 'ccc')");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t4 values (1, 'Name1'), (2, null)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= my_process_stmt_result(stmt);
    DIE_UNLESS(rc == 1);
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1, t2, t3, t4");
  myquery(rc);
}


static void test_bug3035()
{
  MYSQL_STMT *stmt;
  int rc;
  MYSQL_BIND bind_array[12], *bind= bind_array, *bind_end= bind + 12;
  int8 int8_val;
  uint8 uint8_val;
  int16 int16_val;
  uint16 uint16_val;
  int32 int32_val;
  uint32 uint32_val;
  longlong int64_val;
  ulonglong uint64_val;
  double double_val, udouble_val, double_tmp;
  char longlong_as_string[22], ulonglong_as_string[22];

  /* mins and maxes */
  const int8 int8_min= -128;
  const int8 int8_max= 127;
  const uint8 uint8_min= 0;
  const uint8 uint8_max= 255;

  const int16 int16_min= -32768;
  const int16 int16_max= 32767;
  const uint16 uint16_min= 0;
  const uint16 uint16_max= 65535;

  const int32 int32_max= 2147483647L;
  const int32 int32_min= -int32_max - 1;
  const uint32 uint32_min= 0;
  const uint32 uint32_max= 4294967295U;

  /* it might not work okay everyplace */
  const longlong int64_max= LL(9223372036854775807);
  const longlong int64_min= -int64_max - 1;

  const ulonglong uint64_min= 0U;
  const ulonglong uint64_max= ULL(18446744073709551615);

  const char *stmt_text;

  myheader("test_bug3035");

  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "CREATE TABLE t1 (i8 TINYINT, ui8 TINYINT UNSIGNED, "
                              "i16 SMALLINT, ui16 SMALLINT UNSIGNED, "
                              "i32 INT, ui32 INT UNSIGNED, "
                              "i64 BIGINT, ui64 BIGINT UNSIGNED, "
                              "id INTEGER NOT NULL PRIMARY KEY AUTO_INCREMENT)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  bzero((char*) bind_array, sizeof(bind_array));

  for (bind= bind_array; bind < bind_end; bind++)
    bind->error= &bind->error_value;

  bind_array[0].buffer_type= MYSQL_TYPE_TINY;
  bind_array[0].buffer= (void *) &int8_val;

  bind_array[1].buffer_type= MYSQL_TYPE_TINY;
  bind_array[1].buffer= (void *) &uint8_val;
  bind_array[1].is_unsigned= 1;

  bind_array[2].buffer_type= MYSQL_TYPE_SHORT;
  bind_array[2].buffer= (void *) &int16_val;

  bind_array[3].buffer_type= MYSQL_TYPE_SHORT;
  bind_array[3].buffer= (void *) &uint16_val;
  bind_array[3].is_unsigned= 1;

  bind_array[4].buffer_type= MYSQL_TYPE_LONG;
  bind_array[4].buffer= (void *) &int32_val;

  bind_array[5].buffer_type= MYSQL_TYPE_LONG;
  bind_array[5].buffer= (void *) &uint32_val;
  bind_array[5].is_unsigned= 1;

  bind_array[6].buffer_type= MYSQL_TYPE_LONGLONG;
  bind_array[6].buffer= (void *) &int64_val;

  bind_array[7].buffer_type= MYSQL_TYPE_LONGLONG;
  bind_array[7].buffer= (void *) &uint64_val;
  bind_array[7].is_unsigned= 1;

  stmt= mysql_stmt_init(mysql);
  check_stmt(stmt);

  stmt_text= "INSERT INTO t1 (i8, ui8, i16, ui16, i32, ui32, i64, ui64) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  mysql_stmt_bind_param(stmt, bind_array);

  int8_val= int8_min;
  uint8_val= uint8_min;
  int16_val= int16_min;
  uint16_val= uint16_min;
  int32_val= int32_min;
  uint32_val= uint32_min;
  int64_val= int64_min;
  uint64_val= uint64_min;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  int8_val= int8_max;
  uint8_val= uint8_max;
  int16_val= int16_max;
  uint16_val= uint16_max;
  int32_val= int32_max;
  uint32_val= uint32_max;
  int64_val= int64_max;
  uint64_val= uint64_max;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  stmt_text= "SELECT i8, ui8, i16, ui16, i32, ui32, i64, ui64, ui64, "
             "cast(ui64 as signed), ui64, cast(ui64 as signed)"
             "FROM t1 ORDER BY id ASC";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind_array[8].buffer_type= MYSQL_TYPE_DOUBLE;
  bind_array[8].buffer= (void *) &udouble_val;

  bind_array[9].buffer_type= MYSQL_TYPE_DOUBLE;
  bind_array[9].buffer= (void *) &double_val;

  bind_array[10].buffer_type= MYSQL_TYPE_STRING;
  bind_array[10].buffer= (void *) &ulonglong_as_string;
  bind_array[10].buffer_length= sizeof(ulonglong_as_string);

  bind_array[11].buffer_type= MYSQL_TYPE_STRING;
  bind_array[11].buffer= (void *) &longlong_as_string;
  bind_array[11].buffer_length= sizeof(longlong_as_string);

  mysql_stmt_bind_result(stmt, bind_array);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  DIE_UNLESS(int8_val == int8_min);
  DIE_UNLESS(uint8_val == uint8_min);
  DIE_UNLESS(int16_val == int16_min);
  DIE_UNLESS(uint16_val == uint16_min);
  DIE_UNLESS(int32_val == int32_min);
  DIE_UNLESS(uint32_val == uint32_min);
  DIE_UNLESS(int64_val == int64_min);
  DIE_UNLESS(uint64_val == uint64_min);
  DIE_UNLESS(double_val == (longlong) uint64_min);
  double_tmp= ulonglong2double(uint64_val);
  DIE_UNLESS(cmp_double(&udouble_val, &double_tmp));
  DIE_UNLESS(!strcmp(longlong_as_string, "0"));
  DIE_UNLESS(!strcmp(ulonglong_as_string, "0"));

  rc= mysql_stmt_fetch(stmt);

  if (!opt_silent)
  {
    printf("Truncation mask: ");
    for (bind= bind_array; bind < bind_end; bind++)
      printf("%d", (int) bind->error_value);
    printf("\n");
  }
  DIE_UNLESS(rc == MYSQL_DATA_TRUNCATED || rc == 0);

  DIE_UNLESS(int8_val == int8_max);
  DIE_UNLESS(uint8_val == uint8_max);
  DIE_UNLESS(int16_val == int16_max);
  DIE_UNLESS(uint16_val == uint16_max);
  DIE_UNLESS(int32_val == int32_max);
  DIE_UNLESS(uint32_val == uint32_max);
  DIE_UNLESS(int64_val == int64_max);
  DIE_UNLESS(uint64_val == uint64_max);
  DIE_UNLESS(double_val == (longlong) uint64_val);
  double_tmp= ulonglong2double(uint64_val);
  DIE_UNLESS(cmp_double(&udouble_val, &double_tmp));
  DIE_UNLESS(!strcmp(longlong_as_string, "-1"));
  DIE_UNLESS(!strcmp(ulonglong_as_string, "18446744073709551615"));

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

  stmt_text= "DROP TABLE t1";
  mysql_real_query(mysql, stmt_text, strlen(stmt_text));
}


static void test_union2()
{
  MYSQL_STMT *stmt;
  int rc, i;

  myheader("test_union2");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1(col1 INT, \
                                         col2 VARCHAR(40),      \
                                         col3 SMALLINT, \
                                         col4 TIMESTAMP)");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql,
                             "select col1 FROM t1 where col1=1 union distinct "
                             "select col1 FROM t1 where col1=2");
  check_stmt(stmt);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= my_process_stmt_result(stmt);
    DIE_UNLESS(rc == 0);
  }

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


/*
  This tests for various mysql_stmt_send_long_data bugs described in #1664
*/

static void test_bug1664()
{
    MYSQL_STMT *stmt;
    int        rc, int_data;
    const char *data;
    const char *str_data= "Simple string";
    MYSQL_BIND bind[2];
    const char *query= "INSERT INTO test_long_data(col2, col1) VALUES(?, ?)";

    myheader("test_bug1664");

    rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_long_data");
    myquery(rc);

    rc= mysql_query(mysql, "CREATE TABLE test_long_data(col1 int, col2 long varchar)");
    myquery(rc);

    stmt= mysql_stmt_init(mysql);
    check_stmt(stmt);
    rc= mysql_stmt_prepare(stmt, query, strlen(query));
    check_execute(stmt, rc);

    verify_param_count(stmt, 2);

    bzero((char*) bind, sizeof(bind));

    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (void *)str_data;
    bind[0].buffer_length= strlen(str_data);

    bind[1].buffer= (void *)&int_data;
    bind[1].buffer_type= MYSQL_TYPE_LONG;

    rc= mysql_stmt_bind_param(stmt, bind);
    check_execute(stmt, rc);

    int_data= 1;

    /*
      Let us supply empty long_data. This should work and should
      not break following execution.
    */
    data= "";
    rc= mysql_stmt_send_long_data(stmt, 0, data, strlen(data));
    check_execute(stmt, rc);

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    verify_col_data("test_long_data", "col1", "1");
    verify_col_data("test_long_data", "col2", "");

    rc= mysql_query(mysql, "DELETE FROM test_long_data");
    myquery(rc);

    /* This should pass OK */
    data= (char *)"Data";
    rc= mysql_stmt_send_long_data(stmt, 0, data, strlen(data));
    check_execute(stmt, rc);

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    verify_col_data("test_long_data", "col1", "1");
    verify_col_data("test_long_data", "col2", "Data");

    /* clean up */
    rc= mysql_query(mysql, "DELETE FROM test_long_data");
    myquery(rc);

    /*
      Now we are changing int parameter and don't do anything
      with first parameter. Second mysql_stmt_execute() should run
      OK treating this first parameter as string parameter.
    */

    int_data= 2;
    /* execute */
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    verify_col_data("test_long_data", "col1", "2");
    verify_col_data("test_long_data", "col2", str_data);

    /* clean up */
    rc= mysql_query(mysql, "DELETE FROM test_long_data");
    myquery(rc);

    /*
      Now we are sending other long data. It should not be
      concatened to previous.
    */

    data= (char *)"SomeOtherData";
    rc= mysql_stmt_send_long_data(stmt, 0, data, strlen(data));
    check_execute(stmt, rc);

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    verify_col_data("test_long_data", "col1", "2");
    verify_col_data("test_long_data", "col2", "SomeOtherData");

    mysql_stmt_close(stmt);

    /* clean up */
    rc= mysql_query(mysql, "DELETE FROM test_long_data");
    myquery(rc);

    /* Now let us test how mysql_stmt_reset works. */
    stmt= mysql_stmt_init(mysql);
    check_stmt(stmt);
    rc= mysql_stmt_prepare(stmt, query, strlen(query));
    check_execute(stmt, rc);
    rc= mysql_stmt_bind_param(stmt, bind);
    check_execute(stmt, rc);

    data= (char *)"SomeData";
    rc= mysql_stmt_send_long_data(stmt, 0, data, strlen(data));
    check_execute(stmt, rc);

    rc= mysql_stmt_reset(stmt);
    check_execute(stmt, rc);

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    verify_col_data("test_long_data", "col1", "2");
    verify_col_data("test_long_data", "col2", str_data);

    mysql_stmt_close(stmt);

    /* Final clean up */
    rc= mysql_query(mysql, "DROP TABLE test_long_data");
    myquery(rc);
}


static void test_order_param()
{
  MYSQL_STMT *stmt;
  int rc;

  myheader("test_order_param");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE t1(a INT, b char(10))");
  myquery(rc);

  stmt= mysql_simple_prepare(mysql,
                             "select sum(a) + 200, 1 from t1 "
                             " union distinct "
                             "select sum(a) + 200, 1 from t1 group by b ");
  check_stmt(stmt);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql,
                             "select sum(a) + 200, ? from t1 group by b "
                             " union distinct "
                             "select sum(a) + 200, 1 from t1 group by b ");
  check_stmt(stmt);
  mysql_stmt_close(stmt);

  stmt= mysql_simple_prepare(mysql,
                             "select sum(a) + 200, ? from t1 "
                             " union distinct "
                             "select sum(a) + 200, 1 from t1 group by b ");
  check_stmt(stmt);
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_union_param()
{
  MYSQL_STMT *stmt;
  char *query;
  int rc, i;
  MYSQL_BIND      bind[2];
  char            my_val[4];
  ulong           my_length= 3L;
  my_bool         my_null= FALSE;
  myheader("test_union_param");

  strmov(my_val, "abc");

  query= (char*)"select ? as my_col union distinct select ?";
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);

  /*
    We need to bzero bind structure because mysql_stmt_bind_param checks all
    its members.
  */
  bzero((char*) bind, sizeof(bind));

  /* bind parameters */
  bind[0].buffer_type=    MYSQL_TYPE_STRING;
  bind[0].buffer=         (char*) &my_val;
  bind[0].buffer_length=  4;
  bind[0].length=         &my_length;
  bind[0].is_null=        (char*)&my_null;
  bind[1].buffer_type=    MYSQL_TYPE_STRING;
  bind[1].buffer=         (char*) &my_val;
  bind[1].buffer_length=  4;
  bind[1].length=         &my_length;
  bind[1].is_null=        (char*)&my_null;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= my_process_stmt_result(stmt);
    DIE_UNLESS(rc == 1);
  }

  mysql_stmt_close(stmt);
}


static void test_ps_i18n()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *stmt_text;
  MYSQL_BIND bind_array[2];

  const char *koi8= "îÕ, ÚÁ ÒÙÂÁÌËÕ";
  const char *cp1251= "Íó, çà ðûáàëêó";
  char buf1[16], buf2[16];
  ulong buf1_len, buf2_len;


  myheader("test_ps_i18n");

  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  /*
    Create table with binary columns, set session character set to cp1251,
    client character set to koi8, and make sure that there is conversion
    on insert and no conversion on select
  */

  stmt_text= "CREATE TABLE t1 (c1 VARBINARY(255), c2 VARBINARY(255))";

  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "SET CHARACTER_SET_CLIENT=koi8r, "
                 "CHARACTER_SET_CONNECTION=cp1251, "
                 "CHARACTER_SET_RESULTS=koi8r";

  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  bzero((char*) bind_array, sizeof(bind_array));

  bind_array[0].buffer_type= MYSQL_TYPE_STRING;
  bind_array[0].buffer= (void *) koi8;
  bind_array[0].buffer_length= strlen(koi8);

  bind_array[1].buffer_type= MYSQL_TYPE_STRING;
  bind_array[1].buffer= (void *) koi8;
  bind_array[1].buffer_length= strlen(koi8);

  stmt= mysql_stmt_init(mysql);
  check_stmt(stmt);

  stmt_text= "INSERT INTO t1 (c1, c2) VALUES (?, ?)";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  mysql_stmt_bind_param(stmt, bind_array);

  mysql_stmt_send_long_data(stmt, 0, koi8, strlen(koi8));

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  stmt_text= "SELECT c1, c2 FROM t1";

  /* c1 and c2 are binary so no conversion will be done on select */
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind_array[0].buffer= buf1;
  bind_array[0].buffer_length= sizeof(buf1);
  bind_array[0].length= &buf1_len;

  bind_array[1].buffer= buf2;
  bind_array[1].buffer_length= sizeof(buf2);
  bind_array[1].length= &buf2_len;

  mysql_stmt_bind_result(stmt, bind_array);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  DIE_UNLESS(buf1_len == strlen(cp1251));
  DIE_UNLESS(buf2_len == strlen(cp1251));
  DIE_UNLESS(!memcmp(buf1, cp1251, buf1_len));
  DIE_UNLESS(!memcmp(buf2, cp1251, buf1_len));

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  /*
    Now create table with two cp1251 columns, set client character
    set to koi8 and supply columns of one row as string and another as
    binary data. Binary data must not be converted on insert, and both
    columns must be converted to client character set on select.
  */

  stmt_text= "CREATE TABLE t1 (c1 VARCHAR(255) CHARACTER SET cp1251, "
                              "c2 VARCHAR(255) CHARACTER SET cp1251)";

  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "INSERT INTO t1 (c1, c2) VALUES (?, ?)";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  /* this data must be converted */
  bind_array[0].buffer_type= MYSQL_TYPE_STRING;
  bind_array[0].buffer= (void *) koi8;
  bind_array[0].buffer_length= strlen(koi8);

  bind_array[1].buffer_type= MYSQL_TYPE_STRING;
  bind_array[1].buffer= (void *) koi8;
  bind_array[1].buffer_length= strlen(koi8);

  mysql_stmt_bind_param(stmt, bind_array);

  mysql_stmt_send_long_data(stmt, 0, koi8, strlen(koi8));

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  /* this data must not be converted */
  bind_array[0].buffer_type= MYSQL_TYPE_BLOB;
  bind_array[0].buffer= (void *) cp1251;
  bind_array[0].buffer_length= strlen(cp1251);

  bind_array[1].buffer_type= MYSQL_TYPE_BLOB;
  bind_array[1].buffer= (void *) cp1251;
  bind_array[1].buffer_length= strlen(cp1251);

  mysql_stmt_bind_param(stmt, bind_array);

  mysql_stmt_send_long_data(stmt, 0, cp1251, strlen(cp1251));

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  /* Fetch data and verify that rows are in koi8 */

  stmt_text= "SELECT c1, c2 FROM t1";

  /* c1 and c2 are binary so no conversion will be done on select */
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind_array[0].buffer= buf1;
  bind_array[0].buffer_length= sizeof(buf1);
  bind_array[0].length= &buf1_len;

  bind_array[1].buffer= buf2;
  bind_array[1].buffer_length= sizeof(buf2);
  bind_array[1].length= &buf2_len;

  mysql_stmt_bind_result(stmt, bind_array);

  while ((rc= mysql_stmt_fetch(stmt)) == 0)
  {
    DIE_UNLESS(buf1_len == strlen(koi8));
    DIE_UNLESS(buf2_len == strlen(koi8));
    DIE_UNLESS(!memcmp(buf1, koi8, buf1_len));
    DIE_UNLESS(!memcmp(buf2, koi8, buf1_len));
  }
  DIE_UNLESS(rc == MYSQL_NO_DATA);
  mysql_stmt_close(stmt);

  stmt_text= "DROP TABLE t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "SET NAMES DEFAULT";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


static void test_bug3796()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  const char *concat_arg0= "concat_with_";
  enum { OUT_BUFF_SIZE= 30 };
  char out_buff[OUT_BUFF_SIZE];
  char canonical_buff[OUT_BUFF_SIZE];
  ulong out_length;
  const char *stmt_text;
  int rc;

  myheader("test_bug3796");

  /* Create and fill test table */
  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "CREATE TABLE t1 (a INT, b VARCHAR(30))";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "INSERT INTO t1 VALUES(1, 'ONE'), (2, 'TWO')";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  /* Create statement handle and prepare it with select */
  stmt= mysql_stmt_init(mysql);
  stmt_text= "SELECT concat(?, b) FROM t1";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  /* Bind input buffers */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *) concat_arg0;
  bind[0].buffer_length= strlen(concat_arg0);

  mysql_stmt_bind_param(stmt, bind);

  /* Execute the select statement */
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer= (void *) out_buff;
  bind[0].buffer_length= OUT_BUFF_SIZE;
  bind[0].length= &out_length;

  mysql_stmt_bind_result(stmt, bind);

  rc= mysql_stmt_fetch(stmt);
  if (!opt_silent)
    printf("Concat result: '%s'\n", out_buff);
  check_execute(stmt, rc);
  strmov(canonical_buff, concat_arg0);
  strcat(canonical_buff, "ONE");
  DIE_UNLESS(strlen(canonical_buff) == out_length &&
         strncmp(out_buff, canonical_buff, out_length) == 0);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  strmov(canonical_buff + strlen(concat_arg0), "TWO");
  DIE_UNLESS(strlen(canonical_buff) == out_length &&
         strncmp(out_buff, canonical_buff, out_length) == 0);
  if (!opt_silent)
    printf("Concat result: '%s'\n", out_buff);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


static void test_bug4026()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  MYSQL_TIME time_in, time_out;
  MYSQL_TIME datetime_in, datetime_out;
  const char *stmt_text;
  int rc;

  myheader("test_bug4026");

  /* Check that microseconds are inserted and selected successfully */

  /* Create a statement handle and prepare it with select */
  stmt= mysql_stmt_init(mysql);
  stmt_text= "SELECT ?, ?";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  /* Bind input buffers */
  bzero((char*) bind, sizeof(bind));
  bzero((char*) &time_in, sizeof(time_in));
  bzero((char*) &time_out, sizeof(time_out));
  bzero((char*) &datetime_in, sizeof(datetime_in));
  bzero((char*) &datetime_out, sizeof(datetime_out));

  bind[0].buffer_type= MYSQL_TYPE_TIME;
  bind[0].buffer= (void *) &time_in;
  bind[1].buffer_type= MYSQL_TYPE_DATETIME;
  bind[1].buffer= (void *) &datetime_in;

  time_in.hour= 23;
  time_in.minute= 59;
  time_in.second= 59;
  time_in.second_part= 123456;
  /*
    This is not necessary, just to make DIE_UNLESS below work: this field
    is filled in when time is received from server
  */
  time_in.time_type= MYSQL_TIMESTAMP_TIME;

  datetime_in= time_in;
  datetime_in.year= 2003;
  datetime_in.month= 12;
  datetime_in.day= 31;
  datetime_in.time_type= MYSQL_TIMESTAMP_DATETIME;

  mysql_stmt_bind_param(stmt, bind);

  /* Execute the select statement */
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer= (void *) &time_out;
  bind[1].buffer= (void *) &datetime_out;

  mysql_stmt_bind_result(stmt, bind);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);
  if (!opt_silent)
  {
    printf("%d:%d:%d.%lu\n", time_out.hour, time_out.minute, time_out.second,
           time_out.second_part);
    printf("%d-%d-%d %d:%d:%d.%lu\n", datetime_out.year, datetime_out.month,
           datetime_out.day, datetime_out.hour,
           datetime_out.minute, datetime_out.second,
           datetime_out.second_part);
  }
  DIE_UNLESS(memcmp(&time_in, &time_out, sizeof(time_in)) == 0);
  DIE_UNLESS(memcmp(&datetime_in, &datetime_out, sizeof(datetime_in)) == 0);
  mysql_stmt_close(stmt);
}


static void test_bug4079()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  const char *stmt_text;
  uint32 res;
  int rc;

  myheader("test_bug4079");

  /* Create and fill table */
  mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  mysql_query(mysql, "CREATE TABLE t1 (a int)");
  mysql_query(mysql, "INSERT INTO t1 VALUES (1), (2)");

  /* Prepare erroneous statement */
  stmt= mysql_stmt_init(mysql);
  stmt_text= "SELECT 1 < (SELECT a FROM t1)";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  /* Execute the select statement */
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  /* Bind input buffers */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *) &res;

  mysql_stmt_bind_result(stmt, bind);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc != 0 && rc != MYSQL_NO_DATA);
  if (!opt_silent)
    printf("Got error from mysql_stmt_fetch (as expected):\n%s\n",
           mysql_stmt_error(stmt));
  /* buggy version of libmysql hanged up here */
  mysql_stmt_close(stmt);
}


static void test_bug4236()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;
  int rc;
  MYSQL_STMT backup;

  myheader("test_bug4296");

  stmt= mysql_stmt_init(mysql);

  /* mysql_stmt_execute() of statement with statement id= 0 crashed server */
  stmt_text= "SELECT 1";
  /* We need to prepare statement to pass by possible check in libmysql */
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  /* Hack to check that server works OK if statement wasn't found */
  backup.stmt_id= stmt->stmt_id;
  stmt->stmt_id= 0;
  rc= mysql_stmt_execute(stmt);
  DIE_UNLESS(rc);
  /* Restore original statement id to be able to reprepare it */
  stmt->stmt_id= backup.stmt_id;

  mysql_stmt_close(stmt);
}


static void test_bug4030()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[3];
  MYSQL_TIME time_canonical, time_out;
  MYSQL_TIME date_canonical, date_out;
  MYSQL_TIME datetime_canonical, datetime_out;
  const char *stmt_text;
  int rc;

  myheader("test_bug4030");

  /* Check that microseconds are inserted and selected successfully */

  /* Execute a query with time values in prepared mode */
  stmt= mysql_stmt_init(mysql);
  stmt_text= "SELECT '23:59:59.123456', '2003-12-31', "
             "'2003-12-31 23:59:59.123456'";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  /* Bind output buffers */
  bzero((char*) bind, sizeof(bind));
  bzero((char*) &time_canonical, sizeof(time_canonical));
  bzero((char*) &time_out, sizeof(time_out));
  bzero((char*) &date_canonical, sizeof(date_canonical));
  bzero((char*) &date_out, sizeof(date_out));
  bzero((char*) &datetime_canonical, sizeof(datetime_canonical));
  bzero((char*) &datetime_out, sizeof(datetime_out));

  bind[0].buffer_type= MYSQL_TYPE_TIME;
  bind[0].buffer= (void *) &time_out;
  bind[1].buffer_type= MYSQL_TYPE_DATE;
  bind[1].buffer= (void *) &date_out;
  bind[2].buffer_type= MYSQL_TYPE_DATETIME;
  bind[2].buffer= (void *) &datetime_out;

  time_canonical.hour= 23;
  time_canonical.minute= 59;
  time_canonical.second= 59;
  time_canonical.second_part= 123456;
  time_canonical.time_type= MYSQL_TIMESTAMP_TIME;

  date_canonical.year= 2003;
  date_canonical.month= 12;
  date_canonical.day= 31;
  date_canonical.time_type= MYSQL_TIMESTAMP_DATE;

  datetime_canonical= time_canonical;
  datetime_canonical.year= 2003;
  datetime_canonical.month= 12;
  datetime_canonical.day= 31;
  datetime_canonical.time_type= MYSQL_TIMESTAMP_DATETIME;

  mysql_stmt_bind_result(stmt, bind);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);
  if (!opt_silent)
  {
    printf("%d:%d:%d.%lu\n", time_out.hour, time_out.minute, time_out.second,
           time_out.second_part);
    printf("%d-%d-%d\n", date_out.year, date_out.month, date_out.day);
    printf("%d-%d-%d %d:%d:%d.%lu\n", datetime_out.year, datetime_out.month,
           datetime_out.day, datetime_out.hour,
           datetime_out.minute, datetime_out.second,
           datetime_out.second_part);
  }
  DIE_UNLESS(memcmp(&time_canonical, &time_out, sizeof(time_out)) == 0);
  DIE_UNLESS(memcmp(&date_canonical, &date_out, sizeof(date_out)) == 0);
  DIE_UNLESS(memcmp(&datetime_canonical, &datetime_out, sizeof(datetime_out)) == 0);
  mysql_stmt_close(stmt);
}

static void test_view()
{
  MYSQL_STMT *stmt;
  int rc, i;
  MYSQL_BIND      bind[1];
  char            str_data[50];
  ulong           length = 0L;
  long            is_null = 0L;
  const char *query=
    "SELECT COUNT(*) FROM v1 WHERE SERVERNAME=?";

  myheader("test_view");

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,t2,t3,v1");
  myquery(rc);

  rc = mysql_query(mysql, "DROP VIEW IF EXISTS v1,t1,t2,t3");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE TABLE t1 ("
                        " SERVERGRP varchar(20) NOT NULL default '', "
                        " DBINSTANCE varchar(20) NOT NULL default '', "
                        " PRIMARY KEY  (SERVERGRP)) "
                        " CHARSET=latin1 collate=latin1_bin");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE TABLE t2 ("
                        " SERVERNAME varchar(20) NOT NULL, "
                        " SERVERGRP varchar(20) NOT NULL, "
                        " PRIMARY KEY (SERVERNAME)) "
                        " CHARSET=latin1 COLLATE latin1_bin");
  myquery(rc);
  rc= mysql_query(mysql,
                  "CREATE TABLE t3 ("
                  " SERVERGRP varchar(20) BINARY NOT NULL, "
                  " TABNAME varchar(30) NOT NULL, MAPSTATE char(1) NOT NULL, "
                  " ACTSTATE char(1) NOT NULL , "
                  " LOCAL_NAME varchar(30) NOT NULL, "
                  " CHG_DATE varchar(8) NOT NULL default '00000000', "
                  " CHG_TIME varchar(6) NOT NULL default '000000', "
                  " MXUSER varchar(12) NOT NULL default '', "
                  " PRIMARY KEY (SERVERGRP, TABNAME, MAPSTATE, ACTSTATE, "
                  " LOCAL_NAME)) CHARSET=latin1 COLLATE latin1_bin");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE VIEW v1 AS select sql_no_cache"
                  " T0001.SERVERNAME AS SERVERNAME, T0003.TABNAME AS"
                  " TABNAME,T0003.LOCAL_NAME AS LOCAL_NAME,T0002.DBINSTANCE AS"
                  " DBINSTANCE from t2 T0001 join t1 T0002 join t3 T0003 where"
                  " ((T0002.SERVERGRP = T0001.SERVERGRP) and"
                  " (T0002.SERVERGRP = T0003.SERVERGRP)"
                  " and (T0003.MAPSTATE = _latin1'A') and"
                  " (T0003.ACTSTATE = _latin1' '))");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  strmov(str_data, "TEST");
  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= FIELD_TYPE_STRING;
  bind[0].buffer= (char *)&str_data;
  bind[0].buffer_length= 50;
  bind[0].length= &length;
  length= 4;
  bind[0].is_null= (char*)&is_null;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt,rc);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    assert(1 == my_process_stmt_result(stmt));
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1,t2,t3");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW v1");
  myquery(rc);
}


static void test_view_where()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query=
    "select v1.c,v2.c from v1, v2";

  myheader("test_view_where");

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,v1,v2");
  myquery(rc);

  rc = mysql_query(mysql, "DROP VIEW IF EXISTS v1,v2,t1");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE TABLE t1 (a int, b int)");
  myquery(rc);
  rc= mysql_query(mysql,"insert into t1 values (1,2), (1,3), (2,4), (2,5), (3,10)");
  myquery(rc);
  rc= mysql_query(mysql,"create view v1 (c) as select b from t1 where a<3");
  myquery(rc);
  rc= mysql_query(mysql,"create view v2 (c) as select b from t1 where a>=3");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    assert(4 == my_process_stmt_result(stmt));
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW v1, v2");
  myquery(rc);
}


static void test_view_2where()
{
  MYSQL_STMT *stmt;
  int rc, i;
  MYSQL_BIND      bind[8];
  char            parms[8][100];
  ulong           length[8];
  const char *query=
    "select relid, report, handle, log_group, username, variant, type, "
    "version, erfdat, erftime, erfname, aedat, aetime, aename, dependvars, "
    "inactive from V_LTDX where mandt = ? and relid = ? and report = ? and "
    "handle = ? and log_group = ? and username in ( ? , ? ) and type = ?";

  myheader("test_view_2where");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS LTDX");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW IF EXISTS V_LTDX");
  myquery(rc);
  rc= mysql_query(mysql,
                  "CREATE TABLE LTDX (MANDT char(3) NOT NULL default '000', "
                  " RELID char(2) NOT NULL, REPORT varchar(40) NOT NULL,"
                  " HANDLE varchar(4) NOT NULL, LOG_GROUP varchar(4) NOT NULL,"
                  " USERNAME varchar(12) NOT NULL,"
                  " VARIANT varchar(12) NOT NULL,"
                  " TYPE char(1) NOT NULL, SRTF2 int(11) NOT NULL,"
                  " VERSION varchar(6) NOT NULL default '000000',"
                  " ERFDAT varchar(8) NOT NULL default '00000000',"
                  " ERFTIME varchar(6) NOT NULL default '000000',"
                  " ERFNAME varchar(12) NOT NULL,"
                  " AEDAT varchar(8) NOT NULL default '00000000',"
                  " AETIME varchar(6) NOT NULL default '000000',"
                  " AENAME varchar(12) NOT NULL,"
                  " DEPENDVARS varchar(10) NOT NULL,"
                  " INACTIVE char(1) NOT NULL, CLUSTR smallint(6) NOT NULL,"
                  " CLUSTD blob,"
                  " PRIMARY KEY (MANDT, RELID, REPORT, HANDLE, LOG_GROUP, "
                                "USERNAME, VARIANT, TYPE, SRTF2))"
                 " CHARSET=latin1 COLLATE latin1_bin");
  myquery(rc);
  rc= mysql_query(mysql,
                  "CREATE VIEW V_LTDX AS select T0001.MANDT AS "
                  " MANDT,T0001.RELID AS RELID,T0001.REPORT AS "
                  " REPORT,T0001.HANDLE AS HANDLE,T0001.LOG_GROUP AS "
                  " LOG_GROUP,T0001.USERNAME AS USERNAME,T0001.VARIANT AS "
                  " VARIANT,T0001.TYPE AS TYPE,T0001.VERSION AS "
                  " VERSION,T0001.ERFDAT AS ERFDAT,T0001.ERFTIME AS "
                  " ERFTIME,T0001.ERFNAME AS ERFNAME,T0001.AEDAT AS "
                  " AEDAT,T0001.AETIME AS AETIME,T0001.AENAME AS "
                  " AENAME,T0001.DEPENDVARS AS DEPENDVARS,T0001.INACTIVE AS "
                  " INACTIVE from LTDX T0001 where (T0001.SRTF2 = 0)");
  myquery(rc);
  bzero((char*) bind, sizeof(bind));
  for (i=0; i < 8; i++) {
    strmov(parms[i], "1");
    bind[i].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[i].buffer = (char *)&parms[i];
    bind[i].buffer_length = 100;
    bind[i].is_null = 0;
    bind[i].length = &length[i];
    length[i] = 1;
  }
  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt,rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  assert(0 == my_process_stmt_result(stmt));

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP VIEW V_LTDX");
  myquery(rc);
  rc= mysql_query(mysql, "DROP TABLE LTDX");
  myquery(rc);
}


static void test_view_star()
{
  MYSQL_STMT *stmt;
  int rc, i;
  MYSQL_BIND      bind[8];
  char            parms[8][100];
  ulong           length[8];
  const char *query= "SELECT * FROM vt1 WHERE a IN (?,?)";

  myheader("test_view_star");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, vt1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW IF EXISTS t1, vt1");
  myquery(rc);
  rc= mysql_query(mysql, "CREATE TABLE t1 (a int)");
  myquery(rc);
  rc= mysql_query(mysql, "CREATE VIEW vt1 AS SELECT a FROM t1");
  myquery(rc);
  bzero((char*) bind, sizeof(bind));
  for (i= 0; i < 2; i++) {
    sprintf((char *)&parms[i], "%d", i);
    bind[i].buffer_type = MYSQL_TYPE_VAR_STRING;
    bind[i].buffer = (char *)&parms[i];
    bind[i].buffer_length = 100;
    bind[i].is_null = 0;
    bind[i].length = &length[i];
    length[i] = 1;
  }

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt,rc);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    assert(0 == my_process_stmt_result(stmt));
  }

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW vt1");
  myquery(rc);
}


static void test_view_insert()
{
  MYSQL_STMT *insert_stmt, *select_stmt;
  int rc, i;
  MYSQL_BIND      bind[1];
  int             my_val = 0;
  ulong           my_length = 0L;
  long            my_null = 0L;
  const char *query=
    "insert into v1 values (?)";

  myheader("test_view_insert");

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,v1");
  myquery(rc);
  rc = mysql_query(mysql, "DROP VIEW IF EXISTS t1,v1");
  myquery(rc);

  rc= mysql_query(mysql,"create table t1 (a int, primary key (a))");
  myquery(rc);

  rc= mysql_query(mysql, "create view v1 as select a from t1 where a>=1");
  myquery(rc);

  insert_stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(insert_stmt, query, strlen(query));
  check_execute(insert_stmt, rc);
  query= "select * from t1";
  select_stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(select_stmt, query, strlen(query));
  check_execute(select_stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type = FIELD_TYPE_LONG;
  bind[0].buffer = (char *)&my_val;
  bind[0].length = &my_length;
  bind[0].is_null = (char*)&my_null;
  rc= mysql_stmt_bind_param(insert_stmt, bind);
  check_execute(insert_stmt, rc);

  for (i= 0; i < 3; i++)
  {
    my_val= i;

    rc= mysql_stmt_execute(insert_stmt);
    check_execute(insert_stmt, rc);

    rc= mysql_stmt_execute(select_stmt);
    check_execute(select_stmt, rc);
    assert(i + 1 == (int) my_process_stmt_result(select_stmt));
  }
  mysql_stmt_close(insert_stmt);
  mysql_stmt_close(select_stmt);

  rc= mysql_query(mysql, "DROP VIEW v1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_left_join_view()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *query=
    "select t1.a, v1.x from t1 left join v1 on (t1.a= v1.x);";

  myheader("test_left_join_view");

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,v1");
  myquery(rc);

  rc = mysql_query(mysql, "DROP VIEW IF EXISTS v1,t1");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE TABLE t1 (a int)");
  myquery(rc);
  rc= mysql_query(mysql,"insert into t1 values (1), (2), (3)");
  myquery(rc);
  rc= mysql_query(mysql,"create view v1 (x) as select a from t1 where a > 1");
  myquery(rc);
  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    assert(3 == my_process_stmt_result(stmt));
  }
  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "DROP VIEW v1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);
}


static void test_view_insert_fields()
{
  MYSQL_STMT	*stmt;
  char		parm[11][1000];
  ulong         l[11];
  int		rc, i;
  MYSQL_BIND	bind[11];
  const char    *query= "INSERT INTO `v1` ( `K1C4` ,`K2C4` ,`K3C4` ,`K4N4` ,`F1C4` ,`F2I4` ,`F3N5` ,`F7F8` ,`F6N4` ,`F5C8` ,`F9D8` ) VALUES( ? , ? , ? , ? , ? , ? , ? , ? , ? , ? , ? )";

  myheader("test_view_insert_fields");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1, v1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW IF EXISTS t1, v1");
  myquery(rc);
  rc= mysql_query(mysql,
                  "CREATE TABLE t1 (K1C4 varchar(4) NOT NULL,"
                  "K2C4 varchar(4) NOT NULL, K3C4 varchar(4) NOT NULL,"
                  "K4N4 varchar(4) NOT NULL default '0000',"
                  "F1C4 varchar(4) NOT NULL, F2I4 int(11) NOT NULL,"
                  "F3N5 varchar(5) NOT NULL default '00000',"
                  "F4I4 int(11) NOT NULL default '0', F5C8 varchar(8) NOT NULL,"
                  "F6N4 varchar(4) NOT NULL default '0000',"
                  "F7F8 double NOT NULL default '0',"
                  "F8F8 double NOT NULL default '0',"
                  "F9D8 decimal(8,2) NOT NULL default '0.00',"
                  "PRIMARY KEY (K1C4,K2C4,K3C4,K4N4)) "
                  "CHARSET=latin1 COLLATE latin1_bin");
  myquery(rc);
  rc= mysql_query(mysql,
                  "CREATE VIEW v1 AS select sql_no_cache "
                  " K1C4 AS K1C4, K2C4 AS K2C4, K3C4 AS K3C4, K4N4 AS K4N4, "
                  " F1C4 AS F1C4, F2I4 AS F2I4, F3N5 AS F3N5,"
                  " F7F8 AS F7F8, F6N4 AS F6N4, F5C8 AS F5C8, F9D8 AS F9D8"
                  " from t1 T0001");

  bzero((char*) bind, sizeof(bind));
  for (i= 0; i < 11; i++)
  {
    l[i]= 20;
    bind[i].buffer_type= MYSQL_TYPE_STRING;
    bind[i].is_null= 0;
    bind[i].buffer= (char *)&parm[i];

    strmov(parm[i], "1");
    bind[i].buffer_length= 2;
    bind[i].length= &l[i];
  }
  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  mysql_stmt_close(stmt);

  query= "select * from t1";
  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  assert(1 == my_process_stmt_result(stmt));

  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "DROP VIEW v1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);

}

static void test_bug5126()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  int32 c1, c2;
  const char *stmt_text;
  int rc;

  myheader("test_bug5126");

  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "CREATE TABLE t1 (a mediumint, b int)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "INSERT INTO t1 VALUES (8386608, 1)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  stmt_text= "SELECT a, b FROM t1";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  /* Bind output buffers */
  bzero((char*) bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= &c1;
  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= &c2;

  mysql_stmt_bind_result(stmt, bind);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);
  DIE_UNLESS(c1 == 8386608 && c2 == 1);
  if (!opt_silent)
    printf("%ld, %ld\n", (long) c1, (long) c2);
  mysql_stmt_close(stmt);
}


static void test_bug4231()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  MYSQL_TIME tm[2];
  const char *stmt_text;
  int rc;

  myheader("test_bug4231");

  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "CREATE TABLE t1 (a int)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "INSERT INTO t1 VALUES (1)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  stmt_text= "SELECT a FROM t1 WHERE ? = ?";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  /* Bind input buffers */
  bzero((char*) bind, sizeof(bind));
  bzero((char*) tm, sizeof(tm));

  bind[0].buffer_type= MYSQL_TYPE_DATE;
  bind[0].buffer= &tm[0];
  bind[1].buffer_type= MYSQL_TYPE_DATE;
  bind[1].buffer= &tm[1];

  mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  /*
    First set server-side params to some non-zero non-equal values:
    then we will check that they are not used when client sends
    new (zero) times.
  */
  tm[0].time_type = MYSQL_TIMESTAMP_DATE;
  tm[0].year = 2000;
  tm[0].month = 1;
  tm[0].day = 1;
  tm[1]= tm[0];
  --tm[1].year;                                 /* tm[0] != tm[1] */

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);

  /* binds are unequal, no rows should be returned */
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  /* Set one of the dates to zero */
  tm[0].year= tm[0].month= tm[0].day= 0;
  tm[1]= tm[0];
  mysql_stmt_execute(stmt);
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);

  mysql_stmt_close(stmt);
  stmt_text= "DROP TABLE t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


static void test_bug5399()
{
  /*
    Ascii 97 is 'a', which gets mapped to Ascii 65 'A' unless internal
    statement id hash in the server uses binary collation.
  */
#define NUM_OF_USED_STMT 97 
  MYSQL_STMT *stmt_list[NUM_OF_USED_STMT];
  MYSQL_STMT **stmt;
  MYSQL_BIND bind[1];
  char buff[600];
  int rc;
  int32 no;

  myheader("test_bug5399");

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= &no;

  for (stmt= stmt_list; stmt != stmt_list + NUM_OF_USED_STMT; ++stmt)
  {
    sprintf(buff, "select %d", (int) (stmt - stmt_list));
    *stmt= mysql_stmt_init(mysql);
    rc= mysql_stmt_prepare(*stmt, buff, strlen(buff));
    check_execute(*stmt, rc);
    mysql_stmt_bind_result(*stmt, bind);
  }
  if (!opt_silent)
    printf("%d statements prepared.\n", NUM_OF_USED_STMT);

  for (stmt= stmt_list; stmt != stmt_list + NUM_OF_USED_STMT; ++stmt)
  {
    rc= mysql_stmt_execute(*stmt);
    check_execute(*stmt, rc);
    rc= mysql_stmt_store_result(*stmt);
    check_execute(*stmt, rc);
    rc= mysql_stmt_fetch(*stmt);
    DIE_UNLESS(rc == 0);
    DIE_UNLESS((int32) (stmt - stmt_list) == no);
  }

  for (stmt= stmt_list; stmt != stmt_list + NUM_OF_USED_STMT; ++stmt)
    mysql_stmt_close(*stmt);
#undef NUM_OF_USED_STMT
}


static void test_bug5194()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND *bind;
  char *query;
  char *param_str;
  int param_str_length;
  const char *stmt_text;
  int rc;
  float float_array[250] =
  {
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,  0.5,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,
    0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25,  0.25
  };
  float *fa_ptr= float_array;
  /* Number of columns per row */
  const int COLUMN_COUNT= sizeof(float_array)/sizeof(*float_array);
  /* Number of rows per bulk insert to start with */
  const int MIN_ROWS_PER_INSERT= 262;
  /* Max number of rows per bulk insert to end with */
  const int MAX_ROWS_PER_INSERT= 300;
  const int MAX_PARAM_COUNT= COLUMN_COUNT*MAX_ROWS_PER_INSERT;
  const char *query_template= "insert into t1 values %s";
  const int CHARS_PER_PARAM= 5; /* space needed to place ", ?" in the query */
  const int uint16_max= 65535;
  int nrows, i;

  myheader("test_bug5194");

  stmt_text= "drop table if exists t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));

  stmt_text= "create table if not exists t1"
   "(c1 float, c2 float, c3 float, c4 float, c5 float, c6 float, "
   "c7 float, c8 float, c9 float, c10 float, c11 float, c12 float, "
   "c13 float, c14 float, c15 float, c16 float, c17 float, c18 float, "
   "c19 float, c20 float, c21 float, c22 float, c23 float, c24 float, "
   "c25 float, c26 float, c27 float, c28 float, c29 float, c30 float, "
   "c31 float, c32 float, c33 float, c34 float, c35 float, c36 float, "
   "c37 float, c38 float, c39 float, c40 float, c41 float, c42 float, "
   "c43 float, c44 float, c45 float, c46 float, c47 float, c48 float, "
   "c49 float, c50 float, c51 float, c52 float, c53 float, c54 float, "
   "c55 float, c56 float, c57 float, c58 float, c59 float, c60 float, "
   "c61 float, c62 float, c63 float, c64 float, c65 float, c66 float, "
   "c67 float, c68 float, c69 float, c70 float, c71 float, c72 float, "
   "c73 float, c74 float, c75 float, c76 float, c77 float, c78 float, "
   "c79 float, c80 float, c81 float, c82 float, c83 float, c84 float, "
   "c85 float, c86 float, c87 float, c88 float, c89 float, c90 float, "
   "c91 float, c92 float, c93 float, c94 float, c95 float, c96 float, "
   "c97 float, c98 float, c99 float, c100 float, c101 float, c102 float, "
   "c103 float, c104 float, c105 float, c106 float, c107 float, c108 float, "
   "c109 float, c110 float, c111 float, c112 float, c113 float, c114 float, "
   "c115 float, c116 float, c117 float, c118 float, c119 float, c120 float, "
   "c121 float, c122 float, c123 float, c124 float, c125 float, c126 float, "
   "c127 float, c128 float, c129 float, c130 float, c131 float, c132 float, "
   "c133 float, c134 float, c135 float, c136 float, c137 float, c138 float, "
   "c139 float, c140 float, c141 float, c142 float, c143 float, c144 float, "
   "c145 float, c146 float, c147 float, c148 float, c149 float, c150 float, "
   "c151 float, c152 float, c153 float, c154 float, c155 float, c156 float, "
   "c157 float, c158 float, c159 float, c160 float, c161 float, c162 float, "
   "c163 float, c164 float, c165 float, c166 float, c167 float, c168 float, "
   "c169 float, c170 float, c171 float, c172 float, c173 float, c174 float, "
   "c175 float, c176 float, c177 float, c178 float, c179 float, c180 float, "
   "c181 float, c182 float, c183 float, c184 float, c185 float, c186 float, "
   "c187 float, c188 float, c189 float, c190 float, c191 float, c192 float, "
   "c193 float, c194 float, c195 float, c196 float, c197 float, c198 float, "
   "c199 float, c200 float, c201 float, c202 float, c203 float, c204 float, "
   "c205 float, c206 float, c207 float, c208 float, c209 float, c210 float, "
   "c211 float, c212 float, c213 float, c214 float, c215 float, c216 float, "
   "c217 float, c218 float, c219 float, c220 float, c221 float, c222 float, "
   "c223 float, c224 float, c225 float, c226 float, c227 float, c228 float, "
   "c229 float, c230 float, c231 float, c232 float, c233 float, c234 float, "
   "c235 float, c236 float, c237 float, c238 float, c239 float, c240 float, "
   "c241 float, c242 float, c243 float, c244 float, c245 float, c246 float, "
   "c247 float, c248 float, c249 float, c250 float)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  bind= (MYSQL_BIND*) malloc(MAX_PARAM_COUNT * sizeof(MYSQL_BIND));
  query= (char*) malloc(strlen(query_template) +
                        MAX_PARAM_COUNT * CHARS_PER_PARAM + 1);
  param_str= (char*) malloc(COLUMN_COUNT * CHARS_PER_PARAM);

  if (bind == 0 || query == 0 || param_str == 0)
  {
    fprintf(stderr, "Can't allocate enough memory for query structs\n");
    if (bind)
      free(bind);
    if (query)
      free(query);
    if (param_str)
      free(param_str);
    return;
  }

  stmt= mysql_stmt_init(mysql);

  /* setup a template for one row of parameters */
  sprintf(param_str, "(");
  for (i= 1; i < COLUMN_COUNT; ++i)
    strcat(param_str, "?, ");
  strcat(param_str, "?)");
  param_str_length= strlen(param_str);

  /* setup bind array */
  bzero((char*) bind, MAX_PARAM_COUNT * sizeof(MYSQL_BIND));
  for (i= 0; i < MAX_PARAM_COUNT; ++i)
  {
    bind[i].buffer_type= MYSQL_TYPE_FLOAT;
    bind[i].buffer= fa_ptr;
    if (++fa_ptr == float_array + COLUMN_COUNT)
      fa_ptr= float_array;
  }

  /*
    Test each number of rows per bulk insert, so that we can see where
    MySQL fails.
  */
  for (nrows= MIN_ROWS_PER_INSERT; nrows <= MAX_ROWS_PER_INSERT; ++nrows)
  {
    char *query_ptr;
    /* Create statement text for current number of rows */
    sprintf(query, query_template, param_str);
    query_ptr= query + strlen(query);
    for (i= 1; i < nrows; ++i)
    {
      memcpy(query_ptr, ", ", 2);
      query_ptr+= 2;
      memcpy(query_ptr, param_str, param_str_length);
      query_ptr+= param_str_length;
    }
    *query_ptr= '\0';

    rc= mysql_stmt_prepare(stmt, query, query_ptr - query);
    if (rc && nrows * COLUMN_COUNT > uint16_max)
    {
      if (!opt_silent)
        printf("Failed to prepare a statement with %d placeholders "
               "(as expected).\n", nrows * COLUMN_COUNT);
      break;
    }
    else
      check_execute(stmt, rc);

    if (!opt_silent)
      printf("Insert: query length= %d, row count= %d, param count= %lu\n",
             (int) strlen(query), nrows, mysql_stmt_param_count(stmt));

    /* bind the parameter array and execute the query */
    rc= mysql_stmt_bind_param(stmt, bind);
    check_execute(stmt, rc);

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }

  mysql_stmt_close(stmt);
  free(bind);
  free(query);
  free(param_str);
  stmt_text= "drop table t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


static void test_bug5315()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;
  int rc;

  myheader("test_bug5315");

  stmt_text= "SELECT 1";
  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  DIE_UNLESS(rc == 0);
  mysql_change_user(mysql, opt_user, opt_password, current_db);
  rc= mysql_stmt_execute(stmt);
  DIE_UNLESS(rc != 0);
  if (rc)
  {
    if (!opt_silent)
      printf("Got error (as expected):\n%s", mysql_stmt_error(stmt));
  }
  /* check that connection is OK */
  mysql_stmt_close(stmt);
  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  DIE_UNLESS(rc == 0);
  rc= mysql_stmt_execute(stmt);
  DIE_UNLESS(rc == 0);
  mysql_stmt_close(stmt);
}


static void test_bug6049()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  MYSQL_RES *res;
  MYSQL_ROW row;
  const char *stmt_text;
  char buffer[30];
  ulong length;
  int rc;

  myheader("test_bug6049");

  stmt_text= "SELECT MAKETIME(-25, 12, 12)";

  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  res= mysql_store_result(mysql);
  row= mysql_fetch_row(res);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type    = MYSQL_TYPE_STRING;
  bind[0].buffer         = &buffer;
  bind[0].buffer_length  = sizeof(buffer);
  bind[0].length         = &length;

  mysql_stmt_bind_result(stmt, bind);
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);

  if (!opt_silent)
  {
    printf("Result from query: %s\n", row[0]);
    printf("Result from prepared statement: %s\n", (char*) buffer);
  }

  DIE_UNLESS(strcmp(row[0], (char*) buffer) == 0);

  mysql_free_result(res);
  mysql_stmt_close(stmt);
}


static void test_bug6058()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  MYSQL_RES *res;
  MYSQL_ROW row;
  const char *stmt_text;
  char buffer[30];
  ulong length;
  int rc;

  myheader("test_bug6058");

  stmt_text= "SELECT CAST('0000-00-00' AS DATE)";

  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  res= mysql_store_result(mysql);
  row= mysql_fetch_row(res);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type    = MYSQL_TYPE_STRING;
  bind[0].buffer         = &buffer;
  bind[0].buffer_length  = sizeof(buffer);
  bind[0].length         = &length;

  mysql_stmt_bind_result(stmt, bind);
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);

  if (!opt_silent)
  {
    printf("Result from query: %s\n", row[0]);
    printf("Result from prepared statement: %s\n", buffer);
  }

  DIE_UNLESS(strcmp(row[0], buffer) == 0);

  mysql_free_result(res);
  mysql_stmt_close(stmt);
}


static void test_bug6059()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;

  myheader("test_bug6059");

  stmt_text= "SELECT 'foo' INTO OUTFILE 'x.3'";

  stmt= mysql_stmt_init(mysql);
  (void) mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  DIE_UNLESS(mysql_stmt_field_count(stmt) == 0);
  mysql_stmt_close(stmt);
}


static void test_bug6046()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;
  int rc;
  short b= 1;
  MYSQL_BIND bind[1];

  myheader("test_bug6046");

  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "CREATE TABLE t1 (a int, b int)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "INSERT INTO t1 VALUES (1,1),(2,2),(3,1),(4,2)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt= mysql_stmt_init(mysql);

  stmt_text= "SELECT t1.a FROM t1 NATURAL JOIN t1 as X1 "
             "WHERE t1.b > ? ORDER BY t1.a";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  b= 1;
  bzero((char*) bind, sizeof(bind));
  bind[0].buffer= &b;
  bind[0].buffer_type= MYSQL_TYPE_SHORT;

  mysql_stmt_bind_param(stmt, bind);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  mysql_stmt_store_result(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);
}



static void test_basic_cursors()
{
  const char *basic_tables[]=
  {
    "DROP TABLE IF EXISTS t1, t2",

    "CREATE TABLE t1 "
    "(id INTEGER NOT NULL PRIMARY KEY, "
    " name VARCHAR(20) NOT NULL)",

    "INSERT INTO t1 (id, name) VALUES "
    "  (2, 'Ja'), (3, 'Ede'), "
    "  (4, 'Haag'), (5, 'Kabul'), "
    "  (6, 'Almere'), (7, 'Utrecht'), "
    "  (8, 'Qandahar'), (9, 'Amsterdam'), "
    "  (10, 'Amersfoort'), (11, 'Constantine')",

    "CREATE TABLE t2 "
    "(id INTEGER NOT NULL PRIMARY KEY, "
    " name VARCHAR(20) NOT NULL)",

    "INSERT INTO t2 (id, name) VALUES "
    "  (4, 'Guam'), (5, 'Aruba'), "
    "  (6, 'Angola'), (7, 'Albania'), "
    "  (8, 'Anguilla'), (9, 'Argentina'), "
    "  (10, 'Azerbaijan'), (11, 'Afghanistan'), "
    "  (12, 'Burkina Faso'), (13, 'Faroe Islands')"
  };
  const char *queries[]=
  {
    "SELECT * FROM t1",
    "SELECT * FROM t2"
  };

  DBUG_ENTER("test_basic_cursors");
  myheader("test_basic_cursors");

  fill_tables(basic_tables, sizeof(basic_tables)/sizeof(*basic_tables));

  fetch_n(queries, sizeof(queries)/sizeof(*queries), USE_ROW_BY_ROW_FETCH);
  fetch_n(queries, sizeof(queries)/sizeof(*queries), USE_STORE_RESULT);
  DBUG_VOID_RETURN;
}


static void test_cursors_with_union()
{
  const char *queries[]=
  {
    "SELECT t1.name FROM t1 UNION SELECT t2.name FROM t2",
    "SELECT t1.id FROM t1 WHERE t1.id < 5"
  };
  myheader("test_cursors_with_union");
  fetch_n(queries, sizeof(queries)/sizeof(*queries), USE_ROW_BY_ROW_FETCH);
  fetch_n(queries, sizeof(queries)/sizeof(*queries), USE_STORE_RESULT);
}

/*
  Altough mysql_create_db(), mysql_rm_db() are deprecated since 4.0 they
  should not crash server and should not hang in case of errors.

  Since those functions can't be seen in modern API (unless client library
  was compiled with USE_OLD_FUNCTIONS define) we use simple_command() macro.
*/
static void test_bug6081()
{
  int rc;
  myheader("test_bug6081");

  rc= simple_command(mysql, COM_DROP_DB, current_db,
                     (ulong)strlen(current_db), 0);
  myquery(rc);
  rc= simple_command(mysql, COM_DROP_DB, current_db,
                     (ulong)strlen(current_db), 0);
  myquery_r(rc);
  rc= simple_command(mysql, COM_CREATE_DB, current_db,
                     (ulong)strlen(current_db), 0);
  myquery(rc);
  rc= simple_command(mysql, COM_CREATE_DB, current_db,
                     (ulong)strlen(current_db), 0);
  myquery_r(rc);
  rc= mysql_select_db(mysql, current_db);
  myquery(rc);
}


static void test_bug6096()
{
  MYSQL_STMT *stmt;
  MYSQL_RES *query_result, *stmt_metadata;
  const char *stmt_text;
  MYSQL_BIND bind[12];
  MYSQL_FIELD *query_field_list, *stmt_field_list;
  ulong query_field_count, stmt_field_count;
  int rc;
  my_bool update_max_length= TRUE;
  uint i;

  myheader("test_bug6096");

  stmt_text= "drop table if exists t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "create table t1 (c_tinyint tinyint, c_smallint smallint, "
                             " c_mediumint mediumint, c_int int, "
                             " c_bigint bigint, c_float float, "
                             " c_double double, c_varchar varchar(20), "
                             " c_char char(20), c_time time, c_date date, "
                             " c_datetime datetime)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "insert into t1  values (-100, -20000, 30000000, 4, 8, 1.0, "
                                     "2.0, 'abc', 'def', now(), now(), now())";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "select * from t1";

  /* Run select in prepared and non-prepared mode and compare metadata */
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  query_result= mysql_store_result(mysql);
  query_field_list= mysql_fetch_fields(query_result);
  query_field_count= mysql_num_fields(query_result);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH,
                      (void*) &update_max_length);
  mysql_stmt_store_result(stmt);
  stmt_metadata= mysql_stmt_result_metadata(stmt);
  stmt_field_list= mysql_fetch_fields(stmt_metadata);
  stmt_field_count= mysql_num_fields(stmt_metadata);
  DIE_UNLESS(stmt_field_count == query_field_count);

  /* Print out and check the metadata */

  if (!opt_silent)
  {
    printf(" ------------------------------------------------------------\n");
    printf("             |                     Metadata \n");
    printf(" ------------------------------------------------------------\n");
    printf("             |         Query          |   Prepared statement \n");
    printf(" ------------------------------------------------------------\n");
    printf(" field name  |  length   | max_length |  length   |  max_length\n");
    printf(" ------------------------------------------------------------\n");

    for (i= 0; i < query_field_count; ++i)
    {
      MYSQL_FIELD *f1= &query_field_list[i], *f2= &stmt_field_list[i];
      printf(" %-11s | %9lu | %10lu | %9lu | %10lu \n",
             f1->name, f1->length, f1->max_length, f2->length, f2->max_length);
      DIE_UNLESS(f1->length == f2->length);
    }
    printf(" ---------------------------------------------------------------\n");
  }

  /* Bind and fetch the data */

  bzero((char*) bind, sizeof(bind));
  for (i= 0; i < stmt_field_count; ++i)
  {
    bind[i].buffer_type= MYSQL_TYPE_STRING;
    bind[i].buffer_length= stmt_field_list[i].max_length + 1;
    bind[i].buffer= malloc(bind[i].buffer_length);
  }
  mysql_stmt_bind_result(stmt, bind);
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  /* Clean up */

  for (i= 0; i < stmt_field_count; ++i)
    free(bind[i].buffer);
  mysql_stmt_close(stmt);
  mysql_free_result(query_result);
  mysql_free_result(stmt_metadata);
  stmt_text= "drop table t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


/*
  Test of basic checks that are performed in server for components
  of MYSQL_TIME parameters.
*/

static void test_datetime_ranges()
{
  const char *stmt_text;
  int rc, i;
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[6];
  MYSQL_TIME tm[6];

  myheader("test_datetime_ranges");

  stmt_text= "drop table if exists t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "create table t1 (year datetime, month datetime, day datetime, "
                              "hour datetime, min datetime, sec datetime)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt= mysql_simple_prepare(mysql,
                             "INSERT INTO t1 VALUES (?, ?, ?, ?, ?, ?)");
  check_stmt(stmt);
  verify_param_count(stmt, 6);

  bzero((char*) bind, sizeof(bind));
  for (i= 0; i < 6; i++)
  {
    bind[i].buffer_type= MYSQL_TYPE_DATETIME;
    bind[i].buffer= &tm[i];
  }
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  tm[0].year= 2004; tm[0].month= 11; tm[0].day= 10;
  tm[0].hour= 12; tm[0].minute= 30; tm[0].second= 30;
  tm[0].second_part= 0; tm[0].neg= 0;

  tm[5]= tm[4]= tm[3]= tm[2]= tm[1]= tm[0];
  tm[0].year= 10000;  tm[1].month= 13; tm[2].day= 32;
  tm[3].hour= 24; tm[4].minute= 60; tm[5].second= 60;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  DIE_UNLESS(mysql_warning_count(mysql) != 6);

  verify_col_data("t1", "year", "0000-00-00 00:00:00");
  verify_col_data("t1", "month", "0000-00-00 00:00:00");
  verify_col_data("t1", "day", "0000-00-00 00:00:00");
  verify_col_data("t1", "hour", "0000-00-00 00:00:00");
  verify_col_data("t1", "min", "0000-00-00 00:00:00");
  verify_col_data("t1", "sec", "0000-00-00 00:00:00");

  mysql_stmt_close(stmt);

  stmt_text= "delete from t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt= mysql_simple_prepare(mysql, "INSERT INTO t1 (year, month, day) "
                                    "VALUES (?, ?, ?)");
  check_stmt(stmt);
  verify_param_count(stmt, 3);

  /*
    We reuse contents of bind and tm arrays left from previous part of test.
  */
  for (i= 0; i < 3; i++)
    bind[i].buffer_type= MYSQL_TYPE_DATE;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  DIE_UNLESS(mysql_warning_count(mysql) != 3);

  verify_col_data("t1", "year", "0000-00-00 00:00:00");
  verify_col_data("t1", "month", "0000-00-00 00:00:00");
  verify_col_data("t1", "day", "0000-00-00 00:00:00");

  mysql_stmt_close(stmt);

  stmt_text= "drop table t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "create table t1 (day_ovfl time, day time, hour time, min time, sec time)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt= mysql_simple_prepare(mysql,
                             "INSERT INTO t1 VALUES (?, ?, ?, ?, ?)");
  check_stmt(stmt);
  verify_param_count(stmt, 5);

  /*
    Again we reuse what we can from previous part of test.
  */
  for (i= 0; i < 5; i++)
    bind[i].buffer_type= MYSQL_TYPE_TIME;

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  tm[0].year= 0; tm[0].month= 0; tm[0].day= 10;
  tm[0].hour= 12; tm[0].minute= 30; tm[0].second= 30;
  tm[0].second_part= 0; tm[0].neg= 0;

  tm[4]= tm[3]= tm[2]= tm[1]= tm[0];
  tm[0].day= 35; tm[1].day= 34; tm[2].hour= 30; tm[3].minute= 60; tm[4].second= 60;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  DIE_UNLESS(mysql_warning_count(mysql) == 2);

  verify_col_data("t1", "day_ovfl", "838:59:59");
  verify_col_data("t1", "day", "828:30:30");
  verify_col_data("t1", "hour", "270:30:30");
  verify_col_data("t1", "min", "00:00:00");
  verify_col_data("t1", "sec", "00:00:00");

  mysql_stmt_close(stmt);

  stmt_text= "drop table t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


static void test_bug4172()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[3];
  const char *stmt_text;
  MYSQL_RES *res;
  MYSQL_ROW row;
  int rc;
  char f[100], d[100], e[100];
  ulong f_len, d_len, e_len;

  myheader("test_bug4172");

  mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  mysql_query(mysql, "CREATE TABLE t1 (f float, d double, e decimal(10,4))");
  mysql_query(mysql, "INSERT INTO t1 VALUES (12345.1234, 123456.123456, "
                                            "123456.1234)");

  stmt= mysql_stmt_init(mysql);
  stmt_text= "SELECT f, d, e FROM t1";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= f;
  bind[0].buffer_length= sizeof(f);
  bind[0].length= &f_len;
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= d;
  bind[1].buffer_length= sizeof(d);
  bind[1].length= &d_len;
  bind[2].buffer_type= MYSQL_TYPE_STRING;
  bind[2].buffer= e;
  bind[2].buffer_length= sizeof(e);
  bind[2].length= &e_len;

  mysql_stmt_bind_result(stmt, bind);

  mysql_stmt_store_result(stmt);
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  res= mysql_store_result(mysql);
  row= mysql_fetch_row(res);

  if (!opt_silent)
  {
    printf("Binary protocol: float=%s, double=%s, decimal(10,4)=%s\n",
           f, d, e);
    printf("Text protocol:   float=%s, double=%s, decimal(10,4)=%s\n",
           row[0], row[1], row[2]);
  }
  DIE_UNLESS(!strcmp(f, row[0]) && !strcmp(d, row[1]) && !strcmp(e, row[2]));

  mysql_free_result(res);
  mysql_stmt_close(stmt);
}


static void test_conversion()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;
  int rc;
  MYSQL_BIND bind[1];
  char buff[4];
  ulong length;

  myheader("test_conversion");

  stmt_text= "DROP TABLE IF EXISTS t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "CREATE TABLE t1 (a TEXT) DEFAULT CHARSET latin1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "SET character_set_connection=utf8, character_set_client=utf8, "
             " character_set_results=latin1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt= mysql_stmt_init(mysql);

  stmt_text= "INSERT INTO t1 (a) VALUES (?)";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer= buff;
  bind[0].length= &length;
  bind[0].buffer_type= MYSQL_TYPE_STRING;

  mysql_stmt_bind_param(stmt, bind);

  buff[0]= (uchar) 0xC3;
  buff[1]= (uchar) 0xA0;
  length= 2;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  stmt_text= "SELECT a FROM t1";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer_length= sizeof(buff);
  mysql_stmt_bind_result(stmt, bind);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);
  DIE_UNLESS(length == 1);
  DIE_UNLESS((uchar) buff[0] == 0xE0);
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
  stmt_text= "DROP TABLE t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "SET NAMES DEFAULT";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}

static void test_rewind(void)
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind;
  int rc = 0;
  const char *stmt_text;
  long unsigned int length=4, Data=0;
  my_bool isnull=0;

  myheader("test_rewind");

  stmt_text= "CREATE TABLE t1 (a int)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "INSERT INTO t1 VALUES(2),(3),(4)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt= mysql_stmt_init(mysql);

  stmt_text= "SELECT * FROM t1";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  bzero((char*) &bind, sizeof(MYSQL_BIND));
  bind.buffer_type= MYSQL_TYPE_LONG;
  bind.buffer= (void *)&Data; /* this buffer won't be altered */
  bind.length= &length;
  bind.is_null= &isnull;

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  DIE_UNLESS(rc == 0);

  rc= mysql_stmt_bind_result(stmt, &bind);
  DIE_UNLESS(rc == 0);

  /* retreive all result sets till we are at the end */
  while(!mysql_stmt_fetch(stmt))
      printf("fetched result:%ld\n", Data);

  DIE_UNLESS(rc != MYSQL_NO_DATA);

  /* seek to the first row */
  mysql_stmt_data_seek(stmt, 0);

  /* now we should be able to fetch the results again */
  /* but mysql_stmt_fetch returns MYSQL_NO_DATA */
  while(!(rc= mysql_stmt_fetch(stmt)))
      printf("fetched result after seek:%ld\n", Data);
  
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  stmt_text= "DROP TABLE t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  rc= mysql_stmt_free_result(stmt);
  rc= mysql_stmt_close(stmt);
}


static void test_truncation()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;
  int rc;
  uint bind_count;
  MYSQL_BIND *bind_array, *bind;

  myheader("test_truncation");

  /* Prepare the test table */
  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);

  stmt_text= "create table t1 ("
             "i8 tinyint, ui8 tinyint unsigned, "
             "i16 smallint, i16_1 smallint, "
             "ui16 smallint unsigned, i32 int, i32_1 int, "
             "d double, d_1 double, ch char(30), ch_1 char(30), "
             "tx text, tx_1 text, ch_2 char(30) "
             ")";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "insert into t1 VALUES ("
             "-10, "                            /* i8 */
             "200, "                            /* ui8 */
             "32000, "                          /* i16 */
             "-32767, "                         /* i16_1 */
             "64000, "                          /* ui16 */
             "1073741824, "                     /* i32 */
             "1073741825, "                     /* i32_1 */
             "123.456, "                        /* d */
             "-12345678910, "                   /* d_1 */
             "'111111111111111111111111111111',"/* ch */
             "'abcdef', "                       /* ch_1 */
             "'12345 	      ', "              /* tx */
             "'12345.67 	      ', "      /* tx_1 */
             "'12345.67abc'"                    /* ch_2 */
             ")";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "select i8 c1, i8 c2, ui8 c3, i16_1 c4, ui16 c5, "
             "       i16 c6, ui16 c7, i32 c8, i32_1 c9, i32_1 c10, "
             "       d c11, d_1 c12, d_1 c13, ch c14, ch_1 c15, tx c16, "
             "       tx_1 c17, ch_2 c18 "
             "from t1";

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  bind_count= (uint) mysql_stmt_field_count(stmt);

  /*************** Fill in the bind structure and bind it **************/
  bind_array= malloc(sizeof(MYSQL_BIND) * bind_count);
  bzero((char*) bind_array, sizeof(MYSQL_BIND) * bind_count);
  for (bind= bind_array; bind < bind_array + bind_count; bind++)
    bind->error= &bind->error_value;
  bind= bind_array;

  bind->buffer= malloc(sizeof(uint8));
  bind->buffer_type= MYSQL_TYPE_TINY;
  bind->is_unsigned= TRUE;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(uint32));
  bind->buffer_type= MYSQL_TYPE_LONG;
  bind->is_unsigned= TRUE;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(int8));
  bind->buffer_type= MYSQL_TYPE_TINY;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(uint16));
  bind->buffer_type= MYSQL_TYPE_SHORT;
  bind->is_unsigned= TRUE;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(int16));
  bind->buffer_type= MYSQL_TYPE_SHORT;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(uint16));
  bind->buffer_type= MYSQL_TYPE_SHORT;
  bind->is_unsigned= TRUE;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(int8));
  bind->buffer_type= MYSQL_TYPE_TINY;
  bind->is_unsigned= TRUE;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(float));
  bind->buffer_type= MYSQL_TYPE_FLOAT;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(float));
  bind->buffer_type= MYSQL_TYPE_FLOAT;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(double));
  bind->buffer_type= MYSQL_TYPE_DOUBLE;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(longlong));
  bind->buffer_type= MYSQL_TYPE_LONGLONG;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(ulonglong));
  bind->buffer_type= MYSQL_TYPE_LONGLONG;
  bind->is_unsigned= TRUE;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(longlong));
  bind->buffer_type= MYSQL_TYPE_LONGLONG;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(longlong));
  bind->buffer_type= MYSQL_TYPE_LONGLONG;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(longlong));
  bind->buffer_type= MYSQL_TYPE_LONGLONG;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(longlong));
  bind->buffer_type= MYSQL_TYPE_LONGLONG;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(double));
  bind->buffer_type= MYSQL_TYPE_DOUBLE;

  DIE_UNLESS(++bind < bind_array + bind_count);
  bind->buffer= malloc(sizeof(double));
  bind->buffer_type= MYSQL_TYPE_DOUBLE;

  rc= mysql_stmt_bind_result(stmt, bind_array);
  check_execute(stmt, rc);
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_DATA_TRUNCATED);

  /*************** Verify truncation results ***************************/
  bind= bind_array;

  /* signed tiny -> tiny */
  DIE_UNLESS(*bind->error && * (int8*) bind->buffer == -10);

  /* signed tiny -> uint32 */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(*bind->error && * (int32*) bind->buffer == -10);

  /* unsigned tiny -> tiny */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(*bind->error && * (uint8*) bind->buffer == 200);

  /* short -> ushort */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(*bind->error && * (int16*) bind->buffer == -32767);

  /* ushort -> short */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(*bind->error && * (uint16*) bind->buffer == 64000);

  /* short -> ushort (no truncation, data is in the range of target type) */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(! *bind->error && * (uint16*) bind->buffer == 32000);

  /* ushort -> utiny */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(*bind->error && * (int8*) bind->buffer == 0);

  /* int -> float: no truncation, the number is a power of two */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(! *bind->error && * (float*) bind->buffer == 1073741824);

  /* int -> float: truncation, not enough bits in float */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(*bind->error);

  /* int -> double: no truncation */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(! *bind->error && * (double*) bind->buffer == 1073741825);

  /* double -> longlong: fractional part is lost */
  DIE_UNLESS(++bind < bind_array + bind_count);

  /* double -> ulonglong, negative fp number to unsigned integer */
  DIE_UNLESS(++bind < bind_array + bind_count);
  /* Value in the buffer is not defined: don't test it */
  DIE_UNLESS(*bind->error);

  /* double -> longlong, negative fp number to signed integer: no loss */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(! *bind->error && * (longlong*) bind->buffer == LL(-12345678910));

  /* big numeric string -> number */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(*bind->error);

  /* junk string -> number */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(*bind->error && *(longlong*) bind->buffer == 0);

  /* string with trailing spaces -> number */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(! *bind->error && *(longlong*) bind->buffer == 12345);

  /* string with trailing spaces -> double */
  DIE_UNLESS(++bind < bind_array + bind_count);
  DIE_UNLESS(! *bind->error && *(double*) bind->buffer == 12345.67);

  /* string with trailing junk -> double */
  DIE_UNLESS(++bind < bind_array + bind_count);
  /*
    XXX: There must be a truncation error: but it's not the way the server
    behaves, so let's leave it for now.
  */
  DIE_UNLESS(*(double*) bind->buffer == 12345.67);
  /*
    TODO: string -> double,  double -> time, double -> string (truncation
          errors are not supported here yet)
          longlong -> time/date/datetime
          date -> time, date -> timestamp, date -> number
          time -> string, time -> date, time -> timestamp,
          number -> date string -> date
  */
  /*************** Cleanup *********************************************/

  mysql_stmt_close(stmt);

  for (bind= bind_array; bind < bind_array + bind_count; bind++)
    free(bind->buffer);
  free(bind_array);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}

static void test_truncation_option()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;
  int rc;
  uint8 buf;
  my_bool option= 0;
  my_bool error;
  MYSQL_BIND bind;

  myheader("test_truncation_option");

  /* Prepare the test table */
  stmt_text= "select -1";

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) &bind, sizeof(MYSQL_BIND));

  bind.buffer= (void*) &buf;
  bind.buffer_type= MYSQL_TYPE_TINY;
  bind.is_unsigned= TRUE;
  bind.error= &error;

  rc= mysql_stmt_bind_result(stmt, &bind);
  check_execute(stmt, rc);
  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_DATA_TRUNCATED);
  DIE_UNLESS(error);
  rc= mysql_options(mysql, MYSQL_REPORT_DATA_TRUNCATION, (char*) &option);
  myquery(rc);
  /* need to rebind for the new setting to take effect */
  rc= mysql_stmt_bind_result(stmt, &bind);
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  /* The only change is rc - error pointers are still filled in */
  DIE_UNLESS(error == 1);
  /* restore back the defaults */
  option= 1;
  mysql_options(mysql, MYSQL_REPORT_DATA_TRUNCATION, (char*) &option);

  mysql_stmt_close(stmt);
}


/* Bug#6761 - mysql_list_fields doesn't work */

static void test_bug6761(void)
{
  const char *stmt_text;
  MYSQL_RES *res;
  int rc;
  myheader("test_bug6761");

  stmt_text= "CREATE TABLE t1 (a int, b char(255), c decimal)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  res= mysql_list_fields(mysql, "t1", "%");
  DIE_UNLESS(res && mysql_num_fields(res) == 3);
  mysql_free_result(res);

  stmt_text= "DROP TABLE t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


/* Bug#8330 - mysql_stmt_execute crashes (libmysql) */

static void test_bug8330()
{
  const char *stmt_text;
  MYSQL_STMT *stmt[2];
  int i, rc;
  const char *query= "select a,b from t1 where a=?";
  MYSQL_BIND bind[2];
  long lval[2];

  myheader("test_bug8330");

  stmt_text= "drop table if exists t1";
  /* in case some previos test failed */
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "create table t1 (a int, b int)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  bzero((char*) bind, sizeof(bind));
  for (i=0; i < 2; i++)
  {
    stmt[i]= mysql_stmt_init(mysql);
    rc= mysql_stmt_prepare(stmt[i], query, strlen(query));
    check_execute(stmt[i], rc);

    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (void*) &lval[i];
    bind[i].is_null= 0;
    mysql_stmt_bind_param(stmt[i], &bind[i]);
  }

  rc= mysql_stmt_execute(stmt[0]);
  check_execute(stmt[0], rc);

  rc= mysql_stmt_execute(stmt[1]);
  DIE_UNLESS(rc && mysql_stmt_errno(stmt[1]) == CR_COMMANDS_OUT_OF_SYNC);
  rc= mysql_stmt_execute(stmt[0]);
  check_execute(stmt[0], rc);

  mysql_stmt_close(stmt[0]);
  mysql_stmt_close(stmt[1]);

  stmt_text= "drop table t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


/* Bug#7990 - mysql_stmt_close doesn't reset mysql->net.last_error */

static void test_bug7990()
{
  MYSQL_STMT *stmt;
  int rc;
  myheader("test_bug7990");

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, "foo", 3);
  /*
    XXX: the fact that we store errno both in STMT and in
    MYSQL is not documented and is subject to change in 5.0
  */
  DIE_UNLESS(rc && mysql_stmt_errno(stmt) && mysql_errno(mysql));
  mysql_stmt_close(stmt);
  DIE_UNLESS(!mysql_errno(mysql));
}


static void test_view_sp_list_fields()
{
  int		rc;
  MYSQL_RES     *res;

  myheader("test_view_sp_list_fields");

  rc= mysql_query(mysql, "DROP FUNCTION IF EXISTS f1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP TABLE IF EXISTS v1, t1, t2");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW IF EXISTS v1, t1, t2");
  myquery(rc);
  rc= mysql_query(mysql, "create function f1 () returns int return 5");
  myquery(rc);
  rc= mysql_query(mysql, "create table t1 (s1 char,s2 char)");
  myquery(rc);
  rc= mysql_query(mysql, "create table t2 (s1 int);");
  myquery(rc);
  rc= mysql_query(mysql, "create view v1 as select s2,sum(s1) - \
count(s2) as vx from t1 group by s2 having sum(s1) - count(s2) < (select f1() \
from t2);");
  myquery(rc);
  res= mysql_list_fields(mysql, "v1", NullS);
  DIE_UNLESS(res != 0 && mysql_num_fields(res) != 0);
  rc= mysql_query(mysql, "DROP FUNCTION f1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW v1");
  myquery(rc);
  rc= mysql_query(mysql, "DROP TABLE t1, t2");
  mysql_free_result(res);
  myquery(rc);

}


/*
 Test mysql_real_escape_string() with gbk charset

 The important part is that 0x27 (') is the second-byte in a invalid
 two-byte GBK character here. But 0xbf5c is a valid GBK character, so
 it needs to be escaped as 0x5cbf27
*/
#define TEST_BUG8378_IN  "\xef\xbb\xbf\x27\xbf\x10"
#define TEST_BUG8378_OUT "\xef\xbb\x5c\xbf\x5c\x27\x5c\xbf\x10"

static void test_bug8378()
{
#if defined(HAVE_CHARSET_gbk) && !defined(EMBEDDED_LIBRARY)
  MYSQL *old_mysql=mysql;
  char out[9]; /* strlen(TEST_BUG8378)*2+1 */
  char buf[256];
  int len, rc;

  myheader("test_bug8378");

  if (!opt_silent)
    fprintf(stdout, "\n Establishing a test connection ...");
  if (!(mysql= mysql_init(NULL)))
  {
    myerror("mysql_init() failed");
    exit(1);
  }
  if (mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "gbk"))
  {
    myerror("mysql_options() failed");
    exit(1);
  }
  if (!(mysql_real_connect(mysql, opt_host, opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket, 0)))
  {
    myerror("connection failed");
    exit(1);
  }
  if (!opt_silent)
    fprintf(stdout, " OK");

  len= mysql_real_escape_string(mysql, out, TEST_BUG8378_IN, 4);

  /* No escaping should have actually happened. */
  DIE_UNLESS(memcmp(out, TEST_BUG8378_OUT, len) == 0);

  sprintf(buf, "SELECT '%s'", out);
  rc=mysql_real_query(mysql, buf, strlen(buf));
  myquery(rc);

  mysql_close(mysql);

  mysql=old_mysql;
#endif
}


static void test_bug8722()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *stmt_text;

  myheader("test_bug8722");
  /* Prepare test data */
  stmt_text= "drop table if exists t1, v1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "CREATE TABLE t1 (c1 varchar(10), c2 varchar(10), c3 varchar(10),"
                             " c4 varchar(10), c5 varchar(10), c6 varchar(10),"
                             " c7 varchar(10), c8 varchar(10), c9 varchar(10),"
                             "c10 varchar(10))";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "INSERT INTO t1 VALUES (1,2,3,4,5,6,7,8,9,10)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  stmt_text= "CREATE VIEW v1 AS SELECT * FROM t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
  /* Note: if you uncomment following block everything works fine */
/*
  rc= mysql_query(mysql, "sellect * from v1");
  myquery(rc);
  mysql_free_result(mysql_store_result(mysql));
*/

  stmt= mysql_stmt_init(mysql);
  stmt_text= "select * from v1";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  mysql_stmt_close(stmt);
  stmt_text= "drop table if exists t1, v1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);
}


MYSQL_STMT *open_cursor(const char *query)
{
  int rc;
  const ulong type= (ulong)CURSOR_TYPE_READ_ONLY;

  MYSQL_STMT *stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (void*) &type);
  return stmt;
}


static void test_bug8880()
{
  MYSQL_STMT *stmt_list[2], **stmt;
  MYSQL_STMT **stmt_list_end= (MYSQL_STMT**) stmt_list + 2;
  int rc;

  myheader("test_bug8880");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (a int not null primary key, b int)");
  rc= mysql_query(mysql, "insert into t1 values (1,1)");
  myquery(rc);                                  /* one check is enough */
  /*
    when inserting 2 rows everything works well
    mysql_query(mysql, "INSERT INTO t1 VALUES (1,1),(2,2)");
  */
  for (stmt= stmt_list; stmt < stmt_list_end; stmt++)
    *stmt= open_cursor("select a from t1");
  for (stmt= stmt_list; stmt < stmt_list_end; stmt++)
  {
    rc= mysql_stmt_execute(*stmt);
    check_execute(*stmt, rc);
  }
  for (stmt= stmt_list; stmt < stmt_list_end; stmt++)
    mysql_stmt_close(*stmt);
}


static void test_bug9159()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *stmt_text= "select a, b from t1";
  const unsigned long type= CURSOR_TYPE_READ_ONLY;

  myheader("test_bug9159");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (a int not null primary key, b int)");
  rc= mysql_query(mysql, "insert into t1 values (1,1)");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (const void *)&type);

  mysql_stmt_execute(stmt);
  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);
}


/* Crash when opening a cursor to a query with DISTICNT and no key */

static void test_bug9520()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char a[6];
  ulong a_len;
  int rc, row_count= 0;

  myheader("test_bug9520");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (a char(5), b char(5), c char(5),"
                     " primary key (a, b, c))");
  rc= mysql_query(mysql, "insert into t1 values ('x', 'y', 'z'), "
                  " ('a', 'b', 'c'), ('k', 'l', 'm')");
  myquery(rc);

  stmt= open_cursor("select distinct b from t1");

  /*
    Not crashes with:
    stmt= open_cursor("select distinct a from t1");
  */

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char*) a;
  bind[0].buffer_length= sizeof(a);
  bind[0].length= &a_len;

  mysql_stmt_bind_result(stmt, bind);

  while (!(rc= mysql_stmt_fetch(stmt)))
    row_count++;

  DIE_UNLESS(rc == MYSQL_NO_DATA);

  printf("Fetched %d rows\n", row_count);
  DBUG_ASSERT(row_count == 3);

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


/*
  We can't have more than one cursor open for a prepared statement.
  Test re-executions of a PS with cursor; mysql_stmt_reset must close
  the cursor attached to the statement, if there is one.
*/

static void test_bug9478()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char a[6];
  ulong a_len;
  int rc, i;
  DBUG_ENTER("test_bug9478");

  myheader("test_bug9478");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (id integer not null primary key, "
                     " name varchar(20) not null)");
  rc= mysql_query(mysql, "insert into t1 (id, name) values "
                         " (1, 'aaa'), (2, 'bbb'), (3, 'ccc')");
  myquery(rc);

  stmt= open_cursor("select name from t1 where id=2");

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char*) a;
  bind[0].buffer_length= sizeof(a);
  bind[0].length= &a_len;
  mysql_stmt_bind_result(stmt, bind);

  for (i= 0; i < 5; i++)
  {
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= mysql_stmt_fetch(stmt);
    check_execute(stmt, rc);
    if (!opt_silent && i == 0)
      printf("Fetched row: %s\n", a);

    /*
      The query above is a one-row result set. Therefore, there is no
      cursor associated with it, as the server won't bother with opening
      a cursor for a one-row result set. The first row was read from the
      server in the fetch above. But there is eof packet pending in the
      network. mysql_stmt_execute will flush the packet and successfully
      execute the statement.
    */

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    rc= mysql_stmt_fetch(stmt);
    check_execute(stmt, rc);
    if (!opt_silent && i == 0)
      printf("Fetched row: %s\n", a);
    rc= mysql_stmt_fetch(stmt);
    DIE_UNLESS(rc == MYSQL_NO_DATA);

    {
      char buff[8];
      /* Fill in the fethc packet */
      int4store(buff, stmt->stmt_id);
      buff[4]= 1;                               /* prefetch rows */
      rc= ((*mysql->methods->advanced_command)(mysql, COM_STMT_FETCH, buff,
                                               sizeof(buff), 0,0,1,NULL) ||
           (*mysql->methods->read_query_result)(mysql));
      DIE_UNLESS(rc);
      if (!opt_silent && i == 0)
        printf("Got error (as expected): %s\n", mysql_error(mysql));
    }

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    rc= mysql_stmt_fetch(stmt);
    check_execute(stmt, rc);
    if (!opt_silent && i == 0)
      printf("Fetched row: %s\n", a);

    rc= mysql_stmt_reset(stmt);
    check_execute(stmt, rc);
    rc= mysql_stmt_fetch(stmt);
    DIE_UNLESS(rc && mysql_stmt_errno(stmt));
    if (!opt_silent && i == 0)
      printf("Got error (as expected): %s\n", mysql_stmt_error(stmt));
  }
  rc= mysql_stmt_close(stmt);
  DIE_UNLESS(rc == 0);

  /* Test the case with a server side cursor */
  stmt= open_cursor("select name from t1");

  mysql_stmt_bind_result(stmt, bind);

  for (i= 0; i < 5; i++)
  {
    DBUG_PRINT("loop",("i: %d", i));
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    rc= mysql_stmt_fetch(stmt);
    check_execute(stmt, rc);
    if (!opt_silent && i == 0)
      printf("Fetched row: %s\n", a);
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    while (! (rc= mysql_stmt_fetch(stmt)))
    {
      if (!opt_silent && i == 0)
        printf("Fetched row: %s\n", a);
    }
    DIE_UNLESS(rc == MYSQL_NO_DATA);

    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);

    rc= mysql_stmt_fetch(stmt);
    check_execute(stmt, rc);
    if (!opt_silent && i == 0)
      printf("Fetched row: %s\n", a);

    rc= mysql_stmt_reset(stmt);
    check_execute(stmt, rc);
    rc= mysql_stmt_fetch(stmt);
    DIE_UNLESS(rc && mysql_stmt_errno(stmt));
    if (!opt_silent && i == 0)
      printf("Got error (as expected): %s\n", mysql_stmt_error(stmt));
  }

  rc= mysql_stmt_close(stmt);
  DIE_UNLESS(rc == 0);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
  DBUG_VOID_RETURN;
}


/*
  Error message is returned for unsupported features.
  Test also cursors with non-default PREFETCH_ROWS
*/

static void test_bug9643()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  int32 a;
  int rc;
  const char *stmt_text;
  int num_rows= 0;
  ulong type;
  ulong prefetch_rows= 5;

  myheader("test_bug9643");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (id integer not null primary key)");
  rc= mysql_query(mysql, "insert into t1 (id) values "
                         " (1), (2), (3), (4), (5), (6), (7), (8), (9)");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  /* Not implemented in 5.0 */
  type= (ulong) CURSOR_TYPE_SCROLLABLE;
  rc= mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (void*) &type);
  DIE_UNLESS(rc);
  if (! opt_silent)
    printf("Got error (as expected): %s\n", mysql_stmt_error(stmt));

  type= (ulong) CURSOR_TYPE_READ_ONLY;
  rc= mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (void*) &type);
  check_execute(stmt, rc);
  rc= mysql_stmt_attr_set(stmt, STMT_ATTR_PREFETCH_ROWS,
                          (void*) &prefetch_rows);
  check_execute(stmt, rc);
  stmt_text= "select * from t1";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void*) &a;
  bind[0].buffer_length= sizeof(a);
  mysql_stmt_bind_result(stmt, bind);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  while ((rc= mysql_stmt_fetch(stmt)) == 0)
    ++num_rows;
  DIE_UNLESS(num_rows == 9);

  rc= mysql_stmt_close(stmt);
  DIE_UNLESS(rc == 0);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}

/*
  Bug#11111: fetch from view returns wrong data
*/

static void test_bug11111()
{
  MYSQL_STMT    *stmt;
  MYSQL_BIND    bind[2];
  char          buf[2][20];
  ulong         len[2];
  int i;
  int rc;
  const char *query= "SELECT DISTINCT f1,ff2 FROM v1";

  myheader("test_bug11111");

  rc= mysql_query(mysql, "drop table if exists t1, t2, v1");
  myquery(rc);
  rc= mysql_query(mysql, "drop view if exists t1, t2, v1");
  myquery(rc);
  rc= mysql_query(mysql, "create table t1 (f1 int, f2 int)");
  myquery(rc);
  rc= mysql_query(mysql, "create table t2 (ff1 int, ff2 int)");
  myquery(rc);
  rc= mysql_query(mysql, "create view v1 as select * from t1, t2 where f1=ff1");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t1 values (1,1), (2,2), (3,3)");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t2 values (1,1), (2,2), (3,3)");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);

  mysql_stmt_prepare(stmt, query, strlen(query));
  mysql_stmt_execute(stmt);

  bzero((char*) bind, sizeof(bind));
  for (i=0; i < 2; i++)
  {
    bind[i].buffer_type= MYSQL_TYPE_STRING;
    bind[i].buffer= (gptr *)&buf[i];
    bind[i].buffer_length= 20;
    bind[i].length= &len[i];
  }

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  if (!opt_silent)
    printf("return: %s", buf[1]);
  DIE_UNLESS(!strcmp(buf[1],"1"));
  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "drop view v1");
  myquery(rc);
  rc= mysql_query(mysql, "drop table t1, t2");
  myquery(rc);
}

/*
  Check that proper cleanups are done for prepared statement when
  fetching thorugh a cursor.
*/

static void test_bug10729()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char a[21];
  int rc;
  const char *stmt_text;
  int i= 0;
  const char *name_array[3]= { "aaa", "bbb", "ccc" };
  ulong type;

  myheader("test_bug10729");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (id integer not null primary key,"
                                      "name VARCHAR(20) NOT NULL)");
  rc= mysql_query(mysql, "insert into t1 (id, name) values "
                         "(1, 'aaa'), (2, 'bbb'), (3, 'ccc')");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);

  type= (ulong) CURSOR_TYPE_READ_ONLY;
  rc= mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (void*) &type);
  check_execute(stmt, rc);
  stmt_text= "select name from t1";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void*) a;
  bind[0].buffer_length= sizeof(a);
  mysql_stmt_bind_result(stmt, bind);

  for (i= 0; i < 3; i++)
  {
    int row_no= 0;
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    while ((rc= mysql_stmt_fetch(stmt)) == 0)
    {
      DIE_UNLESS(strcmp(a, name_array[row_no]) == 0);
      if (!opt_silent)
        printf("%d: %s\n", row_no, a);
      ++row_no;
    }
    DIE_UNLESS(rc == MYSQL_NO_DATA);
  }
  rc= mysql_stmt_close(stmt);
  DIE_UNLESS(rc == 0);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


/*
  Check that mysql_next_result works properly in case when one of
  the statements used in a multi-statement query is erroneous
*/

static void test_bug9992()
{
  MYSQL *mysql1;
  MYSQL_RES* res ;
  int   rc;

  myheader("test_bug9992");

  if (!opt_silent)
    printf("Establishing a connection with option CLIENT_MULTI_STATEMENTS..\n");

  mysql1= mysql_init(NULL);

  if (!mysql_real_connect(mysql1, opt_host, opt_user, opt_password,
                          opt_db ? opt_db : "test", opt_port, opt_unix_socket,
                          CLIENT_MULTI_STATEMENTS))
  {
    fprintf(stderr, "Failed to connect to the database\n");
    DIE_UNLESS(0);
  }


  /* Sic: SHOW DATABASE is incorrect syntax. */
  rc= mysql_query(mysql1, "SHOW TABLES; SHOW DATABASE; SELECT 1;");

  if (rc)
  {
    fprintf(stderr, "[%d] %s\n", mysql_errno(mysql1), mysql_error(mysql1));
    DIE_UNLESS(0);
  }

  if (!opt_silent)
    printf("Testing mysql_store_result/mysql_next_result..\n");

  res= mysql_store_result(mysql1);
  DIE_UNLESS(res);
  mysql_free_result(res);
  rc= mysql_next_result(mysql1);
  DIE_UNLESS(rc == 1);                         /* Got errors, as expected */

  if (!opt_silent)
    fprintf(stdout, "Got error, as expected:\n [%d] %s\n",
            mysql_errno(mysql1), mysql_error(mysql1));

  mysql_close(mysql1);
}

/* Bug#10736: cursors and subqueries, memroot management */

static void test_bug10736()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  char a[21];
  int rc;
  const char *stmt_text;
  int i= 0;
  ulong type;

  myheader("test_bug10736");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (id integer not null primary key,"
                                      "name VARCHAR(20) NOT NULL)");
  rc= mysql_query(mysql, "insert into t1 (id, name) values "
                         "(1, 'aaa'), (2, 'bbb'), (3, 'ccc')");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);

  type= (ulong) CURSOR_TYPE_READ_ONLY;
  rc= mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (void*) &type);
  check_execute(stmt, rc);
  stmt_text= "select name from t1 where name=(select name from t1 where id=2)";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void*) a;
  bind[0].buffer_length= sizeof(a);
  mysql_stmt_bind_result(stmt, bind);

  for (i= 0; i < 3; i++)
  {
    int row_no= 0;
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    while ((rc= mysql_stmt_fetch(stmt)) == 0)
    {
      if (!opt_silent)
        printf("%d: %s\n", row_no, a);
      ++row_no;
    }
    DIE_UNLESS(rc == MYSQL_NO_DATA);
  }
  rc= mysql_stmt_close(stmt);
  DIE_UNLESS(rc == 0);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}

/* Bug#10794: cursors, packets out of order */

static void test_bug10794()
{
  MYSQL_STMT *stmt, *stmt1;
  MYSQL_BIND bind[2];
  char a[21];
  int id_val;
  ulong a_len;
  int rc;
  const char *stmt_text;
  int i= 0;
  ulong type;

  myheader("test_bug10794");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (id integer not null primary key,"
                                      "name varchar(20) not null)");
  stmt= mysql_stmt_init(mysql);
  stmt_text= "insert into t1 (id, name) values (?, ?)";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void*) &id_val;
  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void*) a;
  bind[1].length= &a_len;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);
  for (i= 0; i < 42; i++)
  {
    id_val= (i+1)*10;
    sprintf(a, "a%d", i);
    a_len= strlen(a); /* safety against broken sprintf */
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
  }
  stmt_text= "select name from t1";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  type= (ulong) CURSOR_TYPE_READ_ONLY;
  mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (const void*) &type);
  stmt1= mysql_stmt_init(mysql);
  mysql_stmt_attr_set(stmt1, STMT_ATTR_CURSOR_TYPE, (const void*) &type);
  bzero((char*) bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void*) a;
  bind[0].buffer_length= sizeof(a);
  bind[0].length= &a_len;
  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  if (!opt_silent)
    printf("Fetched row from stmt: %s\n", a);
  /* Don't optimize: an attribute of the original test case */
  mysql_stmt_free_result(stmt);
  mysql_stmt_reset(stmt);
  stmt_text= "select name from t1 where id=10";
  rc= mysql_stmt_prepare(stmt1, stmt_text, strlen(stmt_text));
  check_execute(stmt1, rc);
  rc= mysql_stmt_bind_result(stmt1, bind);
  check_execute(stmt1, rc);
  rc= mysql_stmt_execute(stmt1);
  while (1)
  {
    rc= mysql_stmt_fetch(stmt1);
    if (rc == MYSQL_NO_DATA)
    {
      if (!opt_silent)
        printf("End of data in stmt1\n");
      break;
    }
    check_execute(stmt1, rc);
    if (!opt_silent)
      printf("Fetched row from stmt1: %s\n", a);
  }
  mysql_stmt_close(stmt);
  mysql_stmt_close(stmt1);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


/* Bug#11172: cursors, crash on a fetch from a datetime column */

static void test_bug11172()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind_in[1], bind_out[2];
  MYSQL_TIME hired;
  int rc;
  const char *stmt_text;
  int i= 0, id;
  ulong type;

  myheader("test_bug11172");

  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (id integer not null primary key,"
                                      "hired date not null)");
  rc= mysql_query(mysql,
                  "insert into t1 (id, hired) values (1, '1933-08-24'), "
                  "(2, '1965-01-01'), (3, '1949-08-17'), (4, '1945-07-07'), "
                  "(5, '1941-05-15'), (6, '1978-09-15'), (7, '1936-03-28')");
  myquery(rc);
  stmt= mysql_stmt_init(mysql);
  stmt_text= "SELECT id, hired FROM t1 WHERE hired=?";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);

  type= (ulong) CURSOR_TYPE_READ_ONLY;
  mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (const void*) &type);

  bzero((char*) bind_in, sizeof(bind_in));
  bzero((char*) bind_out, sizeof(bind_out));
  bzero((char*) &hired, sizeof(hired));
  hired.year= 1965;
  hired.month= 1;
  hired.day= 1;
  bind_in[0].buffer_type= MYSQL_TYPE_DATE;
  bind_in[0].buffer= (void*) &hired;
  bind_in[0].buffer_length= sizeof(hired);
  bind_out[0].buffer_type= MYSQL_TYPE_LONG;
  bind_out[0].buffer= (void*) &id;
  bind_out[1]= bind_in[0];

  for (i= 0; i < 3; i++)
  {
    rc= mysql_stmt_bind_param(stmt, bind_in);
    check_execute(stmt, rc);
    rc= mysql_stmt_bind_result(stmt, bind_out);
    check_execute(stmt, rc);
    rc= mysql_stmt_execute(stmt);
    check_execute(stmt, rc);
    while ((rc= mysql_stmt_fetch(stmt)) == 0)
    {
      if (!opt_silent)
        printf("fetched data %d:%d-%d-%d\n", id,
               hired.year, hired.month, hired.day);
    }
    DIE_UNLESS(rc == MYSQL_NO_DATA);
    mysql_stmt_free_result(stmt) || mysql_stmt_reset(stmt);
  }
  mysql_stmt_close(stmt);
  mysql_rollback(mysql);
  mysql_rollback(mysql);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


/* Bug#11656: cursors, crash on a fetch from a query with distinct. */

static void test_bug11656()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  int rc;
  const char *stmt_text;
  char buf[2][20];
  int i= 0;
  ulong type;

  myheader("test_bug11656");

  mysql_query(mysql, "drop table if exists t1");

  rc= mysql_query(mysql, "create table t1 ("
                  "server varchar(40) not null, "
                  "test_kind varchar(1) not null, "
                  "test_id varchar(30) not null , "
                  "primary key (server,test_kind,test_id))");
  myquery(rc);

  stmt_text= "select distinct test_kind, test_id from t1 "
             "where server in (?, ?)";
  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  type= (ulong) CURSOR_TYPE_READ_ONLY;
  mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (const void*) &type);

  bzero((char*) bind, sizeof(bind));
  strmov(buf[0], "pcint502_MY2");
  strmov(buf[1], "*");
  for (i=0; i < 2; i++)
  {
    bind[i].buffer_type= MYSQL_TYPE_STRING;
    bind[i].buffer= (gptr *)&buf[i];
    bind[i].buffer_length= strlen(buf[i]);
  }
  mysql_stmt_bind_param(stmt, bind);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


/*
  Check that the server signals when NO_BACKSLASH_ESCAPES mode is in effect,
  and mysql_real_escape_string() does the right thing as a result.
*/

static void test_bug10214()
{
  int   len;
  char  out[8];

  myheader("test_bug10214");

  DIE_UNLESS(!(mysql->server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES));

  len= mysql_real_escape_string(mysql, out, "a'b\\c", 5);
  DIE_UNLESS(memcmp(out, "a\\'b\\\\c", len) == 0);

  mysql_query(mysql, "set sql_mode='NO_BACKSLASH_ESCAPES'");
  DIE_UNLESS(mysql->server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES);

  len= mysql_real_escape_string(mysql, out, "a'b\\c", 5);
  DIE_UNLESS(memcmp(out, "a''b\\c", len) == 0);

  mysql_query(mysql, "set sql_mode=''");
}

static void test_client_character_set()
{
  MY_CHARSET_INFO cs;
  char *csname= (char*) "utf8";
  char *csdefault= (char*)mysql_character_set_name(mysql);
  int rc;

  myheader("test_client_character_set");

  rc= mysql_set_character_set(mysql, csname);
  DIE_UNLESS(rc == 0);

  mysql_get_character_set_info(mysql, &cs);
  DIE_UNLESS(!strcmp(cs.csname, "utf8"));
  DIE_UNLESS(!strcmp(cs.name, "utf8_general_ci"));
  /* Restore the default character set */
  rc= mysql_set_character_set(mysql, csdefault);
  myquery(rc);
}

/* Test correct max length for MEDIUMTEXT and LONGTEXT columns */

static void test_bug9735()
{
  MYSQL_RES *res;
  int rc;

  myheader("test_bug9735");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);
  rc= mysql_query(mysql, "create table t1 (a mediumtext, b longtext) "
                         "character set latin1");
  myquery(rc);
  rc= mysql_query(mysql, "select * from t1");
  myquery(rc);
  res= mysql_store_result(mysql);
  verify_prepare_field(res, 0, "a", "a", MYSQL_TYPE_BLOB,
                       "t1", "t1", current_db, (1U << 24)-1, 0);
  verify_prepare_field(res, 1, "b", "b", MYSQL_TYPE_BLOB,
                       "t1", "t1", current_db, ~0U, 0);
  mysql_free_result(res);
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


/* Bug#11183 "mysql_stmt_reset() doesn't reset information about error" */

static void test_bug11183()
{
  int rc;
  MYSQL_STMT *stmt;
  char bug_statement[]= "insert into t1 values (1)";

  myheader("test_bug11183");

  mysql_query(mysql, "drop table t1 if exists");
  mysql_query(mysql, "create table t1 (a int)");

  stmt= mysql_stmt_init(mysql);
  DIE_UNLESS(stmt != 0);

  rc= mysql_stmt_prepare(stmt, bug_statement, strlen(bug_statement));
  check_execute(stmt, rc);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);

  /* Trying to execute statement that should fail on execute stage */
  rc= mysql_stmt_execute(stmt);
  DIE_UNLESS(rc);

  mysql_stmt_reset(stmt);
  DIE_UNLESS(mysql_stmt_errno(stmt) == 0);

  mysql_query(mysql, "create table t1 (a int)");

  /* Trying to execute statement that should pass ok */
  if (mysql_stmt_execute(stmt))
  {
    mysql_stmt_reset(stmt);
    DIE_UNLESS(mysql_stmt_errno(stmt) == 0);
  }

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}

static void test_bug11037()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *stmt_text;

  myheader("test_bug11037");

  mysql_query(mysql, "drop table if exists t1");

  rc= mysql_query(mysql, "create table t1 (id int not null)");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1 values (1)");
  myquery(rc);

  stmt_text= "select id FROM t1";
  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));

  /* expected error */
  rc = mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc==1);
  if (!opt_silent)
    fprintf(stdout, "Got error, as expected:\n [%d] %s\n",
            mysql_stmt_errno(stmt), mysql_stmt_error(stmt));

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc==0);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc==MYSQL_NO_DATA);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc==MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}

/* Bug#10760: cursors, crash in a fetch after rollback. */

static void test_bug10760()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  int rc;
  const char *stmt_text;
  char id_buf[20];
  ulong id_len;
  int i= 0;
  ulong type;

  myheader("test_bug10760");

  mysql_query(mysql, "drop table if exists t1, t2");

  /* create tables */
  rc= mysql_query(mysql, "create table t1 (id integer not null primary key)"
                         " engine=MyISAM");
  myquery(rc);
  for (; i < 42; ++i)
  {
    char buf[100];
    sprintf(buf, "insert into t1 (id) values (%d)", i+1);
    rc= mysql_query(mysql, buf);
    myquery(rc);
  }
  mysql_autocommit(mysql, FALSE);
  /* create statement */
  stmt= mysql_stmt_init(mysql);
  type= (ulong) CURSOR_TYPE_READ_ONLY;
  mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (const void*) &type);

  /*
    1: check that a deadlock within the same connection
    is resolved and an error is returned. The deadlock is modelled
    as follows:
    con1: open cursor for select * from t1;
    con1: insert into t1 (id) values (1)
  */
  stmt_text= "select id from t1 order by 1";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  rc= mysql_query(mysql, "update t1 set id=id+100");
  /*
    If cursors are not materialized, the update will return an error;
    we mainly test that it won't deadlock.
  */
  if (rc && !opt_silent)
    printf("Got error (as expected): %s\n", mysql_error(mysql));
  /*
    2: check that MyISAM tables used in cursors survive
    COMMIT/ROLLBACK.
  */
  rc= mysql_rollback(mysql);                  /* should not close the cursor */
  myquery(rc);
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  /*
    3: check that cursors to InnoDB tables are closed (for now) by
    COMMIT/ROLLBACK.
  */
  if (! have_innodb)
  {
    if (!opt_silent)
      printf("Testing that cursors are closed at COMMIT/ROLLBACK requires "
             "InnoDB.\n");
  }
  else
  {
    stmt_text= "select id from t1 order by 1";
    rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
    check_execute(stmt, rc);

    rc= mysql_query(mysql, "alter table t1 engine=InnoDB");
    myquery(rc);

    bzero(bind, sizeof(bind));
    bind[0].buffer_type= MYSQL_TYPE_STRING;
    bind[0].buffer= (void*) id_buf;
    bind[0].buffer_length= sizeof(id_buf);
    bind[0].length= &id_len;
    check_execute(stmt, rc);
    mysql_stmt_bind_result(stmt, bind);

    rc= mysql_stmt_execute(stmt);
    rc= mysql_stmt_fetch(stmt);
    DIE_UNLESS(rc == 0);
    if (!opt_silent)
      printf("Fetched row %s\n", id_buf);
    rc= mysql_rollback(mysql);                  /* should close the cursor */
    myquery(rc);
#if 0
    rc= mysql_stmt_fetch(stmt);
    DIE_UNLESS(rc);
    if (!opt_silent)
      printf("Got error (as expected): %s\n", mysql_error(mysql));
#endif
  }

  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
  mysql_autocommit(mysql, TRUE);                /* restore default */
}

static void test_bug12001()
{
  MYSQL *mysql_local;
  MYSQL_RES *result;
  const char *query= "DROP TABLE IF EXISTS test_table;"
                     "CREATE TABLE test_table(id INT);"
                     "INSERT INTO test_table VALUES(10);"
                     "UPDATE test_table SET id=20 WHERE id=10;"
                     "SELECT * FROM test_table;"
                     "INSERT INTO non_existent_table VALUES(11);";
  int rc, res;

  myheader("test_bug12001");

  if (!(mysql_local= mysql_init(NULL)))
  {
    fprintf(stdout, "\n mysql_init() failed");
    exit(1);
  }

  /* Create connection that supports multi statements */
  if (!mysql_real_connect(mysql_local, opt_host, opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket, CLIENT_MULTI_STATEMENTS |
                           CLIENT_MULTI_RESULTS))
  {
    fprintf(stdout, "\n mysql_real_connect() failed");
    exit(1);
  }

  rc= mysql_query(mysql_local, query);
  myquery(rc);

  do
  {
    if (mysql_field_count(mysql_local) &&
        (result= mysql_use_result(mysql_local)))
    {
      mysql_free_result(result);
    }
  }
  while (!(res= mysql_next_result(mysql_local)));

  rc= mysql_query(mysql_local, "DROP TABLE IF EXISTS test_table");
  myquery(rc);

  mysql_close(mysql_local);
  DIE_UNLESS(res==1);
}


/* Bug#11909: wrong metadata if fetching from two cursors */

static void test_bug11909()
{
  MYSQL_STMT *stmt1, *stmt2;
  MYSQL_BIND bind[7];
  int rc;
  char firstname[20], midinit[20], lastname[20], workdept[20];
  ulong firstname_len, midinit_len, lastname_len, workdept_len;
  uint32 empno;
  double salary;
  float bonus;
  const char *stmt_text;

  myheader("test_bug11909");

  stmt_text= "drop table if exists t1";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "create table t1 ("
    "  empno int(11) not null, firstname varchar(20) not null,"
    "  midinit varchar(20) not null, lastname varchar(20) not null,"
    "  workdept varchar(6) not null, salary double not null,"
    "  bonus float not null, primary key (empno)"
    ") default charset=latin1 collate=latin1_bin";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "insert into t1 values "
    "(10, 'CHRISTINE', 'I', 'HAAS',     'A00', 52750, 1000), "
    "(20, 'MICHAEL',   'L', 'THOMPSON', 'B01', 41250, 800),"
    "(30, 'SALLY',     'A', 'KWAN',     'C01', 38250, 800),"
    "(50, 'JOHN',      'B', 'GEYER',    'E01', 40175, 800), "
    "(60, 'IRVING',    'F', 'STERN',    'D11', 32250, 500)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  /* ****** Begin of trace ****** */

  stmt1= open_cursor("SELECT empno, firstname, midinit, lastname,"
                     "workdept, salary, bonus FROM t1");

  bzero(bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void*) &empno;

  bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[1].buffer= (void*) firstname;
  bind[1].buffer_length= sizeof(firstname);
  bind[1].length= &firstname_len;

  bind[2].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[2].buffer= (void*) midinit;
  bind[2].buffer_length= sizeof(midinit);
  bind[2].length= &midinit_len;

  bind[3].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[3].buffer= (void*) lastname;
  bind[3].buffer_length= sizeof(lastname);
  bind[3].length= &lastname_len;

  bind[4].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[4].buffer= (void*) workdept;
  bind[4].buffer_length= sizeof(workdept);
  bind[4].length= &workdept_len;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (void*) &salary;

  bind[6].buffer_type= MYSQL_TYPE_FLOAT;
  bind[6].buffer= (void*) &bonus;
  rc= mysql_stmt_bind_result(stmt1, bind);
  check_execute(stmt1, rc);

  rc= mysql_stmt_execute(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_fetch(stmt1);
  DIE_UNLESS(rc == 0);
  DIE_UNLESS(empno == 10);
  DIE_UNLESS(strcmp(firstname, "CHRISTINE") == 0);
  DIE_UNLESS(strcmp(midinit, "I") == 0);
  DIE_UNLESS(strcmp(lastname, "HAAS") == 0);
  DIE_UNLESS(strcmp(workdept, "A00") == 0);
  DIE_UNLESS(salary == (double) 52750.0);
  DIE_UNLESS(bonus == (float) 1000.0);

  stmt2= open_cursor("SELECT empno, firstname FROM t1");
  rc= mysql_stmt_bind_result(stmt2, bind);
  check_execute(stmt2, rc);

  rc= mysql_stmt_execute(stmt2);
  check_execute(stmt2, rc);

  rc= mysql_stmt_fetch(stmt2);
  DIE_UNLESS(rc == 0);

  DIE_UNLESS(empno == 10);
  DIE_UNLESS(strcmp(firstname, "CHRISTINE") == 0);

  rc= mysql_stmt_reset(stmt2);
  check_execute(stmt2, rc);

  /* ERROR: next statement should return 0 */

  rc= mysql_stmt_fetch(stmt1);
  DIE_UNLESS(rc == 0);

  mysql_stmt_close(stmt1);
  mysql_stmt_close(stmt2);
  rc= mysql_rollback(mysql);
  myquery(rc);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}

/* Cursors: opening a cursor to a compilicated query with ORDER BY */

static void test_bug11901()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[2];
  int rc;
  char workdept[20];
  ulong workdept_len;
  uint32 empno;
  const char *stmt_text;

  myheader("test_bug11901");

  stmt_text= "drop table if exists t1, t2";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "create table t1 ("
    "  empno int(11) not null, firstname varchar(20) not null,"
    "  midinit varchar(20) not null, lastname varchar(20) not null,"
    "  workdept varchar(6) not null, salary double not null,"
    "  bonus float not null, primary key (empno), "
    " unique key (workdept, empno) "
    ") default charset=latin1 collate=latin1_bin";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "insert into t1 values "
     "(10,  'CHRISTINE', 'I', 'HAAS',      'A00', 52750, 1000),"
     "(20,  'MICHAEL',   'L', 'THOMPSON',  'B01', 41250, 800), "
     "(30,  'SALLY',     'A', 'KWAN',      'C01', 38250, 800), "
     "(50,  'JOHN',      'B', 'GEYER',     'E01', 40175, 800), "
     "(60,  'IRVING',    'F', 'STERN',     'D11', 32250, 500), "
     "(70,  'EVA',       'D', 'PULASKI',   'D21', 36170, 700), "
     "(90,  'EILEEN',    'W', 'HENDERSON', 'E11', 29750, 600), "
     "(100, 'THEODORE',  'Q', 'SPENSER',   'E21', 26150, 500), "
     "(110, 'VINCENZO',  'G', 'LUCCHESSI', 'A00', 46500, 900), "
     "(120, 'SEAN',      '',  'O\\'CONNELL', 'A00', 29250, 600), "
     "(130, 'DOLORES',   'M', 'QUINTANA',  'C01', 23800, 500), "
     "(140, 'HEATHER',   'A', 'NICHOLLS',  'C01', 28420, 600), "
     "(150, 'BRUCE',     '',  'ADAMSON',   'D11', 25280, 500), "
     "(160, 'ELIZABETH', 'R', 'PIANKA',    'D11', 22250, 400), "
     "(170, 'MASATOSHI', 'J', 'YOSHIMURA', 'D11', 24680, 500), "
     "(180, 'MARILYN',   'S', 'SCOUTTEN',  'D11', 21340, 500), "
     "(190, 'JAMES',     'H', 'WALKER',    'D11', 20450, 400), "
     "(200, 'DAVID',     '',  'BROWN',     'D11', 27740, 600), "
     "(210, 'WILLIAM',   'T', 'JONES',     'D11', 18270, 400), "
     "(220, 'JENNIFER',  'K', 'LUTZ',      'D11', 29840, 600), "
     "(230, 'JAMES',     'J', 'JEFFERSON', 'D21', 22180, 400), "
     "(240, 'SALVATORE', 'M', 'MARINO',    'D21', 28760, 600), "
     "(250, 'DANIEL',    'S', 'SMITH',     'D21', 19180, 400), "
     "(260, 'SYBIL',     'P', 'JOHNSON',   'D21', 17250, 300), "
     "(270, 'MARIA',     'L', 'PEREZ',     'D21', 27380, 500), "
     "(280, 'ETHEL',     'R', 'SCHNEIDER', 'E11', 26250, 500), "
     "(290, 'JOHN',      'R', 'PARKER',    'E11', 15340, 300), "
     "(300, 'PHILIP',    'X', 'SMITH',     'E11', 17750, 400), "
     "(310, 'MAUDE',     'F', 'SETRIGHT',  'E11', 15900, 300), "
     "(320, 'RAMLAL',    'V', 'MEHTA',     'E21', 19950, 400), "
     "(330, 'WING',      '',  'LEE',       'E21', 25370, 500), "
     "(340, 'JASON',     'R', 'GOUNOT',    'E21', 23840, 500)";

  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "create table t2 ("
    " deptno varchar(6) not null, deptname varchar(20) not null,"
    " mgrno int(11) not null, location varchar(20) not null,"
    " admrdept varchar(6) not null, refcntd int(11) not null,"
    " refcntu int(11) not null, primary key (deptno)"
    ") default charset=latin1 collate=latin1_bin";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  stmt_text= "insert into t2 values "
    "('A00', 'SPIFFY COMPUTER SERV', 10, '', 'A00', 0, 0), "
    "('B01', 'PLANNING',             20, '', 'A00', 0, 0), "
    "('C01', 'INFORMATION CENTER',   30, '', 'A00', 0, 0), "
    "('D01', 'DEVELOPMENT CENTER',   0,  '', 'A00', 0, 0),"
    "('D11', 'MANUFACTURING SYSTEM', 60, '', 'D01', 0, 0), "
    "('D21', 'ADMINISTRATION SYSTE', 70, '', 'D01', 0, 0), "
    "('E01', 'SUPPORT SERVICES',     50, '', 'A00', 0, 0), "
    "('E11', 'OPERATIONS',           90, '', 'E01', 0, 0), "
    "('E21', 'SOFTWARE SUPPORT',     100,'', 'E01', 0, 0)";
  rc= mysql_real_query(mysql, stmt_text, strlen(stmt_text));
  myquery(rc);

  /* ****** Begin of trace ****** */

  stmt= open_cursor("select t1.empno, t1.workdept "
                    "from (t1 left join t2 on t2.deptno = t1.workdept) "
                    "where t2.deptno in "
                    "   (select t2.deptno "
                    "    from (t1 left join t2 on t2.deptno = t1.workdept) "
                    "    where t1.empno = ?) "
                    "order by 1");
  bzero(bind, sizeof(bind));

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= &empno;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[1].buffer= (void*) workdept;
  bind[1].buffer_length= sizeof(workdept);
  bind[1].length= &workdept_len;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  empno= 10;
  /* ERROR: next statement causes a server crash */
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "drop table t1, t2");
  myquery(rc);
}

/* Bug#11904: mysql_stmt_attr_set CURSOR_TYPE_READ_ONLY grouping wrong result */

static void test_bug11904()
{
  MYSQL_STMT *stmt1;
  int rc;
  const char *stmt_text;
  const ulong type= (ulong)CURSOR_TYPE_READ_ONLY;
  MYSQL_BIND bind[2];
  int country_id=0;
  char row_data[11]= {0};

  myheader("test_bug11904");

  /* create tables */
  rc= mysql_query(mysql, "DROP TABLE IF EXISTS bug11904b");
  myquery(rc);
  rc= mysql_query(mysql, "CREATE TABLE bug11904b (id int, name char(10), primary key(id, name))");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO bug11904b VALUES (1, 'sofia'), (1,'plovdiv'),"
                          " (1,'varna'), (2,'LA'), (2,'new york'), (3,'heidelberg'),"
                          " (3,'berlin'), (3, 'frankfurt')");

  myquery(rc);
  mysql_commit(mysql);
  /* create statement */
  stmt1= mysql_stmt_init(mysql);
  mysql_stmt_attr_set(stmt1, STMT_ATTR_CURSOR_TYPE, (const void*) &type);

  stmt_text= "SELECT id, MIN(name) FROM bug11904b GROUP BY id";

  rc= mysql_stmt_prepare(stmt1, stmt_text, strlen(stmt_text));
  check_execute(stmt1, rc);

  memset(bind, 0, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer=& country_id;
  bind[0].buffer_length= 0;
  bind[0].length= 0;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer=& row_data;
  bind[1].buffer_length= sizeof(row_data) - 1;
  bind[1].length= 0;

  rc= mysql_stmt_bind_result(stmt1, bind);
  check_execute(stmt1, rc);

  rc= mysql_stmt_execute(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_fetch(stmt1);
  check_execute(stmt1, rc);
  DIE_UNLESS(country_id == 1);
  DIE_UNLESS(memcmp(row_data, "plovdiv", 7) == 0);

  rc= mysql_stmt_fetch(stmt1);
  check_execute(stmt1, rc);
  DIE_UNLESS(country_id == 2);
  DIE_UNLESS(memcmp(row_data, "LA", 2) == 0);

  rc= mysql_stmt_fetch(stmt1);
  check_execute(stmt1, rc);
  DIE_UNLESS(country_id == 3);
  DIE_UNLESS(memcmp(row_data, "berlin", 6) == 0);

  rc= mysql_stmt_close(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_query(mysql, "drop table bug11904b");
  myquery(rc);
}


/* Bug#12243: multiple cursors, crash in a fetch after commit. */

static void test_bug12243()
{
  MYSQL_STMT *stmt1, *stmt2;
  int rc;
  const char *stmt_text;
  ulong type;

  myheader("test_bug12243");

  if (! have_innodb)
  {
    if (!opt_silent)
      printf("This test requires InnoDB.\n");
    return;
  }

  /* create tables */
  mysql_query(mysql, "drop table if exists t1");
  mysql_query(mysql, "create table t1 (a int) engine=InnoDB");
  rc= mysql_query(mysql, "insert into t1 (a) values (1), (2)");
  myquery(rc);
  mysql_autocommit(mysql, FALSE);
  /* create statement */
  stmt1= mysql_stmt_init(mysql);
  stmt2= mysql_stmt_init(mysql);
  type= (ulong) CURSOR_TYPE_READ_ONLY;
  mysql_stmt_attr_set(stmt1, STMT_ATTR_CURSOR_TYPE, (const void*) &type);
  mysql_stmt_attr_set(stmt2, STMT_ATTR_CURSOR_TYPE, (const void*) &type);

  stmt_text= "select a from t1";

  rc= mysql_stmt_prepare(stmt1, stmt_text, strlen(stmt_text));
  check_execute(stmt1, rc);
  rc= mysql_stmt_execute(stmt1);
  check_execute(stmt1, rc);
  rc= mysql_stmt_fetch(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_prepare(stmt2, stmt_text, strlen(stmt_text));
  check_execute(stmt2, rc);
  rc= mysql_stmt_execute(stmt2);
  check_execute(stmt2, rc);
  rc= mysql_stmt_fetch(stmt2);
  check_execute(stmt2, rc);

  rc= mysql_stmt_close(stmt1);
  check_execute(stmt1, rc);
  rc= mysql_commit(mysql);
  myquery(rc);
  rc= mysql_stmt_fetch(stmt2);
  check_execute(stmt2, rc);

  mysql_stmt_close(stmt2);
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
  mysql_autocommit(mysql, TRUE);                /* restore default */
}


/*
  Bug#11718: query with function, join and order by returns wrong type
*/

static void test_bug11718()
{
  MYSQL_RES	*res;
  int rc;
  const char *query= "select str_to_date(concat(f3),'%Y%m%d') from t1,t2 "
                     "where f1=f2 order by f1";

  myheader("test_bug11718");

  rc= mysql_query(mysql, "drop table if exists t1, t2");
  myquery(rc);
  rc= mysql_query(mysql, "create table t1 (f1 int)");
  myquery(rc);
  rc= mysql_query(mysql, "create table t2 (f2 int, f3 numeric(8))");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t1 values (1), (2)");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t2 values (1,20050101), (2,20050202)");
  myquery(rc);
  rc= mysql_query(mysql, query);
  myquery(rc);
  res = mysql_store_result(mysql);

  if (!opt_silent)
    printf("return type: %s", (res->fields[0].type == MYSQL_TYPE_DATE)?"DATE":
           "not DATE");
  DIE_UNLESS(res->fields[0].type == MYSQL_TYPE_DATE);
  rc= mysql_query(mysql, "drop table t1, t2");
  myquery(rc);
}


/*
  Bug #12925: Bad handling of maximum values in getopt
*/
static void test_bug12925()
{
  myheader("test_bug12925");
  if (opt_getopt_ll_test)
    DIE_UNLESS(opt_getopt_ll_test == LL(25600*1024*1024));
}


/*
  Bug#14210 "Simple query with > operator on large table gives server
  crash"
*/

static void test_bug14210()
{
  MYSQL_STMT *stmt;
  int rc, i;
  const char *stmt_text;
  ulong type;

  myheader("test_bug14210");

  mysql_query(mysql, "drop table if exists t1");
  /*
    To trigger the problem the table must be InnoDB, although the problem
    itself is not InnoDB related. In case the table is MyISAM this test
    is harmless.
  */
  mysql_query(mysql, "create table t1 (a varchar(255)) type=InnoDB");
  rc= mysql_query(mysql, "insert into t1 (a) values (repeat('a', 256))");
  myquery(rc);
  rc= mysql_query(mysql, "set @@session.max_heap_table_size=16384");
  /* Create a big enough table (more than max_heap_table_size) */
  for (i= 0; i < 8; i++)
  {
    rc= mysql_query(mysql, "insert into t1 (a) select a from t1");
    myquery(rc);
  }
  /* create statement */
  stmt= mysql_stmt_init(mysql);
  type= (ulong) CURSOR_TYPE_READ_ONLY;
  mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (const void*) &type);

  stmt_text= "select a from t1";

  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  while ((rc= mysql_stmt_fetch(stmt)) == 0)
    ;
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  rc= mysql_stmt_close(stmt);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
  rc= mysql_query(mysql, "set @@session.max_heap_table_size=default");
  myquery(rc);
}

/* Bug#13488: wrong column metadata when fetching from cursor */

static void test_bug13488()
{
  MYSQL_BIND bind[3];
  MYSQL_STMT *stmt1;
  int rc, f1, f2, f3, i;
  const ulong type= CURSOR_TYPE_READ_ONLY;
  const char *query= "select * from t1 left join t2 on f1=f2 where f1=1";

  myheader("test_bug13488");

  rc= mysql_query(mysql, "drop table if exists t1, t2");
  myquery(rc);
  rc= mysql_query(mysql, "create table t1 (f1 int not null primary key)");
  myquery(rc);
  rc= mysql_query(mysql, "create table t2 (f2 int not null primary key, "
                  "f3 int not null)");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t1 values (1), (2)");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t2 values (1,2), (2,4)");
  myquery(rc);

  memset(bind, 0, sizeof(bind));
  for (i= 0; i < 3; i++)
  {
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer_length= 4;
    bind[i].length= 0;
  }
  bind[0].buffer=&f1;
  bind[1].buffer=&f2;
  bind[2].buffer=&f3;

  stmt1= mysql_stmt_init(mysql);
  rc= mysql_stmt_attr_set(stmt1,STMT_ATTR_CURSOR_TYPE, (const void *)&type);
  check_execute(stmt1, rc);

  rc= mysql_stmt_prepare(stmt1, query, strlen(query));
  check_execute(stmt1, rc);

  rc= mysql_stmt_execute(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_bind_result(stmt1, bind);
  check_execute(stmt1, rc);

  rc= mysql_stmt_fetch(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_free_result(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_reset(stmt1);
  check_execute(stmt1, rc);

  rc= mysql_stmt_close(stmt1);
  check_execute(stmt1, rc);

  if (!opt_silent)
    printf("data is: %s", (f1 == 1 && f2 == 1 && f3 == 2)?"OK":
           "wrong");
  DIE_UNLESS(f1 == 1 && f2 == 1 && f3 == 2);
  rc= mysql_query(mysql, "drop table t1, t2");
  myquery(rc);
}

/*
  Bug#13524: warnings of a previous command are not reset when fetching
  from a cursor.
*/

static void test_bug13524()
{
  MYSQL_STMT *stmt;
  int rc;
  unsigned int warning_count;
  const ulong type= CURSOR_TYPE_READ_ONLY;
  const char *query= "select * from t1";

  myheader("test_bug13524");

  rc= mysql_query(mysql, "drop table if exists t1, t2");
  myquery(rc);
  rc= mysql_query(mysql, "create table t1 (a int not null primary key)");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t1 values (1), (2), (3), (4)");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (const void*) &type);
  check_execute(stmt, rc);

  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  warning_count= mysql_warning_count(mysql);
  DIE_UNLESS(warning_count == 0);

  /* Check that DROP TABLE produced a warning (no such table) */
  rc= mysql_query(mysql, "drop table if exists t2");
  myquery(rc);
  warning_count= mysql_warning_count(mysql);
  DIE_UNLESS(warning_count == 1);

  /*
    Check that fetch from a cursor cleared the warning from the previous
    command.
  */
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  warning_count= mysql_warning_count(mysql);
  DIE_UNLESS(warning_count == 0);

  /* Cleanup */
  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}

/*
  Bug#14845 "mysql_stmt_fetch returns MYSQL_NO_DATA when COUNT(*) is 0"
*/

static void test_bug14845()
{
  MYSQL_STMT *stmt;
  int rc;
  const ulong type= CURSOR_TYPE_READ_ONLY;
  const char *query= "select count(*) from t1 where 1 = 0";

  myheader("test_bug14845");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);
  rc= mysql_query(mysql, "create table t1 (id int(11) default null, "
                         "name varchar(20) default null)"
                         "engine=MyISAM DEFAULT CHARSET=utf8");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t1 values (1,'abc'),(2,'def')");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (const void*) &type);
  check_execute(stmt, rc);

  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  /* Cleanup */
  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


/*
  Bug #15510: mysql_warning_count returns 0 after mysql_stmt_fetch which
  should warn
*/
static void test_bug15510()
{
  MYSQL_STMT *stmt;
  int rc;
  const char *query= "select 1 from dual where 1/0";

  myheader("test_bug15510");

  rc= mysql_query(mysql, "set @@sql_mode='ERROR_FOR_DIVISION_BY_ZERO'");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);

  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(mysql_warning_count(mysql));

  /* Cleanup */
  mysql_stmt_close(stmt);
  rc= mysql_query(mysql, "set @@sql_mode=''");
  myquery(rc);
}


/* Test MYSQL_OPT_RECONNECT, Bug#15719 */

static void test_opt_reconnect()
{
  MYSQL *lmysql;
  my_bool my_true= TRUE;

  myheader("test_opt_reconnect");

  if (!(lmysql= mysql_init(NULL)))
  {
    myerror("mysql_init() failed");
    exit(1);
  }

  if (!opt_silent)
    fprintf(stdout, "reconnect before mysql_options: %d\n", lmysql->reconnect);
  DIE_UNLESS(lmysql->reconnect == 0);

  if (mysql_options(lmysql, MYSQL_OPT_RECONNECT, &my_true))
  {
    myerror("mysql_options failed: unknown option MYSQL_OPT_RECONNECT\n");
    exit(1);
  }

  /* reconnect should be 1 */
  if (!opt_silent)
    fprintf(stdout, "reconnect after mysql_options: %d\n", lmysql->reconnect);
  DIE_UNLESS(lmysql->reconnect == 1);

  if (!(mysql_real_connect(lmysql, opt_host, opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket, 0)))
  {
    myerror("connection failed");
    exit(1);
  }

  /* reconnect should still be 1 */
  if (!opt_silent)
    fprintf(stdout, "reconnect after mysql_real_connect: %d\n",
	    lmysql->reconnect);
  DIE_UNLESS(lmysql->reconnect == 1);

  mysql_close(lmysql);

  if (!(lmysql= mysql_init(NULL)))
  {
    myerror("mysql_init() failed");
    exit(1);
  }

  if (!opt_silent)
    fprintf(stdout, "reconnect before mysql_real_connect: %d\n", lmysql->reconnect);
  DIE_UNLESS(lmysql->reconnect == 0);

  if (!(mysql_real_connect(lmysql, opt_host, opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket, 0)))
  {
    myerror("connection failed");
    exit(1);
  }

  /* reconnect should still be 0 */
  if (!opt_silent)
    fprintf(stdout, "reconnect after mysql_real_connect: %d\n",
	    lmysql->reconnect);
  DIE_UNLESS(lmysql->reconnect == 0);

  mysql_close(lmysql);
}


static void test_bug12744()
{
  MYSQL_STMT *prep_stmt = NULL;
  int rc;
  myheader("test_bug12744");

  prep_stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(prep_stmt, "SELECT 1", 8);
  DIE_UNLESS(rc==0);

  mysql_close(mysql);

  if ((rc= mysql_stmt_execute(prep_stmt)))
  {
    if ((rc= mysql_stmt_reset(prep_stmt)))
      printf("OK!\n");
    else
    {
      printf("Error!");
      DIE_UNLESS(1==0);
    }
  }
  else
  {
    fprintf(stderr, "expected error but no error occured\n");
    DIE_UNLESS(1==0);
  }
  rc= mysql_stmt_close(prep_stmt);
  client_connect(0);
}

/* Bug #16143: mysql_stmt_sqlstate returns an empty string instead of '00000' */

static void test_bug16143()
{
  MYSQL_STMT *stmt;
  myheader("test_bug16143");

  stmt= mysql_stmt_init(mysql);
  /* Check mysql_stmt_sqlstate return "no error" */
  DIE_UNLESS(strcmp(mysql_stmt_sqlstate(stmt), "00000") == 0);

  mysql_stmt_close(stmt);
}


/*
  Bug #15613: "libmysqlclient API function mysql_stmt_prepare returns wrong
  field length"
*/

static void test_bug15613()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;
  MYSQL_RES *metadata;
  MYSQL_FIELD *field;
  int rc;
  myheader("test_bug15613");

  /* I. Prepare the table */
  rc= mysql_query(mysql, "set names latin1");
  myquery(rc);
  mysql_query(mysql, "drop table if exists t1");
  rc= mysql_query(mysql,
                  "create table t1 (t text character set utf8, "
                                   "tt tinytext character set utf8, "
                                   "mt mediumtext character set utf8, "
                                   "lt longtext character set utf8, "
                                   "vl varchar(255) character set latin1,"
                                   "vb varchar(255) character set binary,"
                                   "vu varchar(255) character set utf8)");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);

  /* II. Check SELECT metadata */
  stmt_text= ("select t, tt, mt, lt, vl, vb, vu from t1");
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  metadata= mysql_stmt_result_metadata(stmt);
  field= mysql_fetch_fields(metadata);
  if (!opt_silent)
  {
    printf("Field lengths (client character set is latin1):\n"
           "text character set utf8:\t\t%lu\n"
           "tinytext character set utf8:\t\t%lu\n"
           "mediumtext character set utf8:\t\t%lu\n"
           "longtext character set utf8:\t\t%lu\n"
           "varchar(255) character set latin1:\t%lu\n"
           "varchar(255) character set binary:\t%lu\n"
           "varchar(255) character set utf8:\t%lu\n",
           field[0].length, field[1].length, field[2].length, field[3].length,
           field[4].length, field[5].length, field[6].length);
  }
  DIE_UNLESS(field[0].length == 65535);
  DIE_UNLESS(field[1].length == 255);
  DIE_UNLESS(field[2].length == 16777215);
  DIE_UNLESS(field[3].length == 4294967295UL);
  DIE_UNLESS(field[4].length == 255);
  DIE_UNLESS(field[5].length == 255);
  DIE_UNLESS(field[6].length == 255);

  /* III. Cleanup */
  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
  rc= mysql_query(mysql, "set names default");
  myquery(rc);
  mysql_stmt_close(stmt);
}

/*
  Bug#17667: An attacker has the opportunity to bypass query logging.
*/
static void test_bug17667()
{
  int rc;
  struct buffer_and_length {
    const char *buffer;
    const uint length;
  } statements[]= {
    { "drop table if exists bug17667", 29 },
    { "create table bug17667 (c varchar(20))", 37 },
    { "insert into bug17667 (c) values ('regular') /* NUL=\0 with comment */", 68 },
    { "insert into bug17667 (c) values ('NUL=\0 in value')", 50 },
    { "insert into bug17667 (c) values ('5 NULs=\0\0\0\0\0')", 48 },
    { "/* NUL=\0 with comment */ insert into bug17667 (c) values ('encore')", 67 },
    { "drop table bug17667", 19 },
    { NULL, 0 } };

  struct buffer_and_length *statement_cursor;
  FILE *log_file;
  char *master_log_filename;

  myheader("test_bug17667");

  for (statement_cursor= statements; statement_cursor->buffer != NULL;
      statement_cursor++) {
    rc= mysql_real_query(mysql, statement_cursor->buffer,
        statement_cursor->length);
    myquery(rc);
  }

  /* Make sure the server has written the logs to disk before reading it */
  rc= mysql_query(mysql, "flush logs");
  myquery(rc);

  master_log_filename = (char *) malloc(strlen(opt_vardir) + strlen("/log/master.log") + 1);
  strcpy(master_log_filename, opt_vardir);
  strcat(master_log_filename, "/log/master.log");
  printf("Opening '%s'\n", master_log_filename);
  log_file= fopen(master_log_filename, "r");
  free(master_log_filename);

  if (log_file != NULL) {

    for (statement_cursor= statements; statement_cursor->buffer != NULL;
        statement_cursor++) {
      char line_buffer[MAX_TEST_QUERY_LENGTH*2];
      /* more than enough room for the query and some marginalia. */

      do {
        memset(line_buffer, '/', MAX_TEST_QUERY_LENGTH*2);

        if(fgets(line_buffer, MAX_TEST_QUERY_LENGTH*2, log_file) == NULL)
        {
          /* If fgets returned NULL, it indicates either error or EOF */
          if (feof(log_file))
            DIE("Found EOF before all statements where found");
          else
          {
            fprintf(stderr, "Got error %d while reading from file\n",
                    ferror(log_file));
            DIE("Read error");
          }
        }

      } while (my_memmem(line_buffer, MAX_TEST_QUERY_LENGTH*2,
            statement_cursor->buffer, statement_cursor->length) == NULL);

      printf("Found statement starting with \"%s\"\n",
             statement_cursor->buffer);
    }

    printf("success.  All queries found intact in the log.\n");

  }
  else
  {
    fprintf(stderr, "Could not find the log file, VARDIR/log/master.log, so "
            "test_bug17667 is \ninconclusive.  Run test from the "
            "mysql-test/mysql-test-run* program \nto set up the correct "
            "environment for this test.\n\n");
  }

  if (log_file != NULL)
    fclose(log_file);

}


/*
  Bug#14169: type of group_concat() result changed to blob if tmp_table was
  used
*/
static void test_bug14169()
{
  MYSQL_STMT *stmt;
  const char *stmt_text;
  MYSQL_RES *res;
  MYSQL_FIELD *field;
  int rc;

  myheader("test_bug14169");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);
  rc= mysql_query(mysql, "set session group_concat_max_len=1024");
  myquery(rc);
  rc= mysql_query(mysql, "create table t1 (f1 int unsigned, f2 varchar(255))");
  myquery(rc);
  rc= mysql_query(mysql, "insert into t1 values (1,repeat('a',255)),"
                         "(2,repeat('b',255))");
  myquery(rc);
  stmt= mysql_stmt_init(mysql);
  stmt_text= "select f2,group_concat(f1) from t1 group by f2";
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
  myquery(rc);
  res= mysql_stmt_result_metadata(stmt);
  field= mysql_fetch_fields(res);
  if (!opt_silent)
    printf("GROUP_CONCAT() result type %i", field[1].type);
  DIE_UNLESS(field[1].type == MYSQL_TYPE_BLOB);

  rc= mysql_query(mysql, "drop table t1");
  myquery(rc);
}


/*
  Bug#20152: mysql_stmt_execute() writes to MYSQL_TYPE_DATE buffer
*/

static void test_bug20152()
{
  MYSQL_BIND bind[1];
  MYSQL_STMT *stmt;
  MYSQL_TIME tm;
  int rc;
  const char *query= "INSERT INTO t1 (f1) VALUES (?)";

  myheader("test_bug20152");

  memset(bind, 0, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_DATE;
  bind[0].buffer= (void*)&tm;

  tm.year = 2006;
  tm.month = 6;
  tm.day = 18;
  tm.hour = 14;
  tm.minute = 9;
  tm.second = 42;

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS t1");
  myquery(rc);
  rc= mysql_query(mysql, "CREATE TABLE t1 (f1 DATE)");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);
  rc= mysql_stmt_close(stmt);
  check_execute(stmt, rc);
  rc= mysql_query(mysql, "DROP TABLE t1");
  myquery(rc);

  if (tm.hour == 14 && tm.minute == 9 && tm.second == 42) {
    if (!opt_silent)
      printf("OK!");
  } else {
    printf("[14:09:42] != [%02d:%02d:%02d]\n", tm.hour, tm.minute, tm.second);
    DIE_UNLESS(0==1);
  }
}

/* Bug#15752 "Lost connection to MySQL server when calling a SP from C API" */

static void test_bug15752()
{
  MYSQL mysql_local;
  int rc, i;
  const int ITERATION_COUNT= 100;
  char *query= "CALL p1()";

  myheader("test_bug15752");

  rc= mysql_query(mysql, "drop procedure if exists p1");
  myquery(rc);
  rc= mysql_query(mysql, "create procedure p1() select 1");
  myquery(rc);

  mysql_init(&mysql_local);
  if (! mysql_real_connect(&mysql_local, opt_host, opt_user,
                           opt_password, current_db, opt_port,
                           opt_unix_socket,
                           CLIENT_MULTI_STATEMENTS|CLIENT_MULTI_RESULTS))
  {
    printf("Unable connect to MySQL server: %s\n", mysql_error(&mysql_local));
    DIE_UNLESS(0);
  }
  rc= mysql_real_query(&mysql_local, query, strlen(query));
  myquery(rc);
  mysql_free_result(mysql_store_result(&mysql_local));

  rc= mysql_real_query(&mysql_local, query, strlen(query));
  DIE_UNLESS(rc && mysql_errno(&mysql_local) == CR_COMMANDS_OUT_OF_SYNC);

  if (! opt_silent)
    printf("Got error (as expected): %s\n", mysql_error(&mysql_local));

  /* Check some other commands too */

  DIE_UNLESS(mysql_next_result(&mysql_local) == 0);
  mysql_free_result(mysql_store_result(&mysql_local));
  DIE_UNLESS(mysql_next_result(&mysql_local) == -1);

  /* The second problem is not reproducible: add the test case */
  for (i = 0; i < ITERATION_COUNT; i++)
  {
    if (mysql_real_query(&mysql_local, query, strlen(query)))
    {
      printf("\ni=%d %s failed: %s\n", i, query, mysql_error(&mysql_local));
      break;
    }
    mysql_free_result(mysql_store_result(&mysql_local));
    DIE_UNLESS(mysql_next_result(&mysql_local) == 0);
    mysql_free_result(mysql_store_result(&mysql_local));
    DIE_UNLESS(mysql_next_result(&mysql_local) == -1);

  }
  mysql_close(&mysql_local);
  rc= mysql_query(mysql, "drop procedure p1");
  myquery(rc);
}

/*
  Bug#21206: memory corruption when too many cursors are opened at once

  Memory corruption happens when more than 1024 cursors are open
  simultaneously.
*/
static void test_bug21206()
{
  const size_t cursor_count= 1025;

  const char *create_table[]=
  {
    "DROP TABLE IF EXISTS t1",
    "CREATE TABLE t1 (i INT)",
    "INSERT INTO t1 VALUES (1), (2), (3)"
  };
  const char *query= "SELECT * FROM t1";

  Stmt_fetch *fetch_array=
    (Stmt_fetch*) calloc(cursor_count, sizeof(Stmt_fetch));

  Stmt_fetch *fetch;

  DBUG_ENTER("test_bug21206");
  myheader("test_bug21206");

  fill_tables(create_table, sizeof(create_table) / sizeof(*create_table));

  for (fetch= fetch_array; fetch < fetch_array + cursor_count; ++fetch)
  {
    /* Init will exit(1) in case of error */
    stmt_fetch_init(fetch, fetch - fetch_array, query);
  }

  for (fetch= fetch_array; fetch < fetch_array + cursor_count; ++fetch)
    stmt_fetch_close(fetch);

  free(fetch_array);

  DBUG_VOID_RETURN;
}


/*
  Read and parse arguments and MySQL options from my.cnf
*/

static const char *client_test_load_default_groups[]= { "client", 0 };
static char **defaults_argv;

static struct my_option client_test_long_options[] =
{
  {"basedir", 'b', "Basedir for tests.", (gptr*) &opt_basedir,
   (gptr*) &opt_basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"count", 't', "Number of times test to be executed", (char **) &opt_count,
   (char **) &opt_count, 0, GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  {"database", 'D', "Database to use", (char **) &opt_db, (char **) &opt_db,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"debug", '#', "Output debug log", (gptr*) &default_dbug_option,
   (gptr*) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host", (char **) &opt_host, (char **) &opt_host,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection", (char **) &opt_port,
   (char **) &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-arg", 'A', "Send embedded server this as a parameter.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"show-tests", 'T', "Show all tests' names", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"silent", 's', "Be more silent", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"socket", 'S', "Socket file to use for connection",
   (char **) &opt_unix_socket, (char **) &opt_unix_socket, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"testcase", 'c',
   "May disable some code when runs as mysql-test-run testcase.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user", (char **) &opt_user,
   (char **) &opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"vardir", 'v', "Data dir for tests.", (gptr*) &opt_vardir,
   (gptr*) &opt_vardir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"getopt-ll-test", 'g', "Option for testing bug in getopt library",
   (char **) &opt_getopt_ll_test, (char **) &opt_getopt_ll_test, 0,
   GET_LL, REQUIRED_ARG, 0, 0, LONGLONG_MAX, 0, 0, 0},
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


static struct my_tests_st my_tests[]= {
  { "test_view_sp_list_fields", test_view_sp_list_fields},
  { "client_query", client_query },
  { "test_prepare_insert_update", test_prepare_insert_update},
#if NOT_YET_WORKING
  { "test_drop_temp", test_drop_temp },
#endif
  { "test_fetch_seek", test_fetch_seek },
  { "test_fetch_nobuffs", test_fetch_nobuffs },
  { "test_open_direct", test_open_direct },
  { "test_fetch_null", test_fetch_null },
  { "test_ps_null_param", test_ps_null_param },
  { "test_fetch_date", test_fetch_date },
  { "test_fetch_str", test_fetch_str },
  { "test_fetch_long", test_fetch_long },
  { "test_fetch_short", test_fetch_short },
  { "test_fetch_tiny", test_fetch_tiny },
  { "test_fetch_bigint", test_fetch_bigint },
  { "test_fetch_float", test_fetch_float },
  { "test_fetch_double", test_fetch_double },
  { "test_bind_result_ext", test_bind_result_ext },
  { "test_bind_result_ext1", test_bind_result_ext1 },
  { "test_select_direct", test_select_direct },
  { "test_select_prepare", test_select_prepare },
  { "test_select", test_select },
  { "test_select_version", test_select_version },
  { "test_ps_conj_select", test_ps_conj_select },
  { "test_select_show_table", test_select_show_table },
  { "test_func_fields", test_func_fields },
  { "test_long_data", test_long_data },
  { "test_insert", test_insert },
  { "test_set_variable", test_set_variable },
  { "test_select_show", test_select_show },
  { "test_prepare_noparam", test_prepare_noparam },
  { "test_bind_result", test_bind_result },
  { "test_prepare_simple", test_prepare_simple },
  { "test_prepare", test_prepare },
  { "test_null", test_null },
  { "test_debug_example", test_debug_example },
  { "test_update", test_update },
  { "test_simple_update", test_simple_update },
  { "test_simple_delete", test_simple_delete },
  { "test_double_compare", test_double_compare },
  { "client_store_result", client_store_result },
  { "client_use_result", client_use_result },
  { "test_tran_bdb", test_tran_bdb },
  { "test_tran_innodb", test_tran_innodb },
  { "test_prepare_ext", test_prepare_ext },
  { "test_prepare_syntax", test_prepare_syntax },
  { "test_field_names", test_field_names },
  { "test_field_flags", test_field_flags },
  { "test_long_data_str", test_long_data_str },
  { "test_long_data_str1", test_long_data_str1 },
  { "test_long_data_bin", test_long_data_bin },
  { "test_warnings", test_warnings },
  { "test_errors", test_errors },
  { "test_prepare_resultset", test_prepare_resultset },
  { "test_stmt_close", test_stmt_close },
  { "test_prepare_field_result", test_prepare_field_result },
  { "test_multi_stmt", test_multi_stmt },
  { "test_multi_statements", test_multi_statements },
  { "test_prepare_multi_statements", test_prepare_multi_statements },
  { "test_store_result", test_store_result },
  { "test_store_result1", test_store_result1 },
  { "test_store_result2", test_store_result2 },
  { "test_subselect", test_subselect },
  { "test_date", test_date },
  { "test_date_date", test_date_date },
  { "test_date_time", test_date_time },
  { "test_date_ts", test_date_ts },
  { "test_date_dt", test_date_dt },
  { "test_prepare_alter", test_prepare_alter },
  { "test_manual_sample", test_manual_sample },
  { "test_pure_coverage", test_pure_coverage },
  { "test_buffers", test_buffers },
  { "test_ushort_bug", test_ushort_bug },
  { "test_sshort_bug", test_sshort_bug },
  { "test_stiny_bug", test_stiny_bug },
  { "test_field_misc", test_field_misc },
  { "test_set_option", test_set_option },
#ifndef EMBEDDED_LIBRARY
  { "test_prepare_grant", test_prepare_grant },
#endif
  { "test_frm_bug", test_frm_bug },
  { "test_explain_bug", test_explain_bug },
  { "test_decimal_bug", test_decimal_bug },
  { "test_nstmts", test_nstmts },
  { "test_logs;", test_logs },
  { "test_cuted_rows", test_cuted_rows },
  { "test_fetch_offset", test_fetch_offset },
  { "test_fetch_column", test_fetch_column },
  { "test_mem_overun", test_mem_overun },
  { "test_list_fields", test_list_fields },
  { "test_free_result", test_free_result },
  { "test_free_store_result", test_free_store_result },
  { "test_sqlmode", test_sqlmode },
  { "test_ts", test_ts },
  { "test_bug1115", test_bug1115 },
  { "test_bug1180", test_bug1180 },
  { "test_bug1500", test_bug1500 },
  { "test_bug1644", test_bug1644 },
  { "test_bug1946", test_bug1946 },
  { "test_bug2248", test_bug2248 },
  { "test_parse_error_and_bad_length", test_parse_error_and_bad_length },
  { "test_bug2247", test_bug2247 },
  { "test_subqueries", test_subqueries },
  { "test_bad_union", test_bad_union },
  { "test_distinct", test_distinct },
  { "test_subqueries_ref", test_subqueries_ref },
  { "test_union", test_union },
  { "test_bug3117", test_bug3117 },
  { "test_join", test_join },
  { "test_selecttmp", test_selecttmp },
  { "test_create_drop", test_create_drop },
  { "test_rename", test_rename },
  { "test_do_set", test_do_set },
  { "test_multi", test_multi },
  { "test_insert_select", test_insert_select },
  { "test_bind_nagative", test_bind_nagative },
  { "test_derived", test_derived },
  { "test_xjoin", test_xjoin },
  { "test_bug3035", test_bug3035 },
  { "test_union2", test_union2 },
  { "test_bug1664", test_bug1664 },
  { "test_union_param", test_union_param },
  { "test_order_param", test_order_param },
  { "test_ps_i18n", test_ps_i18n },
  { "test_bug3796", test_bug3796 },
  { "test_bug4026", test_bug4026 },
  { "test_bug4079", test_bug4079 },
  { "test_bug4236", test_bug4236 },
  { "test_bug4030", test_bug4030 },
  { "test_bug5126", test_bug5126 },
  { "test_bug4231", test_bug4231 },
  { "test_bug5399", test_bug5399 },
  { "test_bug5194", test_bug5194 },
  { "test_bug5315", test_bug5315 },
  { "test_bug6049", test_bug6049 },
  { "test_bug6058", test_bug6058 },
  { "test_bug6059", test_bug6059 },
  { "test_bug6046", test_bug6046 },
  { "test_bug6081", test_bug6081 },
  { "test_bug6096", test_bug6096 },
  { "test_datetime_ranges", test_datetime_ranges },
  { "test_bug4172", test_bug4172 },
  { "test_conversion", test_conversion },
  { "test_rewind", test_rewind },
  { "test_bug6761", test_bug6761 },
  { "test_view", test_view },
  { "test_view_where", test_view_where },
  { "test_view_2where", test_view_2where },
  { "test_view_star", test_view_star },
  { "test_view_insert", test_view_insert },
  { "test_left_join_view", test_left_join_view },
  { "test_view_insert_fields", test_view_insert_fields },
  { "test_basic_cursors", test_basic_cursors },
  { "test_cursors_with_union", test_cursors_with_union },
  { "test_truncation", test_truncation },
  { "test_truncation_option", test_truncation_option },
  { "test_client_character_set", test_client_character_set },
  { "test_bug8330", test_bug8330 },
  { "test_bug7990", test_bug7990 },
  { "test_bug8378", test_bug8378 },
  { "test_bug8722", test_bug8722 },
  { "test_bug8880", test_bug8880 },
  { "test_bug9159", test_bug9159 },
  { "test_bug9520", test_bug9520 },
  { "test_bug9478", test_bug9478 },
  { "test_bug9643", test_bug9643 },
  { "test_bug10729", test_bug10729 },
  { "test_bug11111", test_bug11111 },
  { "test_bug9992", test_bug9992 },
  { "test_bug10736", test_bug10736 },
  { "test_bug10794", test_bug10794 },
  { "test_bug11172", test_bug11172 },
  { "test_bug11656", test_bug11656 },
  { "test_bug10214", test_bug10214 },
  { "test_bug9735", test_bug9735 },
  { "test_bug11183", test_bug11183 },
  { "test_bug11037", test_bug11037 },
  { "test_bug10760", test_bug10760 },
  { "test_bug12001", test_bug12001 },
  { "test_bug11718", test_bug11718 },
  { "test_bug12925", test_bug12925 },
  { "test_bug11909", test_bug11909 },
  { "test_bug11901", test_bug11901 },
  { "test_bug11904", test_bug11904 },
  { "test_bug12243", test_bug12243 },
  { "test_bug14210", test_bug14210 },
  { "test_bug13488", test_bug13488 },
  { "test_bug13524", test_bug13524 },
  { "test_bug14845", test_bug14845 },
  { "test_bug15510", test_bug15510 },
  { "test_opt_reconnect", test_opt_reconnect },
#ifndef EMBEDDED_LIBRARY
  { "test_bug12744", test_bug12744 },
#endif
  { "test_bug16143", test_bug16143 },
  { "test_bug15613", test_bug15613 },
  { "test_bug20152", test_bug20152 },
  { "test_bug14169", test_bug14169 },
  { "test_bug17667", test_bug17667 },
  { "test_bug19671", test_bug19671 },
  { "test_bug15752", test_bug15752 },
  { "test_bug21206", test_bug21206},
  { 0, 0 }
};


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
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
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
      for (fptr= my_tests; fptr->name; fptr++)
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

  DEBUGGER_OFF;
  MY_INIT(argv[0]);

  load_defaults("my", client_test_load_default_groups, &argc, &argv);
  defaults_argv= argv;
  get_options(&argc, &argv);

  if (mysql_server_init(embedded_server_arg_count,
                        embedded_server_args,
                        (char**) embedded_server_groups))
    DIE("Can't initialize MySQL server");

  client_connect(0);       /* connect to server */

  total_time= 0;
  for (iter_count= 1; iter_count <= opt_count; iter_count++)
  {
    /* Start of tests */
    test_count= 1;
    start_time= time((time_t *)0);
    if (!argc)
    {
      for (fptr= my_tests; fptr->name; fptr++)
	(*fptr->function)();	
    }
    else
    {
      for ( ; *argv ; argv++)
      {
	for (fptr= my_tests; fptr->name; fptr++)
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
	  client_disconnect();
	  free_defaults(defaults_argv);
	  exit(1);
	}
      }
    }

    end_time= time((time_t *)0);
    total_time+= difftime(end_time, start_time);

    /* End of tests */
  }

  client_disconnect();    /* disconnect from server */

  free_defaults(defaults_argv);
  print_test_output();

  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count],MYF(0));

  mysql_server_end();

  my_end(0);

  exit(0);
}
