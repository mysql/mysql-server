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

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <my_getopt.h>
#include <m_string.h>

#define VER "2.0"
#define MAX_TEST_QUERY_LENGTH 300 /* MAX QUERY BUFFER LENGTH */
#define MAX_KEY 64

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
static char query[MAX_TEST_QUERY_LENGTH];
static char current_db[]= "client_test_db";
static unsigned int test_count= 0;
static unsigned int opt_count= 0;
static unsigned int iter_count= 0;

static time_t start_time, end_time;
static double total_time;

const char *default_dbug_option= "d:t:o,/tmp/client_test.trace";

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

void die(const char *file, int line, const char *expr)
{
  fprintf(stderr, "%s:%d: check failed: '%s'\n", file, line, expr);
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

static void client_connect()
{
  int  rc;
  myheader_r("client_connect");

  if (!opt_silent)
    fprintf(stdout, "\n Establishing a connection to '%s' ...",
            opt_host ? opt_host : "");

  if (!(mysql= mysql_init(NULL)))
  {
    myerror("mysql_init() failed");
    exit(1);
  }

  if (!(mysql_real_connect(mysql, opt_host, opt_user,
                           opt_password, opt_db ? opt_db:"test", opt_port,
                           opt_unix_socket, 0)))
  {
    myerror("connection failed");
    mysql_close(mysql);
    fprintf(stdout, "\n Check the connection options using --help or -?\n");
    exit(1);
  }

  if (!opt_silent)
    fprintf(stdout, " OK");

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(mysql, TRUE);

  if (!opt_silent)
    fprintf(stdout, "\n Creating a test database '%s' ...", current_db);
  strxmov(query, "CREATE DATABASE IF NOT EXISTS ", current_db, NullS);

  rc= mysql_query(mysql, query);
  myquery(rc);

  strxmov(query, "USE ", current_db, NullS);
  rc= mysql_query(mysql, query);
  myquery(rc);

  if (!opt_silent)
    fprintf(stdout, " OK");
}


/* Close the connection */

static void client_disconnect()
{
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
  my_print_result_metadata(result);

  rc= mysql_stmt_bind_result(stmt, buffer);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  mysql_field_seek(result, 0);
  while (mysql_stmt_fetch(stmt) == 0)
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

static void verify_prepare_field(MYSQL_RES *result,
                                 unsigned int no, const char *name,
                                 const char *org_name,
                                 enum enum_field_types type,
                                 const char *table,
                                 const char *org_table, const char *db,
                                 unsigned long length, const char *def)
{
  MYSQL_FIELD *field;

  if (!(field= mysql_fetch_field_direct(result, no)))
  {
    fprintf(stdout, "\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  if (!opt_silent)
  {
    fprintf(stdout, "\n field[%d]:", no);
    fprintf(stdout, "\n    name     :`%s`\t(expected: `%s`)", field->name, name);
    fprintf(stdout, "\n    org_name :`%s`\t(expected: `%s`)",
            field->org_name, org_name);
    fprintf(stdout, "\n    type     :`%d`\t(expected: `%d`)", field->type, type);
    fprintf(stdout, "\n    table    :`%s`\t(expected: `%s`)",
            field->table, table);
    fprintf(stdout, "\n    org_table:`%s`\t(expected: `%s`)",
            field->org_table, org_table);
    fprintf(stdout, "\n    database :`%s`\t(expected: `%s`)", field->db, db);
    fprintf(stdout, "\n    length   :`%ld`\t(expected: `%ld`)",
            field->length, length);
    fprintf(stdout, "\n    maxlength:`%ld`", field->max_length);
    fprintf(stdout, "\n    charsetnr:`%d`", field->charsetnr);
    fprintf(stdout, "\n    default  :`%s`\t(expected: `%s`)",
            field->def ? field->def : "(null)", def ? def: "(null)");
    fprintf(stdout, "\n");
  }
  DIE_UNLESS(strcmp(field->name, name) == 0);
  DIE_UNLESS(strcmp(field->org_name, org_name) == 0);
  DIE_UNLESS(field->type == type);
  DIE_UNLESS(strcmp(field->table, table) == 0);
  DIE_UNLESS(strcmp(field->org_table, org_table) == 0);
  DIE_UNLESS(strcmp(field->db, db) == 0);
  DIE_UNLESS(field->length == length);
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
    fprintf(stdout, "\n total affected rows: `%lld` (expected: `%lld`)",
            affected_rows, exp_count);
  DIE_UNLESS(affected_rows == exp_count);
}


/* Utility function to verify the total affected rows */

static void verify_affected_rows(ulonglong exp_count)
{
  ulonglong affected_rows= mysql_affected_rows(mysql);
  if (!opt_silent)
    fprintf(stdout, "\n total affected rows: `%lld` (expected: `%lld`)",
          affected_rows, exp_count);
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
    fprintf(stdout, "\n total affected rows: `%lld` (expected: `%lld`)",
            affected_rows, exp_count);

  DIE_UNLESS(affected_rows == exp_count);
  mysql_stmt_close(stmt);
}


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
  DBUG_ENTER("fill_tables");
  int rc;
  const char **query;
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

my_bool fetch_n(const char **query_list, unsigned query_count)
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
                  "error message: %s", fetch - fetch_array, fetch->query,
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


/* Test simple prepares of all DML statements */

static void test_prepare_simple()
{
  MYSQL_STMT *stmt;
  int        rc;

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
  strmov(query, "UPDATE test_prepare_simple SET id=? WHERE id=? AND name= ?");
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
  strmov(query, "SELECT * FROM test_prepare_simple WHERE id=? AND name= ?");
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

  myheader("test_prepare_field_result");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prepare_field_result");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_prepare_field_result(int_c int, "
                         "var_c varchar(50), ts_c timestamp(14), "
                         "char_c char(3), date_c date, extra tinyint)");
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
  verify_prepare_field(result, 4, "char_c", "char_c", MYSQL_TYPE_STRING,
                       "t1", "test_prepare_field_result", current_db, 3, 0);

  verify_field_count(result, 5);
  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/* Test simple prepare field results */

static void test_prepare_syntax()
{
  MYSQL_STMT *stmt;
  int        rc;

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
  MYSQL_BIND bind[7];

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
      fprintf(stdout, "\n\t big    : %lld (%lu)", big_data, length[4]);

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

    DIE_UNLESS(double_data == o_double_data);
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

  strmov(query, "SELECT * FROM test_select WHERE id= ? AND name=?");
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
  myheader("test_ps_conj_select");

  rc= mysql_query(mysql, "drop table if exists t1");
  myquery(rc);

  rc= mysql_query(mysql, "create table t1 (id1 int(11) NOT NULL default '0', "
                         "value2 varchar(100), value1 varchar(100))");
  myquery(rc);

  rc= mysql_query(mysql, "insert into t1 values (1, 'hh', 'hh'), "
                          "(2, 'hh', 'hh'), (1, 'ii', 'ii'), (2, 'ii', 'ii')");
  myquery(rc);

  strmov(query, "select id1, value1 from t1 where id1= ? or value1= ?");
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
  strcpy(str_data, "hh");
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
                         "(\"abq\", 1, 2, 3, 2003-08-30), "
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

  strmov(query, "SELECT * FROM test_select WHERE session_id= ?");
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
  rc= mysql_stmt_bind_result(stmt, bind);
  data[16]= 0;

  rc= mysql_stmt_fetch(stmt);
  DIE_UNLESS(rc == 0);
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
  strmov(query, "DELETE FROM test_simple_delete WHERE col1= ? AND col2= ? AND col3= 100");
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
  ulong      length, length1;
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

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *) &nData;      /* integer data */
  bind[0].is_null= &is_null[0];
  bind[0].length= 0;

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
    fprintf(stdout, "\n data (big)    : %lld", b_data);

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

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *) t_data;
  bind[0].buffer_length= sizeof(t_data);

  bind[1].buffer_type= MYSQL_TYPE_FLOAT;
  bind[1].buffer= (void *)&s_data;
  bind[1].buffer_length= 0;

  bind[2].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer= (void *)&i_data;
  bind[2].buffer_length= 0;

  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (void *)&b_data;
  bind[3].buffer_length= 0;

  bind[4].buffer_type= MYSQL_TYPE_LONG;
  bind[4].buffer= (void *)&f_data;
  bind[4].buffer_length= 0;

  bind[5].buffer_type= MYSQL_TYPE_STRING;
  bind[5].buffer= (void *)d_data;
  bind[5].buffer_length= sizeof(d_data);

  bind[6].buffer_type= MYSQL_TYPE_LONG;
  bind[6].buffer= (void *)&bData;
  bind[6].buffer_length= 0;

  bind[7].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[7].buffer= (void *)&szData;
  bind[7].buffer_length= 0;

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
  check_execute(stmt, rc);

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
  myheader("test_prepare_ext");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_prepare_ext");
  myquery(rc);

  sql= (char *)"CREATE TABLE test_prepare_ext\
                (\
                 c1  tinyint, \
                 c2  smallint, \
                 c3  mediumint, \
                 c4  int, \
                 c5  integer, \
                 c6  bigint, \
                 c7  float, \
                 c8  double, \
                 c9  double precision, \
                 c10 real, \
                 c11 decimal(7, 4), \
                 c12 numeric(8, 4), \
                 c13 date, \
                 c14 datetime, \
                 c15 timestamp(14), \
                 c16 time, \
                 c17 year, \
                 c18 bit, \
                 c19 bool, \
                 c20 char, \
                 c21 char(10), \
                 c22 varchar(30), \
                 c23 tinyblob, \
                 c24 tinytext, \
                 c25 blob, \
                 c26 text, \
                 c27 mediumblob, \
                 c28 mediumtext, \
                 c29 longblob, \
                 c30 longtext, \
                 c31 enum('one', 'two', 'three'), \
                 c32 set('monday', 'tuesday', 'wednesday'))";

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
  get_bind[1].is_null= 0;
  get_bind[1].length= 0;

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
                                     "WHERE id= ? AND name=?");
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
    fprintf(stdout, "\n total affected rows: %lld", affected_rows);
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
    fprintf(stdout, "\n total affected rows: %lld", affected_rows);
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
  long        length;
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
  length= 0;
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
      fprintf(stdout, "OK, %lld row(s) affected, %d warning(s)\n",
              mysql_affected_rows(mysql_local),
              mysql_warning_count(mysql_local));

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

  rc= mysql_stmt_bind_result(stmt, bind);
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
    check_execute(stmt, rc);

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
#ifdef NOT_USED
      /*
        minute causes problems from date<->time, don't assert, instead
        validate separatly in another routine
      */
      DIE_UNLESS(tm[i].minute == 0 || tm[i].minute == minute+count);
      DIE_UNLESS(tm[i].second == 0 || tm[i].second == sec+count);
#endif
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
  check_execute(stmt, rc);

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

  bzero(buffer, 20);            /* Avoid overruns in printf() */

  bind[0].length= &length;
  bind[0].is_null= &is_null;
  bind[0].buffer_length= 1;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)buffer;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  buffer[1]= 'X';
  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
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
  check_execute(stmt, rc);
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

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)str[0];
  bind[0].is_null= 0;
  bind[0].length= 0;
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

  myheader("test_ushort_bug");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_ushort");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_ushort(a smallint unsigned, \
                                                  b smallint unsigned, \
                                                  c smallint unsigned, \
                                                  d smallint unsigned)");
  myquery(rc);

  rc= mysql_query(mysql, "INSERT INTO test_ushort VALUES(35999, 35999, 35999, 200)");
  myquery(rc);


  stmt= mysql_simple_prepare(mysql, "SELECT * FROM test_ushort");
  check_stmt(stmt);

  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (void *)&short_value;
  bind[0].is_null= 0;
  bind[0].length= &s_length;

  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (void *)&long_value;
  bind[1].is_null= 0;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (void *)&longlong_value;
  bind[2].is_null= 0;
  bind[2].length= &ll_length;

  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (void *)&tiny_value;
  bind[3].is_null= 0;
  bind[3].length= &t_length;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n ushort   : %d (%ld)", short_value, s_length);
    fprintf(stdout, "\n ulong    : %lu (%ld)", (ulong) long_value, l_length);
    fprintf(stdout, "\n longlong : %lld (%ld)", longlong_value, ll_length);
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

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (void *)&short_value;
  bind[0].is_null= 0;
  bind[0].length= &s_length;

  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (void *)&long_value;
  bind[1].is_null= 0;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (void *)&longlong_value;
  bind[2].is_null= 0;
  bind[2].length= &ll_length;

  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (void *)&tiny_value;
  bind[3].is_null= 0;
  bind[3].length= &t_length;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n sshort   : %d (%ld)", short_value, s_length);
    fprintf(stdout, "\n slong    : %ld (%ld)", (long) long_value, l_length);
    fprintf(stdout, "\n longlong : %lld (%ld)", longlong_value, ll_length);
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

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (void *)&short_value;
  bind[0].is_null= 0;
  bind[0].length= &s_length;

  bind[1].buffer_type= MYSQL_TYPE_LONG;
  bind[1].buffer= (void *)&long_value;
  bind[1].is_null= 0;
  bind[1].length= &l_length;

  bind[2].buffer_type= MYSQL_TYPE_LONGLONG;
  bind[2].buffer= (void *)&longlong_value;
  bind[2].is_null= 0;
  bind[2].length= &ll_length;

  bind[3].buffer_type= MYSQL_TYPE_TINY;
  bind[3].buffer= (void *)&tiny_value;
  bind[3].is_null= 0;
  bind[3].length= &t_length;

  rc= mysql_stmt_bind_result(stmt, bind);
  check_execute(stmt, rc);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  if (!opt_silent)
  {
    fprintf(stdout, "\n sshort   : %d (%ld)", short_value, s_length);
    fprintf(stdout, "\n slong    : %ld (%ld)", (long) long_value, l_length);
    fprintf(stdout, "\n longlong : %lld  (%ld)", longlong_value, ll_length);
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

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= table_type;
  bind[0].length= &type_length;
  bind[0].is_null= 0;
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
                       MYSQL_TYPE_STRING,   /* field type */
                       "", "",              /* table and its org name */
                       "", type_length*3, 0);   /* db name, length */

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
                       "@@max_allowed_packet", "",   /* field and its org name */
                       MYSQL_TYPE_LONGLONG, /* field type */
                       "", "",              /* table and its org name */
                       "", 10, 0);            /* db name, length */

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

static void test_prepare_grant()
{
  int rc;

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

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= data_dir;
  bind[0].buffer_length= FN_REFLEN;
  bind[0].is_null= 0;
  bind[0].length= 0;
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

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (void *)data;
  bind[0].buffer_length= 25;
  bind[0].is_null= &is_null;

  is_null= 0;
  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  strcpy(data, "8.0");
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

  strcpy(data, "5.61");
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

  strcpy(data, "10.22"); is_null= 0;
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

  verify_prepare_field(result, 0, "Field", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", NAME_LEN, 0);

  verify_prepare_field(result, 1, "Type", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", 40, 0);

  verify_prepare_field(result, 2, "Null", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", 1, 0);

  verify_prepare_field(result, 3, "Key", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", 3, 0);

  verify_prepare_field(result, 4, "Default", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", NAME_LEN, 0);

  verify_prepare_field(result, 5, "Extra", "", MYSQL_TYPE_VAR_STRING,
                       "", "", "", 20, 0);

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

  verify_prepare_field(result, 6, "key_len", "",
                       (mysql_get_server_version(mysql) <= 50000 ?
                        MYSQL_TYPE_LONGLONG : MYSQL_TYPE_VAR_STRING),
                       "", "", "",
                       (mysql_get_server_version(mysql) <= 50000 ? 3 : 4096),
                       0);

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

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&c1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
  bind[0].length= 0;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (void *)c2;
  bind[1].buffer_length= sizeof(c2);
  bind[1].is_null= 0;
  bind[1].length= 0;

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

  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (void *)&bc1;
  bind[0].buffer_length= 0;
  bind[0].is_null= 0;
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

  myheader("test_sqlmode");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS test_piping");
  myquery(rc);

  rc= mysql_query(mysql, "CREATE TABLE test_piping(name varchar(10))");
  myquery(rc);

  /* PIPES_AS_CONCAT */
  strcpy(query, "SET SQL_MODE= \"PIPES_AS_CONCAT\"");
  if (!opt_silent)
    fprintf(stdout, "\n With %s", query);
  rc= mysql_query(mysql, query);
  myquery(rc);

  strcpy(query, "INSERT INTO test_piping VALUES(?||?)");
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

  strcpy(c1, "My"); strcpy(c2, "SQL");
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);
  verify_col_data("test_piping", "name", "MySQL");

  rc= mysql_query(mysql, "DELETE FROM test_piping");
  myquery(rc);

  strcpy(query, "SELECT connection_id    ()");
  if (!opt_silent)
    fprintf(stdout, "\n  query: %s", query);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt_r(stmt);

  /* ANSI */
  strcpy(query, "SET SQL_MODE= \"ANSI\"");
  if (!opt_silent)
    fprintf(stdout, "\n With %s", query);
  rc= mysql_query(mysql, query);
  myquery(rc);

  strcpy(query, "INSERT INTO test_piping VALUES(?||?)");
  if (!opt_silent)
    fprintf(stdout, "\n  query: %s", query);
  stmt= mysql_simple_prepare(mysql, query);
  check_stmt(stmt);
  if (!opt_silent)
    fprintf(stdout, "\n  total parameters: %ld", mysql_stmt_param_count(stmt));

  rc= mysql_stmt_bind_param(stmt, bind);
  check_execute(stmt, rc);

  strcpy(c1, "My"); strcpy(c2, "SQL");
  rc= mysql_stmt_execute(stmt);
  check_execute(stmt, rc);

  mysql_stmt_close(stmt);
  verify_col_data("test_piping", "name", "MySQL");

  /* ANSI mode spaces ... */
  strcpy(query, "SELECT connection_id    ()");
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
  strcpy(query, "SET SQL_MODE= \"IGNORE_SPACE\"");
  if (!opt_silent)
    fprintf(stdout, "\n With %s", query);
  rc= mysql_query(mysql, query);
  myquery(rc);

  strcpy(query, "SELECT connection_id    ()");
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

  /*
    FIXME If we comment out next string server will crash too :(
    This is another manifestation of bug #1663
  */
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

  myheader("test_subquery");

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

  myheader("test_subquery");

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
  DIE_UNLESS(rc == MYSQL_NO_DATA);

  /* This too should not hang but should not bark */
  rc= mysql_stmt_store_result(stmt);
  check_execute(stmt, rc);

  /* This should return proper error */
  rc= mysql_stmt_fetch(stmt);
  check_execute_r(stmt, rc);
  DIE_UNLESS(rc == MYSQL_NO_DATA);

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
      fprintf(stdout, "droped %i\n", i);

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
      fprintf(stdout, "droped %i\n", i);
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
  MYSQL_BIND bind_array[12];
  int8 int8_val;
  uint8 uint8_val;
  int16 int16_val;
  uint16 uint16_val;
  int32 int32_val;
  uint32 uint32_val;
  longlong int64_val;
  ulonglong uint64_val;
  double double_val, udouble_val;
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

  bzero(bind_array, sizeof(bind_array));

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
  DIE_UNLESS(udouble_val == ulonglong2double(uint64_val));
  DIE_UNLESS(!strcmp(longlong_as_string, "0"));
  DIE_UNLESS(!strcmp(ulonglong_as_string, "0"));

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);

  DIE_UNLESS(int8_val == int8_max);
  DIE_UNLESS(uint8_val == uint8_max);
  DIE_UNLESS(int16_val == int16_max);
  DIE_UNLESS(uint16_val == uint16_max);
  DIE_UNLESS(int32_val == int32_max);
  DIE_UNLESS(uint32_val == uint32_max);
  DIE_UNLESS(int64_val == int64_max);
  DIE_UNLESS(uint64_val == uint64_max);
  DIE_UNLESS(double_val == (longlong) uint64_val);
  DIE_UNLESS(udouble_val == ulonglong2double(uint64_val));
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

    bzero(&bind, sizeof(bind));

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

  strcpy(my_val, "abc");

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
  bind[0].buffer=         my_val;
  bind[0].buffer_length=  4;
  bind[0].length=         &my_length;
  bind[0].is_null=        (char*)&my_null;
  bind[1].buffer_type=    MYSQL_TYPE_STRING;
  bind[1].buffer=         my_val;
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

  bzero(bind_array, sizeof(bind_array));

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
  bzero(bind, sizeof(bind));

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
  strcpy(canonical_buff, concat_arg0);
  strcat(canonical_buff, "ONE");
  DIE_UNLESS(strlen(canonical_buff) == out_length &&
         strncmp(out_buff, canonical_buff, out_length) == 0);

  rc= mysql_stmt_fetch(stmt);
  check_execute(stmt, rc);
  strcpy(canonical_buff + strlen(concat_arg0), "TWO");
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
  bzero(bind, sizeof(bind));
  bzero(&time_in, sizeof(time_in));
  bzero(&time_out, sizeof(time_out));
  bzero(&datetime_in, sizeof(datetime_in));
  bzero(&datetime_out, sizeof(datetime_out));

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
  bzero(bind, sizeof(bind));

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
  bzero(bind, sizeof(bind));
  bzero(&time_canonical, sizeof(time_canonical));
  bzero(&time_out, sizeof(time_out));
  bzero(&date_canonical, sizeof(date_canonical));
  bzero(&date_out, sizeof(date_out));
  bzero(&datetime_canonical, sizeof(datetime_canonical));
  bzero(&datetime_out, sizeof(datetime_out));

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
    "SELECT COUNT(*) FROM v1 WHERE `SERVERNAME`=?";

  myheader("test_view");

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS t1,t2,t3,v1");
  myquery(rc);

  rc = mysql_query(mysql, "DROP VIEW IF EXISTS v1,t1,t2,t3");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE TABLE `t1` ( `SERVERGRP` varchar(20) character set latin1 collate latin1_bin NOT NULL default '', `DBINSTANCE` varchar(20) character set latin1 collate latin1_bin NOT NULL default '', PRIMARY KEY  (`SERVERGRP`)) ENGINE=InnoDB DEFAULT CHARSET=latin1");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE TABLE `t2` ( `SERVERNAME` varchar(20) character set latin1 collate latin1_bin NOT NULL default '', `SERVERGRP` varchar(20) character set latin1 collate latin1_bin NOT NULL default '', PRIMARY KEY  (`SERVERNAME`)) ENGINE=InnoDB DEFAULT CHARSET=latin1;");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE TABLE `t3` ( `SERVERGRP` varchar(20) character set latin1 collate latin1_bin NOT NULL default '', `TABNAME` varchar(30) character set latin1 collate latin1_bin NOT NULL default '', `MAPSTATE` char(1) character set latin1 collate latin1_bin NOT NULL default '', `ACTSTATE` char(1) character set latin1 collate latin1_bin NOT NULL default '', `LOCAL_NAME` varchar(30) character set latin1 collate latin1_bin NOT NULL default '', `CHG_DATE` varchar(8) character set latin1 collate latin1_bin NOT NULL default '00000000', `CHG_TIME` varchar(6) character set latin1 collate latin1_bin NOT NULL default '000000', `MXUSER` varchar(12) character set latin1 collate latin1_bin NOT NULL default '', PRIMARY KEY  (`SERVERGRP`,`TABNAME`,`MAPSTATE`,`ACTSTATE`,`LOCAL_NAME`)) ENGINE=InnoDB DEFAULT CHARSET=latin1;");
  myquery(rc);
  rc= mysql_query(mysql,"CREATE VIEW v1 AS select sql_no_cache T0001.SERVERNAME AS `SERVERNAME`,T0003.TABNAME AS `TABNAME`,T0003.LOCAL_NAME AS `LOCAL_NAME`,T0002.DBINSTANCE AS `DBINSTANCE` from t2 T0001 join t1 T0002 join t3 T0003 where ((T0002.SERVERGRP = T0001.SERVERGRP) and (T0002.SERVERGRP = T0003.SERVERGRP) and (T0003.MAPSTATE = _latin1'A') and (T0003.ACTSTATE = _latin1' '))");
  myquery(rc);

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, query, strlen(query));
  check_execute(stmt, rc);

  strcpy(str_data, "TEST");
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
  const char *query= "SELECT `RELID` ,`REPORT` ,`HANDLE` ,`LOG_GROUP` ,`USERNAME` ,`VARIANT` ,`TYPE` ,`VERSION` ,`ERFDAT` ,`ERFTIME` ,`ERFNAME` ,`AEDAT` ,`AETIME` ,`AENAME` ,`DEPENDVARS` ,`INACTIVE` FROM `V_LTDX` WHERE `MANDT` = ? AND `RELID` = ? AND `REPORT` = ? AND `HANDLE` = ? AND `LOG_GROUP` = ? AND `USERNAME` IN ( ? , ? ) AND `TYPE` = ?";

  myheader("test_view_2where");

  rc= mysql_query(mysql, "DROP TABLE IF EXISTS LTDX");
  myquery(rc);
  rc= mysql_query(mysql, "DROP VIEW IF EXISTS V_LTDX");
  myquery(rc);
  rc= mysql_query(mysql, "CREATE TABLE `LTDX` ( `MANDT` char(3) character set latin1 collate latin1_bin NOT NULL default '000', `RELID` char(2) character set latin1 collate latin1_bin NOT NULL default '', `REPORT` varchar(40) character set latin1 collate latin1_bin NOT NULL default '', `HANDLE` varchar(4) character set latin1 collate latin1_bin NOT NULL default '', `LOG_GROUP` varchar(4) character set latin1 collate latin1_bin NOT NULL default '', `USERNAME` varchar(12) character set latin1 collate latin1_bin NOT NULL default '', `VARIANT` varchar(12) character set latin1 collate latin1_bin NOT NULL default '', `TYPE` char(1) character set latin1 collate latin1_bin NOT NULL default '', `SRTF2` int(11) NOT NULL default '0', `VERSION` varchar(6) character set latin1 collate latin1_bin NOT NULL default '000000', `ERFDAT` varchar(8) character set latin1 collate latin1_bin NOT NULL default '00000000', `ERFTIME` varchar(6) character set latin1 collate latin1_bin NOT NULL default '000000', `ERFNAME` varchar(12) character set latin1 collate latin1_bin NOT NULL default '', `AEDAT` varchar(8) character set latin1 collate latin1_bin NOT NULL default '00000000', `AETIME` varchar(6) character set latin1 collate latin1_bin NOT NULL default '000000', `AENAME` varchar(12) character set latin1 collate latin1_bin NOT NULL default '', `DEPENDVARS` varchar(10) character set latin1 collate latin1_bin NOT NULL default '', `INACTIVE` char(1) character set latin1 collate latin1_bin NOT NULL default '', `CLUSTR` smallint(6) NOT NULL default '0', `CLUSTD` blob, PRIMARY KEY  (`MANDT`,`RELID`,`REPORT`,`HANDLE`,`LOG_GROUP`,`USERNAME`,`VARIANT`,`TYPE`,`SRTF2`)) ENGINE=InnoDB DEFAULT CHARSET=latin1");
  myquery(rc);
  rc= mysql_query(mysql, "CREATE VIEW V_LTDX AS select T0001.MANDT AS `MANDT`,T0001.RELID AS `RELID`,T0001.REPORT AS `REPORT`,T0001.HANDLE AS `HANDLE`,T0001.LOG_GROUP AS `LOG_GROUP`,T0001.USERNAME AS `USERNAME`,T0001.VARIANT AS `VARIANT`,T0001.TYPE AS `TYPE`,T0001.VERSION AS `VERSION`,T0001.ERFDAT AS `ERFDAT`,T0001.ERFTIME AS `ERFTIME`,T0001.ERFNAME AS `ERFNAME`,T0001.AEDAT AS `AEDAT`,T0001.AETIME AS `AETIME`,T0001.AENAME AS `AENAME`,T0001.DEPENDVARS AS `DEPENDVARS`,T0001.INACTIVE AS `INACTIVE` from LTDX T0001 where (T0001.SRTF2 = 0)");
  myquery(rc);
  for (i=0; i < 8; i++) {
    strcpy(parms[i], "1");
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
  long            my_val = 0L;
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
  rc= mysql_query(mysql, "CREATE TABLE t1 ( K1C4 varchar(4) character set latin1 collate latin1_bin NOT NULL default '', K2C4 varchar(4) character set latin1 collate latin1_bin NOT NULL default '', K3C4 varchar(4) character set latin1 collate latin1_bin NOT NULL default '', K4N4 varchar(4) character set latin1 collate latin1_bin NOT NULL default '0000', F1C4 varchar(4) character set latin1 collate latin1_bin NOT NULL default '', F2I4 int(11) NOT NULL default '0', F3N5 varchar(5) character set latin1 collate latin1_bin NOT NULL default '00000', F4I4 int(11) NOT NULL default '0', F5C8 varchar(8) character set latin1 collate latin1_bin NOT NULL default '', F6N4 varchar(4) character set latin1 collate latin1_bin NOT NULL default '0000', F7F8 double NOT NULL default '0', F8F8 double NOT NULL default '0', F9D8 decimal(8,2) NOT NULL default '0.00', PRIMARY KEY  (K1C4,K2C4,K3C4,K4N4)) ENGINE=InnoDB DEFAULT CHARSET=latin1");
  myquery(rc);
  rc= mysql_query(mysql, "CREATE VIEW v1 AS select sql_no_cache K1C4 AS `K1C4`,K2C4 AS `K2C4`,K3C4 AS `K3C4`,K4N4 AS `K4N4`,F1C4 AS `F1C4`,F2I4 AS `F2I4`,F3N5 AS `F3N5`,F7F8 AS `F7F8`,F6N4 AS `F6N4`,F5C8 AS `F5C8`,F9D8 AS `F9D8` from t1 T0001");

  for (i= 0; i < 11; i++)
  {
    l[i]= 20;
    bind[i].buffer_type= MYSQL_TYPE_STRING;
    bind[i].is_null= 0;
    bind[i].buffer= (char *)&parm[i];

    strcpy(parm[i], "1");
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
  bzero(bind, sizeof(bind));

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
  bzero(bind, sizeof(bind));
  bzero(tm, sizeof(tm));

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

  bzero(bind, sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= &no;

  for (stmt= stmt_list; stmt != stmt_list + NUM_OF_USED_STMT; ++stmt)
  {
    sprintf(buff, "select %d", stmt - stmt_list);
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
  bzero(bind, MAX_PARAM_COUNT * sizeof(MYSQL_BIND));
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
             strlen(query), nrows, mysql_stmt_param_count(stmt));

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

  bzero(bind, sizeof(bind));
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

  bzero(bind, sizeof(bind));
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
  int rc;

  myheader("test_bug6059");

  stmt_text= "SELECT 'foo' INTO OUTFILE 'x.3'";

  stmt= mysql_stmt_init(mysql);
  rc= mysql_stmt_prepare(stmt, stmt_text, strlen(stmt_text));
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
  bzero(bind, sizeof(bind));
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
  DBUG_ENTER("test_basic_cursors");
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

  myheader("test_basic_cursors");

  fill_tables(basic_tables, sizeof(basic_tables)/sizeof(*basic_tables));

  fetch_n(queries, sizeof(queries)/sizeof(*queries));
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
  fetch_n(queries, sizeof(queries)/sizeof(*queries));
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
  rc= 1;
  mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (void*)&rc);
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

  bzero(bind, sizeof(bind));
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


static void test_bug4172()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[3];
  const char *stmt_text;
  MYSQL_RES *res;
  MYSQL_ROW row;
  int rc;
  char f[100], d[100], e[100];
  long f_len, d_len, e_len;

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

  bzero(bind, sizeof(bind));
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

  bzero(bind, sizeof(bind));
  bind[0].buffer= buff;
  bind[0].length= &length;
  bind[0].buffer_type= MYSQL_TYPE_STRING;

  mysql_stmt_bind_param(stmt, bind);

  buff[0]= 0xC3;
  buff[1]= 0xA0;
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


/*
  Read and parse arguments and MySQL options from my.cnf
*/

static const char *client_test_load_default_groups[]= { "client", 0 };
static char **defaults_argv;

static struct my_option client_test_long_options[] =
{
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"database", 'D', "Database to use", (char **) &opt_db, (char **) &opt_db,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"debug", '#', "Output debug log", (gptr*) &default_dbug_option,
   (gptr*) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host", (char **) &opt_host, (char **) &opt_host, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user", (char **) &opt_user,
   (char **) &opt_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection", (char **) &opt_port,
   (char **) &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Be more silent", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"socket", 'S', "Socket file to use for connection", (char **) &opt_unix_socket,
   (char **) &opt_unix_socket, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"testcase", 'c', "May disable some code when runs as mysql-test-run testcase.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"count", 't', "Number of times test to be executed", (char **) &opt_count,
   (char **) &opt_count, 0, GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
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
  printf("Usage: %s [OPTIONS]\n", my_progname);
  my_print_help(client_test_long_options);
  print_defaults("my", client_test_load_default_groups);
  my_print_variables(client_test_long_options);
}


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
  case '?':
  case 'I':                                     /* Info */
    usage();
    exit(0);
    break;
  }
  return 0;
}

static void get_options(int argc, char **argv)
{
  int ho_error;

  if ((ho_error= handle_options(&argc, &argv, client_test_long_options,
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
  DEBUGGER_OFF;
  MY_INIT(argv[0]);

  load_defaults("my", client_test_load_default_groups, &argc, &argv);
  defaults_argv= argv;
  get_options(argc, argv);

  client_connect();       /* connect to server */

  total_time= 0;
  for (iter_count= 1; iter_count <= opt_count; iter_count++)
  {
    /* Start of tests */
    test_count= 1;

    start_time= time((time_t *)0);

    client_query();         /* simple client query test */
#if NOT_YET_WORKING
    /* Used for internal new development debugging */
    test_drop_temp();       /* Test DROP TEMPORARY TABLE Access checks */
#endif
    test_fetch_seek();      /* Test stmt seek() functions */
    test_fetch_nobuffs();   /* to fecth without prior bound buffers */
    test_open_direct();     /* direct execution in the middle of open stmts */
    test_fetch_null();      /* to fetch null data */
    test_ps_null_param();   /* Fetch value of null parameter */
    test_fetch_date();      /* to fetch date, time and timestamp */
    test_fetch_str();       /* to fetch string to all types */
    test_fetch_long();      /* to fetch long to all types */
    test_fetch_short();     /* to fetch short to all types */
    test_fetch_tiny();      /* to fetch tiny to all types */
    test_fetch_bigint();    /* to fetch bigint to all types */
    test_fetch_float();     /* to fetch float to all types */
    test_fetch_double();    /* to fetch double to all types */
    test_bind_result_ext(); /* result bind test - extension */
    test_bind_result_ext1(); /* result bind test - extension */
    test_select_direct();   /* direct select - protocol_simple debug */
    test_select_prepare();  /* prepare select - protocol_prep debug */
    test_select();          /* simple select test */
    test_select_version();  /* select with variables */
    test_ps_conj_select();  /* prepare select with "where a=? or b=?" */
    test_select_show_table();/* simple show prepare */
#if NOT_USED
  /*
     Enable this tests from 4.1.1 when mysql_param_result() is
     supported
  */
    test_select_meta();     /* select param meta information */
    test_update_meta();     /* update param meta information */
    test_insert_meta();     /* insert param meta information */
#endif
    test_func_fields();     /* test for new 4.1 MYSQL_FIELD members */
    test_long_data();       /* test for sending text data in chunks */
    test_insert();          /* simple insert test - prepare */
    test_set_variable();    /* prepare with set variables */
    test_select_show();     /* prepare - show test */
    test_prepare_noparam(); /* prepare without parameters */
    test_bind_result();     /* result bind test */
    test_prepare_simple();  /* simple prepare */
    test_prepare();         /* prepare test */
    test_null();            /* test null data handling */
    test_debug_example();   /* some debugging case */
    test_update();          /* prepare-update test */
    test_simple_update();   /* simple prepare with update */
    test_simple_delete();   /* prepare with delete */
    test_double_compare();  /* float comparision */
    client_store_result();  /* usage of mysql_store_result() */
    client_use_result();    /* usage of mysql_use_result() */
    test_tran_bdb();        /* transaction test on BDB table type */
    test_tran_innodb();     /* transaction test on InnoDB table type */
    test_prepare_ext();     /* test prepare with all types
                               conversion -- TODO */
    test_prepare_syntax();  /* syntax check for prepares */
    test_field_names();     /* test for field names */
    test_field_flags();     /* test to help .NET provider team */
    test_long_data_str();   /* long data handling */
    test_long_data_str1();  /* yet another long data handling */
    test_long_data_bin();   /* long binary insertion */
    test_warnings();        /* show warnings test */
    test_errors();          /* show errors test */
    test_prepare_resultset();/* prepare meta info test */
    test_stmt_close();      /* mysql_stmt_close() test -- hangs */
    test_prepare_field_result(); /* prepare meta info */
    test_multi_stmt();      /* multi stmt test */
    test_multi_statements();/* test multi statement execution */
    test_prepare_multi_statements(); /* check that multi statements are
                                       disabled in PS */
    test_store_result();    /* test the store_result */
    test_store_result1();   /* test store result without buffers */
    test_store_result2();   /* test store result for misc case */
    test_subselect();       /* test subselect prepare -TODO*/
    test_date();            /* test the MYSQL_TIME conversion */
    test_date_date();       /* test conversion from DATE to all */
    test_date_time();       /* test conversion from TIME to all */
    test_date_ts()  ;       /* test conversion from TIMESTAMP to all */
    test_date_dt()  ;       /* test conversion from DATETIME to all */
    test_prepare_alter();   /* change table schema in middle of prepare */
    test_manual_sample();   /* sample in the manual */
    test_pure_coverage();   /* keep pure coverage happy */
    test_buffers();         /* misc buffer handling */
    test_ushort_bug();      /* test a simple conv bug from php */
    test_sshort_bug();      /* test a simple conv bug from php */
    test_stiny_bug();       /* test a simple conv bug from php */
    test_field_misc();      /* check the field info for misc case, bug: #74 */
    test_set_option();      /* test the SET OPTION feature, bug #85 */
    /*TODO HF: here should be NO_EMBEDDED_ACCESS_CHECKS*/
#ifndef EMBEDDED_LIBRARY
    test_prepare_grant();   /* Test the GRANT command, bug #89 */
#endif
    test_frm_bug();         /* test the crash when .frm is invalid, bug #93 */
    test_explain_bug();     /* test for the EXPLAIN, bug #115 */
    test_decimal_bug();     /* test for the decimal bug */
    test_nstmts();          /* test n statements */
    test_logs(); ;          /* Test logs */
    test_cuted_rows();      /* Test for WARNINGS from cuted rows */
    test_fetch_offset();    /* Test mysql_stmt_fetch_column with offset */
    test_fetch_column();    /* Test mysql_stmt_fetch_column */
    test_mem_overun();      /* test DBD ovverun bug */
    test_list_fields();     /* test COM_LIST_FIELDS for DEFAULT */
    test_free_result();     /* test mysql_stmt_free_result() */
    test_free_store_result(); /* test to make sure stmt results are cleared
                                 during stmt_free_result() */
    test_sqlmode();         /* test for SQL_MODE */
    test_ts();              /* test for timestamp BR#819 */
    test_bug1115();         /* BUG#1115 */
    test_bug1180();         /* BUG#1180 */
    test_bug1500();         /* BUG#1500 */
    test_bug1644();         /* BUG#1644 */
    test_bug1946();         /* test that placeholders are allowed only in
                               prepared queries */
    test_bug2248();         /* BUG#2248 */
    test_parse_error_and_bad_length(); /* test if bad length param in
                                         mysql_stmt_prepare() triggers error */
    test_bug2247();         /* test that mysql_stmt_affected_rows() returns
                               number of rows affected by last prepared
                               statement execution */
    test_subqueries();      /* repeatable subqueries */
    test_bad_union();       /* correct setup of UNION */
    test_distinct();        /* distinct aggregate functions */
    test_subqueries_ref();  /* outer reference in subqueries converted
                               Item_field -> Item_ref */
    test_union();           /* test union with prepared statements */
    test_bug3117();         /* BUG#3117: LAST_INSERT_ID() */
    test_join();            /* different kinds of join, BUG#2794 */
    test_selecttmp();       /* temporary table used in select execution */
    test_create_drop();     /* some table manipulation BUG#2811 */
    test_rename();          /* rename test */
    test_do_set();          /* DO & SET commands test BUG#3393 */
    test_multi();           /* test of multi delete & update */
    test_insert_select();   /* test INSERT ... SELECT */
    test_bind_nagative();   /* bind negative to unsigned BUG#3223 */
    test_derived();         /* derived table with parameter BUG#3020 */
    test_xjoin();           /* complex join test */
    test_bug3035();         /* inserts of INT32_MAX/UINT32_MAX */
    test_union2();          /* repeatable execution of union (Bug #3577) */
    test_bug1664();         /* test for bugs in mysql_stmt_send_long_data()
                               call (Bug #1664) */
    test_union_param();
    test_order_param();     /* ORDER BY with parameters in select list
                               (Bug #3686 */
    test_ps_i18n();         /* test for i18n support in binary protocol */
    test_bug3796();         /* test for select concat(?, <string>) */
    test_bug4026();         /* test microseconds precision of time types */
    test_bug4079();         /* erroneous subquery in prepared statement */
    test_bug4236();         /* init -> execute */
    test_bug4030();         /* test conversion string -> time types in
                               libmysql */
    test_bug5126();         /* support for mediumint type in libmysql */
    test_bug4231();         /* proper handling of all-zero times and
                               dates in the server */
    test_bug5399();         /* check that statement id uniquely identifies
                               statement */
    test_bug5194();         /* bulk inserts in prepared mode */
    test_bug5315();         /* check that mysql_change_user closes all
                               prepared statements */
    test_bug6049();         /* check support for negative TIME values */
    test_bug6058();         /* check support for 0000-00-00 dates */
    test_bug6059();         /* correct metadata for SELECT ... INTO OUTFILE */
    test_bug6046();         /* NATURAL JOIN transformation works in PS */
    test_bug6081();         /* test of mysql_create_db()/mysql_rm_db() */
    test_bug6096();         /* max_length for numeric columns */
    test_bug4172();         /* floating point conversions in libmysql */

    test_conversion();      /* placeholder value is not converted to
                               character set of column if character set
                               of connection equals to character set of
                               client */
    test_view();            /* Test of VIEWS with prepared statements */
    test_view_where();      /* VIEW with WHERE clause & merge algorithm */
    test_view_2where();     /* VIEW with WHERE * SELECt with WHERE */
    test_view_star();       /* using query with * from VIEW */
    test_view_insert();     /* inserting in VIEW without field list */
    test_left_join_view();  /* left join on VIEW with WHERE condition */
    test_view_insert_fields(); /* insert into VIOEW with fields list */
    test_basic_cursors();
    test_cursors_with_union();
    /*
      XXX: PLEASE RUN THIS PROGRAM UNDER VALGRIND AND VERIFY THAT YOUR TEST
      DOESN'T CONTAIN WARNINGS/ERRORS BEFORE YOU PUSH.
    */

    end_time= time((time_t *)0);
    total_time+= difftime(end_time, start_time);

    /* End of tests */
  }

  client_disconnect();    /* disconnect from server */
  free_defaults(defaults_argv);
  print_test_output();
  my_end(0);

  exit(0);
}
