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
  1) Create schema,table, and optionally any SP or data you want to beign
     the test with. (single client)
  2) Load test (many clients)
  3) Cleanup (disconnection, drop table if specified, single client)

  Examples:

  Supply your own create and query SQL statements, with 50 clients 
  querying (200 selects for each):

    mysqlslap --create="CREATE TABLE A (a int);INSERT INTO A (23)" \
              --query="SELECT * FROM A" --concurrency=50 --iterations=200

  Let the program build the query SQL statement with a table of two int
  columns, three varchar columns, five clients querying (20 times each),
  don't create the table or insert the data (using the previous test's
  schema and data):

    mysqlslap --concurrency=5 --iterations=20 \
              --number-int-cols=2 --number-char-cols=3 \
              --auto-generate-sql

  Tell the program to load the create, insert and query SQL statements from
  the specified files, where the create.sql file has multiple table creation
  statements delimited by ';' and multiple insert statements delimited by ';'.
  The --query file will have multiple queries delimited by ';', run all the 
  load statements, and then run all the queries in the query file
  with five clients (five times each):

    mysqlslap --concurrency=5 \
              --iterations=5 --query=query.sql --create=create.sql \
              --delimiter=";"

TODO:
  Add language for better tests
  String length for files and those put on the command line are not
    setup to handle binary data.
  Report results of each thread into the lock file we use.
  More stats
  Break up tests and run them on multiple hosts at once.
  Allow output to be fed into a database directly.

*/

#define SHOW_VERSION "0.9"

#define HUGE_STRING_LENGTH 8096
#define RAND_STRING_SIZE 126

#include "client_priv.h"
#ifdef HAVE_LIBPTHREAD
#include <my_pthread.h>
#endif
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <mysqld_error.h>
#include <my_dir.h>
#include <signal.h>
#include <stdarg.h>
#include <sslopt-vars.h>
#include <sys/types.h>
#ifndef __WIN__
#include <sys/wait.h>
#endif
#include <ctype.h>

#define MYSLAPLOCK "/myslaplock.lck"
#define MYSLAPLOCK_DIR "/tmp"

#ifdef __WIN__
#define srandom  srand
#define random   rand
#define snprintf _snprintf
#endif

#ifdef HAVE_SMEM 
static char *shared_memory_base_name=0;
#endif

static char **defaults_argv;

static char *host= NULL, *opt_password= NULL, *user= NULL,
            *user_supplied_query= NULL,
            *default_engine= NULL,
            *opt_mysql_unix_port= NULL;

const char *delimiter= "\n";

const char *create_schema_string= "mysqlslap";

const char *lock_directory;
char lock_file_str[FN_REFLEN];

static my_bool opt_preserve;

static my_bool opt_only_print= FALSE;

static my_bool opt_slave;

static my_bool opt_compress= FALSE, tty_password= FALSE,
               opt_silent= FALSE,
               auto_generate_sql= FALSE;

static unsigned long connect_flags= CLIENT_MULTI_RESULTS;

static int verbose, num_int_cols, num_char_cols, delimiter_length;
static int iterations;
static char *default_charset= (char*) MYSQL_DEFAULT_CHARSET_NAME;
static ulonglong actual_queries= 0;
static ulonglong num_of_query;
const char *concurrency_str= NULL;
static char *create_string;
uint *concurrency;

const char *default_dbug_option="d:t:o,/tmp/mysqlslap.trace";
const char *opt_csv_str;
File csv_file;

static uint opt_protocol= 0;

static int get_options(int *argc,char ***argv);
static uint opt_mysql_port= 0;
static uint opt_use_threads;

static const char *load_default_groups[]= { "mysqlslap","client",0 };

typedef struct statement statement;

struct statement {
  char *string;
  size_t length;
  statement *next;
};

typedef struct stats stats;

struct stats {
  long int timing;
  uint users;
  unsigned long long rows;
};

typedef struct thread_context thread_context;

struct thread_context {
  statement *stmt;
  ulonglong limit;
  bool thread;
};

typedef struct conclusions conclusions;

struct conclusions {
  char *engine;
  long int avg_timing;
  long int max_timing;
  long int min_timing;
  uint users;
  unsigned long long avg_rows;
  /* The following are not used yet */
  unsigned long long max_rows;
  unsigned long long min_rows;
};

static statement *create_statements= NULL, 
                 *engine_statements= NULL, 
                 *query_statements= NULL;

/* Prototypes */
void print_conclusions(conclusions *con);
void print_conclusions_csv(conclusions *con);
void generate_stats(conclusions *con, statement *eng, stats *sptr);
uint parse_comma(const char *string, uint **range);
uint parse_delimiter(const char *script, statement **stmt, char delm);
static int drop_schema(MYSQL *mysql, const char *db);
uint get_random_string(char *buf);
static statement *build_table_string(void);
static statement *build_insert_string(void);
static statement *build_query_string(void);
static int create_schema(MYSQL *mysql, const char *db, statement *stmt, 
              statement *engine_stmt);
static int run_scheduler(stats *sptr, statement *stmts, uint concur, 
                         ulonglong limit);
int run_task(thread_context *con);
void statement_cleanup(statement *stmt);

static const char ALPHANUMERICS[]=
  "0123456789ABCDEFGHIJKLMNOPQRSTWXYZabcdefghijklmnopqrstuvwxyz";

#define ALPHANUMERICS_SIZE (sizeof(ALPHANUMERICS)-1)


static long int timedif(struct timeval a, struct timeval b)
{
    register int us, s;
 
    us = a.tv_usec - b.tv_usec;
    us /= 1000;
    s = a.tv_sec - b.tv_sec;
    s *= 1000;
    return s + us;
}

#ifdef __WIN__
static int gettimeofday(struct timeval *tp, void *tzp)
{
  unsigned int ticks;
  ticks= GetTickCount();
  tp->tv_usec= ticks*1000;
  tp->tv_sec= ticks/1000;

  return 0;
}
#endif

int main(int argc, char **argv)
{
  MYSQL mysql;
  int x;
  unsigned long long client_limit;
  statement *eptr;

#ifdef __WIN__
  opt_use_threads= 1;
#endif

  MY_INIT(argv[0]);

  load_defaults("my",load_default_groups,&argc,&argv);
  defaults_argv=argv;
  if (get_options(&argc,&argv))
  {
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }

  /* Seed the random number generator if we will be using it. */
  if (auto_generate_sql)
    srandom((uint)time(NULL));

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

  if (!opt_only_print) 
  {
    if (!(mysql_real_connect(&mysql, host, user, opt_password,
                             NULL, opt_mysql_port,
                             opt_mysql_unix_port, connect_flags)))
    {
      fprintf(stderr,"%s: Error when connecting to server: %s\n",
              my_progname,mysql_error(&mysql));
      free_defaults(defaults_argv);
      my_end(0);
      exit(1);
    }
  }

  /* Main iterations loop */
  eptr= engine_statements;
  do
  {
    /* For the final stage we run whatever queries we were asked to run */
    uint *current;
    conclusions conclusion;

    for (current= concurrency; current && *current; current++)
    {
      stats *head_sptr;
      stats *sptr;

      head_sptr= (stats *)my_malloc(sizeof(stats) * iterations, MYF(MY_ZEROFILL));

      bzero(&conclusion, sizeof(conclusions));

      if (num_of_query)
        client_limit=  num_of_query / *current;
      else
        client_limit= actual_queries;

      for (x= 0, sptr= head_sptr; x < iterations; x++, sptr++)
      {
        /*
          We might not want to load any data, such as when we are calling
          a stored_procedure that doesn't use data, or we know we already have
          data in the table.
        */
        if (!opt_preserve)
          drop_schema(&mysql, create_schema_string);
        /* First we create */
        if (create_statements)
          create_schema(&mysql, create_schema_string, create_statements, eptr);

        run_scheduler(sptr, query_statements, *current, client_limit); 
      }

      generate_stats(&conclusion, eptr, head_sptr);

      if (!opt_silent)
        print_conclusions(&conclusion);
      if (opt_csv_str)
        print_conclusions_csv(&conclusion);

      my_free((byte *)head_sptr, MYF(0));
    }

    if (!opt_preserve)
      drop_schema(&mysql, create_schema_string);
  } while (eptr ? (eptr= eptr->next) : 0);

  if (!opt_only_print) 
    mysql_close(&mysql); /* Close & free connection */


  /* Remove lock file */
  my_delete(lock_file_str, MYF(0));

  /* now free all the strings we created */
  if (opt_password)
    my_free(opt_password, MYF(0));

  my_free((byte *)concurrency, MYF(0));

  statement_cleanup(create_statements);
  statement_cleanup(engine_statements);
  statement_cleanup(query_statements);

#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    my_free(shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
#endif
  free_defaults(defaults_argv);
  my_end(0);

  return 0;
}


static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"auto-generate-sql", 'a',
    "Generate SQL where not supplied by file or command line.",
    (gptr*) &auto_generate_sql, (gptr*) &auto_generate_sql,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
    (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
    0, 0, 0},
  {"concurrency", 'c', "Number of clients to simulate for query to run.",
    (gptr*) &concurrency_str, (gptr*) &concurrency_str, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"create", OPT_CREATE_SLAP_SCHEMA, "File or string to use create tables.",
    (gptr*) &create_string, (gptr*) &create_string, 0, GET_STR, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0},
  {"create-schema", OPT_CREATE_SLAP_SCHEMA, "Schema to run tests in.",
    (gptr*) &create_schema_string, (gptr*) &create_schema_string, 0, GET_STR, 
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"csv", OPT_CREATE_SLAP_SCHEMA,
	"Generate CSV output to named file or to stdout if no file is named.",
    (gptr*) &opt_csv_str, (gptr*) &opt_csv_str, 0, GET_STR, 
    OPT_ARG, 0, 0, 0, 0, 0, 0},
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
  {"host", 'h', "Connect to host.", (gptr*) &host, (gptr*) &host, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"iterations", 'i', "Number of times too run the tests.", (gptr*) &iterations,
    (gptr*) &iterations, 0, GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  {"lock-directory", OPT_MYSQL_LOCK_DIRECTORY, "Directory to use to keep locks.", 
    (gptr*) &lock_directory, (gptr*) &lock_directory, 0, GET_STR, 
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"number-char-cols", 'x', 
    "Number of INT columns to create table with if specifying --auto-generate-sql.",
    (gptr*) &num_char_cols, (gptr*) &num_char_cols, 0, GET_UINT, REQUIRED_ARG,
    1, 0, 0, 0, 0, 0},
  {"number-int-cols", 'y', 
    "Number of VARCHAR columns to create table with if specifying "
      "--auto-generate-sql.", (gptr*) &num_int_cols, (gptr*) &num_int_cols, 0,
    GET_UINT, REQUIRED_ARG, 1, 0, 0, 0, 0, 0},
  {"number-of-queries", OPT_MYSQL_NUMBER_OF_QUERY, 
    "Limit each client to this number of queries (this is not exact).",
    (gptr*) &num_of_query, (gptr*) &num_of_query, 0,
    GET_ULL, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"only-print", OPT_MYSQL_ONLY_PRINT,
    "This causes mysqlslap to not connect to the databases, but instead print "
      "out what it would have done instead.",
    (gptr*) &opt_only_print, (gptr*) &opt_only_print, 0, GET_BOOL,  NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"password", 'p',
    "Password to use when connecting to server. If password is not given it's "
      "asked from the tty.", 0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
    NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_mysql_port,
    (gptr*) &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, MYSQL_PORT, 0, 0, 0, 0,
    0},
  {"preserve-schema", OPT_MYSQL_PRESERVE_SCHEMA,
    "Preserve the schema from the mysqlslap run, this happens unless "
      "--auto-generate-sql or --create are used.",
    (gptr*) &opt_preserve, (gptr*) &opt_preserve, 0, GET_BOOL,
    NO_ARG, TRUE, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL,
    "The protocol of connection (tcp,socket,pipe,memory).",
    0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"query", 'q', "Query to run or file containing query to run.",
    (gptr*) &user_supplied_query, (gptr*) &user_supplied_query,
    0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
    "Base name of shared memory.", (gptr*) &shared_memory_base_name,
    (gptr*) &shared_memory_base_name, 0, GET_STR_ALLOC, REQUIRED_ARG,
    0, 0, 0, 0, 0, 0},
#endif
  {"silent", 's', "Run program in silent mode - no output.",
    (gptr*) &opt_silent, (gptr*) &opt_silent, 0, GET_BOOL,  NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"slave", OPT_MYSQL_SLAP_SLAVE, "Follow master locks for other slap clients",
    (gptr*) &opt_slave, (gptr*) &opt_slave, 0, GET_BOOL,  NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
    (gptr*) &opt_mysql_unix_port, (gptr*) &opt_mysql_unix_port, 0, GET_STR,
    REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
  {"use-threads", OPT_USE_THREADS,
    "Use pthread calls instead of fork() calls (default on Windows)",
      (gptr*) &opt_use_threads, (gptr*) &opt_use_threads, 0, 
      GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", (gptr*) &user,
    (gptr*) &user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"verbose", 'v',
    "More verbose output; You can use this multiple times to get even more "
      "verbose output.", (gptr*) &verbose, (gptr*) &verbose, 0, 
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


uint
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
static statement *
build_table_string(void)
{
  char       buf[512];
  int        col_count;
  statement *ptr;
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
  ptr= (statement *)my_malloc(sizeof(statement), MYF(MY_ZEROFILL));
  ptr->string = (char *)my_malloc(table_string.length+1, MYF(MY_WME));
  ptr->length= table_string.length+1;
  strmov(ptr->string, table_string.str);
  DBUG_PRINT("info", ("create_string %s", ptr->string));
  dynstr_free(&table_string);
  DBUG_RETURN(ptr);
}


/*
  build_insert_string()

  This function builds insert statements when the user opts to not supply
  an insert file or string containing insert data
*/
static statement *
build_insert_string(void)
{
  char       buf[RAND_STRING_SIZE];
  int        col_count;
  statement *ptr;
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

  ptr= (statement *)my_malloc(sizeof(statement), MYF(MY_ZEROFILL));
  ptr->string= (char *)my_malloc(insert_string.length+1, MYF(MY_WME));
  ptr->length= insert_string.length+1;
  strmov(ptr->string, insert_string.str);
  DBUG_PRINT("info", ("generated_insert_data %s", ptr->string));
  dynstr_free(&insert_string);
  DBUG_RETURN(ptr);
}


/*
  build_query_string()

  This function builds a query if the user opts to not supply a query
  statement or file containing a query statement
*/
static statement *
build_query_string(void)
{
  char       buf[512];
  int        col_count;
  statement *ptr;
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
  ptr= (statement *)my_malloc(sizeof(statement), MYF(MY_ZEROFILL));
  ptr->string= (char *)my_malloc(query_string.length+1, MYF(MY_WME));
  ptr->length= query_string.length+1;
  strmov(ptr->string, query_string.str);
  DBUG_PRINT("info", ("user_supplied_query %s", ptr->string));
  dynstr_free(&query_string);
  DBUG_RETURN(ptr);
}

static int
get_options(int *argc,char ***argv)
{
  int ho_error;
  char *tmp_string;
  MY_STAT sbuf;  /* Stat information for the data file */

  DBUG_ENTER("get_options");
  if ((ho_error= handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (!user)
    user= (char *)"root";

  if (create_string || auto_generate_sql)
  {
    if (verbose >= 1)
      fprintf(stderr, "Turning off preserve-schema!\n");
    opt_preserve= FALSE;
  }

  if (auto_generate_sql && (create_string || user_supplied_query))
  {
      fprintf(stderr,
              "%s: Can't use --auto-generate-sql when create and query strings are specified!\n",
              my_progname);
      exit(1);
  }

  parse_comma(concurrency_str ? concurrency_str : "1", &concurrency);

  if (lock_directory)
    snprintf(lock_file_str, FN_REFLEN, "%s/%s", lock_directory, MYSLAPLOCK);
  else
    snprintf(lock_file_str, FN_REFLEN, "%s/%s", MYSLAPLOCK_DIR, MYSLAPLOCK);

  if (opt_csv_str)
  {
    opt_silent= TRUE;
    
    if (opt_csv_str[0] == '-')
    {
      csv_file= fileno(stdout);
    }
    else
    {
      if ((csv_file= my_open(opt_csv_str, O_CREAT|O_WRONLY|O_APPEND, MYF(0)))
          == -1)
      {
        fprintf(stderr,"%s: Could not open csv file: %sn\n",
                my_progname, opt_csv_str);
        exit(1);
      }
    }
  }

  if (opt_only_print)
    opt_silent= TRUE;

  if (auto_generate_sql)
  {
      create_statements= build_table_string();
      query_statements= build_insert_string();
      DBUG_PRINT("info", ("auto-generated insert is %s", query_statements->string));
      query_statements->next= build_query_string();
      DBUG_PRINT("info", ("auto-generated is %s", query_statements->next->string));
      if (verbose >= 1)
      {
        fprintf(stderr, "auto-generated insert is:\n");
        fprintf(stderr,  "%s\n", query_statements->string);
        fprintf(stderr, "auto-generated is:\n");
        fprintf(stderr,  "%s\n", query_statements->next->string);
      }

  }
  else
  {
    if (create_string && my_stat(create_string, &sbuf, MYF(0)))
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
      tmp_string= (char *)my_malloc(sbuf.st_size+1, MYF(MY_WME));
      my_read(data_file, tmp_string, sbuf.st_size, MYF(0));
      tmp_string[sbuf.st_size]= '\0';
      my_close(data_file,MYF(0));
      parse_delimiter(tmp_string, &create_statements, delimiter[0]);
      my_free(tmp_string, MYF(0));
    }
    else if (create_string)
    {
        parse_delimiter(create_string, &create_statements, delimiter[0]);
    }

    if (user_supplied_query && my_stat(user_supplied_query, &sbuf, MYF(0)))
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
      tmp_string= (char *)my_malloc(sbuf.st_size+1, MYF(MY_WME));
      my_read(data_file, tmp_string, sbuf.st_size, MYF(0));
      tmp_string[sbuf.st_size]= '\0';
      my_close(data_file,MYF(0));
      if (user_supplied_query)
        actual_queries= parse_delimiter(tmp_string, &query_statements,
                                        delimiter[0]);
      my_free(tmp_string, MYF(0));
    } 
    else if (user_supplied_query)
    {
        actual_queries= parse_delimiter(user_supplied_query, &query_statements,
                                        delimiter[0]);
    }
  }

  if (default_engine)
    parse_delimiter(default_engine, &engine_statements, ',');

  if (tty_password)
    opt_password= get_tty_password(NullS);
  DBUG_RETURN(0);
}


static int run_query(MYSQL *mysql, const char *query, int len)
{
  if (opt_only_print)
  {
    printf("%.*s;\n", len, query);
    return 0;
  }

  if (verbose >= 2)
    printf("%.*s;\n", len, query);
  return mysql_real_query(mysql, query, len);
}



static int
create_schema(MYSQL *mysql, const char *db, statement *stmt, 
              statement *engine_stmt)
{
  char query[HUGE_STRING_LENGTH];
  statement *ptr;
  int len;
  DBUG_ENTER("create_schema");

  len= snprintf(query, HUGE_STRING_LENGTH, "CREATE SCHEMA `%s`", db);
  DBUG_PRINT("info", ("query %s", query)); 

  if (run_query(mysql, query, len))
  {
    fprintf(stderr,"%s: Cannot create schema %s : %s\n", my_progname, db,
            mysql_error(mysql));
    exit(1);
  }

  if (opt_only_print)
  {
    printf("use %s;\n", db);
  }
  else
  {
    if (verbose >= 2)
      printf("%s;\n", query);
    if (mysql_select_db(mysql,  db))
    {
      fprintf(stderr,"%s: Cannot select schema '%s': %s\n",my_progname, db,
              mysql_error(mysql));
      exit(1);
    }
  }

  if (engine_stmt)
  {
    len= snprintf(query, HUGE_STRING_LENGTH, "set storage_engine=`%s`",
                  engine_stmt->string);
    if (run_query(mysql, query, len))
    {
      fprintf(stderr,"%s: Cannot set default engine: %s\n", my_progname,
              mysql_error(mysql));
      exit(1);
    }
  }

  for (ptr= stmt; ptr && ptr->length; ptr= ptr->next)
  {
    if (run_query(mysql, ptr->string, ptr->length))
    {
      fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
              my_progname, (uint)ptr->length, ptr->string, mysql_error(mysql));
      exit(1);
    }
  }

  DBUG_RETURN(0);
}

static int
drop_schema(MYSQL *mysql, const char *db)
{
  char query[HUGE_STRING_LENGTH];
  int len;
  DBUG_ENTER("drop_schema");
  len= snprintf(query, HUGE_STRING_LENGTH, "DROP SCHEMA IF EXISTS `%s`", db);

  if (run_query(mysql, query, len))
  {
    fprintf(stderr,"%s: Cannot drop database '%s' ERROR : %s\n",
            my_progname, db, mysql_error(mysql));
    exit(1);
  }



  DBUG_RETURN(0);
}

static int
run_scheduler(stats *sptr, statement *stmts, uint concur, ulonglong limit)
{
  uint x;
  File lock_file;
  struct timeval start_time, end_time;
  thread_context con;
  DBUG_ENTER("run_scheduler");

  con.stmt= stmts;
  con.limit= limit;
  con.thread= opt_use_threads ? 1 :0;

  lock_file= my_open(lock_file_str, O_CREAT|O_WRONLY|O_TRUNC, MYF(0));

  if (!opt_slave)
    if (my_lock(lock_file, F_WRLCK, 0, F_TO_EOF, MYF(0)))
    {
      fprintf(stderr,"%s: Could not get lockfile\n",
              my_progname);
      exit(0);
    }

#ifdef HAVE_LIBPTHREAD
  if (opt_use_threads)
  {
    pthread_t mainthread;            /* Thread descriptor */
    pthread_attr_t attr;          /* Thread attributes */

    for (x= 0; x < concur; x++)
    {
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr,
                                   PTHREAD_CREATE_DETACHED);

      /* now create the thread */
      if (pthread_create(&mainthread, &attr, (void *)run_task, 
                         (void *)&con) != 0)
      {
        fprintf(stderr,"%s: Could not create thread\n",
                my_progname);
        exit(0);
      }
    }
  }
#endif
#if !(defined(__WIN__) || defined(__NETWARE__))
#ifdef HAVE_LIBPTHREAD
  else
#endif
  {
    fflush(NULL);
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
                            stmts ? stmts->string : "", pid, getgid()));
        if (verbose >= 2)
          fprintf(stderr,
                  "%s: fork returned 0, calling task pid %d gid %d\n",
                  my_progname, pid, getgid());
        run_task(&con);
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
    }
  }
#endif

  /* Lets release use some clients! */
  if (!opt_slave)
    my_lock(lock_file, F_UNLCK, 0, F_TO_EOF, MYF(0));

  gettimeofday(&start_time, NULL);

  /*
    We look to grab a write lock at this point. Once we get it we know that
    all clients have completed their work.
  */
  if (opt_use_threads)
  {
    if (my_lock(lock_file, F_WRLCK, 0, F_TO_EOF, MYF(0)))
    {
      fprintf(stderr,"%s: Could not get lockfile\n",
              my_progname);
      exit(0);
    }
    my_lock(lock_file, F_UNLCK, 0, F_TO_EOF, MYF(0));
  }
#ifndef __WIN__
  else
  {
WAIT:
    while (x--)
    {
      int status, pid;
      pid= wait(&status);
      DBUG_PRINT("info", ("Parent: child %d status %d", pid, status));
      if (status != 0)
        printf("%s: Child %d died with the status %d\n",
               my_progname, pid, status);
    }
  }
#endif
  gettimeofday(&end_time, NULL);

  my_close(lock_file, MYF(0));

  sptr->timing= timedif(end_time, start_time);
  sptr->users= concur;
  sptr->rows= limit;

  DBUG_RETURN(0);
}


int
run_task(thread_context *con)
{
  ulonglong counter= 0, queries;
  File lock_file= -1;
  MYSQL *mysql;
  MYSQL_RES *result;
  MYSQL_ROW row;
  statement *ptr;

  DBUG_ENTER("run_task");
  DBUG_PRINT("info", ("task script \"%s\"", con->stmt ? con->stmt->string : ""));

  if (!(mysql= mysql_init(NULL)))
    goto end;

  if (con->thread && mysql_thread_init())
    goto end;

  DBUG_PRINT("info", ("trying to connect to host %s as user %s", host, user));
  lock_file= my_open(lock_file_str, O_RDWR, MYF(0));
  my_lock(lock_file, F_RDLCK, 0, F_TO_EOF, MYF(0));
  if (!opt_only_print)
  {
    /* Connect to server */
    static ulong connection_retry_sleep= 100000; /* Microseconds */
    int i, connect_error= 1;
    for (i= 0; i < 10; i++)
    {
      if (mysql_real_connect(mysql, host, user, opt_password,
                             create_schema_string,
                             opt_mysql_port,
                             opt_mysql_unix_port,
                             connect_flags))
      {
        /* Connect suceeded */
        connect_error= 0;
        break;
      }
      my_sleep(connection_retry_sleep);
    }
    if (connect_error)
    {
      fprintf(stderr,"%s: Error when connecting to server: %d %s\n",
              my_progname, mysql_errno(mysql), mysql_error(mysql));
      goto end;
    }
  }
  DBUG_PRINT("info", ("connected."));
  if (verbose >= 3)
    fprintf(stderr, "connected!\n");
  queries= 0;

limit_not_met:
    for (ptr= con->stmt; ptr && ptr->length; ptr= ptr->next)
    {
      if (run_query(mysql, ptr->string, ptr->length))
      {
        fprintf(stderr,"%s: Cannot run query %.*s ERROR : %s\n",
                my_progname, (uint)ptr->length, ptr->string, mysql_error(mysql));
        goto end;
      }
      if (mysql_field_count(mysql))
      {
        result= mysql_store_result(mysql);
        while ((row = mysql_fetch_row(result)))
          counter++;
        mysql_free_result(result);
      }
      queries++;

      if (con->limit && queries == con->limit)
        goto end;
    }

    if (!con->stmt && con->limit && queries < con->limit)
      goto limit_not_met;

end:

  if (lock_file != -1)
  {
    my_lock(lock_file, F_UNLCK, 0, F_TO_EOF, MYF(0));
    my_close(lock_file, MYF(0));
  }

  if (!opt_only_print) 
    mysql_close(mysql);

  if (con->thread)
    my_thread_end();
  DBUG_RETURN(0);
}


uint
parse_delimiter(const char *script, statement **stmt, char delm)
{
  char *retstr;
  char *ptr= (char *)script;
  statement **sptr= stmt;
  statement *tmp;
  uint length= strlen(script);
  uint count= 0; /* We know that there is always one */

  DBUG_PRINT("info", ("Parsing %s\n", script));

  for (tmp= *sptr= (statement *)my_malloc(sizeof(statement), MYF(MY_ZEROFILL));
       (retstr= strchr(ptr, delm)); 
       tmp->next=  (statement *)my_malloc(sizeof(statement), MYF(MY_ZEROFILL)),
       tmp= tmp->next)
  {
    count++;
    tmp->string= my_strndup(ptr, (size_t)(retstr - ptr), MYF(MY_FAE));
    tmp->length= (size_t)(retstr - ptr);
    DBUG_PRINT("info", (" Creating : %.*s\n", (uint)tmp->length, tmp->string));
    ptr+= retstr - ptr + 1;
    if (isspace(*ptr))
      ptr++;
    count++;
  }

  if (ptr != script+length)
  {
    tmp->string= my_strndup(ptr, (size_t)((script + length) - ptr), 
                                       MYF(MY_FAE));
    tmp->length= (size_t)((script + length) - ptr);
    DBUG_PRINT("info", (" Creating : %.*s\n", (uint)tmp->length, tmp->string));
    count++;
  }

  return count;
}


uint
parse_comma(const char *string, uint **range)
{
  uint count= 1,x; /* We know that there is always one */
  char *retstr;
  char *ptr= (char *)string;
  uint *nptr;

  for (;*ptr; ptr++)
    if (*ptr == ',') count++;
  
  /* One extra spot for the NULL */
  nptr= *range= (uint *)my_malloc(sizeof(uint) * (count + 1), MYF(MY_ZEROFILL));

  ptr= (char *)string;
  x= 0;
  while ((retstr= strchr(ptr,',')))
  {
    nptr[x++]= atoi(ptr);
    ptr+= retstr - ptr + 1;
  }
  nptr[x++]= atoi(ptr);

  return count;
}

void
print_conclusions(conclusions *con)
{
  printf("Benchmark\n");
  if (con->engine)
    printf("\tRunning for engine %s\n", con->engine);
  printf("\tAverage number of seconds to run all queries: %ld.%03ld seconds\n",
                    con->avg_timing / 1000, con->avg_timing % 1000);
  printf("\tMinimum number of seconds to run all queries: %ld.%03ld seconds\n",
                    con->min_timing / 1000, con->min_timing % 1000);
  printf("\tMaximum number of seconds to run all queries: %ld.%03ld seconds\n",
                    con->max_timing / 1000, con->max_timing % 1000);
  printf("\tNumber of clients running queries: %d\n", con->users);
  printf("\tAverage number of queries per client: %llu\n", con->avg_rows); 
  printf("\n");
}

void
print_conclusions_csv(conclusions *con)
{
  char buffer[HUGE_STRING_LENGTH];
  snprintf(buffer, HUGE_STRING_LENGTH, 
           "%s,query,%ld.%03ld,%ld.%03ld,%ld.%03ld,%d,%llu\n",
           con->engine ? con->engine : "", /* Storage engine we ran against */
           con->avg_timing / 1000, con->avg_timing % 1000, /* Time to load */
           con->min_timing / 1000, con->min_timing % 1000, /* Min time */
           con->max_timing / 1000, con->max_timing % 1000, /* Max time */
           con->users, /* Children used */
           con->avg_rows  /* Queries run */
          );
  my_write(csv_file, buffer, strlen(buffer), MYF(0));
}

void
generate_stats(conclusions *con, statement *eng, stats *sptr)
{
  stats *ptr;
  int x;

  con->min_timing= sptr->timing; 
  con->max_timing= sptr->timing;
  con->min_rows= sptr->rows;
  con->max_rows= sptr->rows;
  
  /* At the moment we assume uniform */
  con->users= sptr->users;
  con->avg_rows= sptr->rows;
  
  /* With no next, we know it is the last element that was malloced */
  for (ptr= sptr, x= 0; x < iterations; ptr++, x++)
  {
    con->avg_timing+= ptr->timing;

    if (ptr->timing > con->max_timing)
      con->max_timing= ptr->timing;
    if (ptr->timing < con->min_timing)
      con->min_timing= ptr->timing;
  }
  con->avg_timing= con->avg_timing/iterations;

  if (eng && eng->string)
    con->engine= eng->string;
  else
    con->engine= NULL;
}

void
statement_cleanup(statement *stmt)
{
  statement *ptr, *nptr;
  if (!stmt)
    return;

  for (ptr= stmt; ptr; ptr= nptr)
  {
    nptr= ptr->next;
    if (ptr->string)
      my_free(ptr->string, MYF(0)); 
    my_free((byte *)ptr, MYF(0));
  }
}
