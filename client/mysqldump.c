/* Copyright (C) 2000 MySQL AB

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

/* mysqldump.c  - Dump a tables contents and format to an ASCII file
**
** The author's original notes follow :-
**
** AUTHOR: Igor Romanenko (igor@frog.kiev.ua)
** DATE:   December 3, 1994
** WARRANTY: None, expressed, impressed, implied
**	    or other
** STATUS: Public domain
** Adapted and optimized for MySQL by
** Michael Widenius, Sinisa Milivojevic, Jani Tolonen
** -w --where added 9/10/98 by Jim Faucette
** slave code by David Saez Padros <david@ols.es>
** master/autocommit code by Brian Aker <brian@tangent.org>
** SSL by
** Andrei Errapart <andreie@no.spam.ee>
** Tõnu Samuel  <tonu@please.do.not.remove.this.spam.ee>
** XML by Gary Huntress <ghuntress@mediaone.net> 10/10/01, cleaned up
** and adapted to mysqldump 05/11/01 by Jani Tolonen
** Added --single-transaction option 06/06/2002 by Peter Zaitsev
*/

#define DUMP_VERSION "9.11"

#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>

#include "client_priv.h"
#include "mysql.h"
#include "mysql_version.h"
#include "mysqld_error.h"

/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2
#define EX_CONSCHECK 3
#define EX_EOM 4

/* index into 'show fields from table' */

#define SHOW_FIELDNAME  0
#define SHOW_TYPE  1
#define SHOW_NULL  2
#define SHOW_DEFAULT  4
#define SHOW_EXTRA  5
#define QUOTE_CHAR	'`'

/* Size of buffer for dump's select query */
#define QUERY_LENGTH 1536

static char *add_load_option(char *ptr, const char *object,
			     const char *statement);

static char *field_escape(char *to,const char *from,uint length);
static my_bool  verbose=0,tFlag=0,cFlag=0,dFlag=0,quick=0, extended_insert = 0,
  lock_tables=0,ignore_errors=0,flush_logs=0,replace=0,
  ignore=0,opt_drop=0,opt_keywords=0,opt_lock=0,opt_compress=0,
  opt_delayed=0,create_options=0,opt_quoted=0,opt_databases=0,
  opt_alldbs=0,opt_create_db=0,opt_first_slave=0,
  opt_autocommit=0,opt_master_data,opt_disable_keys=0,opt_xml=0,
  opt_delete_master_logs=0, tty_password=0,
  opt_single_transaction=0, opt_comments= 0,
  opt_hex_blob;
static ulong opt_max_allowed_packet, opt_net_buffer_length;
static MYSQL  mysql_connection,*sock=0;
static char  insert_pat[12 * 1024],*opt_password=0,*current_user=0,
             *current_host=0,*path=0,*fields_terminated=0,
             *lines_terminated=0, *enclosed=0, *opt_enclosed=0, *escaped=0,
             *where=0, *default_charset;
static uint     opt_mysql_port=0;
static my_string opt_mysql_unix_port=0;
static int   first_error=0;
static DYNAMIC_STRING extended_row;
#include <sslopt-vars.h>
FILE  *md_result_file;

static struct my_option my_long_options[] =
{
  {"all-databases", 'A',
   "Dump all the databases. This will be same as --databases with all databases selected.",
   (gptr*) &opt_alldbs, (gptr*) &opt_alldbs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"all", 'a', "Include all MySQL specific create options.",
   (gptr*) &create_options, (gptr*) &create_options, 0, GET_BOOL, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"add-drop-table", OPT_DROP, "Add a 'drop table' before each create.",
   (gptr*) &opt_drop, (gptr*) &opt_drop, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"add-locks", OPT_LOCKS, "Add locks around insert statements.",
   (gptr*) &opt_lock, (gptr*) &opt_lock, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"allow-keywords", OPT_KEYWORDS,
   "Allow creation of column names that are keywords.", (gptr*) &opt_keywords,
   (gptr*) &opt_keywords, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are", (gptr*) &charsets_dir,
   (gptr*) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"complete-insert", 'c', "Use complete insert statements.", (gptr*) &cFlag,
   (gptr*) &cFlag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
   (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"databases", 'B',
   "To dump several databases. Note the difference in usage; In this case no tables are given. All name arguments are regarded as databasenames. 'USE db_name;' will be included in the output.",
   (gptr*) &opt_databases, (gptr*) &opt_databases, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", (gptr*) &default_charset,
   (gptr*) &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"delayed-insert", OPT_DELAYED, "Insert rows with INSERT DELAYED.",
   (gptr*) &opt_delayed, (gptr*) &opt_delayed, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"delete-master-logs", OPT_DELETE_MASTER_LOGS,
   "Delete logs on master after backup. This automatically enables --first-slave.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"disable-keys", 'K',
   "'/*!40000 ALTER TABLE tb_name DISABLE KEYS */; and '/*!40000 ALTER TABLE tb_name ENABLE KEYS */; will be put in the output.", (gptr*) &opt_disable_keys,
   (gptr*) &opt_disable_keys, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"extended-insert", 'e',
   "Allows utilization of the new, much faster INSERT syntax.",
   (gptr*) &extended_insert, (gptr*) &extended_insert, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"fields-terminated-by", OPT_FTB,
   "Fields in the textfile are terminated by ...", (gptr*) &fields_terminated,
   (gptr*) &fields_terminated, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fields-enclosed-by", OPT_ENC,
   "Fields in the importfile are enclosed by ...", (gptr*) &enclosed,
   (gptr*) &enclosed, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0 ,0, 0},
  {"fields-optionally-enclosed-by", OPT_O_ENC,
   "Fields in the i.file are opt. enclosed by ...", (gptr*) &opt_enclosed,
   (gptr*) &opt_enclosed, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0 ,0, 0},
  {"fields-escaped-by", OPT_ESC, "Fields in the i.file are escaped by ...",
   (gptr*) &escaped, (gptr*) &escaped, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"first-slave", 'x', "Locks all tables across all databases.",
   (gptr*) &opt_first_slave, (gptr*) &opt_first_slave, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"flush-logs", 'F', "Flush logs file in server before starting dump. "
    "Note that if you dump many databases at once (using the option "
    "--databases= or --all-databases), the logs will be flushed for "
    "each database dumped.",
   (gptr*) &flush_logs, (gptr*) &flush_logs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"force", 'f', "Continue even if we get an sql-error.",
   (gptr*) &ignore_errors, (gptr*) &ignore_errors, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", (gptr*) &current_host,
   (gptr*) &current_host, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"lines-terminated-by", OPT_LTB, "Lines in the i.file are terminated by ...",
   (gptr*) &lines_terminated, (gptr*) &lines_terminated, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"lock-tables", 'l', "Lock all tables for read.", (gptr*) &lock_tables,
   (gptr*) &lock_tables, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"master-data", OPT_MASTER_DATA,
   "This causes the master position and filename to be appended to your output. This automatically enables --first-slave.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"no-autocommit", OPT_AUTOCOMMIT,
   "Wrap tables with autocommit/commit statements.",
   (gptr*) &opt_autocommit, (gptr*) &opt_autocommit, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"single-transaction", OPT_TRANSACTION,
   "Dump all tables in single transaction to get consistent snapshot. Mutually exclusive with --lock-tables.",
   (gptr*) &opt_single_transaction, (gptr*) &opt_single_transaction, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"no-create-db", 'n',
   "'CREATE DATABASE /*!32312 IF NOT EXISTS*/ db_name;' will not be put in the output. The above line will be added otherwise, if --databases or --all-databases option was given.}",
   (gptr*) &opt_create_db, (gptr*) &opt_create_db, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"no-create-info", 't', "Don't write table creation info.",
   (gptr*) &tFlag, (gptr*) &tFlag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"no-data", 'd', "No row information.", (gptr*) &dFlag, (gptr*) &dFlag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"set-variable", 'O',
   "Change the value of a variable. Please note that this option is deprecated; you can set variables directly with --variable-name=value.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"opt", OPT_OPTIMIZE,
   "Same as --add-drop-table --add-locks --all --quick --extended-insert --lock-tables --disable-keys",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's solicited on the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_mysql_port,
   (gptr*) &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, MYSQL_PORT, 0, 0, 0, 0,
   0},
  {"quick", 'q', "Don't buffer query, dump directly to stdout.",
   (gptr*) &quick, (gptr*) &quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"quote-names",'Q', "Quote table and column names with a `",
   (gptr*) &opt_quoted, (gptr*) &opt_quoted, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"result-file", 'r',
   "Direct output to a given file. This option should be used in MSDOS, because it prevents new line '\\n' from being converted to '\\r\\n' (carriage return + line feed).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
   (gptr*) &opt_mysql_unix_port, (gptr*) &opt_mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
  {"tab",'T',
   "Creates tab separated textfile for each table to given path. (creates .sql and .txt files). NOTE: This only works if mysqldump is run on the same machine as the mysqld daemon.",
   (gptr*) &path, (gptr*) &path, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tables", OPT_TABLES, "Overrides option --databases (-B).",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.",
   (gptr*) &current_user, (gptr*) &current_user, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
#endif
  {"verbose", 'v', "Print info about the various stages.",
   (gptr*) &verbose, (gptr*) &verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version",'V', "Output version information and exit.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"where", 'w', "Dump only selected records; QUOTES mandatory!",
   (gptr*) &where, (gptr*) &where, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"xml", 'X', "Dump a database as well formed XML.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET, "",
    (gptr*) &opt_max_allowed_packet, (gptr*) &opt_max_allowed_packet, 0,
    GET_ULONG, REQUIRED_ARG, 24*1024*1024, 4096, 
   (longlong) 2L*1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"net_buffer_length", OPT_NET_BUFFER_LENGTH, "",
    (gptr*) &opt_net_buffer_length, (gptr*) &opt_net_buffer_length, 0,
    GET_ULONG, REQUIRED_ARG, 1024*1024L-1025, 4096, 16*1024L*1024L,
    MALLOC_OVERHEAD-1024, 1024, 0},
  {"comments", 'i', "Write additional information.",
   (gptr*) &opt_comments, (gptr*) &opt_comments, 0, GET_BOOL, NO_ARG,
   1, 0, 0, 0, 0, 0},
  {"hex-blob", OPT_HEXBLOB, "Dump BLOBs in HEX.",
   (gptr*) &opt_hex_blob, (gptr*) &opt_hex_blob, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static const char *load_default_groups[]= { "mysqldump","client",0 };

static void safe_exit(int error);
static void write_header(FILE *sql_file, char *db_name);
static void print_value(FILE *file, MYSQL_RES  *result, MYSQL_ROW row,
			const char *prefix,const char *name,
			int string_value);
static int dump_selected_tables(char *db, char **table_names, int tables);
static int dump_all_tables_in_db(char *db);
static int init_dumping(char *);
static int dump_databases(char **);
static int dump_all_databases();
static char *quote_name(const char *name, char *buff, my_bool force);
static void print_quoted_xml(FILE *output, char *fname, char *str, uint len);
static const char *check_if_ignore_table(const char *table_name);

#include <help_start.h>

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,DUMP_VERSION,
         MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
  NETWARE_SET_SCREEN_MODE(1);
} /* print_version */


static void short_usage_sub(void)
{
  printf("Usage: %s [OPTIONS] database [tables]\n", my_progname);
  printf("OR     %s [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3...]\n",
	 my_progname);
  printf("OR     %s [OPTIONS] --all-databases [OPTIONS]\n", my_progname);
  NETWARE_SET_SCREEN_MODE(1);
}


static void usage(void)
{
  print_version();
  puts("By Igor Romanenko, Monty, Jani & Sinisa");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Dumping definition and data mysql database or table");
  short_usage_sub();
  print_defaults("my",load_default_groups);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
} /* usage */


static void short_usage(void)
{
  short_usage_sub();
  printf("For more options, use %s --help\n", my_progname);
}

#include <help_end.h>


static void write_header(FILE *sql_file, char *db_name)
{
  if (opt_xml)
  {
    fprintf(sql_file,"<?xml version=\"1.0\"?>\n");
    fprintf(sql_file,"<mysqldump>\n");
  }
  else if (opt_comments)
  {
    fprintf(sql_file, "-- MySQL dump %s\n--\n", DUMP_VERSION);
    fprintf(sql_file, "-- Host: %s    Database: %s\n",
	    current_host ? current_host : "localhost", db_name ? db_name : "");
    fputs("-- ------------------------------------------------------\n",
	  sql_file);
    fprintf(sql_file, "-- Server version\t%s\n",
	    mysql_get_server_info(&mysql_connection));
  }
  return;
} /* write_header */

static void write_footer(FILE *sql_file)
{
  if (opt_xml)
    fprintf(sql_file,"</mysqldump>");
  fputs("\n", sql_file);
} /* write_footer */


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch (optid) {
  case OPT_MASTER_DATA:
    opt_master_data=1;
    opt_first_slave=1;
    break;
  case OPT_DELETE_MASTER_LOGS:
    opt_delete_master_logs=1;
    opt_first_slave=1;
    break;
  case 'p':
    if (argument)
    {
      char *start=argument;
      my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
      opt_password=my_strdup(argument,MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
	start[1]=0;				/* Cut length of argument */
    }
    else
      tty_password=1;
    break;
  case 'r':
    if (!(md_result_file = my_fopen(argument, O_WRONLY | FILE_BINARY,
				    MYF(MY_WME))))
      exit(1);
    break;
  case 'W':
#ifdef __WIN__
    opt_mysql_unix_port=MYSQL_NAMEDPIPE;
#endif
    break;
  case 'T':
    opt_disable_keys=0;
    break;
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:o");
    break;
#include <sslopt-case.h>
  case 'V': print_version(); exit(0);
  case 'X':
    opt_xml = 1;
    opt_disable_keys=0;
    break;
  case 'I':
  case '?':
    usage();
    exit(0);
  case (int) OPT_OPTIMIZE:
    extended_insert=opt_drop=opt_lock=quick=create_options=opt_disable_keys=
    lock_tables=1;
    if (opt_single_transaction) lock_tables=0;
    break;
  case (int) OPT_TABLES:
    opt_databases=0;
    break;
  }
  return 0;
}


static int get_options(int *argc, char ***argv)
{
  int ho_error;
  MYSQL_PARAMETERS *mysql_params= mysql_get_parameters();

  opt_max_allowed_packet= *mysql_params->p_max_allowed_packet;
  opt_net_buffer_length= *mysql_params->p_net_buffer_length;

  md_result_file= stdout;
  load_defaults("my",load_default_groups,argc,argv);

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  *mysql_params->p_max_allowed_packet= opt_max_allowed_packet;
  *mysql_params->p_net_buffer_length= opt_net_buffer_length;

  if (opt_delayed)
    opt_lock=0;				/* Can't have lock with delayed */
  if (!path && (enclosed || opt_enclosed || escaped || lines_terminated ||
		fields_terminated))
  {
    fprintf(stderr,
	    "%s: You must use option --tab with --fields-...\n", my_progname);
    return(1);
  }
  
  if (opt_single_transaction && lock_tables) 
  {
    fprintf(stderr, "%s: You can't use --lock-tables and --single-transaction at the same time.\n", my_progname);
    return(1);    
  }

  if (enclosed && opt_enclosed)
  {
    fprintf(stderr, "%s: You can't use ..enclosed.. and ..optionally-enclosed.. at the same time.\n", my_progname);
    return(1);
  }
  if (replace && ignore)
  {
    fprintf(stderr, "%s: You can't use --ignore (-i) and --replace (-r) at the same time.\n",my_progname);
    return(1);
  }
  if ((opt_databases || opt_alldbs) && path)
  {
    fprintf(stderr,
	    "%s: --databases or --all-databases can't be used with --tab.\n",
	    my_progname);
    return(1);
  }
  if (default_charset)
  {
    if (set_default_charset_by_name(default_charset, MYF(MY_WME)))
      exit(1);
  }
  if ((*argc < 1 && !opt_alldbs) || (*argc > 0 && opt_alldbs))
  {
    short_usage();
    return 1;
  }
  if (tty_password)
    opt_password=get_tty_password(NullS);
  return(0);
} /* get_options */


/*
** DBerror -- prints mysql error message and exits the program.
*/
static void DBerror(MYSQL *mysql, const char *when)
{
  DBUG_ENTER("DBerror");
  my_printf_error(0,"Got error: %d: %s %s", MYF(0),
		  mysql_errno(mysql), mysql_error(mysql), when);
  safe_exit(EX_MYSQLERR);
  DBUG_VOID_RETURN;
} /* DBerror */


static void safe_exit(int error)
{
  if (!first_error)
    first_error= error;
  if (ignore_errors)
    return;
  if (sock)
    mysql_close(sock);
  exit(error);
}
/* safe_exit */


/*
** dbConnect -- connects to the host and selects DB.
**        Also checks whether the tablename is a valid table name.
*/
static int dbConnect(char *host, char *user,char *passwd)
{
  DBUG_ENTER("dbConnect");
  if (verbose)
  {
    fprintf(stderr, "-- Connecting to %s...\n", host ? host : "localhost");
  }
  mysql_init(&mysql_connection);
  if (opt_compress)
    mysql_options(&mysql_connection,MYSQL_OPT_COMPRESS,NullS);
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
    mysql_ssl_set(&mysql_connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
#endif
  if (!(sock= mysql_real_connect(&mysql_connection,host,user,passwd,
         NULL,opt_mysql_port,opt_mysql_unix_port,
         0)))
  {
    DBerror(&mysql_connection, "when trying to connect");
    return 1;
  }
  return 0;
} /* dbConnect */


/*
** dbDisconnect -- disconnects from the host.
*/
static void dbDisconnect(char *host)
{
  if (verbose)
    fprintf(stderr, "-- Disconnecting from %s...\n", host ? host : "localhost");
  mysql_close(sock);
} /* dbDisconnect */


static void unescape(FILE *file,char *pos,uint length)
{
  char *tmp;
  DBUG_ENTER("unescape");
  if (!(tmp=(char*) my_malloc(length*2+1, MYF(MY_WME))))
  {
    ignore_errors=0;				/* Fatal error */
    safe_exit(EX_MYSQLERR);			/* Force exit */
  }
  mysql_real_escape_string(&mysql_connection, tmp, pos, length);
  fputc('\'', file);
  fputs(tmp, file);
  fputc('\'', file);
  my_free(tmp, MYF(MY_WME));
  DBUG_VOID_RETURN;
} /* unescape */


static my_bool test_if_special_chars(const char *str)
{
#if MYSQL_VERSION_ID >= 32300
  for ( ; *str ; str++)
    if (!isvar(*str) && *str != '$')
      return 1;
#endif
  return 0;
} /* test_if_special_chars */


static char *quote_name(const char *name, char *buff, my_bool force)
{
  char *to= buff;
  if (!force && !opt_quoted && !test_if_special_chars(name))
    return (char*) name;
  *to++= QUOTE_CHAR;
  while (*name)
  {
    if (*name == QUOTE_CHAR)
      *to= QUOTE_CHAR;
    *to++= *name++;
  }
  to[0]=QUOTE_CHAR;
  to[1]=0;
  return buff;
} /* quote_name */



static char *quote_for_like(const char *name, char *buff)
{
  char *to= buff;
  *to++= '\'';
  while (*name)
  {
    if (*name == '\'' || *name == '_' || *name == '\\' || *name == '%')
      *to++= '\\';
    *to++= *name++;
  }
  to[0]= '\'';
  to[1]= 0;
  return buff;
}


/*
  getStructure -- retrievs database structure, prints out corresponding
  CREATE statement and fills out insert_pat.

  RETURN
    number of fields in table, 0 if error
*/

static uint getTableStructure(char *table, char* db)
{
  MYSQL_RES  *tableRes;
  MYSQL_ROW  row;
  my_bool    init=0;
  uint       numFields;
  char	     *strpos, *result_table, *opt_quoted_table;
  const char *delayed;
  char	     name_buff[NAME_LEN+3],table_buff[NAME_LEN*2+3];
  char	     table_buff2[NAME_LEN*2+3];
  FILE       *sql_file = md_result_file;
  DBUG_ENTER("getTableStructure");

  delayed= opt_delayed ? " DELAYED " : "";

  if (verbose)
    fprintf(stderr, "-- Retrieving table structure for table %s...\n", table);

  sprintf(insert_pat,"SET OPTION SQL_QUOTE_SHOW_CREATE=%d",
	  (opt_quoted || opt_keywords));
  result_table=     quote_name(table, table_buff, 1);
  opt_quoted_table= quote_name(table, table_buff2, 0);
  if (!mysql_query(sock,insert_pat))
  {
    /* using SHOW CREATE statement */
    if (!tFlag)
    {
      /* Make an sql-file, if path was given iow. option -T was given */
      char buff[20+FN_REFLEN];

      sprintf(buff,"show create table %s", result_table);
      if (mysql_query(sock, buff))
      {
        fprintf(stderr, "%s: Can't get CREATE TABLE for table %s (%s)\n",
		      my_progname, result_table, mysql_error(sock));
        safe_exit(EX_MYSQLERR);
        DBUG_RETURN(0);
      }

      if (path)
      {
        char filename[FN_REFLEN], tmp_path[FN_REFLEN];
        convert_dirname(tmp_path,path,NullS);
        sql_file= my_fopen(fn_format(filename, table, tmp_path, ".sql", 4),
				 O_WRONLY, MYF(MY_WME));
        if (!sql_file)			/* If file couldn't be opened */
        {
	  safe_exit(EX_MYSQLERR);
	  DBUG_RETURN(0);
        }
        write_header(sql_file, db);
      }
      if (!opt_xml && opt_comments)
	fprintf(sql_file, "\n--\n-- Table structure for table %s\n--\n\n",
		result_table);
      if (opt_drop)
        fprintf(sql_file, "DROP TABLE IF EXISTS %s;\n", opt_quoted_table);

      tableRes=mysql_store_result(sock);
      row=mysql_fetch_row(tableRes);
      if (!opt_xml)
	fprintf(sql_file, "%s;\n", row[1]);
      mysql_free_result(tableRes);
    }
    sprintf(insert_pat,"show fields from %s", result_table);
    if (mysql_query(sock,insert_pat) || !(tableRes=mysql_store_result(sock)))
    {
      fprintf(stderr, "%s: Can't get info about table: %s\nerror: %s\n",
	      my_progname, result_table, mysql_error(sock));
      if (path)
	my_fclose(sql_file, MYF(MY_WME));
      safe_exit(EX_MYSQLERR);
      DBUG_RETURN(0);
    }

    if (cFlag)
      sprintf(insert_pat, "INSERT %sINTO %s (", delayed, opt_quoted_table);
    else
    {
      sprintf(insert_pat, "INSERT %sINTO %s VALUES ", delayed,
	      opt_quoted_table);
      if (!extended_insert)
        strcat(insert_pat,"(");
    }

    strpos=strend(insert_pat);
    while ((row=mysql_fetch_row(tableRes)))
    {
      if (init)
      {
        if (cFlag)
	  strpos=strmov(strpos,", ");
      }
      init=1;
      if (cFlag)
        strpos=strmov(strpos,quote_name(row[SHOW_FIELDNAME], name_buff, 0));
    }
    numFields = (uint) mysql_num_rows(tableRes);
    mysql_free_result(tableRes);
  }
  else
  {
  /*  fprintf(stderr, "%s: Can't set SQL_QUOTE_SHOW_CREATE option (%s)\n",
      my_progname, mysql_error(sock)); */

    sprintf(insert_pat,"show fields from %s", result_table);
    if (mysql_query(sock,insert_pat) || !(tableRes=mysql_store_result(sock)))
    {
      fprintf(stderr, "%s: Can't get info about table: %s\nerror: %s\n",
		    my_progname, result_table, mysql_error(sock));
      safe_exit(EX_MYSQLERR);
      DBUG_RETURN(0);
    }

    /* Make an sql-file, if path was given iow. option -T was given */
    if (!tFlag)
    {
      if (path)
      {
        char filename[FN_REFLEN], tmp_path[FN_REFLEN];
        convert_dirname(tmp_path,path,NullS);
        sql_file= my_fopen(fn_format(filename, table, tmp_path, ".sql", 4),
				 O_WRONLY, MYF(MY_WME));
        if (!sql_file)			/* If file couldn't be opened */
        {
	  safe_exit(EX_MYSQLERR);
	  DBUG_RETURN(0);
        }
        write_header(sql_file, db);
      }
      if (!opt_xml && opt_comments)
	fprintf(sql_file, "\n--\n-- Table structure for table %s\n--\n\n",
		result_table);
      if (opt_drop)
        fprintf(sql_file, "DROP TABLE IF EXISTS %s;\n",result_table);
      fprintf(sql_file, "CREATE TABLE %s (\n", result_table);
    }
    if (cFlag)
      sprintf(insert_pat, "INSERT %sINTO %s (", delayed, result_table);
    else
    {
      sprintf(insert_pat, "INSERT %sINTO %s VALUES ", delayed, result_table);
      if (!extended_insert)
        strcat(insert_pat,"(");
    }

    strpos=strend(insert_pat);
    while ((row=mysql_fetch_row(tableRes)))
    {
      ulong *lengths=mysql_fetch_lengths(tableRes);
      if (init)
      {
        if (!tFlag)
	  fputs(",\n",sql_file);
        if (cFlag)
	  strpos=strmov(strpos,", ");
      }
      init=1;
      if (cFlag)
        strpos=strmov(strpos,quote_name(row[SHOW_FIELDNAME], name_buff, 0));
      if (!tFlag)
      {
        if (opt_keywords)
	  fprintf(sql_file, "  %s.%s %s", result_table,
		  quote_name(row[SHOW_FIELDNAME],name_buff, 0),
		  row[SHOW_TYPE]);
        else
	  fprintf(sql_file, "  %s %s", quote_name(row[SHOW_FIELDNAME],
						  name_buff, 0),
		  row[SHOW_TYPE]);
        if (row[SHOW_DEFAULT])
        {
	  fputs(" DEFAULT ", sql_file);
	  unescape(sql_file, row[SHOW_DEFAULT], lengths[SHOW_DEFAULT]);
        }
        if (!row[SHOW_NULL][0])
	  fputs(" NOT NULL", sql_file);
        if (row[SHOW_EXTRA][0])
	  fprintf(sql_file, " %s",row[SHOW_EXTRA]);
      }
    }
    numFields = (uint) mysql_num_rows(tableRes);
    mysql_free_result(tableRes);
    if (!tFlag)
    {
      /* Make an sql-file, if path was given iow. option -T was given */
      char buff[20+FN_REFLEN];
      uint keynr,primary_key;
      sprintf(buff,"show keys from %s", result_table);
      if (mysql_query(sock, buff))
      {
        fprintf(stderr, "%s: Can't get keys for table %s (%s)\n",
		my_progname, result_table, mysql_error(sock));
        if (path)
	  my_fclose(sql_file, MYF(MY_WME));
        safe_exit(EX_MYSQLERR);
        DBUG_RETURN(0);
      }

      tableRes=mysql_store_result(sock);
      /* Find first which key is primary key */
      keynr=0;
      primary_key=INT_MAX;
      while ((row=mysql_fetch_row(tableRes)))
      {
        if (atoi(row[3]) == 1)
        {
	  keynr++;
#ifdef FORCE_PRIMARY_KEY
	  if (atoi(row[1]) == 0 && primary_key == INT_MAX)
	    primary_key=keynr;
#endif
	  if (!strcmp(row[2],"PRIMARY"))
	  {
	    primary_key=keynr;
	    break;
	  }
        }
      }
      mysql_data_seek(tableRes,0);
      keynr=0;
      while ((row=mysql_fetch_row(tableRes)))
      {
        if (atoi(row[3]) == 1)
        {
	  if (keynr++)
	    putc(')', sql_file);
	  if (atoi(row[1]))       /* Test if duplicate key */
	    /* Duplicate allowed */
	    fprintf(sql_file, ",\n  KEY %s (",quote_name(row[2],name_buff,0));
	  else if (keynr == primary_key)
	    fputs(",\n  PRIMARY KEY (",sql_file); /* First UNIQUE is primary */
	  else
	    fprintf(sql_file, ",\n  UNIQUE %s (",quote_name(row[2],name_buff,0));
        }
        else
	  putc(',', sql_file);
        fputs(quote_name(row[4], name_buff, 0), sql_file);
        if (row[7])
	  fprintf(sql_file, " (%s)",row[7]);      /* Sub key */
      }
      if (keynr)
        putc(')', sql_file);
      fputs("\n)",sql_file);

      /* Get MySQL specific create options */
      if (create_options)
      {
	char show_name_buff[FN_REFLEN];
        sprintf(buff,"show table status like %s",
		quote_for_like(table, show_name_buff));
        if (mysql_query(sock, buff))
        {
	  if (mysql_errno(sock) != ER_PARSE_ERROR)
	  {					/* If old MySQL version */
	    if (verbose)
	      fprintf(stderr,
		      "-- Warning: Couldn't get status information for table %s (%s)\n",
		      result_table,mysql_error(sock));
	  }
        }
        else if (!(tableRes=mysql_store_result(sock)) ||
		 !(row=mysql_fetch_row(tableRes)))
        {
	  fprintf(stderr,
		  "Error: Couldn't read status information for table %s (%s)\n",
		  result_table,mysql_error(sock));
        }
        else
        {
	  fputs("/*!",sql_file);
	  print_value(sql_file,tableRes,row,"type=","Type",0);
	  print_value(sql_file,tableRes,row,"","Create_options",0);
	  print_value(sql_file,tableRes,row,"comment=","Comment",1);
	  fputs(" */",sql_file);
        }
        mysql_free_result(tableRes);		/* Is always safe to free */
      }
      fputs(";\n", sql_file);
    }
  }
  if (cFlag)
  {
    strpos=strmov(strpos,") VALUES ");
    if (!extended_insert)
      strpos=strmov(strpos,"(");
  }
  if (sql_file != md_result_file)
    my_fclose(sql_file, MYF(MY_WME));
  DBUG_RETURN(numFields);
} /* getTableStructure */


static char *add_load_option(char *ptr,const char *object,
			     const char *statement)
{
  if (object)
  {
    /* Don't escape hex constants */
    if (object[0] == '0' && (object[1] == 'x' || object[1] == 'X'))
      ptr= strxmov(ptr," ",statement," ",object,NullS);
    else
    {
      /* char constant; escape */
      ptr= strxmov(ptr," ",statement," '",NullS);
      ptr= field_escape(ptr,object,(uint) strlen(object));
      *ptr++= '\'';
    }
  }
  return ptr;
} /* add_load_option */


/*
** Allow the user to specify field terminator strings like:
** "'", "\", "\\" (escaped backslash), "\t" (tab), "\n" (newline)
** This is done by doubleing ' and add a end -\ if needed to avoid
** syntax errors from the SQL parser.
*/

static char *field_escape(char *to,const char *from,uint length)
{
  const char *end;
  uint end_backslashes=0;

  for (end= from+length; from != end; from++)
  {
    *to++= *from;
    if (*from == '\\')
      end_backslashes^=1;    /* find odd number of backslashes */
    else
    {
      if (*from == '\'' && !end_backslashes)
	*to++= *from;      /* We want a duplicate of "'" for MySQL */
      end_backslashes=0;
    }
  }
  /* Add missing backslashes if user has specified odd number of backs.*/
  if (end_backslashes)
    *to++= '\\';
  return to;
} /* field_escape */


static char *alloc_query_str(ulong size)
{
  char *query;

  if (!(query= (char*) my_malloc(size, MYF(MY_WME))))
  {
    ignore_errors= 0;   			/* Fatal error */
    safe_exit(EX_MYSQLERR);			/* Force exit */
  }
  return query;
}

/*
** dumpTable saves database contents as a series of INSERT statements.
*/
static void dumpTable(uint numFields, char *table)
{
  char query_buf[QUERY_LENGTH], *end, buff[256],table_buff[NAME_LEN+3];
  char *result_table, table_buff2[NAME_LEN*2+3], *opt_quoted_table;
  char *query= query_buf;
  MYSQL_RES	*res;
  MYSQL_FIELD	*field;
  MYSQL_ROW	row;
  ulong		rownr, row_break, total_length, init_length;
  const char    *table_type;
  int error= 0;

  result_table= quote_name(table,table_buff, 1);
  opt_quoted_table= quote_name(table, table_buff2, 0);

  /* Check table type */
  if ((table_type= check_if_ignore_table(table)))
  {
    if (verbose)
      fprintf(stderr,
	      "-- Skipping data for table '%s' because it's of type %s\n",
	      table, table_type);
    return;
  }

  if (verbose)
    fprintf(stderr, "-- Sending SELECT query...\n");
  if (path)
  {
    char filename[FN_REFLEN], tmp_path[FN_REFLEN];
    convert_dirname(tmp_path,path,NullS);
    my_load_path(tmp_path, tmp_path, NULL);
    fn_format(filename, table, tmp_path, ".txt", 4);
    my_delete(filename, MYF(0)); /* 'INTO OUTFILE' doesn't work, if
				    filename wasn't deleted */
    to_unix_path(filename);
    sprintf(query, "SELECT /*!40001 SQL_NO_CACHE */ * INTO OUTFILE '%s'",
	    filename);
    end= strend(query);
    if (replace)
      end= strmov(end, " REPLACE");
    if (ignore)
      end= strmov(end, " IGNORE");

    if (fields_terminated || enclosed || opt_enclosed || escaped)
      end= strmov(end, " FIELDS");
    end= add_load_option(end, fields_terminated, " TERMINATED BY");
    end= add_load_option(end, enclosed, " ENCLOSED BY");
    end= add_load_option(end, opt_enclosed, " OPTIONALLY ENCLOSED BY");
    end= add_load_option(end, escaped, " ESCAPED BY");
    end= add_load_option(end, lines_terminated, " LINES TERMINATED BY");
    *end= '\0';

    sprintf(buff," FROM %s", result_table);
    end= strmov(end,buff);
    if (where)
    {
      query= alloc_query_str((ulong) (strlen(where) + (end - query) + 10));
      end= strxmov(query, query_buf, " WHERE ", where, NullS);
    }
    if (mysql_real_query(sock, query, (uint) (end - query)))
    {
      DBerror(sock, "when executing 'SELECT INTO OUTFILE'");
      return;
    }
  }
  else
  {
    if (!opt_xml && opt_comments)
      fprintf(md_result_file,"\n--\n-- Dumping data for table %s\n--\n",
	      result_table);
    sprintf(query, "SELECT /*!40001 SQL_NO_CACHE */ * FROM %s",
	    result_table);
    if (where)
    {
      if (!opt_xml && opt_comments)
	fprintf(md_result_file,"-- WHERE:  %s\n",where);
      query= alloc_query_str((ulong) (strlen(where) + strlen(query) + 10));
      strxmov(query, query_buf, " WHERE ", where, NullS);
    }
    if (!opt_xml)
      fputs("\n", md_result_file);
    if (mysql_query(sock, query))
    {
      DBerror(sock, "when retrieving data from server");
      error= EX_CONSCHECK;
      goto err;
    }
    if (quick)
      res=mysql_use_result(sock);
    else
      res=mysql_store_result(sock);
    if (!res)
    {
      DBerror(sock, "when retrieving data from server");
      error= EX_CONSCHECK;
      goto err;
    }
    if (verbose)
      fprintf(stderr, "-- Retrieving rows...\n");
    if (mysql_num_fields(res) != numFields)
    {
      fprintf(stderr,"%s: Error in field count for table: %s !  Aborting.\n",
	      my_progname, result_table);
      error= EX_CONSCHECK;
      goto err;
    }

    if (opt_disable_keys)
      fprintf(md_result_file, "\n/*!40000 ALTER TABLE %s DISABLE KEYS */;\n",
	      opt_quoted_table);
    if (opt_lock)
      fprintf(md_result_file,"LOCK TABLES %s WRITE;\n", opt_quoted_table);

    total_length= opt_net_buffer_length;		/* Force row break */
    row_break=0;
    rownr=0;
    init_length=(uint) strlen(insert_pat)+4;
    if (opt_xml)
      fprintf(md_result_file, "\t<table name=\"%s\">\n", table);

    if (opt_autocommit)
      fprintf(md_result_file, "set autocommit=0;\n");

    while ((row=mysql_fetch_row(res)))
    {
      uint i;
      ulong *lengths=mysql_fetch_lengths(res);
      rownr++;
      if (!extended_insert && !opt_xml)
	fputs(insert_pat,md_result_file);
      mysql_field_seek(res,0);

      if (opt_xml)
        fprintf(md_result_file, "\t<row>\n");

      for (i = 0; i < mysql_num_fields(res); i++)
      {
        int is_blob;
	if (!(field = mysql_fetch_field(res)))
	{
	  sprintf(query,"%s: Not enough fields from table %s! Aborting.\n",
		  my_progname, result_table);
	  fputs(query,stderr);
	  error= EX_CONSCHECK;
	  goto err;
	}
	
        is_blob= (opt_hex_blob && (field->flags & BINARY_FLAG) &&
                  (field->type == FIELD_TYPE_STRING ||
                   field->type == FIELD_TYPE_VAR_STRING ||
                   field->type == FIELD_TYPE_BLOB ||
                   field->type == FIELD_TYPE_LONG_BLOB ||
                   field->type == FIELD_TYPE_MEDIUM_BLOB ||
                   field->type == FIELD_TYPE_TINY_BLOB)) ? 1 : 0;
	if (extended_insert)
	{
	  ulong length = lengths[i];
	  if (i == 0)
	    dynstr_set(&extended_row,"(");
	  else
	    dynstr_append(&extended_row,",");

	  if (row[i])
	  {
	    if (length)
	    {
	      if (!IS_NUM_FIELD(field))
	      {
	        /*
	          "length * 2 + 2" is OK for both HEX and non-HEX modes:
	          - In HEX mode we need exactly 2 bytes per character
	          plus 2 bytes for '0x' prefix.
	          - In non-HEX mode we need up to 2 bytes per character,
	          plus 2 bytes for leading and trailing '\'' characters.
	        */
		if (dynstr_realloc(&extended_row,length * 2+2))
		{
		  fputs("Aborting dump (out of memory)",stderr);
		  error= EX_EOM;
		  goto err;
		}
                if (opt_hex_blob && is_blob)
                {
                  dynstr_append(&extended_row, "0x");
                  extended_row.length+= mysql_hex_string(extended_row.str + 
                                                         extended_row.length,
                                                         row[i], length);
                  extended_row.str[extended_row.length]= '\0';
                }
                else
                {
		  dynstr_append(&extended_row,"\'");
		  extended_row.length +=
		  mysql_real_escape_string(&mysql_connection,
		  			   &extended_row.str[extended_row.length],
		  			   row[i],length);
		  extended_row.str[extended_row.length]='\0';
		  dynstr_append(&extended_row,"\'");
		}
	      }
	      else
	      {
		/* change any strings ("inf", "-inf", "nan") into NULL */
		char *ptr = row[i];
		if (isalpha(*ptr) || (*ptr == '-' && isalpha(ptr[1])))
		  dynstr_append(&extended_row, "NULL");
		else
		{
		  if (field->type == FIELD_TYPE_DECIMAL)
		  {
		    /* add " signs around */
		    dynstr_append(&extended_row, "\'");
		    dynstr_append(&extended_row, ptr);
		    dynstr_append(&extended_row, "\'");
		  }
		  else
		    dynstr_append(&extended_row, ptr);
		}
	      }
	    }
	    else
	      dynstr_append(&extended_row,"\'\'");
	  }
	  else if (dynstr_append(&extended_row,"NULL"))
	  {
	    fputs("Aborting dump (out of memory)",stderr);
	    error= EX_EOM;
	    goto err;
	  }
	}
	else
	{
	  if (i && !opt_xml)
	    fputc(',', md_result_file);
	  if (row[i])
	  {
	    if (!IS_NUM_FIELD(field))
	    {
	      if (opt_xml)
		print_quoted_xml(md_result_file, field->name, row[i],
				 lengths[i]);
	      else if (opt_hex_blob && is_blob)
              { /* sakaik got this idea. */
                ulong counter;
                char xx[4];
                unsigned char *ptr= row[i];
                fputs("0x", md_result_file);
                for (counter = 0; counter < lengths[i]; counter++)
                {
                  sprintf(xx, "%02X", ptr[counter]);
                  fputs(xx, md_result_file);
                }
              }
              else
		unescape(md_result_file, row[i], lengths[i]);
	    }
	    else
	    {
	      /* change any strings ("inf", "-inf", "nan") into NULL */
	      char *ptr = row[i];
	      if (opt_xml)
		fprintf(md_result_file, "\t\t<field name=\"%s\">%s</field>\n",
			field->name,!isalpha(*ptr) ?ptr: "NULL");
	      else if (isalpha(*ptr) || (*ptr == '-' && isalpha(ptr[1])))
	        fputs("NULL", md_result_file);
	      else
	      {
		if (field->type == FIELD_TYPE_DECIMAL)
		{
		  /* add " signs around */
		  fputs("\'", md_result_file);
		  fputs(ptr, md_result_file);
		  fputs("\'", md_result_file);
		}
		else
		  fputs(ptr, md_result_file);
	      }
	    }
	  }
	  else
	  {
	    if (opt_xml)
	      fprintf(md_result_file, "\t\t<field name=\"%s\">%s</field>\n",
		      field->name, "NULL");
	    else
	      fputs("NULL", md_result_file);
	  }
	}
      }

      if (opt_xml)
        fprintf(md_result_file, "\t</row>\n");

      if (extended_insert)
      {
	ulong row_length;
	dynstr_append(&extended_row,")");
        row_length = 2 + extended_row.length;
        if (total_length + row_length < opt_net_buffer_length)
        {
	  total_length += row_length;
	  fputc(',',md_result_file);		/* Always row break */
	  fputs(extended_row.str,md_result_file);
	}
        else
        {
	  if (row_break && !opt_xml)
	    fputs(";\n", md_result_file);
	  row_break=1;				/* This is first row */

	  if (!opt_xml)
	  {
	    fputs(insert_pat,md_result_file);
	    fputs(extended_row.str,md_result_file);
	  }
	  total_length = row_length+init_length;
        }
      }
      else if (!opt_xml)
	fputs(");\n", md_result_file);
    }

    /* XML - close table tag and supress regular output */
    if (opt_xml)
	fprintf(md_result_file, "\t</table>\n");
    else if (extended_insert && row_break)
      fputs(";\n", md_result_file);		/* If not empty table */
    fflush(md_result_file);
    if (mysql_errno(sock))
    {
      sprintf(query,"%s: Error %d: %s when dumping table %s at row: %ld\n",
	      my_progname,
	      mysql_errno(sock),
	      mysql_error(sock),
	      result_table,
	      rownr);
      fputs(query,stderr);
      error= EX_CONSCHECK;
      goto err;
    }
    if (opt_lock)
      fputs("UNLOCK TABLES;\n", md_result_file);
    if (opt_disable_keys)
      fprintf(md_result_file,"/*!40000 ALTER TABLE %s ENABLE KEYS */;\n",
	      opt_quoted_table);
    if (opt_autocommit)
      fprintf(md_result_file, "commit;\n");
    mysql_free_result(res);
    if (query != query_buf)
      my_free(query, MYF(MY_ALLOW_ZERO_PTR));
  } 
  return;

err:
  if (query != query_buf)
    my_free(query, MYF(MY_ALLOW_ZERO_PTR));
  safe_exit(error);
  return;
} /* dumpTable */


static void print_quoted_xml(FILE *output, char *fname, char *str, uint len)
{
  const char *end;

  fprintf(output, "\t\t<field name=\"%s\">", fname);
  for (end = str + len; str != end; str++)
  {
    if (*str == '<')
      fputs("&lt;", output);
    else if (*str == '>')
      fputs("&gt;", output);
    else if (*str == '&')
      fputs("&amp;", output);
    else if (*str == '\"')
      fputs("&quot;", output);
    else
      fputc(*str, output);
  }
  fprintf(output, "</field>\n");
}

static char *getTableName(int reset)
{
  static MYSQL_RES *res = NULL;
  MYSQL_ROW    row;

  if (!res)
  {
    if (!(res = mysql_list_tables(sock,NullS)))
      return(NULL);
  }
  if ((row = mysql_fetch_row(res)))
    return((char*) row[0]);

  if (reset)
    mysql_data_seek(res,0);      /* We want to read again */
  else
  {
    mysql_free_result(res);
    res = NULL;
  }
  return(NULL);
} /* getTableName */


static int dump_all_databases()
{
  MYSQL_ROW row;
  MYSQL_RES *tableres;
  int result=0;

  if (mysql_query(sock, "SHOW DATABASES") ||
      !(tableres = mysql_store_result(sock)))
  {
    my_printf_error(0, "Error: Couldn't execute 'SHOW DATABASES': %s",
		    MYF(0), mysql_error(sock));
    return 1;
  }
  while ((row = mysql_fetch_row(tableres)))
  {
    if (dump_all_tables_in_db(row[0]))
      result=1;
  }
  return result;
}
/* dump_all_databases */


static int dump_databases(char **db_names)
{
  int result=0;
  for ( ; *db_names ; db_names++)
  {
    if (dump_all_tables_in_db(*db_names))
      result=1;
  }
  return result;
} /* dump_databases */


static int init_dumping(char *database)
{
  if (mysql_select_db(sock, database))
  {
    DBerror(sock, "when selecting the database");
    return 1;			/* If --force */
  }
  if (!path && !opt_xml)
  {
    if (opt_databases || opt_alldbs)
    {
      /* length of table name * 2 (if name contain quotas), 2 quotas and 0 */
      char quoted_database_buf[64*2+3];
      char *qdatabase= quote_name(database,quoted_database_buf,opt_quoted);
      if (opt_comments)
	fprintf(md_result_file,"\n--\n-- Current Database: %s\n--\n", database);
      if (!opt_create_db)
	fprintf(md_result_file,"\nCREATE DATABASE /*!32312 IF NOT EXISTS*/ %s;\n",
		qdatabase);
      fprintf(md_result_file,"\nUSE %s;\n", database);
    }
  }
  if (extended_insert)
    if (init_dynamic_string(&extended_row, "", 1024, 1024))
      exit(EX_EOM);
  return 0;
} /* init_dumping */


static int dump_all_tables_in_db(char *database)
{
  char *table;
  uint numrows;
  char table_buff[NAME_LEN*2+3];

  if (init_dumping(database))
    return 1;
  if (opt_xml)
    fprintf(md_result_file, "<database name=\"%s\">\n", database);
  if (lock_tables)
  {
    DYNAMIC_STRING query;
    init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
    for (numrows=0 ; (table = getTableName(1)) ; numrows++)
    {
      dynstr_append(&query, quote_name(table, table_buff, 1));
      dynstr_append(&query, " READ /*!32311 LOCAL */,");
    }
    if (numrows && mysql_real_query(sock, query.str, query.length-1))
      DBerror(sock, "when using LOCK TABLES");
            /* We shall continue here, if --force was given */
    dynstr_free(&query);
  }
  if (flush_logs)
  {
    if (mysql_refresh(sock, REFRESH_LOG))
      DBerror(sock, "when doing refresh");
           /* We shall continue here, if --force was given */
  }
  while ((table = getTableName(0)))
  {
    numrows = getTableStructure(table, database);
    if (!dFlag && numrows > 0)
      dumpTable(numrows,table);
  }
  if (opt_xml)
    fprintf(md_result_file, "</database>\n");
  if (lock_tables)
    mysql_query(sock,"UNLOCK TABLES");
  return 0;
} /* dump_all_tables_in_db */



static int dump_selected_tables(char *db, char **table_names, int tables)
{
  uint numrows;
  char table_buff[NAME_LEN*+3];

  if (init_dumping(db))
    return 1;
  if (lock_tables)
  {
    DYNAMIC_STRING query;
    int i;

    init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
    for (i=0 ; i < tables ; i++)
    {
      dynstr_append(&query, quote_name(table_names[i], table_buff, 1));
      dynstr_append(&query, " READ /*!32311 LOCAL */,");
    }
    if (mysql_real_query(sock, query.str, query.length-1))
      DBerror(sock, "when doing LOCK TABLES");
       /* We shall countinue here, if --force was given */
    dynstr_free(&query);
  }
  if (flush_logs)
  {
    if (mysql_refresh(sock, REFRESH_LOG))
      DBerror(sock, "when doing refresh");
     /* We shall countinue here, if --force was given */
  }
  if (opt_xml)
    fprintf(md_result_file, "<database name=\"%s\">\n", db);
  for (; tables > 0 ; tables-- , table_names++)
  {
    numrows = getTableStructure(*table_names, db);
    if (!dFlag && numrows > 0)
      dumpTable(numrows, *table_names);
  }
  if (opt_xml)
    fprintf(md_result_file, "</database>\n");
  if (lock_tables)
    mysql_query(sock,"UNLOCK TABLES");
  return 0;
} /* dump_selected_tables */


/* Print a value with a prefix on file */
static void print_value(FILE *file, MYSQL_RES  *result, MYSQL_ROW row,
			const char *prefix, const char *name,
			int string_value)
{
  MYSQL_FIELD	*field;
  mysql_field_seek(result, 0);

  for ( ; (field = mysql_fetch_field(result)) ; row++)
  {
    if (!strcmp(field->name,name))
    {
      if (row[0] && row[0][0] && strcmp(row[0],"0")) /* Skip default */
      {
	fputc(' ',file);
	fputs(prefix, file);
	if (string_value)
	  unescape(file,row[0],(uint) strlen(row[0]));
	else
	  fputs(row[0], file);
	return;
      }
    }
  }
  return;					/* This shouldn't happen */
} /* print_value */


/*
  Check if we the table is one of the table types that should be ignored:
  MRG_ISAM, MRG_MYISAM

  SYNOPSIS
    check_if_ignore_table()
    table_name			Table name to check

  GLOBAL VARIABLES
    sock			MySQL socket
    verbose			Write warning messages

  RETURN
    0	Table should be backuped
    #	Type of table (that should be skipped)
*/

static const char *check_if_ignore_table(const char *table_name)
{
  char buff[FN_REFLEN+80], show_name_buff[FN_REFLEN];
  MYSQL_RES *res;
  MYSQL_ROW row;
  const char *result= 0;

  sprintf(buff,"show table status like %s",
	  quote_for_like(table_name, show_name_buff));
  if (mysql_query(sock, buff))
  {
    if (mysql_errno(sock) != ER_PARSE_ERROR)
    {					/* If old MySQL version */
      if (verbose)
	fprintf(stderr,
		"-- Warning: Couldn't get status information for table %s (%s)\n",
		table_name,mysql_error(sock));
      return 0;					/* assume table is ok */
    }
  }
  if (!(res= mysql_store_result(sock)) ||
      !(row= mysql_fetch_row(res)))
  {
    fprintf(stderr,
	    "Error: Couldn't read status information for table %s (%s)\n",
	    table_name, mysql_error(sock));
    if (res)
      mysql_free_result(res);
    return 0;					/* assume table is ok */
  }
  if (strcmp(row[1], (result= "MRG_MyISAM")) &&
      strcmp(row[1], (result= "MRG_ISAM")))
    result= 0;
  mysql_free_result(res);  
  return result;
}


int main(int argc, char **argv)
{
  MYSQL_ROW row;
  MYSQL_RES *master;

  MY_INIT(argv[0]);
  if (get_options(&argc, &argv))
  {
    my_end(0);
    exit(EX_USAGE);
  }
  if (dbConnect(current_host, current_user, opt_password))
    exit(EX_MYSQLERR);
  if (!path)
    write_header(md_result_file, *argv);

  if (opt_first_slave)
  {
    lock_tables=0;				/* No other locks needed */
    if (mysql_query(sock, "FLUSH TABLES WITH READ LOCK"))
    {
      my_printf_error(0, "Error: Couldn't execute 'FLUSH TABLES WITH READ LOCK': %s",
                      MYF(0), mysql_error(sock));
      my_end(0);
      return(first_error);
    }
  }
  else if (opt_single_transaction)
  {
    /* There is no sense to start transaction if all tables are locked */ 
    if (mysql_query(sock, "BEGIN"))
    {
      my_printf_error(0, "Error: Couldn't execute 'BEGIN': %s",
                        MYF(0), mysql_error(sock));
      my_end(0);
      return(first_error);
    }    
  }
  if (opt_alldbs)
    dump_all_databases();
  else if (argc > 1 && !opt_databases)
  {
    /* Only one database and selected table(s) */
    dump_selected_tables(*argv, (argv + 1), (argc - 1));
  }
  else
  {
    /* One or more databases, all tables */
    dump_databases(argv);
  }

  if (opt_first_slave)
  {
    if (opt_delete_master_logs && mysql_query(sock, "FLUSH MASTER"))
    {
      my_printf_error(0, "Error: Couldn't execute 'FLUSH MASTER': %s",
		      MYF(0), mysql_error(sock));
    }
    if (opt_master_data)
    {
      if (mysql_query(sock, "SHOW MASTER STATUS") ||
	  !(master = mysql_store_result(sock)))
	my_printf_error(0, "Error: Couldn't execute 'SHOW MASTER STATUS': %s",
			MYF(0), mysql_error(sock));
      else
      {
	row = mysql_fetch_row(master);
	if (row && row[0] && row[1])
	{
	  if (opt_comments)
	    fprintf(md_result_file,
		    "\n--\n-- Position to start replication from\n--\n\n");
	  fprintf(md_result_file,
		  "CHANGE MASTER TO MASTER_LOG_FILE='%s', \
MASTER_LOG_POS=%s ;\n",row[0],row[1]); 
	}
	mysql_free_result(master);
      }
    }
    if (mysql_query(sock, "UNLOCK TABLES"))
      my_printf_error(0, "Error: Couldn't execute 'UNLOCK TABLES': %s",
		      MYF(0), mysql_error(sock));
  }
  else if (opt_single_transaction) /* Just to make it beautiful enough */
  {
    /*
      In case we were locking all tables, we did not start transaction
      so there is no need to commit it.
    */

    /* This should just free locks as we did not change anything */
    if (mysql_query(sock, "COMMIT"))
    {
      my_printf_error(0, "Error: Couldn't execute 'COMMIT': %s",
  	      MYF(0), mysql_error(sock));
    }		      
  }
  dbDisconnect(current_host);
  write_footer(md_result_file);
  if (md_result_file != stdout)
    my_fclose(md_result_file, MYF(0));
  my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
  if (extended_insert)
    dynstr_free(&extended_row);
  my_end(0);
  return(first_error);
} /* main */
