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

/***************************************************************************
 This is a test sample to test the new features in MySQL client-server
 protocol                                                               

 Main author: venu ( venu@mysql.com )

 NOTES:
  - To be able to test which fields are used, we are not clearing
    the MYSQL_BIND with bzero() but instead just clearing the fields that
    are used by the API.

***************************************************************************/

#include <my_global.h>
#include <my_sys.h>
#include <mysql.h>
#include <my_getopt.h>
#include <m_string.h>
#include <assert.h>

/* set default options */
static char *opt_db=0;
static char *opt_user=0;
static char *opt_password=0;
static char *opt_host=0;
static char *opt_unix_socket=0;
static unsigned int  opt_port;
static my_bool tty_password=0;

static MYSQL *mysql=0;
static char query[255]; 
static char current_db[]= "client_test_db";
static unsigned int test_count= 0;
static unsigned int opt_count= 0;
static unsigned int iter_count= 0;

static time_t start_time, end_time;
static double total_time;

#define myheader(str) \
{ \
  fprintf(stdout,"\n\n#####################################\n"); \
  fprintf(stdout,"%d of (%d/%d): %s",test_count++, iter_count,\
                                     opt_count, str); \
  fprintf(stdout,"  \n#####################################\n"); \
}
#define myheader_r(str) \
{ \
  fprintf(stdout,"\n\n#####################################\n"); \
  fprintf(stdout,"%s", str); \
  fprintf(stdout,"  \n#####################################\n"); \
}

/* 
  TODO: un comment this in 4.1.1 when it supports mysql_param_result 
  and also enable all meta_param_result tests 
*/
#ifndef mysql_param_result
#define mysql_param_result mysql_prepare_result
#endif

static void print_error(const char *msg)
{  
  if (mysql)
  {
    if (mysql->server_version)
      fprintf(stdout,"\n [MySQL-%s]",mysql->server_version);
    else
      fprintf(stdout,"\n [MySQL]");
    fprintf(stdout,"[%d] %s\n",mysql_errno(mysql),mysql_error(mysql));
  }
  else if (msg) fprintf(stderr, " [MySQL] %s\n", msg);
}

static void print_st_error(MYSQL_STMT *stmt, const char *msg)
{  
  if (stmt)
  {
    if (stmt->mysql && stmt->mysql->server_version)
      fprintf(stdout,"\n [MySQL-%s]",stmt->mysql->server_version);
    else
      fprintf(stdout,"\n [MySQL]");

    fprintf(stdout,"[%d] %s\n",mysql_stmt_errno(stmt),
                     mysql_stmt_error(stmt));
  }
	else if (msg) fprintf(stderr, " [MySQL] %s\n", msg);
}
static void client_disconnect();

#define myerror(msg) print_error(msg)
#define mysterror(stmt, msg) print_st_error(stmt, msg)

#define myassert(exp) assert(exp)
#define myassert_r(exp) assert(!(exp))

#define myquery(r) \
{ \
if (r) \
  myerror(NULL); \
 myassert(r == 0); \
}

#define myquery_r(r) \
{ \
if (r) \
  myerror(NULL); \
myassert_r(r == 0); \
}

#define mystmt(stmt,r) \
{ \
if (r) \
  mysterror(stmt,NULL); \
myassert(r == 0);\
}

#define mystmt_r(stmt,r) \
{ \
if (r) \
  mysterror(stmt,NULL); \
myassert_r(r == 0);\
}

#define mystmt_init(stmt) \
{ \
if ( stmt == 0) \
  myerror(NULL); \
myassert(stmt != 0); \
}

#define mystmt_init_r(stmt) \
{ \
myassert(stmt == 0);\
} 

#define mytest(x) if (!x) {myerror(NULL);myassert(TRUE);}
#define mytest_r(x) if (x) {myerror(NULL);myassert(TRUE);}

#define PREPARE(A,B) mysql_prepare(A,B,strlen(B))

/********************************************************
* connect to the server                                 *
*********************************************************/
static void client_connect()
{
  int  rc;
  char buff[255];
  myheader_r("client_connect");  

  fprintf(stdout, "\n Establishig a connection ...");
  if (!(mysql = mysql_init(NULL)))
  { 
	  myerror("mysql_init() failed");
    exit(0);
  }
  if (!(mysql_real_connect(mysql,opt_host,opt_user,
			   opt_password, opt_db ? opt_db:"test", opt_port,
			   opt_unix_socket, 0)))
  {
    myerror("connection failed");    
    mysql_close(mysql);
    fprintf(stdout,"\n Check the connection options using --help or -?\n");
    exit(0);
  }    
  fprintf(stdout," OK");

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(mysql, TRUE);
  fprintf(stdout, "\n Creating a test database '%s' ...", current_db);
  strxmov(buff,"CREATE DATABASE IF NOT EXISTS ", current_db, NullS);
  rc = mysql_query(mysql, buff);
  myquery(rc);
  strxmov(buff,"USE ", current_db, NullS);
  rc = mysql_query(mysql, buff);
  myquery(rc);
  
  fprintf(stdout," OK");
}

/********************************************************
* close the connection                                  *
*********************************************************/
static void client_disconnect()
{  
  myheader_r("client_disconnect");  

  if (mysql)
  {
    char buff[255];
    fprintf(stdout, "\n droping the test database '%s' ...", current_db);
    strxmov(buff,"DROP DATABASE IF EXISTS ", current_db, NullS);
    mysql_query(mysql, buff);
    fprintf(stdout, " OK");
    fprintf(stdout, "\n closing the connection ...");
    mysql_close(mysql);
    fprintf(stdout, " OK\n");
  }
}

/********************************************************
* query processing                                      *
*********************************************************/
static void client_query()
{
  int rc;

  myheader("client_query");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS myclient_test");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE myclient_test(id int primary key auto_increment,\
                                              name varchar(20))");
  myquery(rc);
  
  rc = mysql_query(mysql,"CREATE TABLE myclient_test(id int, name varchar(20))");
  myquery_r(rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('mysql')");
  myquery(rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('monty')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('venu')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('deleted')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('deleted')");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE myclient_test SET name='updated' WHERE name='deleted'");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE myclient_test SET id=3 WHERE name='updated'");
  myquery_r(rc);
}

/********************************************************
* print dashes                                          *
*********************************************************/
static void my_print_dashes(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i,j;

  mysql_field_seek(result,0);
  fputc('\t',stdout);
  fputc('+', stdout);

  for(i=0; i< mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    for(j=0; j < field->max_length+2; j++)
      fputc('-',stdout);
    fputc('+',stdout);
  }
  fputc('\n',stdout);
}

/********************************************************
* print resultset metadata information                  *
*********************************************************/
static void my_print_result_metadata(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i,j;
  unsigned int field_count;

  mysql_field_seek(result,0);
  fputc('\n', stdout);
  fputc('\n', stdout);

  field_count = mysql_num_fields(result);
  for(i=0; i< field_count; i++)
  {
    field = mysql_fetch_field(result);
    j = strlen(field->name);
    if (j < field->max_length)
      j = field->max_length;
    if (j < 4 && !IS_NOT_NULL(field->flags))
      j = 4;
    field->max_length = j;
  }
  my_print_dashes(result);
  fputc('\t',stdout);
  fputc('|', stdout);

  mysql_field_seek(result,0);
  for(i=0; i< field_count; i++)
  {
    field = mysql_fetch_field(result);
    fprintf(stdout, " %-*s |",(int) field->max_length, field->name);
  }
  fputc('\n', stdout);
  my_print_dashes(result);
}

/********************************************************
* process the result set                                *
*********************************************************/
int my_process_result_set(MYSQL_RES *result)
{
  MYSQL_ROW    row;
  MYSQL_FIELD  *field;
  unsigned int i;
  unsigned int row_count=0;
  
  if (!result)
    return 0;

  my_print_result_metadata(result);

  while ((row = mysql_fetch_row(result)) != NULL)
  {
    mysql_field_seek(result,0);
    fputc('\t',stdout);
    fputc('|',stdout);

    for(i=0; i< mysql_num_fields(result); i++)
    {
      field = mysql_fetch_field(result);
      if (row[i] == NULL)
        fprintf(stdout, " %-*s |", (int) field->max_length, "NULL");
      else if (IS_NUM(field->type))
        fprintf(stdout, " %*s |", (int) field->max_length, row[i]);
      else
        fprintf(stdout, " %-*s |", (int) field->max_length, row[i]);
    }
    fputc('\t',stdout);
    fputc('\n',stdout);
    row_count++;
  }
  my_print_dashes(result);

  if (mysql_errno(mysql) != 0)
    fprintf(stderr, "\n\tmysql_fetch_row() failed\n");
  else
    fprintf(stdout,"\n\t%d %s returned\n", row_count, 
                   row_count == 1 ? "row" : "rows");
  return row_count;
}

/********************************************************
* process the stmt result set                           *
*********************************************************/
uint my_process_stmt_result(MYSQL_STMT *stmt)
{
  int         field_count;
  uint        row_count= 0;
  MYSQL_BIND  buffer[50];
  MYSQL_FIELD *field;
  MYSQL_RES   *result;
  char        data[50][255];
  ulong       length[50];
  my_bool     is_null[50];
  int         rc, i;

  if (!(result= mysql_prepare_result(stmt)))
  {
    while (!mysql_fetch(stmt)) 
      row_count++;
    return row_count;
  }
  
  field_count= mysql_num_fields(result);
  for(i=0; i < field_count; i++)
  {
    buffer[i].buffer_type= MYSQL_TYPE_STRING;
    buffer[i].buffer_length=50;
    buffer[i].length=&length[i];
    buffer[i].buffer=(char*) data[i];
    buffer[i].is_null= &is_null[i];
  }

  my_print_result_metadata(result);

  rc= mysql_bind_result(stmt,buffer);
  mystmt(stmt,rc);

  rc= mysql_stmt_store_result(stmt);
  mystmt(stmt,rc);

  mysql_field_seek(result, 0);  
  while (mysql_fetch(stmt) == 0)
  {    
    fputc('\t',stdout);
    fputc('|',stdout);
    
    mysql_field_seek(result,0);
    for (i=0; i < field_count; i++)
    {
      field = mysql_fetch_field(result);
      if (is_null[i])
        fprintf(stdout, " %-*s |", (int) field->max_length, "NULL");
      else if (length[i] == 0)
      {
        data[i][0]='\0';  /* unmodified buffer */
        fprintf(stdout, " %*s |", (int) field->max_length, data[i]);
      }
      else if (IS_NUM(field->type))
        fprintf(stdout, " %*s |", (int) field->max_length, data[i]);
      else
        fprintf(stdout, " %-*s |", (int) field->max_length, data[i]);
    }
    fputc('\t',stdout);
    fputc('\n',stdout);
    row_count++;
  }
  my_print_dashes(result);
  fprintf(stdout,"\n\t%d %s returned\n", row_count, 
                 row_count == 1 ? "row" : "rows");
  mysql_free_result(result);
  return row_count;
}

/********************************************************
* process the stmt result set                           *
*********************************************************/
uint my_stmt_result(const char *query, unsigned long length)
{
  MYSQL_STMT *stmt;
  uint       row_count;
  int        rc;

  fprintf(stdout,"\n\n %s", query);
  stmt= mysql_prepare(mysql,query,length);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt,rc);

  row_count= my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
    
  return row_count;
}

/*
  Utility function to verify a particular column data
*/
static void verify_col_data(const char *table, const char *col, 
                            const char *exp_data)
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  char      query[255]; 
  int       rc, field= 1;
 
  if (table && col)
  {
    strxmov(query,"SELECT ",col," FROM ",table," LIMIT 1", NullS);
    fprintf(stdout,"\n %s", query);
    rc = mysql_query(mysql, query);
    myquery(rc);

    field= 0;
  }

  result = mysql_use_result(mysql);
  mytest(result);

  if (!(row= mysql_fetch_row(result)) || !row[field])
  {
    fprintf(stdout,"\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  fprintf(stdout,"\n obtained: `%s` (expected: `%s`)", 
    row[field], exp_data);
  myassert(strcmp(row[field],exp_data) == 0);
  mysql_free_result(result);
}

/*
  Utility function to verify the field members
*/

static void verify_prepare_field(MYSQL_RES *result, 
      unsigned int no,const char *name, const char *org_name, 
      enum enum_field_types type, const char *table, 
      const char *org_table, const char *db)
{
  MYSQL_FIELD *field;

  if (!(field= mysql_fetch_field_direct(result,no)))
  {
    fprintf(stdout,"\n *** ERROR: FAILED TO GET THE RESULT ***");
    exit(1);
  }
  fprintf(stdout,"\n field[%d]:", no);
  fprintf(stdout,"\n    name     :`%s`\t(expected: `%s`)", field->name, name);
  fprintf(stdout,"\n    org_name :`%s`\t(expected: `%s`)", field->org_name, org_name);
  fprintf(stdout,"\n    type     :`%d`\t(expected: `%d`)", field->type, type);
  fprintf(stdout,"\n    table    :`%s`\t(expected: `%s`)", field->table, table);
  fprintf(stdout,"\n    org_table:`%s`\t(expected: `%s`)", field->org_table, org_table);
  fprintf(stdout,"\n    database :`%s`\t(expected: `%s`)", field->db, db);
  fprintf(stdout,"\n");
  myassert(strcmp(field->name,name) == 0);
  myassert(strcmp(field->org_name,org_name) == 0);
  myassert(field->type == type);
  myassert(strcmp(field->table,table) == 0);
  myassert(strcmp(field->org_table,org_table) == 0);
  myassert(strcmp(field->db,db) == 0);
}

/*
  Utility function to verify the parameter count
*/
static void verify_param_count(MYSQL_STMT *stmt, long exp_count)
{
  long param_count= mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in stmt: %ld (expected: %ld)",
                  param_count, exp_count);
  myassert(param_count == exp_count);
}


/********************************************************
* store result processing                               *
*********************************************************/
static void client_store_result()
{
  MYSQL_RES *result;
  int       rc;

  myheader("client_store_result");

  rc = mysql_query(mysql, "SELECT * FROM myclient_test");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}

/********************************************************
* fetch the results
*********************************************************/
static void client_use_result()
{
  MYSQL_RES *result;
  int       rc;
  myheader("client_use_result");

  rc = mysql_query(mysql, "SELECT * FROM myclient_test");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}

/*
  Separate thread query to test some cases
*/
static my_bool thread_query(char *query)
{
  MYSQL *l_mysql;
  my_bool error;

  error= 0;
  fprintf(stdout,"\n in thread_query(%s)", query);
  if (!(l_mysql = mysql_init(NULL)))
  { 
    myerror("mysql_init() failed");
    return 1;
  }
  if (!(mysql_real_connect(l_mysql,opt_host,opt_user,
			   opt_password, current_db, opt_port,
			   opt_unix_socket, 0)))
  {
    myerror("connection failed");    
    error= 1;
    goto end;
  }    
  if (mysql_query(l_mysql,(char *)query))
  {
     fprintf(stderr,"Query failed (%s)\n",mysql_error(l_mysql));
     error= 1;
     goto end;
  }
  mysql_commit(l_mysql);
end:
  mysql_close(l_mysql);
  return error;
}


/********************************************************
* query processing                                      *
*********************************************************/
static void test_debug_example()
{
  int rc;
  MYSQL_RES *result;

  myheader("test_debug_example");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_debug_example");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_debug_example(id int primary key auto_increment,\
                                              name varchar(20),xxx int)");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_debug_example(name) VALUES('mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"UPDATE test_debug_example SET name='updated' WHERE name='deleted'");
  myquery(rc);

  rc = mysql_query(mysql,"SELECT * FROM test_debug_example where name='mysql'");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);

  rc = mysql_query(mysql,"DROP TABLE test_debug_example");
  myquery(rc);
}

/********************************************************
* to test autocommit feature                            *
*********************************************************/
static void test_tran_bdb()
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_bdb");

  /* set AUTOCOMMIT to OFF */
  rc = mysql_autocommit(mysql, FALSE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_demo_transaction");
  myquery(rc);


  rc = mysql_commit(mysql);
  myquery(rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc = mysql_query(mysql,"CREATE TABLE my_demo_transaction(col1 int ,col2 varchar(30)) TYPE = BDB");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* delete first row, and rollback it */
  rc = mysql_query(mysql,"DELETE FROM my_demo_transaction WHERE col1 = 10");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  row = mysql_fetch_row(result);
  mytest(row);

  row = mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
  mysql_autocommit(mysql,TRUE);
}

/********************************************************
* to test autocommit feature                            *
*********************************************************/
static void test_tran_innodb()
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_innodb");

  /* set AUTOCOMMIT to OFF */
  rc = mysql_autocommit(mysql, FALSE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_demo_transaction");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc = mysql_query(mysql,"CREATE TABLE my_demo_transaction(col1 int ,col2 varchar(30)) TYPE = InnoDB");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* delete first row, and rollback it */
  rc = mysql_query(mysql,"DELETE FROM my_demo_transaction WHERE col1 = 10");
  myquery(rc);

  rc = mysql_rollback(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(result);

  row = mysql_fetch_row(result);
  mytest(row);

  row = mysql_fetch_row(result);
  mytest_r(row);

  mysql_free_result(result);
  mysql_autocommit(mysql,TRUE);
}


/********************************************************
 To test simple prepares of all DML statements
*********************************************************/

static void test_prepare_simple()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_prepare_simple");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_simple");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_simple(id int, name varchar(50))");
  myquery(rc);

  /* alter table */
  strcpy(query,"ALTER TABLE test_prepare_simple ADD new char(20)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);
  mysql_stmt_close(stmt);

  /* insert */
  strcpy(query,"INSERT INTO test_prepare_simple VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);
  mysql_stmt_close(stmt);

  /* update */
  strcpy(query,"UPDATE test_prepare_simple SET id=? WHERE id=? AND name= ?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,3);
  mysql_stmt_close(stmt);

  /* delete */
  strcpy(query,"DELETE FROM test_prepare_simple WHERE id=10");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  mysql_stmt_close(stmt);

  /* delete */
  strcpy(query,"DELETE FROM test_prepare_simple WHERE id=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,1);

  rc = mysql_execute(stmt);
  mystmt_r(stmt, rc);
  mysql_stmt_close(stmt);

  /* select */
  strcpy(query,"SELECT * FROM test_prepare_simple WHERE id=? AND name= ?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);
}


/********************************************************
* to test simple prepare field results                  *
*********************************************************/
static void test_prepare_field_result()
{
  MYSQL_STMT *stmt;
  MYSQL_RES  *result;
  int        rc,param_count;

  myheader("test_prepare_field_result");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_field_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_field_result(int_c int, \
                          var_c varchar(50), ts_c timestamp(14),\
                          char_c char(3), date_c date,extra tinyint)");
  myquery(rc); 

  /* insert */
  strcpy(query,"SELECT int_c,var_c,date_c as date,ts_c,char_c FROM \
                  test_prepare_field_result as t1 WHERE int_c=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,1);

  result = mysql_prepare_result(stmt);
  mytest(result);
  
  my_print_result_metadata(result);

  fprintf(stdout,"\n\n field attributes:\n");
  verify_prepare_field(result,0,"int_c","int_c",MYSQL_TYPE_LONG,
                       "t1","test_prepare_field_result",current_db);
  verify_prepare_field(result,1,"var_c","var_c",MYSQL_TYPE_VAR_STRING,
                       "t1","test_prepare_field_result",current_db);
  verify_prepare_field(result,2,"date","date_c",MYSQL_TYPE_DATE,
                       "t1","test_prepare_field_result",current_db);
  verify_prepare_field(result,3,"ts_c","ts_c",MYSQL_TYPE_TIMESTAMP,
                       "t1","test_prepare_field_result",current_db);
  verify_prepare_field(result,4,"char_c","char_c",MYSQL_TYPE_STRING,
                       "t1","test_prepare_field_result",current_db);

  param_count= mysql_num_fields(result);
  fprintf(stdout,"\n\n total fields: `%d` (expected: `5`)", param_count);
  myassert(param_count == 5);
  mysql_free_result(result);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple prepare field results                  *
*********************************************************/
static void test_prepare_syntax()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_prepare_syntax");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_syntax");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_syntax(id int, name varchar(50), extra int)");
  myquery(rc);

  strcpy(query,"INSERT INTO test_prepare_syntax VALUES(?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init_r(stmt);

  strcpy(query,"SELECT id,name FROM test_prepare_syntax WHERE id=? AND WHERE");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init_r(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);
}


/********************************************************
* to test simple prepare                                *
*********************************************************/
static void test_prepare()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       query[200];
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

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_prepare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE my_prepare(col1 tinyint,\
                                col2 varchar(15), col3 int,\
                                col4 smallint, col5 bigint, \
                                col6 float, col7 double )");
  myquery(rc);

  /* insert by prepare */
  strcpy(query,"INSERT INTO my_prepare VALUES(?,?,?,?,?,?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,7);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer= (char *)&tiny_data;
  /* string */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer= (char *)str_data;
  bind[1].buffer_length= 1000;			/* Max string length */
  /* integer */
  bind[2].buffer_type=FIELD_TYPE_LONG;
  bind[2].buffer= (char *)&int_data;
  /* short */
  bind[3].buffer_type=FIELD_TYPE_SHORT;
  bind[3].buffer= (char *)&small_data;
  /* bigint */
  bind[4].buffer_type=FIELD_TYPE_LONGLONG;
  bind[4].buffer= (char *)&big_data;
  /* float */
  bind[5].buffer_type=FIELD_TYPE_FLOAT;
  bind[5].buffer= (char *)&real_data;
  /* double */
  bind[6].buffer_type=FIELD_TYPE_DOUBLE;
  bind[6].buffer= (char *)&double_data;

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].length= &length[i];
    bind[i].is_null= &is_null[i];
    is_null[i]= 0;
  }

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  int_data = 320;
  small_data = 1867;
  big_data   = 1000;
  real_data = 2;
  double_data = 6578.001;

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data=0; tiny_data < 100; tiny_data++)
  {
    length[1]= my_sprintf(str_data,(str_data, "MySQL%d",int_data));
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
    int_data += 25;
    small_data += 10;
    big_data += 100;
    real_data += 1;
    double_data += 10.09;
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  myassert(tiny_data == (char) my_stmt_result("SELECT * FROM my_prepare",50));
      
  stmt = mysql_prepare(mysql,"SELECT * FROM my_prepare",50);
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  /* get the result */
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  
  o_int_data = 320;
  o_small_data = 1867;
  o_big_data   = 1000;
  o_real_data = 2;
  o_double_data = 6578.001;

  /* now, execute the prepared statement to insert 10 records.. */
  for (o_tiny_data=0; o_tiny_data < 100; o_tiny_data++)
  {
    len = my_sprintf(data, (data, "MySQL%d",o_int_data));
    
    rc = mysql_fetch(stmt);
    mystmt(stmt, rc);

    fprintf(stdout, "\n tiny   : %d (%lu)", tiny_data,length[0]);
    fprintf(stdout, "\n short  : %d (%lu)", small_data,length[3]);
    fprintf(stdout, "\n int    : %d (%lu)", int_data,length[2]);
    fprintf(stdout, "\n big    : %lld (%lu)", big_data,length[4]);

    fprintf(stdout, "\n float  : %f (%lu)", real_data,length[5]);
    fprintf(stdout, "\n double : %f (%lu)", double_data,length[6]);

    fprintf(stdout, "\n str    : %s (%lu)", str_data, length[1]);

    myassert(tiny_data == o_tiny_data);
    myassert(is_null[0] == 0);
    myassert(length[0] == 1);

    myassert(int_data == o_int_data);
    myassert(length[2] == 4);
    
    myassert(small_data == o_small_data);
    myassert(length[3] == 2);
    
    myassert(big_data == o_big_data);
    myassert(length[4] == 8);
    
    myassert(real_data == o_real_data);
    myassert(length[5] == 4);
    
    myassert(double_data == o_double_data);
    myassert(length[6] == 8);
    
    myassert(strcmp(data,str_data) == 0);
    myassert(length[1] == len);
    
    o_int_data += 25;
    o_small_data += 10;
    o_big_data += 100;
    o_real_data += 1;
    o_double_data += 10.09;
  }

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);

}


/********************************************************
* to test double comparision                            *
*********************************************************/
static void test_double_compare()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200],real_data[10], tiny_data;
  double     double_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];
  ulong	     length[3];

  myheader("test_double_compare");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_double_compare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_double_compare(col1 tinyint,\
                                col2 float, col3 double )");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_double_compare VALUES(1,10.2,34.5)");
  myquery(rc);

  strcpy(query, "UPDATE test_double_compare SET col1=100 WHERE col1 = ? AND col2 = ? AND COL3 = ?");
  stmt = mysql_prepare(mysql,query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,3);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(char *)&tiny_data;
  bind[0].is_null= 0;				/* Can never be null */

  /* string->float */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer= (char *)&real_data;
  bind[1].buffer_length=sizeof(real_data);
  bind[1].is_null= 0;
  bind[1].length= &length[1];
  length[1]= 10; 

  /* double */
  bind[2].buffer_type=FIELD_TYPE_DOUBLE;
  bind[2].buffer= (char *)&double_data;
  bind[2].is_null= 0;

  tiny_data = 1;
  strmov(real_data,"10.2");
  double_data = 34.5;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = (int)mysql_affected_rows(mysql);
  fprintf(stdout,"\n total affected rows:%d",rc);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_double_compare");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert((int)tiny_data == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple null                                   *
*********************************************************/
static void test_null()
{
  MYSQL_STMT *stmt;
  int        rc;
  uint       nData;
  MYSQL_BIND bind[2];
  my_bool    is_null[2];

  myheader("test_null");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_null");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_null(col1 int,col2 varchar(50))");
  myquery(rc);

  /* insert by prepare, wrong column name */
  strmov(query,"INSERT INTO test_null(col3,col2) VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init_r(stmt);

  strmov(query,"INSERT INTO test_null(col1,col2) VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer_type=MYSQL_TYPE_LONG;
  bind[0].is_null= &is_null[0];
  is_null[0]= 1;
  bind[1]=bind[0];		

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (nData=0; nData<10; nData++)
  {
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }

  /* Re-bind with MYSQL_TYPE_NULL */
  bind[0].buffer_type= MYSQL_TYPE_NULL;
  is_null[0]= 0; /* reset */
  bind[1]= bind[0];

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt,rc);
  
  for (nData=0; nData<10; nData++)
  {
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }
  
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  nData*= 2;
  myassert(nData == my_stmt_result("SELECT * FROM test_null", 30));

  /* Fetch results */
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&nData; /* this buffer won't be altered */
  bind[0].length= 0;
  bind[1]= bind[0];
  bind[0].is_null= &is_null[0];
  bind[1].is_null= &is_null[1];

  stmt = mysql_prepare(mysql,"SELECT * FROM test_null",30);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt,rc);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt,rc);

  rc= 0;
  is_null[0]= is_null[1]= 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
  {
    myassert(is_null[0]);
    myassert(is_null[1]);
    rc++;
    is_null[0]= is_null[1]= 0;
  }
  myassert(rc == (int)nData);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test fetch null                                    *
*********************************************************/
static void test_fetch_null()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];
  int        i, nData;
  MYSQL_BIND bind[11];
  ulong	     length[11];
  my_bool    is_null[11];

  myheader("test_fetch_null");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_fetch_null");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_fetch_null(col1 tinyint, col2 smallint, \
                                                       col3 int, col4 bigint, \
                                                       col5 float, col6 double, \
                                                       col7 date, col8 time, \
                                                       col9 varbinary(10), \
                                                       col10 varchar(50),\
                                                       col11 char(20))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_fetch_null(col11) VALUES(1000),(88),(389789)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].buffer_type=FIELD_TYPE_LONG;
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }
  bind[i-1].buffer=(char *)&nData;		/* Last column is not null */

  strmov((char *)query , "SELECT * FROM test_fetch_null");

  myassert(3 == my_stmt_result(query,50));

  stmt = mysql_prepare(mysql, query, 50);
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc= 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
  {
    rc++;
    for (i=0; i < 10; i++)
    {
      fprintf(stdout, "\n data[%d] : %s", i, 
              is_null[i] ? "NULL" : "NOT NULL");
      myassert(is_null[i]);
    }
    fprintf(stdout, "\n data[%d]: %d", i, nData);
    myassert(nData == 1000 || nData == 88 || nData == 389789);
    myassert(is_null[i] == 0);
    myassert(length[i] == 4);
  }
  myassert(rc == 3);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple select                                 *
*********************************************************/
static void test_select_version()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];

  myheader("test_select_version");

  strmov((char *)query , "SELECT @@version");
  stmt = PREPARE(mysql, query);
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple select                                 *
*********************************************************/
static void test_select_simple()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];

  myheader("test_select_simple");

  /* insert by prepare */
  strmov((char *)query, "SHOW TABLES FROM mysql");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple select to debug                        *
*********************************************************/
static void test_select_direct()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_select_direct");
  
  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int, id1 tinyint, \
                                                   id2 float, \
                                                   id3 double, \
                                                   name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,5,2.3,4.5,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"SELECT * FROM test_select");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}


/********************************************************
* to test simple select with prepare                    *
*********************************************************/
static void test_select_prepare()
{
  int        rc;
  MYSQL_STMT *stmt;

  myheader("test_select_prepare");
  
  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int, name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"SELECT * FROM test_select",50);
  mystmt_init(stmt);
  
  rc = mysql_execute(stmt);
  mystmt(stmt,rc);

  myassert(1 == my_process_stmt_result(stmt));
  mysql_stmt_close(stmt);

  rc = mysql_query(mysql,"DROP TABLE test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id tinyint, id1 int, \
                                                   id2 float, id3 float, \
                                                   name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select(id,id1,id2,name) VALUES(10,5,2.3,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"SELECT * FROM test_select",25);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt,rc);

  myassert(1 == my_process_stmt_result(stmt));
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple select                                 *
*********************************************************/
static void test_select()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_BIND bind[2];
  ulong length[2];

  myheader("test_select");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(20,'mysql')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"SELECT * FROM test_select WHERE id=? AND name=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  /* string data */
  nData=10;
  strcpy(szData,(char *)"venu");
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=(char *)szData;
  bind[1].buffer_length= 4;
  bind[1].length= &length[1];
  length[1]= 4;
  bind[1].is_null=0;

  bind[0].buffer=(char *)&nData;
  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  myassert(my_process_stmt_result(stmt) == 1);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple select show                            *
*********************************************************/
static void test_select_show()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_select_show");

  mysql_autocommit(mysql,TRUE);

  rc = mysql_query(mysql, "DROP TABLE IF EXISTS test_show");
  myquery(rc);
  
  rc = mysql_query(mysql, "CREATE TABLE test_show(id int(4) NOT NULL, name char(2))");
  myquery(rc);

  stmt = mysql_prepare(mysql, "show columns from test_show", 30);
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
  
  stmt = mysql_prepare(mysql, "show tables from mysql like ?", 50);
  mystmt_init_r(stmt);
  
  strxmov(query,"show tables from ", current_db, " like \'test_show\'", NullS);
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
  
  stmt = mysql_prepare(mysql, "describe test_show", 30);
  mystmt_init(stmt);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  my_process_stmt_result(stmt);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple update                                *
*********************************************************/
static void test_simple_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];   
  ulong	     length[2];

  myheader("test_simple_update");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_update(col1 int,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_update VALUES(1,'MySQL',100)");
  myquery(rc);

  myassert(1 == mysql_affected_rows(mysql));

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert by prepare */
  strmov(query,"UPDATE test_update SET col2=? WHERE col1=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  nData=1;
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;		/* string data */
  bind[0].buffer_length=sizeof(szData);
  bind[0].length= &length[0];
  bind[0].is_null= 0;
  length[0]= my_sprintf(szData, (szData,"updated-data"));

  bind[1].buffer=(char *) &nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;
  bind[1].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  myassert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_update");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple long data handling                     *
*********************************************************/
static void test_long_data()
{
  MYSQL_STMT *stmt;
  int        rc, int_data;
  char       *data=NullS;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];

  myheader("test_long_data");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data(col1 int,\
                                col2 long varchar, col3 long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_long_data(col1,col2) VALUES(?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init_r(stmt);

  strmov(query,"INSERT INTO test_long_data(col1,col2,col3) VALUES(?,?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,3);

  bind[0].buffer=(char *)&int_data;
  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].is_null=0;

  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].is_null=0;
  bind[1].buffer_length=0;			/* Will not be used */
  bind[1].length=0;				/* Will not be used */

  bind[2]=bind[1];
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  int_data= 999;
  data = (char *)"Michael";

  /* supply data in pieces */
  rc = mysql_send_long_data(stmt,1,data,strlen(data));
  data = (char *)" 'Monty' Widenius";
  rc = mysql_send_long_data(stmt,1,data,strlen(data));
  mystmt(stmt, rc);
  rc = mysql_send_long_data(stmt,2,"Venu (venu@mysql.com)",4);
  mystmt(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT * FROM test_long_data");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);

  verify_col_data("test_long_data","col1","999");
  verify_col_data("test_long_data","col2","Michael 'Monty' Widenius");
  verify_col_data("test_long_data","col3","Venu");
  mysql_stmt_close(stmt);
}


/********************************************************
* to test long data (string) handling                   *
*********************************************************/
static void test_long_data_str()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       data[255];
  long       length, length1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  my_bool    is_null[2];

  myheader("test_long_data_str");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_str");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_str(id int, longstr long varchar)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_long_data_str VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer = (char *)&length;
  bind[0].buffer_type = FIELD_TYPE_LONG;
  bind[0].is_null= &is_null[0];
  is_null[0]=0;
  length= 0;

  bind[1].buffer=data;				/* string data */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].length= &length1;
  bind[1].buffer_length=0;			/* Will not be used */
  bind[1].is_null= &is_null[1];
  is_null[1]=0;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  length = 40;
  strmov(data,"MySQL AB");

  /* supply data in pieces */
  for(i=0; i < 4; i++)
  {
    rc = mysql_send_long_data(stmt,1,(char *)data,5);
    mystmt(stmt, rc);
  }
  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longstr), longstr FROM test_long_data_str");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);

  my_sprintf(data,(data,"%d", i*5));
  verify_col_data("test_long_data_str","LENGTH(longstr)", data);
  data[0]='\0';
  while (i--)
   strxmov(data,data,"MySQL",NullS);
  verify_col_data("test_long_data_str","longstr", data);
}


/********************************************************
* to test long data (string) handling                   *
*********************************************************/
static void test_long_data_str1()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  char       data[255];
  long       length, length1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];

  myheader("test_long_data_str1");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_str");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_str(longstr long varchar,blb long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_long_data_str VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer=data;		  /* string data */
  bind[0].buffer_length= sizeof(data);
  bind[0].length= &length1;
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].is_null= 0;
  length1= 0;

  bind[1] = bind[0];
  bind[1].buffer_type=FIELD_TYPE_BLOB;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);
  length = my_sprintf(data, (data, "MySQL AB"));

  /* supply data in pieces */
  for (i=0; i < 3; i++)
  {
    rc = mysql_send_long_data(stmt,0,data,length);
    mystmt(stmt, rc);

    rc = mysql_send_long_data(stmt,1,data,2);
    mystmt(stmt, rc);
  }

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longstr),longstr,LENGTH(blb),blb FROM test_long_data_str");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);

  my_sprintf(data,(data,"%ld",(long)i*length));
  verify_col_data("test_long_data_str","length(longstr)",data);

  my_sprintf(data,(data,"%d",i*2));
  verify_col_data("test_long_data_str","length(blb)",data);
}


/********************************************************
* to test long data (binary) handling                   *
*********************************************************/
static void test_long_data_bin()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       data[255];
  long       length;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];


  myheader("test_long_data_bin");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_bin");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_bin(id int, longbin long varbinary)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_long_data_bin VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  bind[0].buffer = (char *)&length;
  bind[0].buffer_type = FIELD_TYPE_LONG;
  bind[0].is_null= 0;
  length= 0;

  bind[1].buffer=data;		 /* string data */
  bind[1].buffer_type=FIELD_TYPE_LONG_BLOB;
  bind[1].length= 0;		/* Will not be used */
  bind[1].is_null= 0;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  length = 10;
  strmov(data,"MySQL AB");

  /* supply data in pieces */
  {
    int i;
    for (i=0; i < 100; i++)
    {
      rc = mysql_send_long_data(stmt,1,(char *)data,4);
      mystmt(stmt, rc);
    }
  }
  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout," mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longbin), longbin FROM test_long_data_bin");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple delete                                 *
*********************************************************/
static void test_simple_delete()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[30]={0};
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong length[2];

  myheader("test_simple_delete");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_simple_delete");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_simple_delete(col1 int,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_simple_delete VALUES(1,'MySQL',100)");
  myquery(rc);

  myassert(1 == mysql_affected_rows(mysql));

  rc = mysql_commit(mysql);
  myquery(rc);

  /* insert by prepare */
  strmov(query,"DELETE FROM test_simple_delete WHERE col1=? AND col2=? AND col3=100");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  nData=1;
  strmov(szData,"MySQL");
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer= szData;		/* string data */
  bind[1].buffer_length=sizeof(szData);
  bind[1].length= &length[1];
  bind[1].is_null= 0;
  length[1]= 5;

  bind[0].buffer=(char *)&nData;
  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  myassert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_simple_delete");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(0 == my_process_result_set(result));
  mysql_free_result(result);
}



/********************************************************
* to test simple update                                 *
*********************************************************/
static void test_update()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       szData[25];
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong length[2];

  myheader("test_update");

  rc = mysql_autocommit(mysql,TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_update(col1 int primary key auto_increment,\
                                col2 varchar(50), col3 int )");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  strmov(query,"INSERT INTO test_update(col2,col3) VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  /* string data */
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;
  bind[0].buffer_length= sizeof(szData);
  bind[0].length= &length[0];
  length[0]= my_sprintf(szData, (szData, "inserted-data"));
  bind[0].is_null= 0;

  bind[1].buffer=(char *)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;
  bind[1].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  nData=100;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  myassert(1 == mysql_affected_rows(mysql));
  mysql_stmt_close(stmt);

  strmov(query,"UPDATE test_update SET col2=? WHERE col3=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);
  nData=100;

  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;
  bind[0].buffer_length= sizeof(szData);
  bind[0].length= &length[0];
  length[0]= my_sprintf(szData, (szData, "updated-data"));
  bind[1].buffer=(char *)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;
  bind[1].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  myassert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_update");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple prepare                                *
*********************************************************/
static void test_prepare_noparam()
{
  MYSQL_STMT *stmt;
  int        rc;
  MYSQL_RES  *result;

  myheader("test_prepare_noparam");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_prepare");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE my_prepare(col1 int ,col2 varchar(50))");
  myquery(rc);


  /* insert by prepare */
  strmov(query,"INSERT INTO my_prepare VALUES(10,'venu')");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_prepare");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
* to test simple bind result                            *
*********************************************************/
static void test_bind_result()
{
  MYSQL_STMT *stmt;
  int        rc;
  char query[100];
  int        nData;
  ulong	     length, length1;
  char       szData[100];
  MYSQL_BIND bind[2];
  my_bool    is_null[2];

  myheader("test_bind_result");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(10,'venu')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(20,'MySQL')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */

  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (char *) &nData;	/* integer data */
  bind[0].is_null= &is_null[0];
  bind[0].length= 0;

  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;		/* string data */
  bind[1].buffer_length=sizeof(szData);
  bind[1].length= &length1;
  bind[1].is_null= &is_null[1];

  strmov(query , "SELECT * FROM test_bind_result");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 1: %d,%s(%lu)",nData, szData, length1);
  myassert(nData == 10);
  myassert(strcmp(szData,"venu")==0);
  myassert(length1 == 4);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 2: %d,%s(%lu)",nData, szData, length1);
  myassert(nData == 20);
  myassert(strcmp(szData,"MySQL")==0);
  myassert(length1 == 5);

  length=99;
  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);
 
  if (is_null[0])
    fprintf(stdout,"\n row 3: NULL,%s(%lu)", szData, length1);
  myassert(is_null[0]);
  myassert(strcmp(szData,"monty")==0);
  myassert(length1 == 5);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test ext bind result                               *
*********************************************************/
static void test_bind_result_ext()
{
  MYSQL_STMT *stmt;
  int        rc, i;
  const char query[100];
  uchar      t_data;
  short      s_data;
  int        i_data;
  longlong   b_data;
  float      f_data;
  double     d_data;
  char       szData[20], bData[20];
  ulong       szLength, bLength;
  MYSQL_BIND bind[8];
  long	     length[8];
  my_bool    is_null[8];

  myheader("test_bind_result_ext");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 tinyint, c2 smallint, \
                                                        c3 int, c4 bigint, \
                                                        c5 float, c6 double, \
                                                        c7 varbinary(10), \
                                                        c8 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(19,2999,3999,4999999,\
                                                              2345.6,5678.89563,\
                                                              'venu','mysql')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].length=  &length[i];
    bind[i].is_null= &is_null[i];
  }

  bind[0].buffer_type=MYSQL_TYPE_TINY;
  bind[0].buffer=(char *)&t_data;

  bind[1].buffer_type=MYSQL_TYPE_SHORT;
  bind[2].buffer_type=MYSQL_TYPE_LONG;
  
  bind[3].buffer_type=MYSQL_TYPE_LONGLONG;
  bind[1].buffer=(char *)&s_data;
  
  bind[2].buffer=(char *)&i_data;
  bind[3].buffer=(char *)&b_data;

  bind[4].buffer_type=MYSQL_TYPE_FLOAT;
  bind[4].buffer=(char *)&f_data;

  bind[5].buffer_type=MYSQL_TYPE_DOUBLE;
  bind[5].buffer=(char *)&d_data;

  bind[6].buffer_type=MYSQL_TYPE_STRING;
  bind[6].buffer= (char *)szData;
  bind[6].buffer_length= sizeof(szData);
  bind[6].length= &szLength;

  bind[7].buffer_type=MYSQL_TYPE_TINY_BLOB;
  bind[7].buffer=(char *)&bData;
  bind[7].length= &bLength;
  bind[7].buffer_length= sizeof(bData);

  strmov((char *)query , "SELECT * FROM test_bind_result");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n data (tiny)   : %d", t_data);
  fprintf(stdout, "\n data (short)  : %d", s_data);
  fprintf(stdout, "\n data (int)    : %d", i_data);
  fprintf(stdout, "\n data (big)    : %lld", b_data);

  fprintf(stdout, "\n data (float)  : %f", f_data);
  fprintf(stdout, "\n data (double) : %f", d_data);

  fprintf(stdout, "\n data (str)    : %s(%lu)", szData, szLength);
  fprintf(stdout, "\n data (bin)    : %s(%lu)", bData, bLength);


  myassert(t_data == 19);
  myassert(s_data == 2999);
  myassert(i_data == 3999);
  myassert(b_data == 4999999);
  /*myassert(f_data == 2345.60);*/
  /*myassert(d_data == 5678.89563);*/
  myassert(strcmp(szData,"venu")==0);
  myassert(strcmp(bData,"mysql")==0);
  myassert(szLength == 4);
  myassert(bLength == 5);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test ext bind result                               *
*********************************************************/
static void test_bind_result_ext1()
{
  MYSQL_STMT *stmt;
  uint	     i;
  int        rc;
  const char query[100];
  char       t_data[20];
  float      s_data;
  short      i_data;
  short      b_data;
  int        f_data;
  long       bData;
  char       d_data[20];
  double     szData;
  MYSQL_BIND bind[8];
  ulong      length[8];
  my_bool    is_null[8];
  myheader("test_bind_result_ext1");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 tinyint, c2 smallint, \
                                                        c3 int, c4 bigint, \
                                                        c5 float, c6 double, \
                                                        c7 varbinary(10), \
                                                        c8 varchar(10))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(120,2999,3999,54,\
                                                              2.6,58.89,\
                                                              '206','6.7')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind[0].buffer_type=MYSQL_TYPE_STRING;
  bind[0].buffer=(char *) t_data;
  bind[0].buffer_length= sizeof(t_data);
  bind[0].length= &length[0];

  bind[1].buffer_type=MYSQL_TYPE_FLOAT;
  bind[1].buffer=(char *)&s_data;

  bind[2].buffer_type=MYSQL_TYPE_SHORT;
  bind[2].buffer=(char *)&i_data;

  bind[3].buffer_type=MYSQL_TYPE_TINY;
  bind[3].buffer=(char *)&b_data;

  bind[4].buffer_type=MYSQL_TYPE_LONG;
  bind[4].buffer=(char *)&f_data;

  bind[5].buffer_type=MYSQL_TYPE_STRING;
  bind[5].buffer=(char *)d_data;

  bind[6].buffer_type=MYSQL_TYPE_LONG;
  bind[6].buffer=(char *)&bData;

  bind[7].buffer_type=MYSQL_TYPE_DOUBLE;
  bind[7].buffer=(char *)&szData;

  for (i= 0; i < array_elements(bind); i++)
  {
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }

  strmov((char *)query , "SELECT * FROM test_bind_result");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n data (tiny)   : %s(%lu)", t_data, length[0]);
  fprintf(stdout, "\n data (short)  : %f(%lu)", s_data, length[1]);
  fprintf(stdout, "\n data (int)    : %d(%lu)", i_data, length[2]);
  fprintf(stdout, "\n data (big)    : %d(%lu)", b_data, length[3]);

  fprintf(stdout, "\n data (float)  : %d(%lu)", f_data, length[4]);
  fprintf(stdout, "\n data (double) : %s(%lu)", d_data, length[5]);

  fprintf(stdout, "\n data (bin)    : %ld(%lu)", bData, length[6]);
  fprintf(stdout, "\n data (str)    : %g(%lu)", szData, length[7]);

  myassert(strcmp(t_data,"120")==0);
  myassert(i_data == 3999);
  myassert(f_data == 2);
  myassert(strcmp(d_data,"58.89")==0);

  myassert(length[0] == 3);
  myassert(length[1] == 4);
  myassert(length[2] == 2);
  myassert(length[3] == 1);
  myassert(length[4] == 4);
  myassert(length[5] == 5);
  myassert(length[6] == 4);
  myassert(length[7] == 8);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/*
 Generalized fetch conversion routine for all basic types           
*/
static void bind_fetch(int row_count)
{ 
  MYSQL_STMT   *stmt;
  int          rc, i, count= row_count;
  ulong	       bit;
  long         data[10];
  float        f_data;
  double       d_data;
  char         s_data[10];
  ulong	       length[10];
  MYSQL_BIND   bind[7];
  my_bool      is_null[7]; 

  stmt = mysql_prepare(mysql,"INSERT INTO test_bind_fetch VALUES(?,?,?,?,?,?,?)",100);
  mystmt_init(stmt);

  verify_param_count(stmt, 7);
 
  for (i= 0; i < (int) array_elements(bind); i++)
  {  
    bind[i].buffer_type= MYSQL_TYPE_LONG;
    bind[i].buffer= (char *) &data[i];
    bind[i].is_null= 0;
  }   
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
 
  while (count--)
  {
    rc= 10+count;
    for (i= 0; i < (int) array_elements(bind); i++)
    {  
      data[i]= rc+i;
      rc+= 12;
    }
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(row_count == (int)
           my_stmt_result("SELECT * FROM test_bind_fetch",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_bind_fetch",50);
  myquery(rc);

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].buffer= (char *) &data[i];
    bind[i].length= &length[i];
    bind[i].is_null= &is_null[i];
  }

  bind[0].buffer_type= MYSQL_TYPE_TINY;
  bind[1].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[3].buffer_type= MYSQL_TYPE_LONGLONG;

  bind[4].buffer_type= MYSQL_TYPE_FLOAT;
  bind[4].buffer= (char *)&f_data;

  bind[5].buffer_type= MYSQL_TYPE_DOUBLE;
  bind[5].buffer= (char *)&d_data;

  bind[6].buffer_type= MYSQL_TYPE_STRING;
  bind[6].buffer= (char *)&s_data;
  bind[6].buffer_length= sizeof(s_data);

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  while (row_count--)
  {
    rc = mysql_fetch(stmt);
    mystmt(stmt,rc);

    fprintf(stdout, "\n");
    fprintf(stdout, "\n tiny     : %ld(%lu)", data[0], length[0]);
    fprintf(stdout, "\n short    : %ld(%lu)", data[1], length[1]);
    fprintf(stdout, "\n int      : %ld(%lu)", data[2], length[2]);
    fprintf(stdout, "\n longlong : %ld(%lu)", data[3], length[3]);
    fprintf(stdout, "\n float    : %f(%lu)",  f_data,  length[4]);
    fprintf(stdout, "\n double   : %g(%lu)",  d_data,  length[5]);
    fprintf(stdout, "\n char     : %s(%lu)",  s_data,  length[6]);

    bit= 1;
    rc= 10+row_count;
    for (i=0; i < 4; i++)
    {
      myassert(data[i] == rc+i);
      myassert(length[i] == bit);
      bit<<= 1;
      rc+= 12;
    }

    /* FLOAT */
    rc+= i;
    myassert((int)f_data == rc);
    myassert(length[4] == 4);

    /* DOUBLE */
    rc+= 13;
    myassert((int)d_data == rc);
    myassert(length[5] == 8);

    /* CHAR */
    rc+= 13;
    {
      char buff[20];
      long len= my_sprintf(buff, (buff, "%d", rc));
      myassert(strcmp(s_data,buff)==0);
      myassert(length[6] == (ulong) len);
    }
  }
  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of date, time and ts                 *
*********************************************************/
static void test_fetch_date()
{
  MYSQL_STMT *stmt;
  uint	     i;
  int        rc, year;
  char       date[25], time[25], ts[25], ts_4[15], ts_6[20], dt[20];
  ulong      d_length, t_length, ts_length, ts4_length, ts6_length, 
             dt_length, y_length;
  MYSQL_BIND bind[8];
  my_bool    is_null[8];
  ulong	     length[8];

  myheader("test_fetch_date");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(c1 date, c2 time, \
                                                        c3 timestamp(14), \
                                                        c4 year, \
                                                        c5 datetime, \
                                                        c6 timestamp(4), \
                                                        c7 timestamp(6))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES('2002-01-02',\
                                                              '12:49:00',\
                                                              '2002-01-02 17:46:59', \
                                                              2010,\
                                                              '2010-07-10', \
                                                              '2020','1999-12-29')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  for (i= 0; i < array_elements(bind); i++)
  {
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
  }

  bind[0].buffer_type=MYSQL_TYPE_STRING;
  bind[1]=bind[2]=bind[0];

  bind[0].buffer=(char *)&date;
  bind[0].buffer_length= sizeof(date);
  bind[0].length= &d_length;

  bind[1].buffer=(char *)&time;
  bind[1].buffer_length= sizeof(time);
  bind[1].length= &t_length;

  bind[2].buffer=(char *)&ts;
  bind[2].buffer_length= sizeof(ts);
  bind[2].length= &ts_length;

  bind[3].buffer_type=MYSQL_TYPE_LONG;
  bind[3].buffer=(char *)&year;
  bind[3].length= &y_length;

  bind[4].buffer_type=MYSQL_TYPE_STRING;
  bind[4].buffer=(char *)&dt;
  bind[4].buffer_length= sizeof(dt);
  bind[4].length= &dt_length;

  bind[5].buffer_type=MYSQL_TYPE_STRING;
  bind[5].buffer=(char *)&ts_4;
  bind[5].buffer_length= sizeof(ts_4);
  bind[5].length= &ts4_length;

  bind[6].buffer_type=MYSQL_TYPE_STRING;
  bind[6].buffer=(char *)&ts_6;
  bind[6].buffer_length= sizeof(ts_6);
  bind[6].length= &ts6_length;

  myassert(1 == my_stmt_result("SELECT * FROM test_bind_result",50));

  stmt = mysql_prepare(mysql, "SELECT * FROM test_bind_result", 50);
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  
  ts_4[0]='\0';
  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout, "\n date   : %s(%lu)", date, d_length);
  fprintf(stdout, "\n time   : %s(%lu)", time, t_length);
  fprintf(stdout, "\n ts     : %s(%lu)", ts, ts_length);
  fprintf(stdout, "\n year   : %d(%lu)", year, y_length);
  fprintf(stdout, "\n dt     : %s(%lu)", dt,  dt_length);
  fprintf(stdout, "\n ts(4)  : %s(%lu)", ts_4, ts4_length);
  fprintf(stdout, "\n ts(6)  : %s(%lu)", ts_6, ts6_length);

  myassert(strcmp(date,"2002-01-02")==0);
  myassert(d_length == 10);

  myassert(strcmp(time,"12:49:00")==0);
  myassert(t_length == 8);

  myassert(strcmp(ts,"2002-01-02 17:46:59")==0);
  myassert(ts_length == 19);

  myassert(year == 2010);
  myassert(y_length == 4);
  
  myassert(strcmp(dt,"2010-07-10 00:00:00")==0);
  myassert(dt_length == 19);

  myassert(ts_4[0] == '\0');
  myassert(ts4_length == 0);

  myassert(strcmp(ts_6,"1999-12-29 00:00:00")==0);
  myassert(ts6_length == 19);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test fetching of str to all types                  *
*********************************************************/
static void test_fetch_str()
{
  int rc;

  myheader("test_fetch_str");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 char(10),\
                                                     c2 char(10),\
                                                     c3 char(20),\
                                                     c4 char(20),\
                                                     c5 char(30),\
                                                     c6 char(40),\
                                                     c7 char(20))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind_fetch(3);
}

/********************************************************
* to test fetching of long to all types                 *
*********************************************************/
static void test_fetch_long()
{
  int rc;

  myheader("test_fetch_long");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 int unsigned,\
                                                     c2 int unsigned,\
                                                     c3 int,\
                                                     c4 int,\
                                                     c5 int,\
                                                     c6 int unsigned,\
                                                     c7 int)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind_fetch(4);
}


/********************************************************
* to test fetching of short to all types                 *
*********************************************************/
static void test_fetch_short()
{
  int rc;

  myheader("test_fetch_short");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 smallint unsigned,\
                                                     c2 smallint,\
                                                     c3 smallint unsigned,\
                                                     c4 smallint,\
                                                     c5 smallint,\
                                                     c6 smallint,\
                                                     c7 smallint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(5);
}


/********************************************************
* to test fetching of tiny to all types                 *
*********************************************************/
static void test_fetch_tiny()
{
  int rc;

  myheader("test_fetch_tiny");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 tinyint unsigned,\
                                                     c2 tinyint,\
                                                     c3 tinyint unsigned,\
                                                     c4 tinyint,\
                                                     c5 tinyint,\
                                                     c6 tinyint,\
                                                     c7 tinyint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(3);

}


/********************************************************
* to test fetching of longlong to all types             *
*********************************************************/
static void test_fetch_bigint()
{
  int rc;

  myheader("test_fetch_bigint");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 bigint,\
                                                     c2 bigint,\
                                                     c3 bigint unsigned,\
                                                     c4 bigint unsigned,\
                                                     c5 bigint unsigned,\
                                                     c6 bigint unsigned,\
                                                     c7 bigint unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(2);

}


/********************************************************
* to test fetching of float to all types                 *
*********************************************************/
static void test_fetch_float()
{
  int rc;

  myheader("test_fetch_float");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 float(3),\
                                                     c2 float,\
                                                     c3 float unsigned,\
                                                     c4 float,\
                                                     c5 float,\
                                                     c6 float,\
                                                     c7 float(10) unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(2);

}

/********************************************************
* to test fetching of double to all types               *
*********************************************************/
static void test_fetch_double()
{
  int rc;

  myheader("test_fetch_double");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_fetch");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_fetch(c1 double(5,2),\
                                                     c2 double unsigned,\
                                                     c3 double unsigned,\
                                                     c4 double unsigned,\
                                                     c5 double unsigned,\
                                                     c6 double unsigned,\
                                                     c7 double unsigned)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);
  
  bind_fetch(3);

}

/********************************************************
* to test simple prepare with all possible types        *
*********************************************************/
static void test_prepare_ext()
{
  MYSQL_STMT *stmt;
  uint	     i;
  int        rc;
  char       *sql;
  int        nData=1;
  char       tData=1;
  short      sData=10;
  longlong   bData=20;
  MYSQL_BIND bind[6];
  myheader("test_prepare_ext");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_ext");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  sql = (char *)"CREATE TABLE test_prepare_ext\
			(\
			c1  tinyint,\
			c2  smallint,\
			c3  mediumint,\
			c4  int,\
			c5  integer,\
			c6  bigint,\
			c7  float,\
			c8  double,\
			c9  double precision,\
			c10 real,\
			c11 decimal(7,4),\
      c12 numeric(8,4),\
			c13 date,\
			c14 datetime,\
			c15 timestamp(14),\
			c16 time,\
			c17 year,\
			c18 bit,\
      c19 bool,\
			c20 char,\
			c21 char(10),\
			c22 varchar(30),\
			c23 tinyblob,\
			c24 tinytext,\
			c25 blob,\
			c26 text,\
			c27 mediumblob,\
			c28 mediumtext,\
			c29 longblob,\
			c30 longtext,\
			c31 enum('one','two','three'),\
			c32 set('monday','tuesday','wednesday'))";

  rc = mysql_query(mysql,sql);
  myquery(rc);

  /* insert by prepare - all integers */
  strmov(query,(char *)"INSERT INTO test_prepare_ext(c1,c2,c3,c4,c5,c6) VALUES(?,?,?,?,?,?)");
  stmt = mysql_prepare(mysql,query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,6);

  /*tinyint*/
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer= (char *)&tData;

  /*smallint*/
  bind[1].buffer_type=FIELD_TYPE_SHORT;
  bind[1].buffer= (char *)&sData;

  /*mediumint*/
  bind[2].buffer_type=FIELD_TYPE_LONG;
  bind[2].buffer= (char *)&nData;

  /*int*/
  bind[3].buffer_type=FIELD_TYPE_LONG;
  bind[3].buffer= (char *)&nData;

  /*integer*/
  bind[4].buffer_type=FIELD_TYPE_LONG;
  bind[4].buffer= (char *)&nData;

  /*bigint*/
  bind[5].buffer_type=FIELD_TYPE_LONGLONG;
  bind[5].buffer= (char *)&bData;

  for (i= 0; i < array_elements(bind); i++)
    bind[i].is_null=0;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  /*
  *  integer to integer
  */
  for (nData=0; nData<10; nData++, tData++, sData++,bData++)
  {
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"SELECT c1,c2,c3,c4,c5,c6 FROM test_prepare_ext",100);
  mystmt_init(stmt);

  /* get the result */
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  myassert(nData == (int)my_process_stmt_result(stmt));

  mysql_stmt_close(stmt);
}



/********************************************************
* to test real and alias names                          *
*********************************************************/
static void test_field_names()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_field_names");

  fprintf(stdout,"\n %d,%d,%d",MYSQL_TYPE_DECIMAL,MYSQL_TYPE_NEWDATE,MYSQL_TYPE_ENUM);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_names1");
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_names2");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_names1(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_names2(id int,name varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* with table name included with TRUE column name */
  rc = mysql_query(mysql,"SELECT id as 'id-alias' FROM test_field_names1");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  myassert(0 == my_process_result_set(result));
  mysql_free_result(result);

  /* with table name included with TRUE column name */
  rc = mysql_query(mysql,"SELECT t1.id as 'id-alias',test_field_names2.name FROM test_field_names1 t1,test_field_names2");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  myassert(0 == my_process_result_set(result));
  mysql_free_result(result);
}

/********************************************************
* to test warnings                                      *
*********************************************************/
static void test_warnings()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_warnings");

  rc = mysql_query(mysql,"SHOW WARNINGS");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}

/********************************************************
* to test errors                                        *
*********************************************************/
static void test_errors()
{
  int        rc;
  MYSQL_RES  *result;

  myheader("test_errors");

  rc = mysql_query(mysql,"SHOW ERRORS");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  my_process_result_set(result);
  mysql_free_result(result);
}



/********************************************************
* to test simple prepare-insert                         *
*********************************************************/
static void test_insert()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200];
  char       str_data[50];
  char       tiny_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];
  ulong	     length[2];

  myheader("test_insert");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_insert");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_insert(col1 tinyint,\
                                col2 varchar(50))");
  myquery(rc);

  /* insert by prepare */
  bzero(bind, sizeof(bind));
  strmov(query,"INSERT INTO test_prep_insert VALUES(?,?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(char *)&tiny_data;
  bind[0].is_null= 0;

  /* string */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=str_data;
  bind[1].buffer_length=sizeof(str_data);;
  bind[1].is_null= 0;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data=0; tiny_data < 3; tiny_data++)
  {
    length[1] = my_sprintf(str_data, (str_data, "MySQL%d",tiny_data));
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_prep_insert");
  myquery(rc);

  /* get the result */
  result = mysql_store_result(mysql);
  mytest(result);

  myassert((int)tiny_data == my_process_result_set(result));
  mysql_free_result(result);

}

/********************************************************
* to test simple prepare-resultset info                 *
*********************************************************/
static void test_prepare_resultset()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200];
  MYSQL_RES  *result;

  myheader("test_prepare_resultset");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_resultset");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_resultset(id int,\
                                name varchar(50),extra double)");
  myquery(rc);

  strmov(query,"SELECT * FROM test_prepare_resultset");
  stmt = PREPARE(mysql, query);
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  result = mysql_prepare_result(stmt);
  mytest(result);
  my_print_result_metadata(result);
  mysql_stmt_close(stmt);
}

/********************************************************
* to test field flags (verify .NET provider)            *
*********************************************************/

static void test_field_flags()
{
  int          rc;
  MYSQL_RES    *result;
  MYSQL_FIELD  *field;
  unsigned int i;


  myheader("test_field_flags");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_flags");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_flags(id int NOT NULL AUTO_INCREMENT PRIMARY KEY,\
                                                        id1 int NOT NULL,\
                                                        id2 int UNIQUE,\
                                                        id3 int,\
                                                        id4 int NOT NULL,\
                                                        id5 int,\
                                                        KEY(id3,id4))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* with table name included with TRUE column name */
  rc = mysql_query(mysql,"SELECT * FROM test_field_flags");
  myquery(rc);

  result = mysql_use_result(mysql);
  mytest(result);

  mysql_field_seek(result,0);
  fputc('\n', stdout);  

  for(i=0; i< mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    fprintf(stdout,"\n field:%d",i);
    if (field->flags & NOT_NULL_FLAG)
      fprintf(stdout,"\n  NOT_NULL_FLAG");
    if (field->flags & PRI_KEY_FLAG)
      fprintf(stdout,"\n  PRI_KEY_FLAG");
    if (field->flags & UNIQUE_KEY_FLAG)
      fprintf(stdout,"\n  UNIQUE_KEY_FLAG");
    if (field->flags & MULTIPLE_KEY_FLAG)
      fprintf(stdout,"\n  MULTIPLE_KEY_FLAG");
    if (field->flags & AUTO_INCREMENT_FLAG)
      fprintf(stdout,"\n  AUTO_INCREMENT_FLAG");

  }
  mysql_free_result(result);
}

/**************************************************************
 * Test mysql_stmt_close for open stmts                       *
**************************************************************/
static void test_stmt_close()
{
  MYSQL *lmysql;
  MYSQL_STMT *stmt1, *stmt2, *stmt3, *stmt_x;
  MYSQL_BIND  bind[1];
  MYSQL_RES   *result;
  char  query[100];
  unsigned int  count;
  int   rc;
  
  myheader("test_stmt_close");  

  fprintf(stdout, "\n Establishing a test connection ...");
  if (!(lmysql = mysql_init(NULL)))
  { 
	  myerror("mysql_init() failed");
    exit(0);
  }
  if (!(mysql_real_connect(lmysql,opt_host,opt_user,
			   opt_password, current_db, opt_port,
			   opt_unix_socket, 0)))
  {
    myerror("connection failed");
    exit(0);
  }   
  fprintf(stdout," OK");
  

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(lmysql, TRUE);
  
  rc = mysql_query(lmysql,"DROP TABLE IF EXISTS test_stmt_close");
  myquery(rc);
  
  rc = mysql_query(lmysql,"CREATE TABLE test_stmt_close(id int)");
  myquery(rc);

  strmov(query,"ALTER TABLE test_stmt_close ADD name varchar(20)");
  stmt1= PREPARE(lmysql, query);
  mystmt_init(stmt1);
  
  verify_param_count(stmt1, 0);
  
  strmov(query,"INSERT INTO test_stmt_close(id) VALUES(?)");
  stmt_x= PREPARE(mysql, query);
  mystmt_init(stmt_x);

  verify_param_count(stmt_x, 1);
  
  strmov(query,"UPDATE test_stmt_close SET id=? WHERE id=?");
  stmt3= PREPARE(lmysql, query);
  mystmt_init(stmt3);
  
  verify_param_count(stmt3, 2);
  
  strmov(query,"SELECT * FROM test_stmt_close WHERE id=?");
  stmt2= PREPARE(lmysql, query);
  mystmt_init(stmt2);

  verify_param_count(stmt2, 1);

  rc= mysql_stmt_close(stmt1);
  fprintf(stdout,"\n mysql_close_stmt(1) returned: %d", rc);
  myassert(rc == 0);
  
  mysql_close(lmysql); /* it should free all open stmts(stmt3, 2 and 1) */
 
#if NOT_VALID
  rc= mysql_stmt_close(stmt3);
  fprintf(stdout,"\n mysql_close_stmt(3) returned: %d", rc);
  myassert( rc == 1);
  
  rc= mysql_stmt_close(stmt2);
  fprintf(stdout,"\n mysql_close_stmt(2) returned: %d", rc);
  myassert( rc == 1);
#endif

  count= 100;
  bind[0].buffer=(char *)&count;
  bind[0].buffer_type=MYSQL_TYPE_LONG;
  bind[0].is_null=0;

  rc = mysql_bind_param(stmt_x, bind);
  mystmt(stmt_x, rc);
  
  rc = mysql_execute(stmt_x);
  mystmt(stmt_x, rc);

  rc= (ulong)mysql_stmt_affected_rows(stmt_x);
  fprintf(stdout,"\n total rows affected: %d", rc);
  myassert (rc == 1);

  rc= mysql_stmt_close(stmt_x);
  fprintf(stdout,"\n mysql_close_stmt(x) returned: %d", rc);
  myassert( rc == 0);

  rc = mysql_query(mysql,"SELECT id FROM test_stmt_close");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));
  mysql_free_result(result);
}


/********************************************************
 * To test simple set-variable prepare                   *
*********************************************************/
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
  
  stmt1 = mysql_prepare(mysql, "show variables like 'max_error_count'", 50);
  mystmt_init(stmt1);

  get_bind[0].buffer_type= MYSQL_TYPE_STRING;
  get_bind[0].buffer= (char *)var;
  get_bind[0].is_null= 0;
  get_bind[0].length= &length;
  get_bind[0].buffer_length= (int)NAME_LEN;
  length= NAME_LEN;

  get_bind[1].buffer_type= MYSQL_TYPE_LONG;
  get_bind[1].buffer= (char *)&get_count;
  get_bind[1].is_null= 0;
  get_bind[1].length= 0;

  rc = mysql_execute(stmt1);
  mystmt(stmt1, rc);
  
  rc = mysql_bind_result(stmt1, get_bind);
  mystmt(stmt1, rc);

  rc = mysql_fetch(stmt1);
  mystmt(stmt1, rc);

  fprintf(stdout, "\n max_error_count(default): %d", get_count);
  def_count= get_count;

  myassert(strcmp(var,"max_error_count") == 0);
  rc = mysql_fetch(stmt1);
  myassert(rc == MYSQL_NO_DATA);

  stmt = mysql_prepare(mysql, "set max_error_count=?", 50);
  mystmt_init(stmt);

  set_bind[0].buffer_type= MYSQL_TYPE_LONG;
  set_bind[0].buffer= (char *)&set_count;
  set_bind[0].is_null= 0;
  set_bind[0].length= 0;
  
  rc = mysql_bind_param(stmt, set_bind);
  mystmt(stmt,rc);
  
  set_count= 31;
  rc= mysql_execute(stmt);
  mystmt(stmt,rc);

  mysql_commit(mysql);

  rc = mysql_execute(stmt1);
  mystmt(stmt1, rc);
  
  rc = mysql_fetch(stmt1);
  mystmt(stmt1, rc);

  fprintf(stdout, "\n max_error_count         : %d", get_count);
  myassert(get_count == set_count);

  rc = mysql_fetch(stmt1);
  myassert(rc == MYSQL_NO_DATA);
  
  /* restore back to default */
  set_count= def_count;
  rc= mysql_execute(stmt);
  mystmt(stmt, rc);
  
  rc = mysql_execute(stmt1);
  mystmt(stmt1, rc);
  
  rc = mysql_fetch(stmt1);
  mystmt(stmt1, rc);

  fprintf(stdout, "\n max_error_count(default): %d", get_count);
  myassert(get_count == set_count);

  rc = mysql_fetch(stmt1);
  myassert(rc == MYSQL_NO_DATA);
  
  mysql_stmt_close(stmt);
  mysql_stmt_close(stmt1);
}

#if NOT_USED
/* Insert meta info .. */
static void test_insert_meta()
{
  MYSQL_STMT *stmt;
  int        rc;
  char       query[200];
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_insert_meta");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_insert");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_insert(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query,"INSERT INTO test_prep_insert VALUES(10,'venu1','test')");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  mysql_stmt_close(stmt);

  strmov(query,"INSERT INTO test_prep_insert VALUES(?,'venu',?)");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n obtained: `%s` (expected: `%s`)", field->name, "col1");
  myassert(strcmp(field->name,"col1")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n obtained: `%s` (expected: `%s`)", field->name, "col3");
  myassert(strcmp(field->name,"col3")==0);

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
  char       query[200];
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_update_meta");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_update");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_update(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query,"UPDATE test_prep_update SET col1=10, col2='venu1' WHERE col3='test'");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  mysql_stmt_close(stmt);

  strmov(query,"UPDATE test_prep_update SET col1=?, col2='venu' WHERE col3=?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col1");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_update");
  myassert(strcmp(field->name,"col1")==0);
  myassert(strcmp(field->table,"test_prep_update")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col3");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_update");
  myassert(strcmp(field->name,"col3")==0);
  myassert(strcmp(field->table,"test_prep_update")==0);

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
  char       query[200];
  MYSQL_RES  *result;
  MYSQL_FIELD *field;

  myheader("test_select_meta");

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_select");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_select(col1 tinyint,\
                                col2 varchar(50), col3 varchar(30))");
  myquery(rc);

  strmov(query,"SELECT * FROM test_prep_select WHERE col1=10");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,0);

  result= mysql_param_result(stmt);
  mytest_r(result);

  strmov(query,"SELECT col1, col3 from test_prep_select WHERE col1=? AND col3='test' AND col2= ?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  verify_param_count(stmt,2);

  result= mysql_param_result(stmt);
  mytest(result);

  my_print_result_metadata(result);

  mysql_field_seek(result, 0);
  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col1");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_select");
  myassert(strcmp(field->name,"col1")==0);
  myassert(strcmp(field->table,"test_prep_select")==0);

  field= mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout, "\n col obtained: `%s` (expected: `%s`)", field->name, "col2");
  fprintf(stdout, "\n tab obtained: `%s` (expected: `%s`)", field->table, "test_prep_select");
  myassert(strcmp(field->name,"col2")==0);
  myassert(strcmp(field->table,"test_prep_select")==0);

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

  rc = mysql_autocommit(mysql, TRUE);
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_dateformat");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_dateformat(id int, \
                                                       ts timestamp)");
  myquery(rc);

  rc = mysql_query(mysql, "INSERT INTO test_dateformat(id) values(10)");
  myquery(rc);

  rc = mysql_query(mysql, "SELECT ts FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n table name: `%s` (expected: `%s`)", field->table,
                  "test_dateformat");
  myassert(strcmp(field->table, "test_dateformat")==0);

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);

  /* DATE_FORMAT */
  rc = mysql_query(mysql, "SELECT DATE_FORMAT(ts,'%Y') AS 'venu' FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n table name: `%s` (expected: `%s`)", field->table, "");
  myassert(field->table[0] == '\0');

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);

  /* FIELD ALIAS TEST */
  rc = mysql_query(mysql, "SELECT DATE_FORMAT(ts,'%Y')  AS 'YEAR' FROM test_dateformat");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  field = mysql_fetch_field(result);
  mytest(field);
  fprintf(stdout,"\n field name: `%s` (expected: `%s`)", field->name, "YEAR");
  fprintf(stdout,"\n field org name: `%s` (expected: `%s`)",field->org_name,"");
  myassert(strcmp(field->name, "YEAR")==0);
  myassert(field->org_name[0] == '\0');

  field = mysql_fetch_field(result);
  mytest_r(field); /* no more fields */

  mysql_free_result(result);
}


/* Multiple stmts .. */
static void test_multi_stmt()
{

  MYSQL_STMT  *stmt, *stmt1, *stmt2;
  int         rc, id;
  char        name[50];
  MYSQL_BIND  bind[2];
  ulong       length[2];
  my_bool     is_null[2];
  myheader("test_multi_stmt");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_multi_table");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_multi_table(id int, name char(20))");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_multi_table values(10,'mysql')");
  myquery(rc);

  stmt = mysql_prepare(mysql, "SELECT * FROM test_multi_table WHERE id = ?", 100);
  mystmt_init(stmt);

  stmt2 = mysql_prepare(mysql, "UPDATE test_multi_table SET name='updated' WHERE id=10",100);
  mystmt_init(stmt2);

  verify_param_count(stmt,1);

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (char *)&id;
  bind[0].is_null= &is_null[0];
  bind[0].length= &length[0];
  is_null[0]= 0;
  length[0]= 0;

  bind[1].buffer_type = MYSQL_TYPE_STRING;
  bind[1].buffer = (char *)name;
  bind[1].length = &length[1];
  bind[1].is_null= &is_null[1];
    
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt, rc);
  
  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  id = 10;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  
  id = 999;  
  rc = mysql_fetch(stmt);
  mystmt(stmt, rc);

  fprintf(stdout, "\n int_data: %d(%lu)", id, length[0]);
  fprintf(stdout, "\n str_data: %s(%lu)", name, length[1]);
  myassert(id == 10);
  myassert(strcmp(name,"mysql")==0);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  /* alter the table schema now */
  stmt1 = mysql_prepare(mysql,"DELETE FROM test_multi_table WHERE id = ? AND name=?",100);
  mystmt_init(stmt1);

  verify_param_count(stmt1,2);

  rc = mysql_bind_param(stmt1, bind);
  mystmt(stmt1, rc);
  
  rc = mysql_execute(stmt2);
  mystmt(stmt2, rc);

  rc = (int)mysql_stmt_affected_rows(stmt2);
  fprintf(stdout,"\n total rows affected(update): %d", rc);
  myassert(rc == 1);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  
  rc = mysql_fetch(stmt);
  mystmt(stmt, rc);

  fprintf(stdout, "\n int_data: %d(%lu)", id, length[0]);
  fprintf(stdout, "\n str_data: %s(%lu)", name, length[1]);
  myassert(id == 10);
  myassert(strcmp(name,"updated")==0);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  rc = mysql_execute(stmt1);
  mystmt(stmt1, rc);

  rc = (int)mysql_stmt_affected_rows(stmt1);
  fprintf(stdout,"\n total rows affected(delete): %d", rc);
  myassert(rc == 1);

  mysql_stmt_close(stmt1);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  myassert(0 == my_stmt_result("SELECT * FROM test_multi_table",50));

  mysql_stmt_close(stmt);
  mysql_stmt_close(stmt2);

}


/********************************************************
* to test simple sample - manual                        *
*********************************************************/
static void test_manual_sample()
{
  unsigned int param_count;
  MYSQL_STMT   *stmt;
  short        small_data;
  int          int_data, i;
  char         str_data[50], query[255];
  ulonglong    affected_rows;
  MYSQL_BIND   bind[3];
  my_bool      is_null[3];

  myheader("test_manual_sample");

  /*
    Sample which is incorporated directly in the manual under Prepared 
    statements section (Example from mysql_execute()
  */

  mysql_autocommit(mysql, 1);
  if (mysql_query(mysql,"DROP TABLE IF EXISTS test_table"))
  {
    fprintf(stderr, "\n drop table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  if (mysql_query(mysql,"CREATE TABLE test_table(col1 int, col2 varchar(50), \
                                                 col3 smallint,\
                                                 col4 timestamp(14))"))
  {
    fprintf(stderr, "\n create table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  
  /* Prepare a insert query with 3 parameters */
  strmov(query, "INSERT INTO test_table(col1,col2,col3) values(?,?,?)");
  if (!(stmt = mysql_prepare(mysql,query,strlen(query))))
  {
    fprintf(stderr, "\n prepare, insert failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  fprintf(stdout, "\n prepare, insert successful");

  /* Get the parameter count from the statement */
  param_count= mysql_param_count(stmt);

  fprintf(stdout, "\n total parameters in insert: %d", param_count);
  if (param_count != 3) /* validate parameter count */
  {
    fprintf(stderr, "\n invalid parameter count returned by MySQL");
    exit(0);
  }

  /* Bind the data for the parameters */

  /* INTEGER PART */
  memset(bind,0,sizeof(bind));
  bind[0].buffer_type= MYSQL_TYPE_LONG;
  bind[0].buffer= (char *)&int_data;

  /* STRING PART */
  bind[1].buffer_type= MYSQL_TYPE_VAR_STRING;
  bind[1].buffer= (char *)str_data;
  bind[1].buffer_length= sizeof(str_data);
 
  /* SMALLINT PART */
  bind[2].buffer_type= MYSQL_TYPE_SHORT;
  bind[2].buffer= (char *)&small_data;       

  for (i= 0; i < (int) array_elements(bind); i++)
  {
    bind[i].is_null= &is_null[i];
    is_null[i]=0;
  }

  /* Bind the buffers */
  if (mysql_bind_param(stmt, bind))
  {
    fprintf(stderr, "\n param bind failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }

  /* Specify the data */
  int_data= 10;             /* integer */
  strmov(str_data,"MySQL"); /* string  */
  /* INSERT SMALLINT data as NULL */
  is_null[2]= 1;

  /* Execute the insert statement - 1*/
  if (mysql_execute(stmt))
  {
    fprintf(stderr, "\n execute 1 failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
    
  /* Get the total rows affected */   
  affected_rows= mysql_stmt_affected_rows(stmt);

  fprintf(stdout, "\n total affected rows: %lld", affected_rows);
  if (affected_rows != 1) /* validate affected rows */
  {
    fprintf(stderr, "\n invalid affected rows by MySQL");
    exit(0);
  }

  /* Re-execute the insert, by changing the values */
  int_data= 1000;             
  strmov(str_data,"The most popular open source database"); 
  small_data= 1000;         /* smallint */
  is_null[2]= 0;

  /* Execute the insert statement - 2*/
  if (mysql_execute(stmt))
  {
    fprintf(stderr, "\n execute 2 failed");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
    
  /* Get the total rows affected */   
  affected_rows= mysql_stmt_affected_rows(stmt);

  fprintf(stdout, "\n total affected rows: %lld", affected_rows);
  if (affected_rows != 1) /* validate affected rows */
  {
    fprintf(stderr, "\n invalid affected rows by MySQL");
    exit(0);
  }

  /* Close the statement */
  if (mysql_stmt_close(stmt))
  {
    fprintf(stderr, "\n failed while closing the statement");
    fprintf(stderr, "\n %s", mysql_stmt_error(stmt));
    exit(0);
  }
  myassert(2 == my_stmt_result("SELECT * FROM test_table",50));

  /* DROP THE TABLE */
  if (mysql_query(mysql,"DROP TABLE test_table"))
  {
    fprintf(stderr, "\n drop table failed");
    fprintf(stderr, "\n %s", mysql_error(mysql));
    exit(0);
  }
  fprintf(stdout, "Success !!!");
}


/********************************************************
* to test alter table scenario in the middle of prepare *
*********************************************************/
static void test_prepare_alter()
{
  MYSQL_STMT  *stmt;
  int         rc, id;
  long        length;
  MYSQL_BIND  bind[1];
  my_bool     is_null;

  myheader("test_prepare_alter");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_alter");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_alter(id int, name char(20))");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_prep_alter values(10,'venu'),(20,'mysql')");
  myquery(rc);

  stmt = mysql_prepare(mysql, "INSERT INTO test_prep_alter VALUES(?,'monty')", 100);
  mystmt_init(stmt);

  verify_param_count(stmt,1);

  bind[0].buffer_type= MYSQL_TYPE_SHORT;
  bind[0].buffer= (char *)&id;
  bind[0].is_null= &is_null;

  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt, rc);
  
  id = 30; length= 0;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  if (thread_query((char *)"ALTER TABLE test_prep_alter change id id_new varchar(20)"))
    exit(0);

  is_null=1;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  myassert(4 == my_stmt_result("SELECT * FROM test_prep_alter",50));

  mysql_stmt_close(stmt);
}

/********************************************************
* to test the support of multi-query executions         *
*********************************************************/
static void test_multi_query()
{
  MYSQL *l_mysql, *org_mysql;
  MYSQL_RES *result;
  int    rc;

  const char *query= "DROP TABLE IF EXISTS test_multi_tab;\
                  CREATE TABLE test_multi_tab(id int,name char(20));\
                  INSERT INTO test_multi_tab(xxxx) VALUES(10);\
                  UPDATE test_multi_tab SET id=10 WHERE unkown_col=10;\
                  CREATE TABLE test_multi_tab(id int,name char(20));\
                  INSERT INTO test_multi_tab(id) VALUES(10),(20);\
                  INSERT INTO test_multi_tab VALUES(20,'insert;comma');\
                  SELECT * FROM test_multi_tab;\
                  UPDATE test_multi_tab SET unknown_col=100 WHERE id=100;\
                  UPDATE test_multi_tab SET name='new;name' WHERE id=20;\
                  DELETE FROM test_multi_tab WHERE name='new;name';\
                  SELECT * FROM test_multi_tab;\
                  DELETE FROM test_multi_tab WHERE id=10;\
                  SELECT * FROM test_multi_tab;\
                  DROP TABLE test_multi_tab;\
                  DROP TABLE test_multi_tab;\
                  DROP TABLE IF EXISTS test_multi_tab";
  uint  count, rows[16]={0,1054,1054,1050,2,1,3,1054,2,2,1,1,0,0,1051,0}, exp_value;
  
  myheader("test_multi_query");

  rc = mysql_query(mysql, query); /* syntax error */
  myquery_r(rc);

  myassert(0 == mysql_next_result(mysql));
  myassert(0 == mysql_more_results(mysql));

  if (!(l_mysql = mysql_init(NULL)))
  { 
    fprintf(stdout,"\n mysql_init() failed");
    exit(1);
  }
  if (!(mysql_real_connect(l_mysql,opt_host,opt_user,
			   opt_password, current_db, opt_port,
			   opt_unix_socket, CLIENT_MULTI_QUERIES))) /* enable multi queries */
  {
    fprintf(stdout,"\n connection failed(%s)", mysql_error(l_mysql));    
    exit(1);
  }    
  org_mysql= mysql;
  mysql= l_mysql;

  rc = mysql_query(mysql, query);
  myquery(rc);

  count= exp_value= 0;
  while (mysql_more_results(mysql) && count < array_elements(rows))
  {
    fprintf(stdout,"\n Query %d: ", count);
    if ((rc= mysql_next_result(mysql)))
    {
      exp_value= mysql_errno(mysql);
      fprintf(stdout, "ERROR %d: %s", exp_value, mysql_error(mysql));
    }
    else
    {
      if ((result= mysql_store_result(mysql)))
        my_process_result_set(result);
      else
        fprintf(stdout,"OK, %d row(s) affected, %d warning(s)",  exp_value, 
                        mysql_warning_count(mysql));
      exp_value= (uint) mysql_affected_rows(mysql);
    }
    myassert(rows[count++] == exp_value);    
  }
  mysql= org_mysql;
}


/********************************************************
* to test simple bind store result                      *
*********************************************************/
static void test_store_result()
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];
  long        nData;
  char       szData[100];
  MYSQL_BIND bind[2];
  ulong	     length, length1;
  my_bool    is_null[2];

  myheader("test_store_result");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_store_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result VALUES(10,'venu'),(20,'mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */
  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (char*) &nData;	/* integer data */
  bind[0].length= &length;
  bind[0].is_null= &is_null[0];

  length= 0; 
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;		/* string data */
  bind[1].buffer_length=sizeof(szData);
  bind[1].length= &length1;
  bind[1].is_null= &is_null[1];
  length1= 0;

  strmov((char *)query , "SELECT * FROM test_store_result");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 1: %ld,%s(%lu)", nData, szData, length1);
  myassert(nData == 10);
  myassert(strcmp(szData,"venu")==0);
  myassert(length1 == 4);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 2: %ld,%s(%lu)",nData, szData, length1);
  myassert(nData == 20);
  myassert(strcmp(szData,"mysql")==0);
  myassert(length1 == 5);

  length=99;
  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);
 
  if (is_null[0])
    fprintf(stdout,"\n row 3: NULL,%s(%lu)", szData, length1);
  myassert(is_null[0]);
  myassert(strcmp(szData,"monty")==0);
  myassert(length1 == 5);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 1: %ld,%s(%lu)",nData, szData, length1);
  myassert(nData == 10);
  myassert(strcmp(szData,"venu")==0);
  myassert(length1 == 4);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 2: %ld,%s(%lu)",nData, szData, length1);
  myassert(nData == 20);
  myassert(strcmp(szData,"mysql")==0);
  myassert(length1 == 5);

  length=99;
  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);
 
  if (is_null[0])
    fprintf(stdout,"\n row 3: NULL,%s(%lu)", szData, length1);
  myassert(is_null[0]);
  myassert(strcmp(szData,"monty")==0);
  myassert(length1 == 5);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple bind store result                      *
*********************************************************/
static void test_store_result1()
{
  MYSQL_STMT *stmt;
  int        rc;

  myheader("test_store_result1");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_store_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result VALUES(10,'venu'),(20,'mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  stmt = mysql_prepare(mysql,"SELECT * FROM test_store_result",100);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  rc = 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
    rc++;
  fprintf(stdout, "\n total rows: %d", rc);
  myassert(rc == 3);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  rc = 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
    rc++;
  fprintf(stdout, "\n total rows: %d", rc);
  myassert(rc == 3);

  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple bind store result                      *
*********************************************************/
static void test_store_result2()
{
  MYSQL_STMT *stmt;
  int        rc;
  int        nData;
  long       length;
  MYSQL_BIND bind[1];

  myheader("test_store_result2");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_store_result");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_store_result(col1 int ,col2 varchar(50))");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result VALUES(10,'venu'),(20,'mysql')");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_store_result(col2) VALUES('monty')");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (char *) &nData;	/* integer data */
  bind[0].length= &length;
  bind[0].is_null= 0;
  length= 0; 

  strmov((char *)query , "SELECT col1 FROM test_store_result where col1= ?");
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);
  
  nData = 10; length= 0;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  nData = 0;
  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 1: %d",nData);
  myassert(nData == 10);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  nData = 20;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  nData = 0;
  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 1: %d",nData);
  myassert(nData == 20);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);
  mysql_stmt_close(stmt);
}


/********************************************************
* to test simple subselect prepare                      *
*********************************************************/
static void test_subselect()
{
#if TO_BE_FIXED_IN_SERVER
  MYSQL_STMT *stmt;
  int        rc;
  int        id;
  ulong      length;
  MYSQL_BIND bind[1];

  myheader("test_subselect");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_sub1");
  myquery(rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_sub2");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_sub1(id int)");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_sub2(id int, id1 int)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_sub1 values(2)");
  myquery(rc);

  rc = mysql_query(mysql,"INSERT INTO test_sub2 VALUES(1,7),(2,7)");
  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  /* fetch */

  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (char *) &id;	
  bind[0].length= &length;
  bind[0],is_null= 0;
  length=0;

  strmov((char *)query , "SELECT ROW(1,7) IN (select id, id1 from test_sub2 WHERE id1=?)");
  myassert(1 == my_stmt_result("SELECT ROW(1,7) IN (select id, id1 from test_sub2 WHERE id1=8)",100));
  myassert(1 == my_stmt_result("SELECT ROW(1,7) IN (select id, id1 from test_sub2 WHERE id1=7)",100));
  stmt = mysql_prepare(mysql, query, strlen(query));
  mystmt_init(stmt);

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);

  id = 7; 
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  fprintf(stdout,"\n row 1: %d(%lu)",id, length);
  myassert(id == 1);

  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
#endif
}

/*
  Generalized conversion routine to handle DATE, TIME and DATETIME
  conversion using MYSQL_TIME structure
*/
static void test_bind_date_conv(uint row_count)
{ 
  MYSQL_STMT   *stmt;
  uint         rc, i, count= row_count;
  ulong	       length[4];
  MYSQL_BIND   bind[4];
  my_bool      is_null[4]={0};
  MYSQL_TIME   tm[4]; 
  ulong        second_part;
  uint         year, month, day, hour, minute, sec;

  stmt = mysql_prepare(mysql,"INSERT INTO test_date VALUES(?,?,?,?)", 100);
  mystmt_init(stmt);

  verify_param_count(stmt, 4);  

  bind[0].buffer_type= MYSQL_TYPE_TIMESTAMP;
  bind[1].buffer_type= MYSQL_TYPE_TIME;
  bind[2].buffer_type= MYSQL_TYPE_DATETIME;
  bind[3].buffer_type= MYSQL_TYPE_DATE;
 
  second_part= 0;

  year=2000;
  month=01;
  day=10;

  hour=11;
  minute=16;
  sec= 20;

  for (i= 0; i < (int) array_elements(bind); i++)
  {  
    bind[i].buffer= (char *) &tm[i];
    bind[i].is_null= &is_null[i];
    bind[i].length= &length[i];
    bind[i].buffer_length= 30;
    length[i]=20;
  }   

  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt,rc);
 
  for (count= 0; count < row_count; count++)
  { 
    for (i= 0; i < (int) array_elements(bind); i++)
    {
      tm[i].neg= 0;   
      tm[i].second_part= second_part+count;
      tm[i].year= year+count;
      tm[i].month= month+count;
      tm[i].day= day+count;
      tm[i].hour= hour+count;
      tm[i].minute= minute+count;
      tm[i].second= sec+count;
    }   
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);    
  }

  rc = mysql_commit(mysql);
  myquery(rc);

  mysql_stmt_close(stmt);

  myassert(row_count == my_stmt_result("SELECT * FROM test_date",50));

  stmt = mysql_prepare(mysql,"SELECT * FROM test_date",50);
  myquery(rc);

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  for (count=0; count < row_count; count++)
  {
    rc = mysql_fetch(stmt);
    mystmt(stmt,rc);

    fprintf(stdout, "\n");
    for (i= 0; i < array_elements(bind); i++)
    {  
      fprintf(stdout, "\n");
      fprintf(stdout," time[%d]: %02d-%02d-%02d %02d:%02d:%02d.%02lu",
                      i, tm[i].year, tm[i].month, tm[i].day, 
                      tm[i].hour, tm[i].minute, tm[i].second,
                      tm[i].second_part);                      

      myassert(tm[i].year == 0 || tm[i].year == year+count);
      myassert(tm[i].month == 0 || tm[i].month == month+count);
      myassert(tm[i].day == 0 || tm[i].day == day+count);

      myassert(tm[i].hour == 0 || tm[i].hour == hour+count);
      /* 
         minute causes problems from date<->time, don't assert, instead
         validate separatly in another routine
       */
      /*myassert(tm[i].minute == 0 || tm[i].minute == minute+count);
      myassert(tm[i].second == 0 || tm[i].second == sec+count);*/

      myassert(tm[i].second_part == 0 || tm[i].second_part == second_part+count);
    }
  }
  rc = mysql_fetch(stmt);
  myassert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/*
  Test DATE, TIME, DATETIME and TS with MYSQL_TIME conversion
*/

static void test_date()
{
  int        rc;

  myheader("test_date");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 TIMESTAMP(14), \
                                                 c2 TIME,\
                                                 c3 DATETIME,\
                                                 c4 DATE)");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(5);
}

/*
  Test all time types to DATE and DATE to all types
*/

static void test_date_date()
{
  int        rc;

  myheader("test_date_date");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 DATE, \
                                                 c2 DATE,\
                                                 c3 DATE,\
                                                 c4 DATE)");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(3);
}

/*
  Test all time types to TIME and TIME to all types
*/

static void test_date_time()
{
  int        rc;

  myheader("test_date_time");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 TIME, \
                                                 c2 TIME,\
                                                 c3 TIME,\
                                                 c4 TIME)");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(3);
}

/*
  Test all time types to TIMESTAMP and TIMESTAMP to all types
*/

static void test_date_ts()
{
  int        rc;

  myheader("test_date_ts");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 TIMESTAMP(10), \
                                                 c2 TIMESTAMP(14),\
                                                 c3 TIMESTAMP,\
                                                 c4 TIMESTAMP(6))");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(2);
}

/*
  Test all time types to DATETIME and DATETIME to all types
*/

static void test_date_dt()
{
  int        rc;

  myheader("test_date_dt");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_date");
  myquery(rc);
  
  rc= mysql_query(mysql,"CREATE TABLE test_date(c1 datetime, \
                                                 c2 datetime,\
                                                 c3 datetime,\
                                                 c4 date)");

  myquery(rc);

  rc = mysql_commit(mysql);
  myquery(rc);

  test_bind_date_conv(2);
}

/*
  Misc tests to keep pure coverage happy
*/
static void test_pure_coverage()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  int        rc;
  ulong      length;
  
  myheader("test_pure_coverage");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_pure");
  myquery(rc);
  
  rc = mysql_query(mysql,"CREATE TABLE test_pure(c1 int, c2 varchar(20))");
  myquery(rc);

  stmt = mysql_prepare(mysql,"insert into test_pure(c67788) values(10)",100);
  mystmt_init_r(stmt);
  
#ifndef DBUG_OFF
  stmt = mysql_prepare(mysql,(const char *)0,0);
  mystmt_init_r(stmt);
  
  stmt = mysql_prepare(mysql,"insert into test_pure(c2) values(10)",100);
  mystmt_init(stmt);

  verify_param_count(stmt, 0);

  rc = mysql_bind_param(stmt, bind);
  mystmt_r(stmt, rc);

  mysql_stmt_close(stmt);
#endif

  stmt = mysql_prepare(mysql,"insert into test_pure(c2) values(?)",100);
  mystmt_init(stmt);

#ifndef DBUG_OFF
  rc = mysql_execute(stmt);
  mystmt_r(stmt, rc);/* No parameters supplied */
#endif

  bind[0].length= &length;
  bind[0].is_null= 0;
  bind[0].buffer_length= 0;

  bind[0].buffer_type= MYSQL_TYPE_GEOMETRY;
  rc = mysql_bind_param(stmt, bind);
  mystmt_r(stmt, rc); /* unsupported buffer type */
  
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  rc = mysql_bind_param(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_send_long_data(stmt, 20, (char *)"venu", 4);
  mystmt_r(stmt, rc); /* wrong param number */

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc); 

  mysql_stmt_close(stmt);

  stmt = mysql_prepare(mysql,"select * from test_pure",100);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

#ifndef DBUG_OFF
  rc = mysql_bind_result(stmt, (MYSQL_BIND *)0);
  mystmt_r(stmt, rc);
  
  bind[0].buffer_type= MYSQL_TYPE_GEOMETRY;
  rc = mysql_bind_result(stmt, bind);
  mystmt_r(stmt, rc); /* unsupported buffer type */
#endif

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);
  
  rc = mysql_stmt_store_result(stmt);
  mystmt_r(stmt, rc); /* commands out of sync */

  mysql_stmt_close(stmt);

  mysql_query(mysql,"DROP TABLE test_pure");
  mysql_commit(mysql);
}

/*
  test for string buffer fetch
*/

static void test_buffers()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[1];
  int        rc;
  ulong      length;
  my_bool    is_null;
  char       buffer[20];
  
  myheader("test_buffers");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_buffer");
  myquery(rc);
  
  rc = mysql_query(mysql,"CREATE TABLE test_buffer(str varchar(20))");
  myquery(rc);

  rc = mysql_query(mysql,"insert into test_buffer values('MySQL')\
                          ,('Database'),('Open-Source'),('Popular')");
  myquery(rc);
  
  stmt = mysql_prepare(mysql,"select str from test_buffer",100);
  mystmt_init(stmt);
  
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  bind[0].length= &length;
  bind[0].is_null= &is_null;
  bind[0].buffer_length= 1;
  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)buffer;
  
  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  buffer[1]='X';
  rc = mysql_fetch(stmt);
  mystmt(stmt, rc);
  fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  myassert(buffer[0] == 'M');
  myassert(buffer[1] == 'X');
  myassert(length == 5);

  bind[0].buffer_length=8;
  rc = mysql_bind_result(stmt, bind);/* re-bind */
  mystmt(stmt, rc);
  
  rc = mysql_fetch(stmt);
  mystmt(stmt, rc);
  fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  myassert(strncmp(buffer,"Database",8) == 0);
  myassert(length == 8);
  
  bind[0].buffer_length=12;
  rc = mysql_bind_result(stmt, bind);/* re-bind */
  mystmt(stmt, rc);
  
  rc = mysql_fetch(stmt);
  mystmt(stmt, rc);
  fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  myassert(strcmp(buffer,"Open-Source") == 0);
  myassert(length == 11);
  
  bind[0].buffer_length=6;
  rc = mysql_bind_result(stmt, bind);/* re-bind */
  mystmt(stmt, rc);
  
  rc = mysql_fetch(stmt);
  mystmt(stmt, rc);
  fprintf(stdout, "\n data: %s (%lu)", buffer, length);
  myassert(strncmp(buffer,"Popula",6) == 0);
  myassert(length == 7);
  
  mysql_stmt_close(stmt);
}

/* 
 Test the direct query execution in the middle of open stmts 
*/
static void test_open_direct()
{
  MYSQL_STMT  *stmt;
  MYSQL_RES   *result;
  int         rc;
  
  myheader("test_open_direct");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_open_direct");
  myquery(rc);

  rc = mysql_query(mysql,"CREATE TABLE test_open_direct(id int, name char(6))");
  myquery(rc);

  stmt = mysql_prepare(mysql,"INSERT INTO test_open_direct values(10,'mysql')", 100);
  mystmt_init(stmt);

  rc = mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  myassert(0 == my_process_result_set(result));

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  fprintf(stdout, "\n total affected rows: %lld", mysql_stmt_affected_rows(stmt));
  myassert(1 == mysql_stmt_affected_rows(stmt));
  
  rc = mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  myassert(1 == my_process_result_set(result));

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  fprintf(stdout, "\n total affected rows: %lld", mysql_stmt_affected_rows(stmt));
  myassert(1 == mysql_stmt_affected_rows(stmt));
  
  rc = mysql_query(mysql, "SELECT * FROM test_open_direct");
  myquery(rc);

  result = mysql_store_result(mysql);
  mytest(result);

  myassert(2 == my_process_result_set(result));
  mysql_stmt_close(stmt);

  /* run a direct query in the middle of a fetch */
  stmt= mysql_prepare(mysql,"SELECT * FROM test_open_direct",100);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt, rc);

  rc = mysql_query(mysql,"INSERT INTO test_open_direct(id) VALUES(20)");
  myquery_r(rc);

  rc = mysql_stmt_close(stmt);
  mystmt(stmt, rc);

  rc = mysql_query(mysql,"INSERT INTO test_open_direct(id) VALUES(20)");
  myquery(rc);

  /* run a direct query with store result */
  stmt= mysql_prepare(mysql,"SELECT * FROM test_open_direct",100);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = mysql_stmt_store_result(stmt);
  mystmt(stmt, rc);

  rc = mysql_fetch(stmt);
  mystmt(stmt, rc);

  rc = mysql_query(mysql,"drop table test_open_direct");
  myquery(rc);

  rc = mysql_stmt_close(stmt);
  mystmt(stmt, rc);
}

/*
  To test fetch without prior bound buffers
*/
static void test_fetch_nobuffs()
{
  MYSQL_STMT *stmt;
  MYSQL_BIND bind[4];
  char       str[4][50];
  int        rc;

  myheader("test_fetch_nobuffs");

  stmt = mysql_prepare(mysql,"SELECT DATABASE(), CURRENT_USER(), CURRENT_DATE(), CURRENT_TIME()",100);
  mystmt_init(stmt);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
    rc++;
  fprintf(stdout, "\n total rows: %d", rc);
  myassert(rc == 1);

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)str[0];
  bind[0].is_null= 0;
  bind[0].length= 0;
  bind[0].buffer_length= sizeof(str[0]);
  bind[1]= bind[2]= bind[3]= bind[0];
  bind[1].buffer= (char *)str[1];
  bind[2].buffer= (char *)str[2];
  bind[3].buffer= (char *)str[3];

  rc = mysql_bind_result(stmt, bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = 0;
  while (mysql_fetch(stmt) != MYSQL_NO_DATA)
  {
    rc++;
    fprintf(stdout, "\n CURRENT_DATABASE(): %s", str[0]);
    fprintf(stdout, "\n CURRENT_USER()    : %s", str[1]);
    fprintf(stdout, "\n CURRENT_DATE()    : %s", str[2]);
    fprintf(stdout, "\n CURRENT_TIME()    : %s", str[3]);
  }
  fprintf(stdout, "\n total rows: %d", rc);
  myassert(rc == 1);

  mysql_stmt_close(stmt);
}

static struct my_option myctest_long_options[] =
{
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"database", 'D', "Database to use", (char **) &opt_db, (char **) &opt_db,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
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
  {"socket", 'S', "Socket file to use for connection", (char **) &opt_unix_socket,
   (char **) &opt_unix_socket, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"count", 't', "Number of times test to be executed", (char **) &opt_count,
   (char **) &opt_count, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void usage(void)
{
  /*
   *  show the usage string when the user asks for this
  */    
  putc('\n',stdout);
  puts("***********************************************************************\n");
  puts("                Test for client-server protocol 4.1");
  puts("                        By Monty & Venu \n");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,");
  puts("and you are welcome to modify and redistribute it under the GPL license\n");
  puts("                 Copyright (C) 1995-2003 MySQL AB ");
  puts("-----------------------------------------------------------------------\n");
  fprintf(stdout,"usage: %s [OPTIONS]\n\n", my_progname);  
  fprintf(stdout,"\
  -?, --help		Display this help message and exit.\n\
  -D  --database=...    Database name to be used for test.\n\
  -h, --host=...	Connect to host.\n\
  -p, --password[=...]	Password to use when connecting to server.\n");
#ifdef __WIN__
  fprintf(stdout,"\
  -W, --pipe	        Use named pipes to connect to server.\n");
#endif
  fprintf(stdout,"\
  -P, --port=...	Port number to use for connection.\n\
  -S, --socket=...	Socket file to use for connection.\n");
#ifndef DONT_ALLOW_USER_CHANGE
  fprintf(stdout,"\
  -u, --user=#		User for login if not current user.\n");
#endif  
  fprintf(stdout,"\
  -t, --count=...	Execute the test count times.\n");
  puts("***********************************************************************\n");
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case 'p':
    if (argument)
    {
      char *start=argument;
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
        start[1]=0;
    }
    else
      tty_password= 1;
    break;
  case '?':
  case 'I':					/* Info */
    usage();
    exit(0);
    break;
  }
  return 0;
}

static const char *load_default_groups[]= { "client",0 };

static void get_options(int argc, char **argv)
{
  int ho_error;
  load_defaults("my",load_default_groups,&argc,&argv);

  if ((ho_error=handle_options(&argc,&argv, myctest_long_options, 
                               get_one_option)))
    exit(ho_error);

  /*free_defaults(argv);*/
  if (tty_password)
    opt_password=get_tty_password(NullS);
  if (!opt_count)
    opt_count= 1;
  return;
}

/*
  Print the test output on successful execution before exiting
*/

static void print_test_output()
{
  fprintf(stdout,"\n\n");
  fprintf(stdout,"All '%d' tests were successful (in '%d' iterations)", 
                 test_count-1, opt_count);
  fprintf(stdout,"\n  Total execution time: %g SECS", total_time);
  if (opt_count > 1)
    fprintf(stdout," (Avg: %g SECS)", total_time/opt_count);
  
  fprintf(stdout,"\n\n!!! SUCCESS !!!\n");
}

/********************************************************
* main routine                                          *
*********************************************************/
int main(int argc, char **argv)
{  
  MY_INIT(argv[0]);
  get_options(argc,argv);
    
  client_connect();       /* connect to server */
  
  total_time= 0;
  for (iter_count=1; iter_count <= opt_count; iter_count++) 
  {
    /* Start of tests */
    test_count= 1;
   
    start_time= time((time_t *)0);
   
    test_fetch_nobuffs();   /* to fecth without prior bound buffers */
    test_open_direct();     /* direct execution in the middle of open stmts */
    test_fetch_null();      /* to fetch null data */
    test_fetch_date();      /* to fetch date,time and timestamp */
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
    test_select_simple();   /* simple select prepare */
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
    client_query();         /* simple client query test */
    client_store_result();  /* usage of mysql_store_result() */
    client_use_result();    /* usage of mysql_use_result() */  
    test_tran_bdb();        /* transaction test on BDB table type */
    test_tran_innodb();     /* transaction test on InnoDB table type */ 
    test_prepare_ext();     /* test prepare with all types conversion -- TODO */
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
    test_multi_stmt();      /* multi stmt test -TODO*/
    test_multi_query();     /* test multi query execution */
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

    end_time= time((time_t *)0);
    total_time+= difftime(end_time, start_time);
    /* End of tests */
  }
  
  client_disconnect();    /* disconnect from server */
  print_test_output();
  
  return(0);
}

