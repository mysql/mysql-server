/* Copyright (C) 2005 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   original idea: Brian Aker via playing with ab for too many years
   coded by: Patrick Galbraith
*/


/*
  MySQL Slap

  A simple program designed to work as if multiple clients querying the database,
  then reporting the timing of each stage.

  MySQL slap runs three stages:
  1) Create table (single client)
  2) Insert data (many clients)
  3) Load test (many clients)
  4) Cleanup (disconnection, drop table if specified, single client)

  Examples:

  Supply your own create, insert and query SQL statements, with eight
  clients loading data (eight inserts for each) and 50 clients querying (200
  selects for each):

    mysqlslap --create="CREATE TABLE A (a int)" \
              --data="INSERT INTO A (23)" --load-concurrency=8 --number-rows=8 \
              --query="SELECT * FROM A" --concurrency=50 --iterations=200 \
              --load-concurrency=5

  Let the program build create, insert and query SQL statements with a table
  of two int columns, three varchar columns, with five clients loading data
  (12 inserts each), five clients querying (20 times each), and drop schema
  before creating:

    mysqlslap --concurrency=5 --concurrency-load=5 --iterations=20 \
              --number-int-cols=2 --number-char-cols=3 --number-rows=12 \
              --auto-generate-sql

  Let the program build the query SQL statement with a table of two int
  columns, three varchar columns, five clients querying (20 times each),
  don't create the table or insert the data (using the previous test's
  schema and data):

    mysqlslap --concurrency=5 --iterations=20 \
              --number-int-cols=2 --number-char-cols=3 \
              --number-rows=12 --auto-generate-sql \
              --skip-data-load --skip-create-schema

  Tell the program to load the create, insert and query SQL statements from
  the specified files, where the create.sql file has multiple table creation
  statements delimited by ';', multiple insert statements delimited by ';',
  and multiple queries delimited by ';', run all the load statements with
  five clients (five times each), and run all the queries in the query file
  with five clients (five times each):

    mysqlslap --drop-schema --concurrency=5 --concurrency-load=5 \
              --iterations=5 --query=query.sql --create=create.sql \
              --data=insert.sql --delimiter=";" --number-rows=5

  Same as the last test run, with short options

    mysqlslap -D -c 5 -l 5 -i 5 -q query.sql \
              --create create.sql -d insert.sql -F ";" -n 5

TODO:
  Add language for better tests
  String length for files and those put on the command line are not
  setup to handle binary data.
*/

#define SHOW_VERSION "0.1"

#define HUGE_STRING_LENGTH 8096
#define RAND_STRING_SIZE 126

#include "client_priv.h"
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <mysqld_error.h>
#include <my_dir.h>
#include <signal.h>
#include <stdarg.h>
#include <sslopt-vars.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>


static char **defaults_argv;

static char *host= NULL, *opt_password= NULL, *user= NULL,
            *user_supplied_query= NULL, *user_supplied_data= NULL,
            *create_string= NULL, *default_engine= NULL,
            *opt_mysql_unix_port= NULL;

const char *delimiter= "\n";

const char *create_schema_string= "mysqlslap";

static my_bool opt_preserve_enter= FALSE, opt_preserve_exit= FALSE;

static my_bool opt_only_print= FALSE;

static my_bool opt_compress= FALSE, tty_password= FALSE,
               create_string_alloced= FALSE,
               insert_string_alloced= FALSE, query_string_alloced= FALSE,
               generated_insert_flag= FALSE, opt_silent= FALSE,
               auto_generate_sql= FALSE;

static int verbose, num_int_cols, num_char_cols, delimiter_length;
static char *default_charset= (char*) MYSQL_DEFAULT_CHARSET_NAME;
static unsigned int number_of_rows, number_of_iterations;
static unsigned int actual_insert_rows= 0;
static unsigned int  concurrency, concurrency_load, children_spawned;

const char *default_dbug_option="d:t:o,/tmp/mysqlslap.trace";

static uint opt_protocol= 0;

static int get_options(int *argc,char ***argv);
static uint opt_mysql_port= 0;

static const char *load_default_groups[]= { "mysqlslap","client",0 };

typedef struct statement statement;

struct statement {
  char *string;
  size_t length;
  statement *next;
};

static statement *create_statements= NULL, 
                 *insert_statements= NULL, 
                 *query_statements= NULL;

/* Prototypes */
int parse_delimeter(const char *script, statement **stmt);
static int drop_schema(MYSQL *mysql, const char *db);
unsigned int get_random_string(char *buf);
static int build_table_string(void);
static int build_insert_string(void);
static int build_query_string(void);
static int create_schema(MYSQL *mysql, const char *db, statement *stmt);
static int run_scheduler(statement *stmts,
              int(*task)(statement *stmt), unsigned int concur);
int run_task(statement *stmt);
int load_data(statement *load_stmt);

static const char ALPHANUMERICS[]=
  "0123456789ABCDEFGHIJKLMNOPQRSTWXYZabcdefghijklmnopqrstuvwxyz";

#define ALPHANUMERICS_SIZE (sizeof(ALPHANUMERICS)-1)



/* Return the time in ms between two timevals */
static double timedif (struct timeval end, struct timeval begin)
{
  double seconds;
  DBUG_ENTER("timedif");

  seconds= (double)(end.tv_usec - begin.tv_usec)/1000000;
  DBUG_PRINT("info", ("end.tv_usec %d - begin.tv_usec %d = "
                      "%d microseconds ( fseconds %f)",
                      end.tv_usec, begin.tv_usec,
                      (end.tv_usec - begin.tv_usec),
                      seconds));
  seconds += (double)(end.tv_sec - begin.tv_sec);
  DBUG_PRINT("info", ("end.tv_sec %d - begin.tv_sec %d = "
                      "%d seconds (fseconds %f)",
                      end.tv_sec, begin.tv_sec,
                      (end.tv_sec - begin.tv_sec), seconds));

  DBUG_PRINT("info", ("returning time %f seconds", seconds));
  DBUG_RETURN(seconds);
}


int main(int argc, char **argv)
{
  MYSQL mysql;
  int client_flag= 0;
  double time_difference;
  struct timeval start_time, load_time, run_time;

  DBUG_ENTER("main");
  MY_INIT(argv[0]);

  /* Seed the random number generator if we will be using it. */
  if (auto_generate_sql)
    srandom((unsigned int)time(NULL));

  load_defaults("my",load_default_groups,&argc,&argv);
  defaults_argv=argv;
  if (get_options(&argc,&argv))
  {
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }
  /* globals? Yes, so we only have to run strlen once */
  delimiter_length= strlen(delimiter);

  if (argc > 2)
  {
    fprintf(stderr,"%s: Too many arguments\n",my_progname);
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }
  mysql_init(&mysql);
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
    mysql_ssl_set(&mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
                  opt_ssl_capath, opt_ssl_cipher);
#endif
  if (opt_protocol)
    mysql_options(&mysql,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    mysql_options(&mysql,MYSQL_SHARED_MEMORY_BASE_NAME,shared_memory_base_name);
#endif
  mysql_options(&mysql, MYSQL_SET_CHARSET_NAME, default_charset);

  client_flag|= CLIENT_MULTI_RESULTS;
  if (!opt_only_print) 
  {
    if (!(mysql_real_connect(&mysql,host,user,opt_password,
                             argv[0],opt_mysql_port,opt_mysql_unix_port,
                             client_flag)))
    {
      fprintf(stderr,"%s: %s\n",my_progname,mysql_error(&mysql));
      free_defaults(defaults_argv);
      my_end(0);
      exit(1);
    }
  }

  /*
    We might not want to load any data, such as when we are calling
    a stored_procedure that doesn't use data, or we know we already have
    data in the table.
  */
  if (!opt_preserve_enter)
    drop_schema(&mysql, create_schema_string);

  if (create_statements)
    create_schema(&mysql, create_schema_string, create_statements);

  if (insert_statements)
  {
    gettimeofday(&start_time, NULL);
    run_scheduler(insert_statements, load_data, concurrency_load);
    gettimeofday(&load_time, NULL);
    time_difference= timedif(load_time, start_time);

    if (!opt_silent)
    {
      printf("Seconds to load data: %.5f\n",time_difference);
      printf("Number of clients loading data: %d\n", children_spawned);
      printf("Number of inserts per client: %d\n", number_of_rows * actual_insert_rows);
    }
  }

  if (query_statements)
  {
    gettimeofday(&start_time, NULL);
    run_scheduler(query_statements, run_task, concurrency);
    gettimeofday(&run_time, 0);
    time_difference= timedif(run_time, start_time);

    if (!opt_silent)
    {
      printf("Seconds to run all queries: %.5f\n", time_difference);
      printf("Number of clients running queries: %d\n", children_spawned);
      printf("Number of queries per client: %d\n", number_of_iterations);
    }
  }
  if (!opt_preserve_exit)
    drop_schema(&mysql, create_schema_string);

  if (!opt_only_print) 
    mysql_close(&mysql); /* Close & free connection */

  /* now free all the strings we created */
  if (opt_password)
    my_free(opt_password, MYF(0));
  if (create_string_alloced)
    my_free(create_string, MYF(0));
  if (insert_string_alloced)
    my_free(user_supplied_data, MYF(0));
  if (query_string_alloced)
    my_free(user_supplied_query, MYF(0));

  if (create_statements)
  {
    statement *ptr, *nptr;
    for (ptr= create_statements; ptr;)
    {
      nptr= ptr->next;
      my_free(ptr->string, MYF(0)); 
      my_free((byte *)ptr, MYF(0));
      ptr= nptr;
    }
  }

  if (insert_statements)
  {
    statement *ptr, *nptr;
    for (ptr= insert_statements; ptr;)
    {
      nptr= ptr->next;
      my_free(ptr->string, MYF(0)); 
      my_free((byte *)ptr, MYF(0));
      ptr= nptr;
    }
  }

  if (query_statements)
  {
    statement *ptr, *nptr;
    for (ptr= query_statements; ptr;)
    {
      nptr= ptr->next;
      my_free(ptr->string, MYF(0)); 
      my_free((byte *)ptr, MYF(0));
      ptr= nptr;
    }
  }

#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    my_free(shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
#endif
  free_defaults(defaults_argv);
  my_end(0);

  DBUG_RETURN(0); /* No compiler warnings */
}

static struct my_option my_long_options[] =
{
  {"auto-generate-sql", 'a',
    "Generate SQL where not supplied by file or command line.",
    (gptr*) &auto_generate_sql, (gptr*) &auto_generate_sql,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
    (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
    0, 0, 0},
  {"concurrency-load", 'l', "Number of clients to use when loading data.",
    (gptr*) &concurrency_load, (gptr*) &concurrency_load, 0,
    GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  {"concurrency", 'c', "Number of clients to simulate for query to run.",
    (gptr*) &concurrency, (gptr*) &concurrency, 0, GET_UINT,
    REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  {"create", OPT_CREATE_SLAP_SCHEMA, "File or string to use create tables.",
    (gptr*) &create_string, (gptr*) &create_string, 0, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0},
  {"create-schema", OPT_CREATE_SLAP_SCHEMA, "Schema to run tests in.",
    (gptr*) &create_schema_string, (gptr*) &create_schema_string, 0, GET_STR, 
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"data", 'd',
    "File or string with INSERT to use for populating data.",
    (gptr*) &user_supplied_data, (gptr*) &user_supplied_data, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
    (gptr*) &default_dbug_option, (gptr*) &default_dbug_option, 0, GET_STR,
    OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"delimiter", 'F',
    "Delimiter to use in SQL statements supplied in file or command line.",
    (gptr*) &delimiter, (gptr*) &delimiter, 0, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0},
  {"engine", 'e', "Storage engine to use for creating the table.",
    (gptr*) &default_engine, (gptr*) &default_engine, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", (gptr*) &host, (gptr*) &host, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"iterations", 'i', "Number of iterations.", (gptr*) &number_of_iterations,
    (gptr*) &number_of_iterations, 0, GET_UINT, REQUIRED_ARG,
    1, 0, 0, 0, 0, 0},
  {"number-char-cols", 'x', 
    "Number of INT columns to create table with if specifying --sql-generate-sql.",
    (gptr*) &num_char_cols, (gptr*) &num_char_cols, 0, GET_UINT, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0},
  {"number-int-cols", 'y', 
    "Number of VARCHAR columns to create table with if specifying \
      --sql-generate-sql.", (gptr*) &num_int_cols, (gptr*) &num_int_cols, 0,
    GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"number-rows", 'n', "Number of rows to insert when loading data.", 
    (gptr*) &number_of_rows, (gptr*) &number_of_rows, 0, GET_UINT, 
    REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  {"only-print", OPT_MYSQL_ONLY_PRINT,
    "This causes mysqlslap to not connect to the databases, but instead print \
      out what it would have done instead.",
    (gptr*) &opt_only_print, (gptr*) &opt_only_print, 0, GET_BOOL,  NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"password", 'p',
    "Password to use when connecting to server. If password is not given it's \
      asked from the tty.", 0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_mysql_port,
    (gptr*) &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, MYSQL_PORT, 0, 0, 0, 0,
    0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
    NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"preserve-schema-enter", OPT_MYSQL_PRESERVE_SCHEMA_ENTER,
    "Preserve the schema from the mysqlslap run.",
    (gptr*) &opt_preserve_enter, (gptr*) &opt_preserve_enter, 0, GET_BOOL,
    NO_ARG, 0, 0, 0, 0, 0, 0},
  {"preserve-schema-exit", OPT_MYSQL_PRESERVE_SCHEMA_EXIT,
    "Preserve the schema from the mysqlslap run.",
    (gptr*) &opt_preserve_exit, (gptr*) &opt_preserve_exit, 0, GET_BOOL,
    NO_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL,
    "The protocol of connection (tcp,socket,pipe,memory).",
    0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Run program in silent mode - no output.",
    (gptr*) &opt_silent, (gptr*) &opt_silent, 0, GET_BOOL,  NO_ARG,
    0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
    "Base name of shared memory.", (gptr*) &shared_memory_base_name,
    (gptr*) &shared_memory_base_name, 0, GET_STR_ALLOC, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0},
#endif
  {"query", 'q', "Query to run or file containing query to run.",
    (gptr*) &user_supplied_query, (gptr*) &user_supplied_query,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
    (gptr*) &opt_mysql_unix_port, (gptr*) &opt_mysql_unix_port, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", (gptr*) &user,
    (gptr*) &user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"verbose", 'v',
    "More verbose output; You can use this multiple times to get even more \
      verbose output.", (gptr*) &verbose, (gptr*) &verbose, 0, 
      GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
    NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


#include <help_start.h>

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,SHOW_VERSION,
         MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}


static void usage(void)
{
  print_version();
  puts("Copyright (C) 2005 MySQL AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\
       \nand you are welcome to modify and redistribute it under the GPL \
       license\n");
  puts("Run a query multiple times against the server\n");
  printf("Usage: %s [OPTIONS]\n",my_progname);
  print_defaults("my",load_default_groups);
  my_print_help(my_long_options);
}

#include <help_end.h>

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
               char *argument)
{
  DBUG_ENTER("get_one_option");
  switch(optid) {
#ifdef __NETWARE__
  case OPT_AUTO_CLOSE:
    setscreenmode(SCR_AUTOCLOSE_ON_EXIT);
    break;
#endif
  case 'v':
    verbose++;
    break;
  case 'p':
    if (argument)
    {
      char *start= argument;
      my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
      opt_password= my_strdup(argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
        start[1]= 0;				/* Cut length of argument */
      tty_password= 0;
    }
    else
      tty_password= 1;
    break;
  case 'W':
#ifdef __WIN__
    opt_protocol= MYSQL_PROTOCOL_PIPE;
#endif
    break;
  case OPT_MYSQL_PROTOCOL:
    {
      if ((opt_protocol= find_type(argument, &sql_protocol_typelib,0)) <= 0)
      {
        fprintf(stderr, "Unknown option to protocol: %s\n", argument);
        exit(1);
      }
      break;
    }
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
#include <sslopt-case.h>
  case 'V':
    print_version();
    exit(0);
    break;
  case '?':
  case 'I':					/* Info */
    usage();
    exit(0);
  }
  DBUG_RETURN(0);
}


unsigned int
get_random_string(char *buf)
{
  char *buf_ptr= buf;
  int x;
  DBUG_ENTER("get_random_string");
  for (x= RAND_STRING_SIZE; x > 0; x--)
    *buf_ptr++= ALPHANUMERICS[random() % ALPHANUMERICS_SIZE];
  DBUG_PRINT("info", ("random string: '%*s'", buf_ptr - buf, buf));
  DBUG_RETURN(buf_ptr - buf);
}


/*
  build_table_string

  This function builds a create table query if the user opts to not supply
  a file or string containing a create table statement
*/
static int
build_table_string(void)
{
  char       buf[512];
  int        col_count;
  DYNAMIC_STRING table_string;
  DBUG_ENTER("build_table_string");

  DBUG_PRINT("info", ("num int cols %d num char cols %d",
                      num_int_cols, num_char_cols));

  init_dynamic_string(&table_string, "", 1024, 1024);

  dynstr_append(&table_string, "CREATE TABLE `t1` (");
  for (col_count= 1; col_count <= num_int_cols; col_count++)
  {
    sprintf(buf, "intcol%d INT(32)", col_count); 
    dynstr_append(&table_string, buf);

    if (col_count < num_int_cols || num_char_cols > 0)
      dynstr_append(&table_string, ",");
  }
  for (col_count= 1; col_count <= num_char_cols; col_count++)
  {
    sprintf(buf, "charcol%d VARCHAR(128)", col_count);
    dynstr_append(&table_string, buf);

    if (col_count < num_char_cols)
      dynstr_append(&table_string, ",");
  }
  dynstr_append(&table_string, ")");
  create_string= (char *)my_malloc(table_string.length+1, MYF(MY_WME));
  create_string_alloced= 1;
  strmov(create_string, table_string.str);
  DBUG_PRINT("info", ("create_string %s", create_string));
  dynstr_free(&table_string);
  DBUG_RETURN(0);
}

/*
  build_insert_string()

  This function builds insert statements when the user opts to not supply
  an insert file or string containing insert data
*/
static int
build_insert_string(void)
{
  char       buf[RAND_STRING_SIZE];
  int        col_count;
  DYNAMIC_STRING insert_string;
  DBUG_ENTER("build_insert_string");

  init_dynamic_string(&insert_string, "", 1024, 1024);

  dynstr_append_mem(&insert_string, "INSERT INTO t1 VALUES (", 23);
  for (col_count= 1; col_count <= num_int_cols; col_count++)
  {
    sprintf(buf, "%ld", random());
    dynstr_append(&insert_string, buf);

    if (col_count < num_int_cols || num_char_cols > 0)
      dynstr_append_mem(&insert_string, ",", 1);
  }
  for (col_count= 1; col_count <= num_char_cols; col_count++)
  {
    int buf_len= get_random_string(buf);
    dynstr_append_mem(&insert_string, "'", 1);
    dynstr_append_mem(&insert_string, buf, buf_len);
    dynstr_append_mem(&insert_string, "'", 1);

    if (col_count < num_char_cols)
      dynstr_append_mem(&insert_string, ",", 1);
  }
  dynstr_append_mem(&insert_string, ")", 1);

  /*
    since this function can be called if the user wants varying insert
    statement in the for loop where inserts run, free in advance
  */
  if (insert_string_alloced)
    my_free(user_supplied_data,MYF(0));
  user_supplied_data= (char *)my_malloc(insert_string.length+1, MYF(MY_WME));
  insert_string_alloced= 1;
  strmov(user_supplied_data, insert_string.str);
  DBUG_PRINT("info", ("generated_insert_data %s", user_supplied_data));
  dynstr_free(&insert_string);
  DBUG_RETURN(insert_string.length+1);
}

/*
  build_query_string()

  This function builds a query if the user opts to not supply a query
  statement or file containing a query statement
*/
static int
build_query_string(void)
{
  char       buf[512];
  int        col_count;
  static DYNAMIC_STRING query_string;
  DBUG_ENTER("build_query_string");

  init_dynamic_string(&query_string, "", 1024, 1024);

  dynstr_append_mem(&query_string, "SELECT ", 7);
  for (col_count= 1; col_count <= num_int_cols; col_count++)
  {
    sprintf(buf, "intcol%d", col_count);
    dynstr_append(&query_string, buf);

    if (col_count < num_int_cols || num_char_cols > 0)
      dynstr_append_mem(&query_string, ",", 1);

  }
  for (col_count= 1; col_count <= num_char_cols; col_count++)
  {
    sprintf(buf, "charcol%d", col_count);
    dynstr_append(&query_string, buf);

    if (col_count < num_char_cols)
      dynstr_append_mem(&query_string, ",", 1);

  }
  dynstr_append_mem(&query_string, " FROM t1", 8);
  user_supplied_query= (char *)my_malloc(query_string.length+1, MYF(MY_WME));
  query_string_alloced= 1;
  strmov(user_supplied_query, query_string.str);
  DBUG_PRINT("info", ("user_supplied_query %s", user_supplied_query));
  dynstr_free(&query_string);
  DBUG_RETURN(0);
}

static int 
get_options(int *argc,char ***argv)
{
  int ho_error;
  MY_STAT sbuf;  /* Stat information for the data file */

  DBUG_ENTER("get_options");
  if ((ho_error= handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  /*
    Default policy - if they don't supply either char or int cols, and
    also no data, then default to 1 of each.
  */
  if (num_int_cols == 0 && num_char_cols == 0 && auto_generate_sql &&
      !user_supplied_data)
  {
    num_int_cols= 1;
    num_char_cols= 1;
  }

  if (!default_engine)
    default_engine= (char *)"MYISAM";

  if (!user)
    user= (char *)"root";

  if (auto_generate_sql && (create_string ||
     user_supplied_data || user_supplied_query))
  {
      fprintf(stderr,
              "%s: Can't use --auto-generate-sql when create, insert, and query strings are specified!\n",
              my_progname);
      exit(1);
  }

  if (opt_only_print)
    opt_silent= TRUE;

  if (!create_string && auto_generate_sql)
  {
      build_table_string();
  }
  else if (create_string && my_stat(create_string, &sbuf, MYF(0)))
  {
    File data_file;
    if (!MY_S_ISREG(sbuf.st_mode))
    {
      fprintf(stderr,"%s: Create file was not a regular file\n",
              my_progname);
      exit(1);
    }
    if ((data_file= my_open(create_string, O_RDWR, MYF(0))) == -1)
    {
      fprintf(stderr,"%s: Could not open create file\n", my_progname);
      exit(1);
    }
    create_string= (char *)my_malloc(sbuf.st_size+1, MYF(MY_WME));
    create_string_alloced= 1;
    my_read(data_file, create_string, sbuf.st_size, MYF(0));
    create_string[sbuf.st_size]= '\0';
    my_close(data_file,MYF(0));
  }

  if (create_string)
    parse_delimeter(create_string, &create_statements);

  if (!user_supplied_data && auto_generate_sql)
  {
    int length;
    generated_insert_flag= 1;
    length= build_insert_string();
    DBUG_PRINT("info", ("user_supplied_data is %s", user_supplied_data));
  }
  else if (user_supplied_data && my_stat(user_supplied_data, &sbuf, MYF(0)))
  {
    File data_file;
    if (!MY_S_ISREG(sbuf.st_mode))
    {
      fprintf(stderr,"%s: User data supplied file was not a regular file\n",
              my_progname);
      exit(1);
    }
    if ((data_file= my_open(user_supplied_data, O_RDWR, MYF(0))) == -1)
    {
      fprintf(stderr,"%s: Could not open data supplied file\n", my_progname);
      exit(1);
    }
    user_supplied_data= (char *)my_malloc(sbuf.st_size+1, MYF(MY_WME));
    insert_string_alloced= 1;
    my_read(data_file, user_supplied_data, sbuf.st_size, MYF(0));
    user_supplied_data[sbuf.st_size]= '\0';
    my_close(data_file,MYF(0));
  }

  if (user_supplied_data)
    parse_delimeter(user_supplied_data, &insert_statements);

  if (!user_supplied_query && auto_generate_sql)
  {
    build_query_string();
  }
  else if (user_supplied_query && my_stat(user_supplied_query, &sbuf, MYF(0)))
  {
    File data_file;
    if (!MY_S_ISREG(sbuf.st_mode))
    {
      fprintf(stderr,"%s: User query supplied file was not a regular file\n",
              my_progname);
      exit(1);
    }
    if ((data_file= my_open(user_supplied_query, O_RDWR, MYF(0))) == -1)
    {
      fprintf(stderr,"%s: Could not open query supplied file\n", my_progname);
      exit(1);
    }
    user_supplied_query= (char *)my_malloc(sbuf.st_size+1, MYF(MY_WME));
    query_string_alloced= 1;
    my_read(data_file, user_supplied_query, sbuf.st_size, MYF(0));
    user_supplied_query[sbuf.st_size]= '\0';
    my_close(data_file,MYF(0));
  }

  if (user_supplied_query)
    parse_delimeter(user_supplied_query, &query_statements);

  if (tty_password)
    opt_password= get_tty_password(NullS);
  DBUG_RETURN(0);
}


static int
create_schema(MYSQL *mysql, const char *db, statement *stmt)
{
  char query[HUGE_STRING_LENGTH];
  statement *ptr;

  DBUG_ENTER("create_schema");

  snprintf(query, HUGE_STRING_LENGTH, "CREATE SCHEMA `%s`", db);
  DBUG_PRINT("info", ("query %s", query)); 
  if (opt_only_print) 
  {
    printf("%s;\n", query);
  }
  else
  {
    if (mysql_query(mysql, query))
    {
      fprintf(stderr,"%s: Cannot create schema %s : %s\n", my_progname, db,
              mysql_error(mysql));
      exit(1);
    }
  }

  if (opt_only_print) 
  {
    printf("use %s;\n", db);
  }
  else
  {
    if (mysql_select_db(mysql,  db))
    {
      fprintf(stderr,"%s: Cannot select schema '%s': %s\n",my_progname, db,
              mysql_error(mysql));
      exit(1);
    }
  }

  snprintf(query, HUGE_STRING_LENGTH, "set storage_engine=`%s`",
           default_engine);
  if (opt_only_print) 
  {
    printf("%s;\n", query);
  }
  else
  {
    if (mysql_query(mysql, query))
    {
      fprintf(stderr,"%s: Cannot set default engine: %s\n", my_progname,
              mysql_error(mysql));
      exit(1);
    }
  }

  for (ptr= stmt; ptr; ptr= ptr->next)
  {
    if (opt_only_print) 
    {
      printf("%.*s;\n", (uint)ptr->length, ptr->string);
    }
    else
    {
      if (mysql_real_query(mysql, ptr->string, ptr->length))
      {
        fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                my_progname, (uint)ptr->length, ptr->string, mysql_error(mysql));
        exit(1);
      }
    }
  }

  DBUG_RETURN(0);
}

static int
drop_schema(MYSQL *mysql,const char *db)
{
  char query[HUGE_STRING_LENGTH];

  DBUG_ENTER("drop_schema");
  snprintf(query, HUGE_STRING_LENGTH, "DROP SCHEMA IF EXISTS `%s`", db);
  if (opt_only_print) 
  {
    printf("%s;\n", query);
  }
  else
  {
    if (mysql_query(mysql, query))
    {
      fprintf(stderr,"%s: Cannot drop database '%s' ERROR : %s\n",
              my_progname, db, mysql_error(mysql));
      exit(1);
    }
  }

  DBUG_RETURN(0);
}


static int
run_scheduler(statement *stmts,
              int(*task)(statement *stmt), unsigned int concur)
{
  uint x;

  DBUG_ENTER("run_scheduler");
  /* reset to 0 */
  children_spawned= 0;

  for (x= 0; x < concur; x++)
  {
    int pid;
    DBUG_PRINT("info", ("x %d concurrency %d", x, concurrency));
    pid= fork();
    switch(pid)
    {
    case 0:
      /* child */
      DBUG_PRINT("info", ("fork returned 0, calling task(\"%s\"), pid %d gid %d",
                          stmts->string, pid, getgid()));
      if (verbose >= 2)
        fprintf(stderr,
                "%s: fork returned 0, calling task pid %d gid %d\n",
                my_progname, pid, getgid());
      task(stmts);
      exit(0);
      break;
    case -1:
      /* error */
      DBUG_PRINT("info",
                 ("fork returned -1, failing pid %d gid %d", pid, getgid()));
      fprintf(stderr,
              "%s: Failed on fork: -1, max procs per parent exceeded.\n",
              my_progname);
      /*exit(1);*/
      goto WAIT;
    default:
      /* parent, forked */
      DBUG_PRINT("info", ("default, break: pid %d gid %d", pid, getgid()));
      if (verbose >= 2)
        fprintf(stderr,"%s: fork returned %d, gid %d\n",
                my_progname, pid, getgid());
      break;
    }
    children_spawned++;
  }

WAIT:
  while (x--)
  {
    int status, pid;
    pid= wait(&status);
    DBUG_PRINT("info", ("Parent: child %d status %d", pid, status));
  }

  DBUG_RETURN(0);
}

int
run_task(statement *qstmt)
{
  uint counter= 0, x;
  MYSQL mysql;
  MYSQL_RES *result;
  MYSQL_ROW row;

  DBUG_ENTER("run_task");
  DBUG_PRINT("info", ("task script \"%s\"", qstmt->string));

  mysql_init(&mysql);

  DBUG_PRINT("info", ("trying to connect to host %s as user %s", host, user));
  if (!opt_only_print) 
  {
    if (!(mysql_real_connect(&mysql, host, user, opt_password,
                             "mysqlslap", opt_mysql_port, opt_mysql_unix_port,
                             0)))
    {
      fprintf(stderr,"%s: %s\n",my_progname,mysql_error(&mysql));
      exit(1);
    }
  }
  DBUG_PRINT("info", ("connected."));

  for (x= 0; x < number_of_iterations; x++)
  {
    statement *ptr;
    for (ptr= qstmt; ptr; ptr= ptr->next)
    {
      if (opt_only_print) 
      {
        printf("%.*s;\n", (uint)ptr->length, ptr->string);
      }
      else
      {
        if (mysql_real_query(&mysql, ptr->string, ptr->length))
        {
          fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                  my_progname, (uint)ptr->length, ptr->string, mysql_error(&mysql));
          exit(1);
        }

        result= mysql_store_result(&mysql);
        while ((row = mysql_fetch_row(result)))
          counter++;
        mysql_free_result(result);
        result= 0;
      }
    }
  }

  if (!opt_only_print) 
    mysql_close(&mysql);
  DBUG_RETURN(0);
}


int
load_data(statement *load_stmt)
{
  uint x;
  MYSQL mysql;

  DBUG_ENTER("load_data");
  DBUG_PRINT("info", ("task load_data, pid %d", getpid()));
  mysql_init(&mysql);

  if (!opt_only_print) 
  {
    if (!(mysql_real_connect(&mysql, host, user, opt_password,
                             "mysqlslap", opt_mysql_port, opt_mysql_unix_port,
                             0)))
    {
      fprintf(stderr,"%s: Unable to connect to mysqlslap ERROR: %s\n",
              my_progname, mysql_error(&mysql));
      exit(1);
    }
  }

  for (x= 0; x < number_of_rows; x++)
  {
    statement *ptr;
    for (ptr= load_stmt; ptr; ptr= ptr->next)
    {
      if (opt_only_print) 
      {
        printf("%.*s;\n", (uint)ptr->length, ptr->string);
      }
      else
      {
        if (mysql_real_query(&mysql, ptr->string, ptr->length))
        {
          DBUG_PRINT("info", ("iteration %d with INSERT statement %s", ptr->string));
          fprintf(stderr,"%s: Cannot insert into table using sql: %.*s ERROR: %s\n",
                  my_progname, (uint)ptr->length, ptr->string,
                  mysql_error(&mysql));
          exit(1);
        }
      }
    }
  }

  if (!opt_only_print) 
    mysql_close(&mysql);
  DBUG_RETURN(0);
}

int
parse_delimeter(const char *script, statement **stmt)
{
  char *retstr;
  char *ptr= (char *)script;
  statement **sptr= stmt;
  statement *tmp;
  uint length= strlen(script);

  DBUG_PRINT("info", ("Parsing %s\n", script));

  for (*sptr= (statement *)my_malloc(sizeof(statement), MYF(MY_ZEROFILL)), tmp= *sptr;
       (retstr= strchr(ptr, delimiter[0])); 
       tmp->next=  (statement *)my_malloc(sizeof(statement), MYF(MY_ZEROFILL)),
       tmp= tmp->next)
  {
    tmp->string= my_strdup_with_length(ptr, (size_t)(retstr - ptr), MYF(MY_FAE));
    tmp->length= (size_t)(retstr - script);
    DBUG_PRINT("info", (" Creating : %.*s\n", (uint)tmp->length, tmp->string));
    ptr+= retstr - script + 1;
    if (isspace(*ptr))
      ptr++;
  }
  tmp->string= my_strdup_with_length(ptr, (size_t)((script + length) - ptr), 
                                     MYF(MY_FAE));
  tmp->length= (size_t)((script + length) - ptr);
  DBUG_PRINT("info", (" Creating : %.*s\n", (uint)tmp->length, tmp->string));

  return 0;
}
