/***************************************************************************
                           client_test.c  -  description
                             -------------------------
    begin                : Sun Feb 3 2002
    copyright            : (C) MySQL AB 1995-2002, www.mysql.com
    author               : venu ( venu@mysql.com )
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This is a test sample to test the new features in MySQL client-server  * 
 *  protocol                                                               *
 *                                                                         *
 ***************************************************************************/

#include <my_global.h>

#if defined(__WIN__) || defined(_WIN32) || defined(_WIN64)
#include  <windows.h>
#endif

/* standrad headers */
#include <stdio.h>
#include <string.h>

#include <assert.h>


/* mysql client headers */
#include <my_sys.h>
#include <mysql.h>
#include <my_getopt.h>

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

/* set default options */
static char *opt_db=(char *)"test";
static char *opt_user=(char *)"root";
static char *opt_password=(char *)"";
static char *opt_host=0;
static char *opt_unix_socket=0;
static uint  opt_port;
static my_bool tty_password=0;

#define myheader(str) { printf("\n\n#######################\n"); \
                        printf("%s",str); \
                        printf("\n#######################\n"); \
                      }

#define init_bind(x) (bzero(x,sizeof(x)))

void print_error(MYSQL *mysql, const char *msg)
{  
  if(mysql)
  {
    fprintf(stderr,"\n [MySQL]%s \n",mysql_error(mysql));
  }
  else if(msg) fprintf(stderr, "%s\n", msg);
}

void print_st_error(MYSQL_STMT *stmt, const char *msg)
{  
  if(stmt)
  {
    fprintf(stderr,"\n [MySQL]%s \n",mysql_stmt_error(stmt));
  }
  else if(msg) fprintf(stderr, "%s\n", msg);
}

#define myerror(mysql, msg) print_error(mysql, msg)
#define mysterror(stmt, msg) print_st_error(stmt, msg)

#define myassert(x) if(x) {\
  fprintf(stderr,"ASSERTION FAILED AT %d@%s\n",__LINE__, __FILE__);\
  exit(1);\
}
#define myassert_r(x) if(!x) {\
  fprintf(stderr,"ASSERTION FAILED AT %d@%s\n",__LINE__, __FILE__);\
  exit(1);\
}

#define myquery(mysql,r) \
if( r != 0) \
{ \
  myerror(mysql,NULL); \
  myassert(true);\
}

#define myquery_r(mysql,r) \
if( r != 0) \
{ \
  myerror(mysql,NULL); \
  myassert_r(true);\
}

#define mystmt(stmt,r) \
if( r != 0) \
{ \
  mysterror(stmt,NULL); \
  myassert(true);\
}

#define myxquery(mysql,stmt) \
if( stmt == 0) \
{ \
  myerror(mysql,NULL); \
  myassert(true);\
}

#define myxquery_r(mysql,stmt) \
if( stmt == 0) \
{ \
  myerror(mysql,NULL); \
  myassert_r(true);\
} \
else myassert(true);

#define mystmt_r(stmt,r) \
if( r != 0) \
{ \
  mysterror(stmt,NULL); \
  myassert_r(true);\
}

#define mytest(mysql,x) if(!x) {myerror(mysql,NULL);myassert(true);}
#define mytest_r(mysql,x) if(x) {myerror(mysql,NULL);myassert(true);}

/********************************************************
* connect to the server                                 *
*********************************************************/
MYSQL *client_connect()
{
  MYSQL *mysql;

  myheader("client_connect");  

  if(!(mysql = mysql_init(NULL)))
  { 
	  myerror(NULL, "mysql_init() failed");
    exit(0);
  }
  if (!(mysql_real_connect(mysql,opt_host,opt_user,
			   opt_password, opt_db, opt_port,
			   opt_unix_socket, 0)))
  {
    myerror(mysql, "connection failed");
    exit(0);
  }    

  /* set AUTOCOMMIT to ON*/
  mysql_autocommit(mysql, true);
  return(mysql);
}

/********************************************************
* close the connection                                  *
*********************************************************/
void client_disconnect(MYSQL *mysql)
{
  myheader("client_disconnect");

  mysql_close(mysql);
}

/********************************************************
* query processing                                      *
*********************************************************/
void client_query(MYSQL *mysql)
{
  int rc;

  myheader("client_query");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS myclient_test");
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE myclient_test(id int primary key auto_increment,\
                                              name varchar(20))");
  myquery(mysql,rc);
  
  rc = mysql_query(mysql,"CREATE TABLE myclient_test(id int, name varchar(20))");
  myquery_r(mysql,rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('mysql')");
  myquery(mysql,rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('monty')");
  myquery(mysql,rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('venu')");
  myquery(mysql,rc);
  
  rc = mysql_query(mysql,"INSERT INTO myclient_test(name) VALUES('deleted')");
  myquery(mysql,rc);

  rc = mysql_query(mysql,"UPDATE myclient_test SET name='updated' WHERE name='deleted'");
  myquery(mysql,rc);
  
  rc = mysql_query(mysql,"UPDATE myclient_test SET id=3 WHERE name='updated'");
  myquery_r(mysql,rc);
}

/********************************************************
* print dashes                                          *
*********************************************************/
void my_print_dashes(MYSQL_RES *result)
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
void my_print_result_metadata(MYSQL_RES *result)
{
  MYSQL_FIELD  *field;
  unsigned int i,j;
  unsigned int field_count;

  mysql_field_seek(result,0);
  fputc('\n', stdout);  

  field_count = mysql_num_fields(result);
  for(i=0; i< field_count; i++)
  {
    field = mysql_fetch_field(result);
    j = strlen(field->name);
    if(j < field->max_length)
      j = field->max_length;
    if(j < 4 && !IS_NOT_NULL(field->flags))
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
    fprintf(stdout, " %-*s |",field->max_length, field->name);
  }
  fputc('\n', stdout);
  my_print_dashes(result);
}

/********************************************************
* process the result set                                *
*********************************************************/
int my_process_result_set(MYSQL *mysql, MYSQL_RES *result)
{
  MYSQL_ROW    row;
  MYSQL_FIELD  *field;
  unsigned int i;
  unsigned int row_count=0;

  my_print_result_metadata(result);

  while((row = mysql_fetch_row(result)) != NULL)
  {   
    mysql_field_seek(result,0);
    fputc('\t',stdout);
    fputc('|',stdout);

    for(i=0; i< mysql_num_fields(result); i++)
    {
      field = mysql_fetch_field(result);
      if(row[i] == NULL)
        fprintf(stdout, " %-*s |", field->max_length, "NULL");
      else if (IS_NUM(field->type))
        fprintf(stdout, " %*s |", field->max_length, row[i]);
      else
        fprintf(stdout, " %-*s |", field->max_length, row[i]);
    }
    fputc('\t',stdout);
    fputc('\n',stdout);
    row_count++;
  }
  my_print_dashes(result);

  if (mysql_errno(mysql) != 0)
    fprintf(stderr, "\n\tmysql_fetch_row() failed\n");
  else 
    fprintf(stdout,"\n\t%d rows returned\n", row_count);
  return(row_count);
}

/********************************************************
* store result processing                               *
*********************************************************/
void client_store_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  int       rc;
  
  myheader("client_store_result");

  rc = mysql_query(mysql, "SELECT * FROM myclient_test");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);
  mysql_free_result(result);
}

/********************************************************
* use result processing                                 *
*********************************************************/
void client_use_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  int       rc;
  myheader("client_use_result");

  rc = mysql_query(mysql, "SELECT * FROM myclient_test");    
  myquery(mysql,rc);

  /* get the result */
  result = mysql_use_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);
  mysql_free_result(result);
}


/********************************************************
* query processing                                      *
*********************************************************/
void test_debug_example(MYSQL *mysql)
{
  int rc;
  MYSQL_RES *result;

  myheader("test_debug_example");

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_debug_example");
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_debug_example(id int primary key auto_increment,\
                                              name varchar(20),xxx int)");
  myquery(mysql,rc);
  
  rc = mysql_query(mysql,"INSERT INTO test_debug_example(name) VALUES('mysql')");
  myquery(mysql,rc);

  rc = mysql_query(mysql,"UPDATE test_debug_example SET name='updated' WHERE name='deleted'");
  myquery(mysql,rc);

  rc = mysql_query(mysql,"SELECT * FROM test_debug_example");
  myquery(mysql,rc);
    
  result = mysql_use_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);  
  mysql_free_result(result);

  rc = mysql_query(mysql,"DROP TABLE test_debug_example");
  myquery(mysql,rc);
}

/********************************************************
* to test autocommit feature                            *
*********************************************************/
void test_tran_bdb(MYSQL *mysql)
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_bdb");

  /* set AUTOCOMMIT to OFF */
  rc = mysql_autocommit(mysql, false);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_demo_transaction");  
  myquery(mysql,rc);  
    
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc = mysql_query(mysql,"CREATE TABLE my_demo_transaction(col1 int ,col2 varchar(30)) TYPE = BDB");
  myquery(mysql,rc);  

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(10,'venu')");
  myquery(mysql,rc);  

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(20,'mysql')");
  myquery(mysql,rc);  

  rc = mysql_rollback(mysql);
  myquery(mysql,rc);

  /* delete first row, and rollback it */
  rc = mysql_query(mysql,"DELETE FROM my_demo_transaction WHERE col1 = 10");
  myquery(mysql,rc); 

  rc = mysql_rollback(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);  

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_use_result(mysql);
  mytest(mysql,result);
  
  row = mysql_fetch_row(result);
  mytest(mysql,row);
  
  row = mysql_fetch_row(result);
  mytest_r(mysql,row);

  mysql_free_result(result); 
  mysql_autocommit(mysql,true);
}

/********************************************************
* to test autocommit feature                            *
*********************************************************/
void test_tran_innodb(MYSQL *mysql)
{
  MYSQL_RES *result;
  MYSQL_ROW row;
  int       rc;

  myheader("test_tran_innodb");

  /* set AUTOCOMMIT to OFF */
  rc = mysql_autocommit(mysql, false);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_demo_transaction");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* create the table 'mytran_demo' of type BDB' or 'InnoDB' */
  rc = mysql_query(mysql,"CREATE TABLE my_demo_transaction(col1 int ,col2 varchar(30)) TYPE = InnoDB");
  myquery(mysql,rc);  

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(10,'venu')");
  myquery(mysql,rc);  

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO my_demo_transaction VALUES(20,'mysql')");
  myquery(mysql,rc);  

  rc = mysql_rollback(mysql);
  myquery(mysql,rc);

  /* delete first row, and rollback it */
  rc = mysql_query(mysql,"DELETE FROM my_demo_transaction WHERE col1 = 10");
  myquery(mysql,rc); 

  rc = mysql_rollback(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);  

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_demo_transaction");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_use_result(mysql);
  mytest(mysql,result);
  
  row = mysql_fetch_row(result);
  mytest(mysql,row);
  
  row = mysql_fetch_row(result);
  mytest_r(mysql,row);

  mysql_free_result(result);
  mysql_autocommit(mysql,true);    
}

/********************************************************
* to test simple prepares of all DML statements         *
*********************************************************/
void test_prepare_simple(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count,length;
  const char *query;

  myheader("test_prepare_simple"); 
  
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_simple");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_simple(id int, name varchar(50))");
  myquery(mysql,rc);  

  /* alter table */
  query = "ALTER TABLE test_prepare_simple ADD new char(20)";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in alter:%d\n", param_count);
  assert(param_count == 0);
  mysql_stmt_close(stmt);

  /* insert */
  query = "INSERT INTO test_prepare_simple VALUES(?,?)";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in insert:%d\n", param_count);
  assert(param_count == 2);
  mysql_stmt_close(stmt);

  /* update */
  query = "UPDATE test_prepare_simple SET id=? WHERE id=? AND name= ?";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in update:%d\n", param_count);
  assert(param_count == 3);
  mysql_stmt_close(stmt);

  /* delete */
  query = "DELETE FROM test_prepare_simple WHERE id=10";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in delete:%d\n", param_count);
  assert(param_count == 0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  mysql_stmt_close(stmt);

  /* delete */
  query = "DELETE FROM test_prepare_simple WHERE id=?";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in delete:%d\n", param_count);
  assert(param_count == 1);

  rc = mysql_execute(stmt);
  mystmt_r(stmt, rc);
  mysql_stmt_close(stmt);

  /* select */
  query = "SELECT * FROM test_prepare_simple WHERE id=? AND name= ?";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in select:%d\n", param_count);
  assert(param_count == 2);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);
}


/********************************************************
* to test simple prepare field results                  *
*********************************************************/
void test_prepare_field_result(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count,length;
  const char *query;

  myheader("test_prepare_field_result"); 
 
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_field_result");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_field_result(id int, name varchar(50), extra int)");
  myquery(mysql,rc);  


  /* insert */
  query = "SELECT id,name FROM test_prepare_field_result WHERE id=?";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout,"\n total parameters in insert:%d\n", param_count);
  assert(param_count == 1);
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);
}


/********************************************************
* to test simple prepare field results                  *
*********************************************************/
void test_prepare_syntax(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,length;
  const char *query;

  myheader("test_prepare_syntax"); 
 
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_syntax");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_syntax(id int, name varchar(50), extra int)");
  myquery(mysql,rc); 

  query = "INSERT INTO test_prepare_syntax VALUES(?";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery_r(mysql,stmt);    

  query = "SELECT id,name FROM test_prepare_syntax WHERE id=? AND WHERE";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery_r(mysql,stmt);   

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);
}


/********************************************************
* to test simple prepare                                *
*********************************************************/
void test_prepare(MYSQL *mysql)
{  
  MYSQL_STMT *stmt;
  int        rc,param_count;
  char       query[200];
  int        int_data;
  char       str_data[50];
  char       tiny_data;
  short      small_data;
  longlong   big_data;
  double     real_data;
  double     double_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[8];  

  myheader("test_prepare"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql, true);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_prepare");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE my_prepare(col1 tinyint,\
                                col2 varchar(50), col3 int,\
                                col4 smallint, col5 bigint, \
                                col6 float, col7 double )");
  myquery(mysql,rc);  

  /* insert by prepare */
  strcpy(query,"INSERT INTO my_prepare VALUES(?,?,?,?,?,?,?)");
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 7);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(gptr)&tiny_data;
  /* string */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=str_data;
  /* integer */
  bind[2].buffer_type=FIELD_TYPE_LONG;
  bind[2].buffer= (gptr)&int_data;	
  /* short */
  bind[3].buffer_type=FIELD_TYPE_SHORT;
  bind[3].buffer= (gptr)&small_data;	
  /* bigint */
  bind[4].buffer_type=FIELD_TYPE_LONGLONG;
  bind[4].buffer= (gptr)&big_data;		
  /* float */
  bind[5].buffer_type=FIELD_TYPE_DOUBLE;
  bind[5].buffer= (gptr)&real_data;		
  /* double */
  bind[6].buffer_type=FIELD_TYPE_DOUBLE;
  bind[6].buffer= (gptr)&double_data;	
      
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
    bind[1].buffer_length = sprintf(str_data,"MySQL%d",int_data);
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
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_prepare");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert((int)tiny_data == my_process_result_set(mysql,result));  
  mysql_free_result(result);

}

/********************************************************
* to test double comparision                            *
*********************************************************/
void test_double_compare(MYSQL *mysql)
{  
  MYSQL_STMT *stmt;
  int        rc,param_count;
  char       query[200],real_data[10], tiny_data;
  double     double_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[3];  

  myheader("test_double_compare"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql, true);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_double_compare");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_double_compare(col1 tinyint,\
                                col2 float, col3 double )");
  myquery(mysql,rc);  

  rc = mysql_query(mysql,"INSERT INTO test_double_compare VALUES(1,10.2,34.5)");
  myquery(mysql,rc);  

  strcpy(query, "UPDATE test_double_compare SET col1=100 WHERE col1 = ? AND col2 = ? AND COL3 = ?");
  stmt = mysql_prepare(mysql,query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in update:%d\n", param_count);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(gptr)&tiny_data;
  /* string->float */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer= (gptr)&real_data;		
  /* double */
  bind[2].buffer_type=FIELD_TYPE_DOUBLE;
  bind[2].buffer= (gptr)&double_data;	
      
  tiny_data = 1;
  strcpy(real_data,"10.2");
  double_data = 34.5;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  rc = (int)mysql_affected_rows(mysql);
  printf("\n total affected rows:%d",rc);
  
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_double_compare");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert((int)tiny_data == my_process_result_set(mysql,result));  
  mysql_free_result(result);

}




/********************************************************
* to test simple null                                   *
*********************************************************/
void test_null(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  const char *query;
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  

  myheader("test_null"); 

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_null");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_null(col1 int,col2 varchar(50))");
  myquery(mysql,rc);  

  /* insert by prepare, wrong column name */
  query = "INSERT INTO test_null(col3,col2) VALUES(?,?)";
  nData = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery_r(mysql,stmt);   

  query = "INSERT INTO test_null(col1,col2) VALUES(?,?)";
  nData = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 2);

  bind[0].is_null=1;
  bind[1].is_null=1;		/* string data */
      
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (nData=0; nData<9; nData++)
  {
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }
  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_null");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(nData == my_process_result_set(mysql,result));  
  mysql_free_result(result);

}



/********************************************************
* to test simple select                                 *
*********************************************************/
void test_select_simple(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,length;
  const char query[100];
  MYSQL_RES  *result;

  myheader("test_select_simple"); 


  /* insert by prepare */
  strcpy((char *)query, "SHOW TABLES FROM mysql");
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  length = mysql_param_count(stmt);
  fprintf(stdout," total parameters in select:%d\n", length);
  assert(length == 0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);  
  mysql_free_result(result);
  
#if 0
  strcpy((char *)query , "SELECT @@ VERSION");
  length = strlen(query);
  rc = mysql_query(mysql,query);
  mytest(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);  
  mysql_free_result(result);
#endif
}


/********************************************************
* to test simple select                                 *
*********************************************************/
void test_select(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count=0;
  const char *query;
  char       *szData=(char *)"updated-value";
  int        nData=1;
  MYSQL_BIND bind[2];  
  MYSQL_RES  *result;
  

  myheader("test_select"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_select");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_select(id int,name varchar(50))");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* insert a row and commit the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(10,'venu')");
  myquery(mysql,rc);  

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

    /* now insert the second row, and rollback the transaction */
  rc = mysql_query(mysql,"INSERT INTO test_select VALUES(20,'mysql')");
  myquery(mysql,rc);  

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  query = "SELECT * FROM test_select WHERE id=? AND name=?";
  nData = strlen(query);  
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in select:%d\n", param_count);
  assert(param_count == 2);

  /* string data */
  nData=10;
  szData=(char *)"venu";
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;
  bind[0].buffer=(gptr)&nData;
  bind[0].buffer_type=FIELD_TYPE_LONG;
      
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert( 1 == my_process_result_set(mysql,result));  
  mysql_free_result(result);

  mysql_stmt_close(stmt);

  /* bit complicated SELECT */
}




/********************************************************
* to test simple update                                *
*********************************************************/
void test_simple_update(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  const char *query;
  char       *szData=(char *)"updated-value";
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  
  

  myheader("test_simple_update"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_update");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_update(col1 int,\
                                col2 varchar(50), col3 int )");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"INSERT INTO test_update VALUES(1,'MySQL',100)");
  myquery(mysql,rc);  
  
  assert(1 == mysql_affected_rows(mysql));

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* insert by prepare */
  query = "UPDATE test_update SET col2=? WHERE col1=?";
  nData = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in update:%d\n", param_count);
  assert(param_count == 2);

  nData=1;
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;		/* string data */
  bind[1].buffer=(gptr)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;
      
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  assert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_update");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(1 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}


/********************************************************
* to test simple long data handling                     *
*********************************************************/
void test_long_data(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  const char *query;
  char       *data=NullS;
  int        length;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  
  

  myheader("test_long_data"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data(col1 int,\
                                col2 long varchar, col3 long varbinary)");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  query = "INSERT INTO test_long_data(col2) VALUES(?)";
  length=strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);     

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 1);

  bind[0].buffer=data;		  /* string data */
  bind[0].is_long_data=1;   /* specify long data suppy during run-time */

  /* Non string or binary type, error */
  bind[0].buffer_type=FIELD_TYPE_LONG;
  rc = mysql_bind_param(stmt,bind);
  fprintf(stdout,"mysql_bind_param() returned %d\n",rc);
  mystmt_r(stmt, rc);
  
  bind[0].buffer_type=FIELD_TYPE_STRING;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  assert(rc == MYSQL_NEED_DATA);

  data = (char *)"Micheal";

  /* supply data in pieces */
  rc = mysql_send_long_data(stmt,0,data,7);
  mystmt(stmt, rc);

  /* try to execute mysql_execute() now, it should return 
     MYSQL_NEED_DATA as the long data supply is not yet over 
  */
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  assert(rc == MYSQL_NEED_DATA);

  /* append data again ..*/

  /* supply data in pieces */
  data = (char *)" 'monty' widenius";
  rc = mysql_send_long_data(stmt,0,data,17);
  mystmt(stmt, rc);

  /* Indiate end of data supply */
  rc = mysql_send_long_data(stmt,0,0,MYSQL_LONG_DATA_END);
  mystmt(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT col2 FROM test_long_data");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(1 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}

/********************************************************
* to test long data (string) handling                   *
*********************************************************/
void test_long_data_str(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  const char *query;
  char       data[255];
  int        length;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  
  

  myheader("test_long_data_str"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_str");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_str(id int, longstr long varchar)");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  query = "INSERT INTO test_long_data_str VALUES(?,?)";
  length=strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);     

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 2);

  bind[0].buffer = (gptr)&length;
  bind[0].buffer_type = FIELD_TYPE_LONG;

  bind[1].buffer=data;		  /* string data */
  bind[1].is_long_data=1;   /* specify long data suppy during run-time */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  length = 10;
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  assert(rc == MYSQL_NEED_DATA);

  length = 40;
  sprintf(data,"MySQL AB");

  /* supply data in pieces */
  {
    int i;
    for(i=0; i < 4; i++)
    {
      rc = mysql_send_long_data(stmt,1,(char *)data,5);
      mystmt(stmt, rc);
    }

    /* try to execute mysql_execute() now, it should return 
       MYSQL_NEED_DATA as the long data supply is not yet over 
    */
    rc = mysql_execute(stmt);
    fprintf(stdout,"mysql_execute() returned %d\n",rc);
    assert(rc == MYSQL_NEED_DATA);
  }

  /* Indiate end of data supply */
  rc = mysql_send_long_data(stmt,1,0,MYSQL_LONG_DATA_END);
  mystmt(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longstr), longstr FROM test_long_data_str");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(1 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}


/********************************************************
* to test long data (string) handling                   *
*********************************************************/
void test_long_data_str1(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  const char *query;
  char       *data=(char *)"MySQL AB";
  int        length;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  
  

  myheader("test_long_data_str1"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_str");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_str(longstr long varchar,blb long varbinary)");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  query = "INSERT INTO test_long_data_str VALUES(?,?)";
  length=strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);     

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 2);

  bind[0].buffer=data;		  /* string data */
  bind[0].is_long_data=1;   /* specify long data suppy during run-time */
  bind[0].buffer_type=FIELD_TYPE_STRING;

  bind[1] = bind[0];
  bind[1].buffer_type=FIELD_TYPE_BLOB;

  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  length = 10;
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  assert(rc == MYSQL_NEED_DATA);

  length = strlen(data);

  /* supply data in pieces */
  {
    int i;
    for(i=0; i < 2; i++)
    {
      rc = mysql_send_long_data(stmt,0,data,length);
      mystmt(stmt, rc);

      rc = mysql_send_long_data(stmt,1,data,2);
      mystmt(stmt, rc);
    }
    /* try to execute mysql_execute() now, it should return 
       MYSQL_NEED_DATA as the long data supply is not yet over 
    */
    rc = mysql_execute(stmt);
    fprintf(stdout,"mysql_execute() returned %d\n",rc);
    assert(rc == MYSQL_NEED_DATA);
  }

  /* Indiate end of data supply */
  rc = mysql_send_long_data(stmt,1,0,MYSQL_LONG_DATA_END);
  mystmt(stmt, rc);
  
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  assert(rc == MYSQL_NEED_DATA);

  rc = mysql_send_long_data(stmt,0,0,MYSQL_LONG_DATA_END);
  mystmt(stmt, rc);
  
  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longstr),longstr,LENGTH(blb),blb FROM test_long_data_str");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(1 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}


/********************************************************
* to test long data (binary) handling                   *
*********************************************************/
void test_long_data_bin(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  const char *query;
  char       data[255];
  int        length;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  
  

  myheader("test_long_data_bin"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_long_data_bin");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_long_data_bin(id int, longbin long varbinary)");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  query = "INSERT INTO test_long_data_bin VALUES(?,?)";
  length=strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);     

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 2);

  bind[0].buffer = (gptr)&length;
  bind[0].buffer_type = FIELD_TYPE_LONG;

  bind[1].buffer=data;		  /* string data */
  bind[1].is_long_data=1;   /* specify long data suppy during run-time */
  bind[1].buffer_type=FIELD_TYPE_LONG_BLOB;
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  length = 10;
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  assert(rc == MYSQL_NEED_DATA);

  sprintf(data,"MySQL AB");

  /* supply data in pieces */
  {
    int i;
    for(i=0; i < 100; i++)
    {
      rc = mysql_send_long_data(stmt,1,(char *)data,4);
      mystmt(stmt, rc);
    }

    /* try to execute mysql_execute() now, it should return 
       MYSQL_NEED_DATA as the long data supply is not yet over 
    */
    rc = mysql_execute(stmt);
    fprintf(stdout,"mysql_execute() returned %d\n",rc);
    assert(rc == MYSQL_NEED_DATA);
  }

  /* Indiate end of data supply */
  rc = mysql_send_long_data(stmt,1,0,MYSQL_LONG_DATA_END);
  mystmt(stmt, rc);

  /* execute */
  rc = mysql_execute(stmt);
  fprintf(stdout,"mysql_execute() returned %d\n",rc);
  mystmt(stmt,rc);

  mysql_stmt_close(stmt);

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* now fetch the results ..*/
  rc = mysql_query(mysql,"SELECT LENGTH(longbin), longbin FROM test_long_data_bin");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(1 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}


/********************************************************
* to test simple delete                                 *
*********************************************************/
void test_simple_delete(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  const char *query;
  char       szData[30]={0};
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  
  

  myheader("test_simple_delete"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_simple_delete");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_simple_delete(col1 int,\
                                col2 varchar(50), col3 int )");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"INSERT INTO test_simple_delete VALUES(1,'MySQL',100)");
  myquery(mysql,rc);  
  
  assert(1 == mysql_affected_rows(mysql));

  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* insert by prepare */
  query = "DELETE FROM test_simple_delete WHERE col1=? AND col2=? AND col3=100";
  nData = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in delete:%d\n", param_count);
  assert(param_count == 2);

  nData=1;
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;		/* string data */  
  bind[0].buffer=(gptr)&nData;
  bind[0].buffer_type=FIELD_TYPE_LONG;
      
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  strcpy(szData,"MySQL");
  //bind[1].buffer_length = 5;
  nData=1;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  assert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_simple_delete");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(0 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}



/********************************************************
* to test simple update                                 *
*********************************************************/
void test_update(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  const char *query;
  char       *szData=(char *)"updated-value";
  int        nData=1;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  
  

  myheader("test_update"); 

  init_bind(bind);
  rc = mysql_autocommit(mysql,true);
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_update");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_update(col1 int primary key auto_increment,\
                                col2 varchar(50), col3 int )");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  query = "INSERT INTO test_update(col2,col3) VALUES(?,?)";
  nData = strlen(query);  
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 2);

  /* string data */
  szData=(char *)"inserted-data";
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;
  bind[1].buffer=(gptr)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;
      
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  nData=100;
  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  
  assert(1 == mysql_affected_rows(mysql));
  mysql_stmt_close(stmt);

  /* insert by prepare */
  query = "UPDATE test_update SET col2=? WHERE col3=?";
  nData = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in update:%d\n", param_count);
  assert(param_count == 2);
  nData=100;szData=(char *)"updated-data";

  
  bind[0].buffer_type=FIELD_TYPE_STRING;
  bind[0].buffer=szData;
  bind[1].buffer=(gptr)&nData;
  bind[1].buffer_type=FIELD_TYPE_LONG;

      
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);
  assert(1 == mysql_affected_rows(mysql));

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_update");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(1 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}


/********************************************************
* to test simple prepare                                *
*********************************************************/
void test_init_prepare(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        length, param_count, rc;
  const char *query;
  MYSQL_RES  *result;

  myheader("test_init_prepare"); 
  
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS my_prepare");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE my_prepare(col1 int ,col2 varchar(50))");
  myquery(mysql,rc);  
  
 
  /* insert by prepare */
  query = "INSERT INTO my_prepare VALUES(10,'venu')";
  length = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 0);

  rc = mysql_execute(stmt);
  mystmt(stmt, rc);

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM my_prepare");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(1 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}


/********************************************************
* to test simple bind result                            *
*********************************************************/
void test_bind_result(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc;
  const char query[100];
  int        nData;
  char       szData[100];
  MYSQL_BIND bind[2];  

  myheader("test_bind_result");

  init_bind(bind);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_bind_result");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_bind_result(col1 int ,col2 varchar(50))");
  myquery(mysql,rc);       
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(10,'venu')");
  myquery(mysql,rc);  

  rc = mysql_query(mysql,"INSERT INTO test_bind_result VALUES(20,'MySQL')");
  myquery(mysql,rc);  
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* fetch */   

  bind[0].buffer_type=FIELD_TYPE_LONG;
  bind[0].buffer= (gptr) &nData;	/* integer data */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=szData;		/* string data */
  
  strcpy((char *)query , "SELECT * FROM test_bind_result");
  nData = strlen(query);
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   
  
  rc = mysql_bind_result(stmt,bind);
  mystmt(stmt, rc);  

  rc = mysql_execute(stmt);
  mystmt(stmt, rc); 

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  printf("\n row 1:%d,%s",nData, szData);
  assert(nData == 10);
  assert(strcmp(szData,"venu")==0);

  rc = mysql_fetch(stmt);
  mystmt(stmt,rc);

  printf("\n row 2:%d,%s",nData, szData);
  assert(nData == 20);
  assert(strcmp(szData,"MySQL")==0);

  rc = mysql_fetch(stmt);
  assert(rc == MYSQL_NO_DATA);

  mysql_stmt_close(stmt);
}

/********************************************************
* to test simple prepare with all possible types        *
*********************************************************/
void test_prepare_ext(MYSQL *mysql)
{
  MYSQL_STMT *stmt;
  int        rc,param_count;
  char       *query;
  int        nData=1;
  MYSQL_RES  *result;
  char       tData=1;
  short      sData=10;
  longlong   bData=20;
  MYSQL_BIND bind_int[6];   


  myheader("test_prepare_ext"); 
  
  init_bind(bind_int);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_ext");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  query = (char *)"CREATE TABLE test_prepare_ext\
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

  rc = mysql_query(mysql,query);
  myquery(mysql,rc);  

  /* insert by prepare - all integers */
  query = (char *)"INSERT INTO test_prepare_ext(c1,c2,c3,c4,c5,c6) VALUES(?,?,?,?,?,?)";
  stmt = mysql_prepare(mysql,query);
  myquery(mysql,rc);  

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 6);

  /*tinyint*/
  bind_int[0].buffer_type=FIELD_TYPE_TINY;
  bind_int[0].buffer= (void *)&tData;

  /*smallint*/
  bind_int[1].buffer_type=FIELD_TYPE_SHORT;
  bind_int[1].buffer= (void *)&sData;

  /*mediumint*/
  bind_int[2].buffer_type=FIELD_TYPE_LONG;
  bind_int[2].buffer= (void *)&nData;

  /*int*/
  bind_int[3].buffer_type=FIELD_TYPE_LONG;
  bind_int[3].buffer= (void *)&nData;

  /*integer*/
  bind_int[4].buffer_type=FIELD_TYPE_LONG;
  bind_int[4].buffer= (void *)&nData;

  /*bigint*/
  bind_int[5].buffer_type=FIELD_TYPE_LONGLONG;
  bind_int[5].buffer= (void *)&bData;
  
  rc = mysql_bind_param(stmt,bind_int);
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
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT c1,c2,c3,c4,c5,c6 FROM test_prepare_ext");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(nData == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}




/********************************************************
* to test real and alias names                          *
*********************************************************/
void test_field_names(MYSQL *mysql)
{
  int        rc;
  MYSQL_RES  *result;
  
  myheader("test_field_names"); 

  printf("\n%d,%d,%d",MYSQL_TYPE_DECIMAL,MYSQL_TYPE_NEWDATE,MYSQL_TYPE_ENUM);
  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_names1");  
  myquery(mysql,rc);     

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_names2");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_names1(id int,name varchar(50))");
  myquery(mysql,rc);   

  rc = mysql_query(mysql,"CREATE TABLE test_field_names2(id int,name varchar(50))");
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);
    
  /* with table name included with true column name */
  rc = mysql_query(mysql,"SELECT id as 'id-alias' FROM test_field_names1");
  myquery(mysql,rc);
    
  result = mysql_use_result(mysql);
  mytest(mysql,result);

  assert(0 == my_process_result_set(mysql,result));  
  mysql_free_result(result);  

  /* with table name included with true column name */
  rc = mysql_query(mysql,"SELECT t1.id as 'id-alias',test_field_names2.name FROM test_field_names1 t1,test_field_names2");
  myquery(mysql,rc);
    
  result = mysql_use_result(mysql);
  mytest(mysql,result);

  assert(0 == my_process_result_set(mysql,result));  
  mysql_free_result(result);
}

/********************************************************
* to test warnings                                      *
*********************************************************/
void test_warnings(MYSQL *mysql)
{
  int        rc;
  MYSQL_RES  *result;
  
  myheader("test_warnings"); 

  rc = mysql_query(mysql,"USE test");  
  myquery(mysql,rc);     
  
  rc = mysql_query(mysql,"SHOW WARNINGS");  
  myquery(mysql,rc);     
    
  result = mysql_use_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);  
  mysql_free_result(result);
}

/********************************************************
* to test errors                                        *
*********************************************************/
void test_errors(MYSQL *mysql)
{
  int        rc;
  MYSQL_RES  *result;
  
  myheader("test_errors"); 
  
  rc = mysql_query(mysql,"SHOW ERRORS");  
  myquery(mysql,rc);     
    
  result = mysql_use_result(mysql);
  mytest(mysql,result);

  my_process_result_set(mysql,result);  
  mysql_free_result(result);
}



/********************************************************
* to test simple prepare-insert                         *
*********************************************************/
void test_insert(MYSQL *mysql)
{  
  MYSQL_STMT *stmt;
  int        rc,param_count;
  char       query[200];
  char       str_data[50];
  char       tiny_data;
  MYSQL_RES  *result;
  MYSQL_BIND bind[2];  

  myheader("test_insert"); 

  rc = mysql_autocommit(mysql, true);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prep_insert");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prep_insert(col1 tinyint,\
                                col2 varchar(50))");
  myquery(mysql,rc);  

  /* insert by prepare */
  bzero(bind, sizeof(bind));
  strcpy(query,"INSERT INTO test_prep_insert VALUES(?,?)");
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 2);

  /* tinyint */
  bind[0].buffer_type=FIELD_TYPE_TINY;
  bind[0].buffer=(gptr)&tiny_data;
  /* string */
  bind[1].buffer_type=FIELD_TYPE_STRING;
  bind[1].buffer=str_data;
      
  rc = mysql_bind_param(stmt,bind);
  mystmt(stmt, rc);

  /* now, execute the prepared statement to insert 10 records.. */
  for (tiny_data=0; tiny_data < 3; tiny_data++)
  {
    bind[1].buffer_length = sprintf(str_data,"MySQL%d",tiny_data);
    rc = mysql_execute(stmt);
    mystmt(stmt, rc);
  }

  mysql_stmt_close(stmt);

  /* now fetch the results ..*/
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  /* test the results now, only one row should exists */
  rc = mysql_query(mysql,"SELECT * FROM test_prep_insert");
  myquery(mysql,rc);
    
  /* get the result */
  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert((int)tiny_data == my_process_result_set(mysql,result));  
  mysql_free_result(result);

}

/********************************************************
* to test simple prepare-resultset info                 *
*********************************************************/
void test_prepare_resultset(MYSQL *mysql)
{  
  MYSQL_STMT *stmt;
  int        rc,param_count;
  char       query[200];
  MYSQL_RES  *result;

  myheader("test_prepare_resultset"); 

  rc = mysql_autocommit(mysql, true);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_prepare_resultset");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_prepare_resultset(id int,\
                                name varchar(50),extra double)");
  myquery(mysql,rc);  

  /* insert by prepare */
  strcpy(query,"INSERT INTO test_prepare_resultset(id,name) VALUES(?,?)");
  stmt = mysql_prepare(mysql, query);
  myxquery(mysql,stmt);   

  param_count = mysql_param_count(stmt);
  fprintf(stdout," total parameters in insert:%d\n", param_count);
  assert(param_count == 2);

  rc = mysql_query(mysql,"SELECT * FROM test_prepare_resultset");
  myquery(mysql,rc);  

  /* get the prepared-result */
  result = mysql_prepare_result(stmt);
  assert( result != 0);

  my_print_result_metadata(result);  
  mysql_free_result(result);

  result = mysql_store_result(mysql);
  mytest(mysql,result);

  assert(0 == my_process_result_set(mysql,result));  
  mysql_free_result(result);

  /* get the prepared-result */
  result = mysql_prepare_result(stmt);
  assert( result != 0);

  my_print_result_metadata(result);  
  mysql_free_result(result);
  
  mysql_stmt_close(stmt);
}

/********************************************************
* to test field flags (verify .NET provider)            *
*********************************************************/

void test_field_flags(MYSQL *mysql)
{
  int          rc;
  MYSQL_RES    *result;
  MYSQL_FIELD  *field;
  unsigned int i;

  
  myheader("test_field_flags"); 

  rc = mysql_query(mysql,"DROP TABLE IF EXISTS test_field_flags");  
  myquery(mysql,rc);     
    
  rc = mysql_commit(mysql);
  myquery(mysql,rc);

  rc = mysql_query(mysql,"CREATE TABLE test_field_flags(id int NOT NULL AUTO_INCREMENT PRIMARY KEY,\
                                                        id1 int NOT NULL,\
                                                        id2 int UNIQUE,\
                                                        id3 int,\
                                                        id4 int NOT NULL,\
                                                        id5 int,\
                                                        KEY(id3,id4))");
  myquery(mysql,rc);   

  rc = mysql_commit(mysql);
  myquery(mysql,rc);
  
  /* with table name included with true column name */
  rc = mysql_query(mysql,"SELECT * FROM test_field_flags");
  myquery(mysql,rc);
    
  result = mysql_use_result(mysql);
  mytest(mysql,result);

  mysql_field_seek(result,0);
  fputc('\n', stdout);  

  for(i=0; i< mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    printf("\nfield:%d",i);
    if(field->flags & NOT_NULL_FLAG)
      printf("\n  NOT_NULL_FLAG");
    if(field->flags & PRI_KEY_FLAG)
      printf("\n  PRI_KEY_FLAG");
    if(field->flags & UNIQUE_KEY_FLAG)
      printf("\n  UNIQUE_KEY_FLAG");
    if(field->flags & MULTIPLE_KEY_FLAG)
      printf("\n  MULTIPLE_KEY_FLAG");
    if(field->flags & AUTO_INCREMENT_FLAG)
      printf("\n  AUTO_INCREMENT_FLAG");

  }
  mysql_free_result(result);
}

static struct my_option myctest_long_options[] =
{
  {"help", '?', "Display this help and exit", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"database", 'D', "Database to use", (gptr*) &opt_db, (gptr*) &opt_db,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host", (gptr*) &opt_host, (gptr*) &opt_host, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login if not current user", (gptr*) &opt_user,
   (gptr*) &opt_user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection", (gptr*) &opt_port,
   (gptr*) &opt_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection", (gptr*) &opt_unix_socket,
   (gptr*) &opt_unix_socket, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void usage(void)
{
  /*
   *  show the usage string when the user asks for this
  */    
  puts("***********************************************************************\n");
  puts("                Test for client-server protocol 4.1");
  puts("                        By Monty & Venu \n");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,");
  puts("and you are welcome to modify and redistribute it under the GPL license\n");
  puts("                 Copyright (C) 1995-2002 MySQL AB ");
  puts("-----------------------------------------------------------------------\n");
  printf("usage: %s [OPTIONS]\n\n", my_progname);  
  printf("\
  -?, --help		Display this help message and exit.\n\
  -D  --database=...    Database name to be used for test.\n\
  -h, --host=...	Connect to host.\n\
  -p, --password[=...]	Password to use when connecting to server.\n");
#ifdef __WIN__
  printf("\
  -W, --pipe	        Use named pipes to connect to server.\n");
#endif
  printf("\
  -P, --port=...	Port number to use for connection.\n\
  -S, --socket=...	Socket file to use for connection.\n");
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user.\n");
#endif  
  printf("*********************************************************************\n");
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case 'p':
    if (argument)
    {
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
    }
    else
      tty_password= 1;
    break;
  case '?':
  case 'I':					/* Info */
    usage();
    exit(1);
    break;
  }
  return 0;
}

static const char *load_default_groups[]= { "client",0 };

static void get_options(int argc, char **argv)
{
  int ho_error;

  load_defaults("my",load_default_groups,&argc,&argv);

  if ((ho_error=handle_options(&argc, &argv, myctest_long_options, 
                               get_one_option)))
    exit(ho_error);

  free_defaults(argv);
  if (tty_password)
    opt_password=get_tty_password(NullS);
  return;
}

/********************************************************
* main routine                                          *
*********************************************************/
int main(int argc, char **argv)
{
  MYSQL *mysql;

  
  MY_INIT(argv[0]);
  get_options(argc,argv);  /* don't work -- options : TODO */
    
  mysql = client_connect();  /* connect to server */

  test_select(mysql);        /* simple prepare-select */
  test_insert(mysql);        /* prepare with insert */
  test_bind_result(mysql);   /* result bind test */   
  test_prepare(mysql);       /* prepare test */
  test_prepare_simple(mysql);/* simple prepare */ 
  test_null(mysql);          /* test null data handling */
  test_debug_example(mysql); /* some debugging case */
  test_update(mysql);        /* prepare-update test */
  test_simple_update(mysql); /* simple prepare with update */
  test_long_data(mysql);     /* long data handling in pieces */
  test_simple_delete(mysql); /* prepare with delete */
  test_field_names(mysql);   /* test for field names */
  test_double_compare(mysql);/* float comparision */ 
  client_query(mysql);       /* simple client query test */
  client_store_result(mysql);/* usage of mysql_store_result() */
  client_use_result(mysql);  /* usage of mysql_use_result() */  
  test_tran_bdb(mysql);      /* transaction test on BDB table type */
  test_tran_innodb(mysql);   /* transaction test on InnoDB table type */ 
  test_prepare_ext(mysql);   /* test prepare with all types conversion -- TODO */
  test_prepare_syntax(mysql);/* syntax check for prepares */
  test_prepare_field_result(mysql); /* prepare meta info */
  test_field_names(mysql);   /* test for field names */
  test_field_flags(mysql);   /* test to help .NET provider team */
  test_long_data_str(mysql); /* long data handling */
  test_long_data_str1(mysql);/* yet another long data handling */
  test_long_data_bin(mysql); /* long binary insertion */
  test_warnings(mysql);      /* show warnings test */
  test_errors(mysql);        /* show errors test */
  test_select_simple(mysql); /* simple select prepare */
  test_prepare_resultset(mysql);/* prepare meta info test */

  client_disconnect(mysql);  /* disconnect from server */
  
  fprintf(stdout,"\ndone !!!\n");
  return(0);
}

