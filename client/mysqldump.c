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
** 10 Jun 2003: SET NAMES and --no-set-names by Alexander Barkov
*/

#define DUMP_VERSION "10.7"

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
#define EX_EOF 5 /* ferror for output file was got */

/* index into 'show fields from table' */

#define SHOW_FIELDNAME  0
#define SHOW_TYPE  1
#define SHOW_NULL  2
#define SHOW_DEFAULT  4
#define SHOW_EXTRA  5

/* Size of buffer for dump's select query */
#define QUERY_LENGTH 1536

static char *add_load_option(char *ptr, const char *object,
			     const char *statement);
static ulong find_set(TYPELIB *lib, const char *x, uint length,
		      char **err_pos, uint *err_len);

static char *field_escape(char *to,const char *from,uint length);
static my_bool  verbose=0,tFlag=0,cFlag=0,dFlag=0,quick= 1, extended_insert= 1,
		lock_tables=1,ignore_errors=0,flush_logs=0,replace=0,
		ignore=0,opt_drop=1,opt_keywords=0,opt_lock=1,opt_compress=0,
                opt_delayed=0,create_options=1,opt_quoted=0,opt_databases=0,
                opt_alldbs=0,opt_create_db=0,opt_first_slave=0,opt_set_charset,
		opt_autocommit=0,opt_master_data,opt_disable_keys=1,opt_xml=0,
		opt_delete_master_logs=0, tty_password=0,
		opt_single_transaction=0, opt_comments= 0, opt_compact= 0;
static ulong opt_max_allowed_packet, opt_net_buffer_length;
static MYSQL mysql_connection,*sock=0;
static char  insert_pat[12 * 1024],*opt_password=0,*current_user=0,
             *current_host=0,*path=0,*fields_terminated=0,
             *lines_terminated=0, *enclosed=0, *opt_enclosed=0, *escaped=0,
             *where=0,
             *opt_compatible_mode_str= 0,
             *err_ptr= 0;
static char compatible_mode_normal_str[255];
static ulong opt_compatible_mode= 0;
static uint     opt_mysql_port= 0, err_len= 0;
static my_string opt_mysql_unix_port=0;
static int   first_error=0;
static DYNAMIC_STRING extended_row;
#include <sslopt-vars.h>
FILE  *md_result_file;
#ifdef HAVE_SMEM
static char *shared_memory_base_name=0;
#endif
static uint opt_protocol= 0;
static char *default_charset= (char*) MYSQL_UNIVERSAL_CLIENT_CHARSET;
static CHARSET_INFO *charset_info= &my_charset_latin1;
const char *default_dbug_option="d:t:o,/tmp/mysqldump.trace";
/* do we met VIEWs during tables scaning */
my_bool was_views= 0;

const char *compatible_mode_names[]=
{
  "MYSQL323", "MYSQL40", "POSTGRESQL", "ORACLE", "MSSQL", "DB2",
  "MAXDB", "NO_KEY_OPTIONS", "NO_TABLE_OPTIONS", "NO_FIELD_OPTIONS",
  "ANSI",
  NullS
};
#define MASK_ANSI_QUOTES \
(\
 (1<<2)  | /* POSTGRESQL */\
 (1<<3)  | /* ORACLE     */\
 (1<<4)  | /* MSSQL      */\
 (1<<5)  | /* DB2        */\
 (1<<6)  | /* MAXDB      */\
 (1<<10)   /* ANSI       */\
)
TYPELIB compatible_mode_typelib= {array_elements(compatible_mode_names) - 1,
				  "", compatible_mode_names};


static struct my_option my_long_options[] =
{
  {"all", 'a', "Deprecated. Use --create-options instead.",
   (gptr*) &create_options, (gptr*) &create_options, 0, GET_BOOL, NO_ARG, 1,
   0, 0, 0, 0, 0},
  {"all-databases", 'A',
   "Dump all the databases. This will be same as --databases with all databases selected.",
   (gptr*) &opt_alldbs, (gptr*) &opt_alldbs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"add-drop-table", OPT_DROP, "Add a 'drop table' before each create.",
   (gptr*) &opt_drop, (gptr*) &opt_drop, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0,
   0},
  {"add-locks", OPT_LOCKS, "Add locks around insert statements.",
   (gptr*) &opt_lock, (gptr*) &opt_lock, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0,
   0},
  {"allow-keywords", OPT_KEYWORDS,
   "Allow creation of column names that are keywords.", (gptr*) &opt_keywords,
   (gptr*) &opt_keywords, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", (gptr*) &charsets_dir,
   (gptr*) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compatible", OPT_COMPATIBLE,
   "Change the dump to be compatible with a given mode. By default tables are dumped in a format optimized for MySQL. Legal modes are: ansi, mysql323, mysql40, postgresql, oracle, mssql, db2, maxdb, no_key_options, no_table_options, no_field_options. One can use several modes separated by commas. Note: Requires MySQL server version 4.1.0 or higher. This option is ignored with earlier server versions.",
   (gptr*) &opt_compatible_mode_str, (gptr*) &opt_compatible_mode_str, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compact", OPT_COMPACT,
   "Give less verbose output (useful for debugging). Disables structure comments and header/footer constructs.  Enables options --skip-add-drop-table --no-set-names --skip-disable-keys --skip-lock-tables",
   (gptr*) &opt_compact, (gptr*) &opt_compact, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"complete-insert", 'c', "Use complete insert statements.", (gptr*) &cFlag,
   (gptr*) &cFlag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
   (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"create-options", OPT_CREATE_OPTIONS,
   "Include all MySQL specific create options.",
   (gptr*) &create_options, (gptr*) &create_options, 0, GET_BOOL, NO_ARG, 1,
   0, 0, 0, 0, 0},
  {"databases", 'B',
   "To dump several databases. Note the difference in usage; In this case no tables are given. All name arguments are regarded as databasenames. 'USE db_name;' will be included in the output.",
   (gptr*) &opt_databases, (gptr*) &opt_databases, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit",
   0,0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log", (gptr*) &default_dbug_option,
   (gptr*) &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
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
   (gptr*) &opt_disable_keys, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"extended-insert", 'e',
   "Allows utilization of the new, much faster INSERT syntax.",
   (gptr*) &extended_insert, (gptr*) &extended_insert, 0, GET_BOOL, NO_ARG,
   1, 0, 0, 0, 0, 0},
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
   (gptr*) &lock_tables, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"master-data", OPT_MASTER_DATA,
   "This causes the master position and filename to be appended to your output. This automatically enables --first-slave.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"no-autocommit", OPT_AUTOCOMMIT,
   "Wrap tables with autocommit/commit statements.",
   (gptr*) &opt_autocommit, (gptr*) &opt_autocommit, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"single-transaction", OPT_TRANSACTION,
   "Dump all tables in single transaction to get consistent snapshot. Mutually exclusive with --lock-tables.",
   (gptr*) &opt_single_transaction, (gptr*) &opt_single_transaction, 0,
   GET_BOOL, NO_ARG,  0, 0, 0, 0, 0, 0},
  {"no-create-db", 'n',
   "'CREATE DATABASE /*!32312 IF NOT EXISTS*/ db_name;' will not be put in the output. The above line will be added otherwise, if --databases or --all-databases option was given.}.",
   (gptr*) &opt_create_db, (gptr*) &opt_create_db, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"no-create-info", 't', "Don't write table creation info.",
   (gptr*) &tFlag, (gptr*) &tFlag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"no-data", 'd', "No row information.", (gptr*) &dFlag, (gptr*) &dFlag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"no-set-names", 'N',
   "Deprecated. Use --skip-set-charset instead.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"set-charset", OPT_SET_CHARSET,
   "Add 'SET NAMES default_character_set' to the output. Enabled by default; suppress with --skip-set-charset.",
   (gptr*) &opt_set_charset, (gptr*) &opt_set_charset, 0, GET_BOOL, NO_ARG, 1,
   0, 0, 0, 0, 0},
  {"set-variable", 'O',
   "Change the value of a variable. Please note that this option is deprecated; you can set variables directly with --variable-name=value.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"opt", OPT_OPTIMIZE,
   "Same as --add-drop-table, --add-locks, --create-options, --quick, --extended-insert, --lock-tables, --set-charset, and --disable-keys. Enabled by default, disable with --skip-opt.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's solicited on the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_mysql_port,
   (gptr*) &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, MYSQL_PORT, 0, 0, 0, 0,
   0},
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol of connection (tcp,socket,pipe,memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q', "Don't buffer query, dump directly to stdout.",
   (gptr*) &quick, (gptr*) &quick, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"quote-names",'Q', "Quote table and column names with backticks (`).",
   (gptr*) &opt_quoted, (gptr*) &opt_quoted, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0,
   0, 0},
  {"result-file", 'r',
   "Direct output to a given file. This option should be used in MSDOS, because it prevents new line '\\n' from being converted to '\\r\\n' (carriage return + line feed).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", (gptr*) &shared_memory_base_name, (gptr*) &shared_memory_base_name,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"skip-opt", OPT_SKIP_OPTIMIZATION,
   "Disable --opt. Disables --add-drop-table, --add-locks, --create-options, --quick, --extended-insert, --lock-tables, --set-charset, and --disable-keys.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
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
static const char *check_if_ignore_table(const char *table_name);
static my_bool getViewStructure(char *table, char* db);
static my_bool dump_all_views_in_db(char *database);

#include <help_start.h>

/*
  exit with message if ferror(file)
  
  SYNOPSIS
    check_io()
    file	- checked file
*/

void check_io(FILE *file)
{
  if (ferror(file))
  {
    fprintf(stderr, "%s: Got errno %d on write\n", my_progname, errno);
    safe_exit(EX_EOF);
  }
}

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
    fputs("<?xml version=\"1.0\"?>\n", sql_file);
    fputs("<mysqldump>\n", sql_file);
    check_io(sql_file);
  }
  else if (!opt_compact)
  {
    if (opt_comments)
    {
      fprintf(sql_file, "-- MySQL dump %s\n--\n", DUMP_VERSION);
      fprintf(sql_file, "-- Host: %s    Database: %s\n",
	      current_host ? current_host : "localhost", db_name ? db_name :
	      "");
      fputs("-- ------------------------------------------------------\n",
	    sql_file);
      fprintf(sql_file, "-- Server version\t%s\n",
	      mysql_get_server_info(&mysql_connection));
    }
    if (opt_set_charset)
      fprintf(sql_file,
"\n/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;"
"\n/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;"
"\n/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;"
"\n/*!40101 SET NAMES %s */;\n",default_charset);
    if (!path)
    {
      fprintf(md_result_file,"\
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;\n\
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;\n\
");
    }
    fprintf(sql_file,
	    "/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE=\"%s%s%s\" */;\n",
	    path?"":"NO_AUTO_VALUE_ON_ZERO",compatible_mode_normal_str[0]==0?"":",",
	    compatible_mode_normal_str);
    check_io(sql_file);
  }
} /* write_header */


static void write_footer(FILE *sql_file)
{
  if (opt_xml)
  {
    fputs("</mysqldump>\n", sql_file);
    check_io(sql_file);
  }
  else if (!opt_compact)
  {
    fprintf(sql_file,"\n/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;\n");
    if (!path)
    {
      fprintf(md_result_file,"\
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;\n\
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;\n");
    }
    if (opt_set_charset)
      fprintf(sql_file,
"/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;\n"
"/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;\n"
"/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;\n");
    fputs("\n", sql_file);
    check_io(sql_file);
  }
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
    opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
    break;
  case 'N':
    opt_set_charset= 0;
    break;
  case 'T':
    opt_disable_keys=0;
    break;
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    break;
#include <sslopt-case.h>
  case 'V': print_version(); exit(0);
  case 'X':
    opt_xml = 1;
    extended_insert= opt_drop= opt_lock= 
      opt_disable_keys= opt_autocommit= opt_create_db= 0;
    break;
  case 'I':
  case '?':
    usage();
    exit(0);
  case (int) OPT_OPTIMIZE:
    extended_insert= opt_drop= opt_lock= quick= create_options=
      opt_disable_keys= lock_tables= opt_set_charset= 1;
    if (opt_single_transaction) lock_tables=0;
    break;
  case (int) OPT_SKIP_OPTIMIZATION:
    extended_insert= opt_drop= opt_lock= quick= create_options=
      opt_disable_keys= lock_tables= opt_set_charset= 0;
    break;
  case (int) OPT_COMPACT:
  if (opt_compact)
  {
    opt_comments= opt_drop= opt_disable_keys= opt_lock= 0;
    opt_set_charset= 0;
  }
  case (int) OPT_TABLES:
    opt_databases=0;
    break;
  case (int) OPT_COMPATIBLE:
    {  
      char buff[255];
      char *end= compatible_mode_normal_str;
      int i;
      ulong mode;

      opt_quoted= 1;
      opt_set_charset= 0;
      opt_compatible_mode_str= argument;
      opt_compatible_mode= find_set(&compatible_mode_typelib,
				    argument, strlen(argument),
				    &err_ptr, &err_len);
      if (err_len)
      {
	strmake(buff, err_ptr, min(sizeof(buff), err_len));
	fprintf(stderr, "Invalid mode to --compatible: %s\n", buff);
	exit(1);
      }
#if !defined(DBUG_OFF)
      {
	uint size_for_sql_mode= 0;
	const char **ptr;
	for (ptr= compatible_mode_names; *ptr; ptr++)
	  size_for_sql_mode+= strlen(*ptr);
	size_for_sql_mode+= sizeof(compatible_mode_names)-1;
	DBUG_ASSERT(sizeof(compatible_mode_normal_str)>=size_for_sql_mode);
      }
#endif
      mode= opt_compatible_mode;
      for (i= 0, mode= opt_compatible_mode; mode; mode>>= 1, i++)
      {
	if (mode & 1)
	{
	  end= strmov(end, compatible_mode_names[i]);
	  end= strmov(end, ",");
	}
      }
      if (end!=compatible_mode_normal_str)
	end[-1]= 0;
      break;
    }
  case (int) OPT_MYSQL_PROTOCOL:
    {
      if ((opt_protocol= find_type(argument, &sql_protocol_typelib,0)) <= 0)
      {
	fprintf(stderr, "Unknown option to protocol: %s\n", argument);
	exit(1);
      }
      break;
    }
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
  if (opt_single_transaction)
    lock_tables= 0;
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
  if (strcmp(default_charset, charset_info->csname) &&
      !(charset_info= get_charset_by_csname(default_charset, 
  					    MY_CS_PRIMARY, MYF(MY_WME))))
    exit(1);
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
  char buff[20+FN_REFLEN];
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
  if (opt_protocol)
    mysql_options(&mysql_connection,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    mysql_options(&mysql_connection,MYSQL_SHARED_MEMORY_BASE_NAME,shared_memory_base_name);
#endif
  mysql_options(&mysql_connection, MYSQL_SET_CHARSET_NAME, default_charset);
  if (!(sock= mysql_real_connect(&mysql_connection,host,user,passwd,
         NULL,opt_mysql_port,opt_mysql_unix_port,
         0)))
  {
    DBerror(&mysql_connection, "when trying to connect");
    return 1;
  }
  sprintf(buff, "/*!40100 SET @@SQL_MODE=\"%s\" */",
	  compatible_mode_normal_str);
  if (mysql_query(sock, buff))
  {
    fprintf(stderr, "%s: Can't set the compatible mode %s (error %s)\n",
	    my_progname, compatible_mode_normal_str, mysql_error(sock));
    mysql_close(sock);
    safe_exit(EX_MYSQLERR);
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
  check_io(file);
  my_free(tmp, MYF(MY_WME));
  DBUG_VOID_RETURN;
} /* unescape */


static my_bool test_if_special_chars(const char *str)
{
#if MYSQL_VERSION_ID >= 32300
  for ( ; *str ; str++)
    if (!my_isvar(charset_info,*str) && *str != '$')
      return 1;
#endif
  return 0;
} /* test_if_special_chars */



static char *quote_name(const char *name, char *buff, my_bool force)
{
  char *to= buff;
  char qtype= (opt_compatible_mode & MASK_ANSI_QUOTES) ? '\"' : '`';

  if (!force && !opt_quoted && !test_if_special_chars(name))
    return (char*) name;
  *to++= qtype;
  while (*name)
  {
    if (*name == qtype)
      *to++= qtype;
    *to++= *name++;
  }
  to[0]= qtype;
  to[1]= 0;
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
  Quote and print a string.
  
  SYNOPSIS
    print_quoted_xml()
    output	- output file
    str		- string to print
    len		- its length
    
  DESCRIPTION
    Quote '<' '>' '&' '\"' singns and print a string to the xml_file.
*/

static void print_quoted_xml(FILE *xml_file, const char *str, ulong len)
{
  const char *end;
  
  for (end= str + len; str != end; str++)
  {
    switch (*str) {
    case '<':
      fputs("&lt;", xml_file);
      break;
    case '>':
      fputs("&gt;", xml_file);
      break;
    case '&':
      fputs("&amp;", xml_file);
      break;
    case '\"':
      fputs("&quot;", xml_file);
      break;
    default:
      fputc(*str, xml_file);
      break;
    }
  }
  check_io(xml_file);
}


/*
  Print xml tag with one attribute.
  
  SYNOPSIS
    print_xml_tag1()
    xml_file	- output file
    sbeg	- line beginning
    stag_atr	- tag and attribute
    sval	- value of attribute
    send	- line ending
    
  DESCRIPTION
    Print tag with one attribute to the xml_file. Format is:
      sbeg<stag_atr="sval">send
  NOTE
    sval MUST be a NULL terminated string.
    sval string will be qouted before output.
*/

static void print_xml_tag1(FILE * xml_file, const char* sbeg,
			   const char* stag_atr, const char* sval,
			   const char* send)
{
  fputs(sbeg, xml_file);
  fputs("<", xml_file);
  fputs(stag_atr, xml_file);
  fputs("\"", xml_file);
  print_quoted_xml(xml_file, sval, strlen(sval));
  fputs("\">", xml_file);
  fputs(send, xml_file);
  check_io(xml_file);
}


/*
  Print xml tag with many attributes.

  SYNOPSIS
    print_xml_row()
    xml_file	- output file
    row_name	- xml tag name
    tableRes	- query result
    row		- result row
    
  DESCRIPTION
    Print tag with many attribute to the xml_file. Format is:
      \t\t<row_name Atr1="Val1" Atr2="Val2"... />
  NOTE
    All atributes and values will be quoted before output.
*/

static void print_xml_row(FILE *xml_file, const char *row_name,
			  MYSQL_RES *tableRes, MYSQL_ROW *row)
{
  uint i;
  MYSQL_FIELD *field;
  ulong *lengths= mysql_fetch_lengths(tableRes);
  
  fprintf(xml_file, "\t\t<%s", row_name);
  check_io(xml_file);
  mysql_field_seek(tableRes, 0);
  for (i= 0; (field= mysql_fetch_field(tableRes)); i++)
  {
    if ((*row)[i])
    {
      fputc(' ', xml_file);
      print_quoted_xml(xml_file, field->name, field->name_length);
      fputs("=\"", xml_file);
      print_quoted_xml(xml_file, (*row)[i], lengths[i]);
      fputc('"', xml_file);
      check_io(xml_file);
    }
  }
  fputs(" />\n", xml_file);
  check_io(xml_file);
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
  if (!create_options)
    strmov(strend(insert_pat), "/*!40102 ,SQL_MODE=concat(@@sql_mode, _utf8 ',NO_KEY_OPTIONS,NO_TABLE_OPTIONS,NO_FIELD_OPTIONS') */");

  result_table=     quote_name(table, table_buff, 1);
  opt_quoted_table= quote_name(table, table_buff2, 0);
  if (!opt_xml && !mysql_query(sock,insert_pat))
  {
    /* using SHOW CREATE statement */
    if (!tFlag)
    {
      /* Make an sql-file, if path was given iow. option -T was given */
      char buff[20+FN_REFLEN];
      MYSQL_FIELD *field;

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
      {
        fprintf(sql_file, "\n--\n-- Table structure for table %s\n--\n\n",
		result_table);
	check_io(sql_file);
      }
      if (opt_drop)
      {
        fprintf(sql_file, "DROP TABLE IF EXISTS %s;\n", opt_quoted_table);
	check_io(sql_file);
      }

      tableRes= mysql_store_result(sock);
      field= mysql_fetch_field_direct(tableRes, 0);
      if (strcmp(field->name, "View") == 0)
      {
        if (verbose)
          fprintf(stderr, "-- It's a view, skipped\n");
        was_views= 1;
        DBUG_RETURN(0)
      }
      row= mysql_fetch_row(tableRes);
      fprintf(sql_file, "%s;\n", row[1]);
      check_io(sql_file);
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
    if (verbose)
      fprintf(stderr,
              "%s: Warning: Can't set SQL_QUOTE_SHOW_CREATE option (%s)\n",
              my_progname, mysql_error(sock));

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
      if (!opt_xml)
	fprintf(sql_file, "CREATE TABLE %s (\n", result_table);
      else
        print_xml_tag1(sql_file, "\t", "table_structure name=", table, "\n");
      check_io(sql_file);
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
        if (!opt_xml && !tFlag)
	{
	  fputs(",\n",sql_file);
	  check_io(sql_file);
	}
        if (cFlag)
	  strpos=strmov(strpos,", ");
      }
      init=1;
      if (cFlag)
        strpos=strmov(strpos,quote_name(row[SHOW_FIELDNAME], name_buff, 0));
      if (!tFlag)
      {
	if (opt_xml)
	{
	  print_xml_row(sql_file, "field", tableRes, &row);
	  continue;
	}
	
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
	check_io(sql_file);
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
	if (opt_xml)
	{
	  print_xml_row(sql_file, "key", tableRes, &row);
	  continue;
	}
        
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
	    fprintf(sql_file, ",\n  UNIQUE %s (",quote_name(row[2],name_buff,
							    0));
        }
        else
	  putc(',', sql_file);
        fputs(quote_name(row[4], name_buff, 0), sql_file);
        if (row[7])
	  fprintf(sql_file, " (%s)",row[7]);      /* Sub key */
	check_io(sql_file);
      }
      if (!opt_xml)
      {
	if (keynr)
	  putc(')', sql_file);
	fputs("\n)",sql_file);
	check_io(sql_file);
      }

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
	  if (opt_xml)
	  {
	    print_xml_row(sql_file, "options", tableRes, &row);
	  }
	  else
	  {
	    fputs("/*!",sql_file);
	    print_value(sql_file,tableRes,row,"engine=","Engine",0);
	    print_value(sql_file,tableRes,row,"","Create_options",0);
	    print_value(sql_file,tableRes,row,"comment=","Comment",1);
	    fputs(" */",sql_file);
	    check_io(sql_file);
	  }
        }
        mysql_free_result(tableRes);		/* Is always safe to free */
      }
      if (!opt_xml)
	fputs(";\n", sql_file);
      else
	fputs("\t</table_structure>\n", sql_file);
      check_io(sql_file);
    }
  }
  if (cFlag)
  {
    strpos=strmov(strpos,") VALUES ");
    if (!extended_insert)
      strpos=strmov(strpos,"(");
  }
  if (sql_file != md_result_file)
  {
    fputs("\n", sql_file);
    write_footer(sql_file);
    my_fclose(sql_file, MYF(MY_WME));
  }
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
    {
      fprintf(md_result_file,"\n--\n-- Dumping data for table %s\n--\n",
	      result_table);
      check_io(md_result_file);
    }
    sprintf(query, "SELECT /*!40001 SQL_NO_CACHE */ * FROM %s",
	    result_table);
    if (where)
    {
      if (!opt_xml && opt_comments)
      {
	fprintf(md_result_file,"-- WHERE:  %s\n",where);
	check_io(md_result_file);
      }
      query= alloc_query_str((ulong) (strlen(where) + strlen(query) + 10));
      strxmov(query, query_buf, " WHERE ", where, NullS);
    }
    if (!opt_xml && !opt_compact)
    {
      fputs("\n", md_result_file);
      check_io(md_result_file);
    }
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
    {
      fprintf(md_result_file, "\n/*!40000 ALTER TABLE %s DISABLE KEYS */;\n",
	      opt_quoted_table);
      check_io(md_result_file);
    }
    if (opt_lock)
    {
      fprintf(md_result_file,"LOCK TABLES %s WRITE;\n", opt_quoted_table);
      check_io(md_result_file);
    }

    total_length= opt_net_buffer_length;		/* Force row break */
    row_break=0;
    rownr=0;
    init_length=(uint) strlen(insert_pat)+4;
    if (opt_xml)
      print_xml_tag1(md_result_file, "\t", "table_data name=", table, "\n");

    if (opt_autocommit)
    {
      fprintf(md_result_file, "set autocommit=0;\n");
      check_io(md_result_file);
    }

    while ((row=mysql_fetch_row(res)))
    {
      uint i;
      ulong *lengths=mysql_fetch_lengths(res);
      rownr++;
      if (!extended_insert && !opt_xml)
      {
	fputs(insert_pat,md_result_file);
	check_io(md_result_file);
      }
      mysql_field_seek(res,0);

      if (opt_xml)
      {
        fputs("\t<row>\n", md_result_file);
	check_io(md_result_file);
      }

      for (i = 0; i < mysql_num_fields(res); i++)
      {
	if (!(field = mysql_fetch_field(res)))
	{
	  sprintf(query,"%s: Not enough fields from table %s! Aborting.\n",
		  my_progname, result_table);
	  fputs(query,stderr);
	  error= EX_CONSCHECK;
	  goto err;
	}
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
		if (dynstr_realloc(&extended_row,length * 2+2))
		{
		  fputs("Aborting dump (out of memory)",stderr);
		  error= EX_EOM;
		  goto err;
		}
		dynstr_append(&extended_row,"'");
		extended_row.length +=
		  mysql_real_escape_string(&mysql_connection,
					   &extended_row.str[extended_row.length],row[i],length);
		extended_row.str[extended_row.length]='\0';
		dynstr_append(&extended_row,"'");
	      }
	      else
	      {
		/* change any strings ("inf", "-inf", "nan") into NULL */
		char *ptr = row[i];
		if (my_isalpha(charset_info, *ptr) || (*ptr == '-' &&
		    my_isalpha(charset_info, ptr[1])))
		  dynstr_append(&extended_row, "NULL");
		else
		{
		  if (field->type == FIELD_TYPE_DECIMAL)
		  {
		    /* add " signs around */
		    dynstr_append(&extended_row, "'");
		    dynstr_append(&extended_row, ptr);
		    dynstr_append(&extended_row, "'");
		  }
		  else
		    dynstr_append(&extended_row, ptr);
		}
	      }
	    }
	    else
	      dynstr_append(&extended_row,"''");
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
	  {
	    fputc(',', md_result_file);
	    check_io(md_result_file);
	  }
	  if (row[i])
	  {
	    if (!IS_NUM_FIELD(field))
	    {
	      if (opt_xml)
	      {
	        print_xml_tag1(md_result_file, "\t\t", "field name=",
			      field->name, "");
		print_quoted_xml(md_result_file, row[i], lengths[i]);
		fputs("</field>\n", md_result_file);
	      }
	      else
		unescape(md_result_file, row[i], lengths[i]);
	    }
	    else
	    {
	      /* change any strings ("inf", "-inf", "nan") into NULL */
	      char *ptr = row[i];
	      if (opt_xml)
	      {
	        print_xml_tag1(md_result_file, "\t\t", "field name=",
			       field->name, "");
		fputs(!my_isalpha(charset_info, *ptr) ? ptr: "NULL",
		      md_result_file);
		fputs("</field>\n", md_result_file);
	      }
	      else if (my_isalpha(charset_info, *ptr) ||
		       (*ptr == '-' && my_isalpha(charset_info, ptr[1])))
	        fputs("NULL", md_result_file);
	      else if (field->type == FIELD_TYPE_DECIMAL)
	      {
		/* add " signs around */
		fputc('\'', md_result_file);
		fputs(ptr, md_result_file);
		fputc('\'', md_result_file);
	      }
	      else
		fputs(ptr, md_result_file);
	    }
	  }
	  else
            fputs("NULL", md_result_file);
          check_io(md_result_file);
	}
      }

      if (opt_xml)
      {
        fputs("\t</row>\n", md_result_file);
	check_io(md_result_file);
      }

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
	  if (row_break)
	    fputs(";\n", md_result_file);
	  row_break=1;				/* This is first row */

          fputs(insert_pat,md_result_file);
          fputs(extended_row.str,md_result_file);
	  total_length = row_length+init_length;
        }
	check_io(md_result_file);
      }
      else if (!opt_xml)
      {
	fputs(");\n", md_result_file);
	check_io(md_result_file);
      }
    }

    /* XML - close table tag and supress regular output */
    if (opt_xml)
	fputs("\t</table_data>\n", md_result_file);
    else if (extended_insert && row_break)
      fputs(";\n", md_result_file);		/* If not empty table */
    fflush(md_result_file);
    check_io(md_result_file);
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
    {
      fputs("UNLOCK TABLES;\n", md_result_file);
      check_io(md_result_file);
    }
    if (opt_disable_keys)
    {
      fprintf(md_result_file,"/*!40000 ALTER TABLE %s ENABLE KEYS */;\n",
	      opt_quoted_table);
      check_io(md_result_file);
    }
    if (opt_autocommit)
    {
      fprintf(md_result_file, "commit;\n");
      check_io(md_result_file);
    }
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
  if (was_views)
  {
    if (mysql_query(sock, "SHOW DATABASES") ||
        !(tableres = mysql_store_result(sock)))
    {
      my_printf_error(0, "Error: Couldn't execute 'SHOW DATABASES': %s",
                      MYF(0), mysql_error(sock));
      return 1;
    }
    while ((row = mysql_fetch_row(tableres)))
    {
      if (dump_all_views_in_db(row[0]))
        result=1;
    }
  }
  return result;
}
/* dump_all_databases */


static int dump_databases(char **db_names)
{
  int result=0;
  char **db;
  for (db= db_names ; *db ; db++)
  {
    if (dump_all_tables_in_db(*db))
      result=1;
  }
  if (!result && was_views)
  {
    for (db= db_names ; *db ; db++)
    {
      if (dump_all_views_in_db(*db))
        result=1;
    }
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
      /*
	length of table name * 2 (if name contain quotas), 2 quotas and 0
      */
      char quoted_database_buf[64*2+3];
      char *qdatabase= quote_name(database,quoted_database_buf,opt_quoted);
      if (opt_comments)
      {
	fprintf(md_result_file,"\n--\n-- Current Database: %s\n--\n", qdatabase);
	check_io(md_result_file);
      }
      if (!opt_create_db)
      {
        char qbuf[256];
        MYSQL_ROW row;
        MYSQL_RES *dbinfo;

        sprintf(qbuf,"SHOW CREATE DATABASE WITH IF NOT EXISTS %s",
		qdatabase);

        if (mysql_query(sock, qbuf) || !(dbinfo = mysql_store_result(sock)))
        {
          /* Old server version, dump generic CREATE DATABASE */
	  fprintf(md_result_file,
		  "\nCREATE DATABASE /*!32312 IF NOT EXISTS*/ %s;\n",
		  qdatabase);
	}
	else
        {
	  row = mysql_fetch_row(dbinfo);
	  if (row[1])
	  {
	    fprintf(md_result_file,"\n%s;\n",row[1]);
          }
	}
      }
      fprintf(md_result_file,"\nUSE %s;\n", qdatabase);
      check_io(md_result_file);
    }
  }
  if (extended_insert && init_dynamic_string(&extended_row, "", 1024, 1024))
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
    print_xml_tag1(md_result_file, "", "database name=", database, "\n");
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
  {
    fputs("</database>\n", md_result_file);
    check_io(md_result_file);
  }
  if (lock_tables)
    mysql_query(sock,"UNLOCK TABLES");
  return 0;
} /* dump_all_tables_in_db */

/*
   dump structure of views of database

   SYNOPSIS
     dump_all_views_in_db()
     database  database name

  RETURN
    0 OK
    1 ERROR
*/

static my_bool dump_all_views_in_db(char *database)
{
  char *table;
  uint numrows;
  char table_buff[NAME_LEN*2+3];

  if (init_dumping(database))
    return 1;
  if (opt_xml)
    print_xml_tag1(md_result_file, "", "database name=", database, "\n");
  if (lock_tables)
  {
    DYNAMIC_STRING query;
    init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
    for (numrows= 0 ; (table= getTableName(1)); numrows++)
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
  while ((table= getTableName(0)))
     getViewStructure(table, database);
  if (opt_xml)
  {
    fputs("</database>\n", md_result_file);
    check_io(md_result_file);
  }
  if (lock_tables)
    mysql_query(sock,"UNLOCK TABLES");
  return 0;
} /* dump_all_tables_in_db */

static int dump_selected_tables(char *db, char **table_names, int tables)
{
  uint numrows;
  int i;
  char table_buff[NAME_LEN*+3];

  if (init_dumping(db))
    return 1;
  if (lock_tables)
  {
    DYNAMIC_STRING query;

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
    print_xml_tag1(md_result_file, "", "database name=", db, "\n");
  for (i=0 ; i < tables ; i++)
  {
    numrows = getTableStructure(table_names[i], db);
    if (!dFlag && numrows > 0)
      dumpTable(numrows, table_names[i]);
  }
  if (was_views)
  {
    for (i=0 ; i < tables ; i++)
      getViewStructure(table_names[i], db);
  }
  if (opt_xml)
  {
    fputs("</database>\n", md_result_file);
    check_io(md_result_file);
  }
  if (lock_tables)
    mysql_query(sock,"UNLOCK TABLES");
  return 0;
} /* dump_selected_tables */



static ulong find_set(TYPELIB *lib, const char *x, uint length,
		      char **err_pos, uint *err_len)
{
  const char *end= x + length;
  ulong found= 0;
  uint find;
  char buff[255];

  *err_pos= 0;                  /* No error yet */
  while (end > x && my_isspace(charset_info, end[-1]))
    end--;

  *err_len= 0;
  if (x != end)
  {
    const char *start= x;
    for (;;)
    {
      const char *pos= start;
      uint var_len;

      for (; pos != end && *pos != ','; pos++) ;
      var_len= (uint) (pos - start);
      strmake(buff, start, min(sizeof(buff), var_len));
      find= find_type(buff, lib, var_len);
      if (!find)
      {
        *err_pos= (char*) start;
        *err_len= var_len;
      }
      else
        found|= ((longlong) 1 << (find - 1));
      if (pos == end)
        break;
      start= pos + 1;
    }
  }
  return found;
}


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
	check_io(file);
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


/*
  Getting VIEW structure

  SYNOPSIS
    getViewStructure()
    table   view name
    db      db name

  RETURN
    0 OK
    1 ERROR
*/

static my_bool getViewStructure(char *table, char* db)
{
  MYSQL_RES  *tableRes;
  MYSQL_ROW  row;
  MYSQL_FIELD *field;
  char	     *result_table, *opt_quoted_table;
  char	     table_buff[NAME_LEN*2+3];
  char	     table_buff2[NAME_LEN*2+3];
  char       buff[20+FN_REFLEN];
  FILE       *sql_file = md_result_file;
  DBUG_ENTER("getViewStructure");

  if (tFlag)
    DBUG_RETURN(0);

  if (verbose)
    fprintf(stderr, "-- Retrieving view structure for table %s...\n", table);

  sprintf(insert_pat,"SET OPTION SQL_QUOTE_SHOW_CREATE=%d",
	  (opt_quoted || opt_keywords));
  result_table=     quote_name(table, table_buff, 1);
  opt_quoted_table= quote_name(table, table_buff2, 0);

  sprintf(buff,"show create table %s", result_table);
  if (mysql_query(sock, buff))
  {
    fprintf(stderr, "%s: Can't get CREATE TABLE for view %s (%s)\n",
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
      DBUG_RETURN(1);
    }
    write_header(sql_file, db);
  }
  tableRes= mysql_store_result(sock);
  field= mysql_fetch_field_direct(tableRes, 0);
  if (strcmp(field->name, "View") != 0)
  {
    if (verbose)
      fprintf(stderr, "-- It's base table, skipped\n");
    DBUG_RETURN(0)
  }

  if (!opt_xml && opt_comments)
  {
    fprintf(sql_file, "\n--\n-- View structure for view %s\n--\n\n",
            result_table);
    check_io(sql_file);
  }
  if (opt_drop)
  {
    fprintf(sql_file, "DROP VIEW IF EXISTS %s;\n", opt_quoted_table);
    check_io(sql_file);
  }

  row= mysql_fetch_row(tableRes);
  fprintf(sql_file, "%s;\n", row[1]);
  check_io(sql_file);
  mysql_free_result(tableRes);

  if (sql_file != md_result_file)
  {
    fputs("\n", sql_file);
    write_footer(sql_file);
    my_fclose(sql_file, MYF(MY_WME));
  }
  DBUG_RETURN(0);
}


int main(int argc, char **argv)
{
  MYSQL_ROW row;
  MYSQL_RES *master;
  compatible_mode_normal_str[0]= 0;

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
	  check_io(md_result_file);
	}
	mysql_free_result(master);
      }
    }
    if (mysql_query(sock, "UNLOCK TABLES"))
      my_printf_error(0, "Error: Couldn't execute 'UNLOCK TABLES': %s",
		      MYF(0), mysql_error(sock));
  }
  else if (opt_single_transaction) /* Just to make it beautiful enough */
#ifdef HAVE_SMEM
  my_free(shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
#endif
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
  if (!path)
    write_footer(md_result_file);
  if (md_result_file != stdout)
    my_fclose(md_result_file, MYF(0));
  my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
  if (extended_insert)
    dynstr_free(&extended_row);
  my_end(0);
  return(first_error);
} /* main */
