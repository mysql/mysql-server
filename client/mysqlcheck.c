/*
   Copyright (c) 2001, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define CHECK_VERSION "2.5.1"

#include "client_priv.h"
#include "my_default.h"
#include <m_ctype.h>
#include <mysql_version.h>
#include <mysqld_error.h>
#include <sslopt-vars.h>
#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2

/* ALTER instead of repair. */
#define MAX_ALTER_STR_SIZE 128 * 1024
#define KEY_PARTITIONING_CHANGED_STR "KEY () partitioning changed"

static MYSQL mysql_connection, *sock = 0;
static my_bool opt_alldbs = 0, opt_check_only_changed = 0, opt_extended = 0,
               opt_compress = 0, opt_databases = 0, opt_fast = 0,
               opt_medium_check = 0, opt_quick = 0, opt_all_in_1 = 0,
               opt_silent = 0, opt_auto_repair = 0, ignore_errors = 0,
               tty_password= 0, opt_frm= 0, debug_info_flag= 0, debug_check_flag= 0,
               opt_fix_table_names= 0, opt_fix_db_names= 0, opt_upgrade= 0,
               opt_write_binlog= 1;
static uint verbose = 0, opt_mysql_port=0;
static int my_end_arg;
static char * opt_mysql_unix_port = 0;
static char *opt_password = 0, *current_user = 0, 
	    *default_charset= 0, *current_host= 0;
static char *opt_plugin_dir= 0, *opt_default_auth= 0;
static int first_error = 0;
static char *opt_skip_database;
DYNAMIC_ARRAY tables4repair, tables4rebuild, alter_table_cmds;
#ifdef HAVE_SMEM
static char *shared_memory_base_name=0;
#endif
static uint opt_protocol=0;
static char *opt_bind_addr = NULL;

enum operations { DO_CHECK=1, DO_REPAIR, DO_ANALYZE, DO_OPTIMIZE, DO_UPGRADE };

static struct my_option my_long_options[] =
{
  {"all-databases", 'A',
   "Check all the databases. This is the same as --databases with all databases selected.",
   &opt_alldbs, &opt_alldbs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"analyze", 'a', "Analyze given tables.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"all-in-1", '1',
   "Instead of issuing one query for each table, use one query per database, naming all tables in the database in a comma-separated list.",
   &opt_all_in_1, &opt_all_in_1, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"auto-repair", OPT_AUTO_REPAIR,
   "If a checked table is corrupted, automatically fix it. Repairing will be done after all tables have been checked, if corrupted ones were found.",
   &opt_auto_repair, &opt_auto_repair, 0, GET_BOOL, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"bind-address", 0, "IP address to bind to.",
   (uchar**) &opt_bind_addr, (uchar**) &opt_bind_addr, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory for character set files.", &charsets_dir,
   &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"check", 'c', "Check table for errors.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"check-only-changed", 'C',
   "Check only tables that have changed since last check or haven't been closed properly.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"check-upgrade", 'g',
   "Check tables for version-dependent changes. May be used with --auto-repair to correct tables requiring version-dependent updates.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", OPT_COMPRESS, "Use compression in server/client protocol.",
   &opt_compress, &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"databases", 'B',
   "Check several databases. Note the difference in usage; in this case no tables are given. All name arguments are regarded as database names.",
   &opt_databases, &opt_databases, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit.",
   0, 0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit.",
   &debug_check_flag, &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", OPT_DEBUG_INFO, "Print some debug info at exit.",
   &debug_info_flag, &debug_info_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", &default_charset,
   &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default_auth", OPT_DEFAULT_AUTH,
   "Default authentication client-side plugin to use.",
   &opt_default_auth, &opt_default_auth, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fast",'F', "Check only tables that haven't been closed properly.",
   &opt_fast, &opt_fast, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"fix-db-names", OPT_FIX_DB_NAMES, "Fix database names.",
    &opt_fix_db_names, &opt_fix_db_names,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"fix-table-names", OPT_FIX_TABLE_NAMES, "Fix table names.",
    &opt_fix_table_names, &opt_fix_table_names,
    0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"force", 'f', "Continue even if we get an SQL error.",
   &ignore_errors, &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"extended", 'e',
   "If you are using this option with CHECK TABLE, it will ensure that the table is 100 percent consistent, but will take a long time. If you are using this option with REPAIR TABLE, it will force using old slow repair with keycache method, instead of much faster repair by sorting.",
   &opt_extended, &opt_extended, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host",'h', "Connect to host.", &current_host,
   &current_host, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"medium-check", 'm',
   "Faster than extended-check, but only finds 99.99 percent of all errors. Should be good enough for most cases.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"write-binlog", OPT_WRITE_BINLOG,
   "Log ANALYZE, OPTIMIZE and REPAIR TABLE commands. Use --skip-write-binlog "
   "when commands should not be sent to replication slaves.",
   &opt_write_binlog, &opt_write_binlog, 0, GET_BOOL, NO_ARG,
   1, 0, 0, 0, 0, 0},
  {"optimize", 'o', "Optimize table.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"password", 'p',
   "Password to use when connecting to server. If password is not given, it's solicited on the tty.",
   0, 0, 0, GET_PASSWORD, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"plugin_dir", OPT_PLUGIN_DIR, "Directory for client-side plugins.",
   &opt_plugin_dir, &opt_plugin_dir, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &opt_mysql_port, &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0,
   0},
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol to use for connection (tcp, socket, pipe, memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q',
   "If you are using this option with CHECK TABLE, it prevents the check from scanning the rows to check for wrong links. This is the fastest check. If you are using this option with REPAIR TABLE, it will try to repair only the index tree. This is the fastest repair method for a table.",
   &opt_quick, &opt_quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"repair", 'r',
   "Can fix almost anything except unique keys that aren't unique.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", &shared_memory_base_name, &shared_memory_base_name,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"silent", 's', "Print only error messages.", &opt_silent,
   &opt_silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip_database", 0, "Don't process the database specified as argument", 
   &opt_skip_database, &opt_skip_database, 0, GET_STR, REQUIRED_ARG, 
   0, 0, 0, 0, 0, 0},
  {"socket", 'S', "The socket file to use for connection.",
   &opt_mysql_unix_port, &opt_mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
  {"tables", OPT_TABLES, "Overrides option --databases (-B).", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"use-frm", OPT_FRM,
   "When used with REPAIR, get table structure from .frm file, so the table can be repaired even if .MYI header is corrupted.",
   &opt_frm, &opt_frm, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", &current_user,
   &current_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"verbose", 'v', "Print info about the various stages.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static const char *load_default_groups[] = { "mysqlcheck", "client", 0 };


static void print_version(void);
static void usage(void);
static int get_options(int *argc, char ***argv);
static int process_all_databases();
static int process_databases(char **db_names);
static int process_selected_tables(char *db, char **table_names, int tables);
static int process_all_tables_in_db(char *database);
static int process_one_db(char *database);
static int use_db(char *database);
static int handle_request_for_tables(char *tables, uint length);
static int dbConnect(char *host, char *user,char *passwd);
static void dbDisconnect(char *host);
static void DBerror(MYSQL *mysql, const char *when);
static void safe_exit(int error);
static void print_result();
static uint fixed_name_length(const char *name);
static char *fix_table_name(char *dest, char *src);
int what_to_do = 0;


static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n", my_progname, CHECK_VERSION,
   MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
} /* print_version */


static void usage(void)
{
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  puts("This program can be used to CHECK (-c, -m, -C), REPAIR (-r), ANALYZE (-a),");
  puts("or OPTIMIZE (-o) tables. Some of the options (like -e or -q) can be");
  puts("used at the same time. Not all options are supported by all storage engines.");
  puts("Please consult the MySQL manual for latest information about the");
  puts("above. The options -c, -r, -a, and -o are exclusive to each other, which");
  puts("means that the last option will be used, if several was specified.\n");
  puts("The option -c will be used by default, if none was specified. You");
  puts("can change the default behavior by making a symbolic link, or");
  puts("copying this file somewhere with another name, the alternatives are:");
  puts("mysqlrepair:   The default option will be -r");
  puts("mysqlanalyze:  The default option will be -a");
  puts("mysqloptimize: The default option will be -o\n");
  printf("Usage: %s [OPTIONS] database [tables]\n", my_progname);
  printf("OR     %s [OPTIONS] --databases DB1 [DB2 DB3...]\n",
	 my_progname);
  printf("OR     %s [OPTIONS] --all-databases\n", my_progname);
  print_defaults("my", load_default_groups);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
} /* usage */


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  int orig_what_to_do= what_to_do;

  switch(optid) {
  case 'a':
    what_to_do = DO_ANALYZE;
    break;
  case 'c':
    what_to_do = DO_CHECK;
    break;
  case 'C':
    what_to_do = DO_CHECK;
    opt_check_only_changed = 1;
    break;
  case 'I': /* Fall through */
  case '?':
    usage();
    exit(0);
  case 'm':
    what_to_do = DO_CHECK;
    opt_medium_check = 1;
    break;
  case 'o':
    what_to_do = DO_OPTIMIZE;
    break;
  case OPT_FIX_DB_NAMES:
    what_to_do= DO_UPGRADE;
    opt_databases= 1;
    break;
  case OPT_FIX_TABLE_NAMES:
    what_to_do= DO_UPGRADE;
    break;
  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";			/* Don't require password */
    if (argument)
    {
      char *start = argument;
      my_free(opt_password);
      opt_password = my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
	start[1] = 0;                             /* Cut length of argument */
      tty_password= 0;
    }
    else
      tty_password = 1;
    break;
  case 'r':
    what_to_do = DO_REPAIR;
    break;
  case 'g':
    what_to_do= DO_CHECK;
    opt_upgrade= 1;
    break;
  case 'W':
#ifdef __WIN__
    opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
    break;
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:o");
    debug_check_flag= 1;
    break;
#include <sslopt-case.h>
  case OPT_TABLES:
    opt_databases = 0;
    break;
  case 'v':
    verbose++;
    break;
  case 'V': print_version(); exit(0);
  case OPT_MYSQL_PROTOCOL:
    opt_protocol= find_type_or_exit(argument, &sql_protocol_typelib,
                                    opt->name);
    break;
  }

  if (orig_what_to_do && (what_to_do != orig_what_to_do))
  {
    fprintf(stderr, "Error:  %s doesn't support multiple contradicting commands.\n",
            my_progname);
    return 1;
  }
  return 0;
}


static int get_options(int *argc, char ***argv)
{
  int ho_error;

  if (*argc == 1)
  {
    usage();
    exit(0);
  }

  my_getopt_use_args_separator= TRUE;
  if ((ho_error= load_defaults("my", load_default_groups, argc, argv)) ||
      (ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);
  my_getopt_use_args_separator= FALSE;

  if (!what_to_do)
  {
    size_t pnlen= strlen(my_progname);

    if (pnlen < 6) /* name too short */
      what_to_do = DO_CHECK;
    else if (!strcmp("repair", my_progname + pnlen - 6))
      what_to_do = DO_REPAIR;
    else if (!strcmp("analyze", my_progname + pnlen - 7))
      what_to_do = DO_ANALYZE;
    else if  (!strcmp("optimize", my_progname + pnlen - 8))
      what_to_do = DO_OPTIMIZE;
    else
      what_to_do = DO_CHECK;
  }

  /*
    If there's no --default-character-set option given with
    --fix-table-name or --fix-db-name set the default character set to "utf8".
  */
  if (!default_charset)
  {
    if (opt_fix_db_names || opt_fix_table_names)
      default_charset= (char*) "utf8";
    else
      default_charset= (char*) MYSQL_AUTODETECT_CHARSET_NAME;
  }
  if (strcmp(default_charset, MYSQL_AUTODETECT_CHARSET_NAME) &&
      !get_charset_by_csname(default_charset, MY_CS_PRIMARY, MYF(MY_WME)))
  {
    printf("Unsupported character set: %s\n", default_charset);
    return 1;
  }
  if (*argc > 0 && opt_alldbs)
  {
    printf("You should give only options, no arguments at all, with option\n");
    printf("--all-databases. Please see %s --help for more information.\n",
	   my_progname);
    return 1;
  }

  if (*argc < 1 && !opt_alldbs)
  {
    printf("You forgot to give the arguments! Please see %s --help\n",
	   my_progname);
    printf("for more information.\n");
    return 1;
  }
  if (tty_password)
    opt_password = get_tty_password(NullS);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;
  return(0);
} /* get_options */


static int process_all_databases()
{
  MYSQL_ROW row;
  MYSQL_RES *tableres;
  int result = 0;

  if (mysql_query(sock, "SHOW DATABASES") ||
      !(tableres = mysql_store_result(sock)))
  {
    my_printf_error(0, "Error: Couldn't execute 'SHOW DATABASES': %s",
		    MYF(0), mysql_error(sock));
    return 1;
  }
  while ((row = mysql_fetch_row(tableres)))
  {
    if (process_one_db(row[0]))
      result = 1;
  }
  return result;
}
/* process_all_databases */


static int process_databases(char **db_names)
{
  int result = 0;
  for ( ; *db_names ; db_names++)
  {
    if (process_one_db(*db_names))
      result = 1;
  }
  return result;
} /* process_databases */


static int process_selected_tables(char *db, char **table_names, int tables)
{
  if (use_db(db))
    return 1;
  if (opt_all_in_1 && what_to_do != DO_UPGRADE)
  {
    /* 
      We need table list in form `a`, `b`, `c`
      that's why we need 2 more chars added to to each table name
      space is for more readable output in logs and in case of error
    */	  
    char *table_names_comma_sep, *end;
    size_t tot_length= 0;
    int             i= 0;

    for (i = 0; i < tables; i++)
      tot_length+= fixed_name_length(*(table_names + i)) + 2;

    if (!(table_names_comma_sep = (char *)
	  my_malloc((sizeof(char) * tot_length) + 4, MYF(MY_WME))))
      return 1;

    for (end = table_names_comma_sep + 1; tables > 0;
	 tables--, table_names++)
    {
      end= fix_table_name(end, *table_names);
      *end++= ',';
    }
    *--end = 0;
    handle_request_for_tables(table_names_comma_sep + 1, (uint) (tot_length - 1));
    my_free(table_names_comma_sep);
  }
  else
    for (; tables > 0; tables--, table_names++)
      handle_request_for_tables(*table_names, fixed_name_length(*table_names));
  return 0;
} /* process_selected_tables */


static uint fixed_name_length(const char *name)
{
  const char *p;
  uint extra_length= 2;  /* count the first/last backticks */
  
  for (p= name; *p; p++)
  {
    if (*p == '`')
      extra_length++;
    else if (*p == '.')
      extra_length+= 2;
  }
  return (uint) ((p - name) + extra_length);
}


static char *fix_table_name(char *dest, char *src)
{
  *dest++= '`';
  for (; *src; src++)
  {
    switch (*src) {
    case '.':            /* add backticks around '.' */
      *dest++= '`';
      *dest++= '.';
      *dest++= '`';
      break;
    case '`':            /* escape backtick character */
      *dest++= '`';
      /* fall through */
    default:
      *dest++= *src;
    }
  }
  *dest++= '`';
  return dest;
}


static int process_all_tables_in_db(char *database)
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  uint num_columns;

  LINT_INIT(res);
  if (use_db(database))
    return 1;
  if ((mysql_query(sock, "SHOW /*!50002 FULL*/ TABLES") &&
       mysql_query(sock, "SHOW TABLES")) ||
      !(res= mysql_store_result(sock)))
  {
    my_printf_error(0, "Error: Couldn't get table list for database %s: %s",
		    MYF(0), database, mysql_error(sock));
    return 1;
  }

  num_columns= mysql_num_fields(res);

  if (opt_all_in_1 && what_to_do != DO_UPGRADE)
  {
    /*
      We need table list in form `a`, `b`, `c`
      that's why we need 2 more chars added to to each table name
      space is for more readable output in logs and in case of error
     */

    char *tables, *end;
    uint tot_length = 0;

    while ((row = mysql_fetch_row(res)))
      tot_length+= fixed_name_length(row[0]) + 2;
    mysql_data_seek(res, 0);

    if (!(tables=(char *) my_malloc(sizeof(char)*tot_length+4, MYF(MY_WME))))
    {
      mysql_free_result(res);
      return 1;
    }
    for (end = tables + 1; (row = mysql_fetch_row(res)) ;)
    {
      if ((num_columns == 2) && (strcmp(row[1], "VIEW") == 0))
        continue;

      end= fix_table_name(end, row[0]);
      *end++= ',';
    }
    *--end = 0;
    if (tot_length)
      handle_request_for_tables(tables + 1, tot_length - 1);
    my_free(tables);
  }
  else
  {
    while ((row = mysql_fetch_row(res)))
    {
      /* Skip views if we don't perform renaming. */
      if ((what_to_do != DO_UPGRADE) && (num_columns == 2) && (strcmp(row[1], "VIEW") == 0))
        continue;

      handle_request_for_tables(row[0], fixed_name_length(row[0]));
    }
  }
  mysql_free_result(res);
  return 0;
} /* process_all_tables_in_db */


static int run_query(const char *query)
{
  if (mysql_query(sock, query))
  {
    fprintf(stderr, "Failed to %s\n", query);
    fprintf(stderr, "Error: %s\n", mysql_error(sock));
    return 1;
  }
  return 0;
}


static int fix_table_storage_name(const char *name)
{
  char qbuf[100 + NAME_LEN*4];
  int rc= 0;
  if (strncmp(name, "#mysql50#", 9))
    return 1;
  sprintf(qbuf, "RENAME TABLE `%s` TO `%s`", name, name + 9);
  rc= run_query(qbuf);
  if (verbose)
    printf("%-50s %s\n", name, rc ? "FAILED" : "OK");
  return rc;
}

static int fix_database_storage_name(const char *name)
{
  char qbuf[100 + NAME_LEN*4];
  int rc= 0;
  if (strncmp(name, "#mysql50#", 9))
    return 1;
  sprintf(qbuf, "ALTER DATABASE `%s` UPGRADE DATA DIRECTORY NAME", name);
  rc= run_query(qbuf);
  if (verbose)
    printf("%-50s %s\n", name, rc ? "FAILED" : "OK");
  return rc;
}

static int rebuild_table(char *name)
{
  char *query, *ptr;
  int rc= 0;
  query= (char*)my_malloc(sizeof(char) * (12 + fixed_name_length(name) + 6 + 1),
                          MYF(MY_WME));
  if (!query)
    return 1;
  ptr= strmov(query, "ALTER TABLE ");
  ptr= fix_table_name(ptr, name);
  ptr= strxmov(ptr, " FORCE", NullS);
  if (mysql_real_query(sock, query, (uint)(ptr - query)))
  {
    fprintf(stderr, "Failed to %s\n", query);
    fprintf(stderr, "Error: %s\n", mysql_error(sock));
    rc= 1;
  }
  my_free(query);
  return rc;
}

static int process_one_db(char *database)
{
  if (opt_skip_database && opt_alldbs &&
      !strcmp(database, opt_skip_database))
    return 0;

  if (what_to_do == DO_UPGRADE)
  {
    int rc= 0;
    if (opt_fix_db_names && !strncmp(database,"#mysql50#", 9))
    {
      rc= fix_database_storage_name(database);
      database+= 9;
    }
    if (rc || !opt_fix_table_names)
      return rc;
  }
  return process_all_tables_in_db(database);
}


static int use_db(char *database)
{
  if (mysql_get_server_version(sock) >= FIRST_INFORMATION_SCHEMA_VERSION &&
      !my_strcasecmp(&my_charset_latin1, database, INFORMATION_SCHEMA_DB_NAME))
    return 1;
  if (mysql_get_server_version(sock) >= FIRST_PERFORMANCE_SCHEMA_VERSION &&
      !my_strcasecmp(&my_charset_latin1, database, PERFORMANCE_SCHEMA_DB_NAME))
    return 1;
  if (mysql_select_db(sock, database))
  {
    DBerror(sock, "when selecting the database");
    return 1;
  }
  return 0;
} /* use_db */

static int disable_binlog()
{
  const char *stmt= "SET SQL_LOG_BIN=0";
  return run_query(stmt);
}

static int handle_request_for_tables(char *tables, uint length)
{
  char *query, *end, options[100], message[100];
  uint query_length= 0;
  const char *op = 0;

  options[0] = 0;
  end = options;
  switch (what_to_do) {
  case DO_CHECK:
    op = "CHECK";
    if (opt_quick)              end = strmov(end, " QUICK");
    if (opt_fast)               end = strmov(end, " FAST");
    if (opt_medium_check)       end = strmov(end, " MEDIUM"); /* Default */
    if (opt_extended)           end = strmov(end, " EXTENDED");
    if (opt_check_only_changed) end = strmov(end, " CHANGED");
    if (opt_upgrade)            end = strmov(end, " FOR UPGRADE");
    break;
  case DO_REPAIR:
    op= (opt_write_binlog) ? "REPAIR" : "REPAIR NO_WRITE_TO_BINLOG";
    if (opt_quick)              end = strmov(end, " QUICK");
    if (opt_extended)           end = strmov(end, " EXTENDED");
    if (opt_frm)                end = strmov(end, " USE_FRM");
    break;
  case DO_ANALYZE:
    op= (opt_write_binlog) ? "ANALYZE" : "ANALYZE NO_WRITE_TO_BINLOG";
    break;
  case DO_OPTIMIZE:
    op= (opt_write_binlog) ? "OPTIMIZE" : "OPTIMIZE NO_WRITE_TO_BINLOG";
    break;
  case DO_UPGRADE:
    return fix_table_storage_name(tables);
  }

  if (!(query =(char *) my_malloc((sizeof(char)*(length+110)), MYF(MY_WME))))
    return 1;
  if (opt_all_in_1)
  {
    /* No backticks here as we added them before */
    query_length= sprintf(query, "%s TABLE %s %s", op, tables, options);
  }
  else
  {
    char *ptr;

    ptr= strmov(strmov(query, op), " TABLE ");
    ptr= fix_table_name(ptr, tables);
    ptr= strxmov(ptr, " ", options, NullS);
    query_length= (uint) (ptr - query);
  }
  if (mysql_real_query(sock, query, query_length))
  {
    sprintf(message, "when executing '%s TABLE ... %s'", op, options);
    DBerror(sock, message);
    return 1;
  }
  print_result();
  my_free(query);
  return 0;
}


static void print_result()
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  char prev[NAME_LEN*2+2];
  char prev_alter[MAX_ALTER_STR_SIZE];
  uint i;
  my_bool found_error=0, table_rebuild=0;

  res = mysql_use_result(sock);

  prev[0] = '\0';
  prev_alter[0]= 0;
  for (i = 0; (row = mysql_fetch_row(res)); i++)
  {
    int changed = strcmp(prev, row[0]);
    my_bool status = !strcmp(row[2], "status");

    if (status)
    {
      /*
        if there was an error with the table, we have --auto-repair set,
        and this isn't a repair op, then add the table to the tables4repair
        list
      */
      if (found_error && opt_auto_repair && what_to_do != DO_REPAIR &&
	  strcmp(row[3],"OK"))
      {
        if (table_rebuild)
        {
          if (prev_alter[0])
            insert_dynamic(&alter_table_cmds, (uchar*) prev_alter);
          else
            insert_dynamic(&tables4rebuild, (uchar*) prev);
        }
        else
          insert_dynamic(&tables4repair, prev);
      }
      found_error=0;
      table_rebuild=0;
      prev_alter[0]= 0;
      if (opt_silent)
	continue;
    }
    if (status && changed)
      printf("%-50s %s", row[0], row[3]);
    else if (!status && changed)
    {
      printf("%s\n%-9s: %s", row[0], row[2], row[3]);
      if (opt_auto_repair && strcmp(row[2],"note"))
      {
        const char *alter_txt= strstr(row[3], "ALTER TABLE");
        found_error=1;
        if (alter_txt)
        {
          table_rebuild=1;
          if (!strncmp(row[3], KEY_PARTITIONING_CHANGED_STR,
                       strlen(KEY_PARTITIONING_CHANGED_STR)) &&
              strstr(alter_txt, "PARTITION BY"))
          {
            if (strlen(alter_txt) >= MAX_ALTER_STR_SIZE)
            {
              printf("Error: Alter command too long (>= %d),"
                     " please do \"%s\" or dump/reload to fix it!\n",
                     MAX_ALTER_STR_SIZE,
                     alter_txt);
              table_rebuild= 0;
              prev_alter[0]= 0;
            }
            else
              strcpy(prev_alter, alter_txt);
          }
        }
      }
    }
    else
      printf("%-9s: %s", row[2], row[3]);
    strmov(prev, row[0]);
    putchar('\n');
  }
  /* add the last table to be repaired to the list */
  if (found_error && opt_auto_repair && what_to_do != DO_REPAIR)
  {
    if (table_rebuild)
    {
      if (prev_alter[0])
        insert_dynamic(&alter_table_cmds, (uchar*) prev_alter);
      else
        insert_dynamic(&tables4rebuild, (uchar*) prev);
    }
    else
      insert_dynamic(&tables4repair, prev);
  }
  mysql_free_result(res);
}


static int dbConnect(char *host, char *user, char *passwd)
{
  DBUG_ENTER("dbConnect");
  if (verbose)
  {
    fprintf(stderr, "# Connecting to %s...\n", host ? host : "localhost");
  }
  mysql_init(&mysql_connection);
  if (opt_compress)
    mysql_options(&mysql_connection, MYSQL_OPT_COMPRESS, NullS);
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
  {
    mysql_ssl_set(&mysql_connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
    mysql_options(&mysql_connection, MYSQL_OPT_SSL_CRL, opt_ssl_crl);
    mysql_options(&mysql_connection, MYSQL_OPT_SSL_CRLPATH, opt_ssl_crlpath);
  }
#endif
  if (opt_protocol)
    mysql_options(&mysql_connection,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
  if (opt_bind_addr)
    mysql_options(&mysql_connection, MYSQL_OPT_BIND, opt_bind_addr);
#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    mysql_options(&mysql_connection,MYSQL_SHARED_MEMORY_BASE_NAME,shared_memory_base_name);
#endif

  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(&mysql_connection, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(&mysql_connection, MYSQL_DEFAULT_AUTH, opt_default_auth);

  mysql_options(&mysql_connection, MYSQL_SET_CHARSET_NAME, default_charset);
  mysql_options(&mysql_connection, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(&mysql_connection, MYSQL_OPT_CONNECT_ATTR_ADD,
                 "program_name", "mysqlcheck");
  if (!(sock = mysql_real_connect(&mysql_connection, host, user, passwd,
         NULL, opt_mysql_port, opt_mysql_unix_port, 0)))
  {
    DBerror(&mysql_connection, "when trying to connect");
    DBUG_RETURN(1);
  }
  mysql_connection.reconnect= 1;
  DBUG_RETURN(0);
} /* dbConnect */


static void dbDisconnect(char *host)
{
  if (verbose)
    fprintf(stderr, "# Disconnecting from %s...\n", host ? host : "localhost");
  mysql_close(sock);
} /* dbDisconnect */


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


int main(int argc, char **argv)
{
  MY_INIT(argv[0]);
  /*
  ** Check out the args
  */
  if (get_options(&argc, &argv))
  {
    my_end(my_end_arg);
    exit(EX_USAGE);
  }
  if (dbConnect(current_host, current_user, opt_password))
    exit(EX_MYSQLERR);

  if (!opt_write_binlog)
  {
    if (disable_binlog()) {
      first_error= 1;
      goto end;
    }
  }

  if (opt_auto_repair &&
      (my_init_dynamic_array(&tables4repair, sizeof(char)*(NAME_LEN*2+2),16,64) ||
       my_init_dynamic_array(&tables4rebuild, sizeof(char)*(NAME_LEN*2+2),16,64) ||
       my_init_dynamic_array(&alter_table_cmds, MAX_ALTER_STR_SIZE, 0, 1)))
  {
    first_error = 1;
    goto end;
  }

  if (opt_alldbs)
    process_all_databases();
  /* Only one database and selected table(s) */
  else if (argc > 1 && !opt_databases)
    process_selected_tables(*argv, (argv + 1), (argc - 1));
  /* One or more databases, all tables */
  else
    process_databases(argv);
  if (opt_auto_repair)
  {
    uint i;

    if (!opt_silent && (tables4repair.elements || tables4rebuild.elements))
      puts("\nRepairing tables");
    what_to_do = DO_REPAIR;
    for (i = 0; i < tables4repair.elements ; i++)
    {
      char *name= (char*) dynamic_array_ptr(&tables4repair, i);
      handle_request_for_tables(name, fixed_name_length(name));
    }
    for (i = 0; i < tables4rebuild.elements ; i++)
      rebuild_table((char*) dynamic_array_ptr(&tables4rebuild, i));
    for (i = 0; i < alter_table_cmds.elements ; i++)
      run_query((char*) dynamic_array_ptr(&alter_table_cmds, i));
  }
 end:
  dbDisconnect(current_host);
  if (opt_auto_repair)
  {
    delete_dynamic(&tables4repair);
    delete_dynamic(&tables4rebuild);
  }
  my_free(opt_password);
#ifdef HAVE_SMEM
  my_free(shared_memory_base_name);
#endif
  my_end(my_end_arg);
  return(first_error!=0);
} /* main */
