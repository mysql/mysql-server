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

/* By Jani Tolonen, 2001-04-20, MySQL Development Team */

#define CHECK_VERSION "2.4.3"

#include "client_priv.h"
#include <m_ctype.h>
#include <mysql_version.h>
#include <mysqld_error.h>
#include <sslopt-vars.h>

/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2

static MYSQL mysql_connection, *sock = 0;
static my_bool opt_alldbs = 0, opt_check_only_changed = 0, opt_extended = 0,
               opt_compress = 0, opt_databases = 0, opt_fast = 0,
               opt_medium_check = 0, opt_quick = 0, opt_all_in_1 = 0,
               opt_silent = 0, opt_auto_repair = 0, ignore_errors = 0,
               tty_password = 0, opt_frm = 0;
static uint verbose = 0, opt_mysql_port=0;
static my_string opt_mysql_unix_port = 0;
static char *opt_password = 0, *current_user = 0, 
	    *default_charset = (char *)MYSQL_DEFAULT_CHARSET_NAME,
	    *current_host = 0;
static int first_error = 0;
DYNAMIC_ARRAY tables4repair;
#ifdef HAVE_SMEM
static char *shared_memory_base_name=0;
#endif
static uint opt_protocol=0;
static CHARSET_INFO *charset_info= &my_charset_latin1;

enum operations {DO_CHECK, DO_REPAIR, DO_ANALYZE, DO_OPTIMIZE};

static struct my_option my_long_options[] =
{
  {"all-databases", 'A',
   "Check all the databases. This will be same as  --databases with all databases selected.",
   (gptr*) &opt_alldbs, (gptr*) &opt_alldbs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"analyze", 'a', "Analyze given tables.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"all-in-1", '1',
   "Instead of issuing one query for each table, use one query per database, naming all tables in the database in a comma-separated list.",
   (gptr*) &opt_all_in_1, (gptr*) &opt_all_in_1, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"auto-repair", OPT_AUTO_REPAIR,
   "If a checked table is corrupted, automatically fix it. Repairing will be done after all tables have been checked, if corrupted ones were found.",
   (gptr*) &opt_auto_repair, (gptr*) &opt_auto_repair, 0, GET_BOOL, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", (gptr*) &charsets_dir,
   (gptr*) &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"check", 'c', "Check table for errors.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"check-only-changed", 'C',
   "Check only tables that have changed since last check or haven't been closed properly.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", OPT_COMPRESS, "Use compression in server/client protocol.",
   (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"databases", 'B',
   "To check several databases. Note the difference in usage; In this case no tables are given. All name arguments are regarded as databasenames.",
   (gptr*) &opt_databases, (gptr*) &opt_databases, 0, GET_BOOL, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", (gptr*) &default_charset,
   (gptr*) &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"fast",'F', "Check only tables that haven't been closed properly.",
   (gptr*) &opt_fast, (gptr*) &opt_fast, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"force", 'f', "Continue even if we get an sql-error.",
   (gptr*) &ignore_errors, (gptr*) &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"extended", 'e',
   "If you are using this option with CHECK TABLE, it will ensure that the table is 100 percent consistent, but will take a long time. If you are using this option with REPAIR TABLE, it will force using old slow repair with keycache method, instead of much faster repair by sorting.",
   (gptr*) &opt_extended, (gptr*) &opt_extended, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host",'h', "Connect to host.", (gptr*) &current_host,
   (gptr*) &current_host, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"medium-check", 'm',
   "Faster than extended-check, but only finds 99.99 percent of all errors. Should be good enough for most cases.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"optimize", 'o', "Optimize table.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0,
   0, 0},
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
  {"quick", 'q',
   "If you are using this option with CHECK TABLE, it prevents the check from scanning the rows to check for wrong links. This is the fastest check. If you are using this option with REPAIR TABLE, it will try to repair only the index tree. This is the fastest repair method for a table.",
   (gptr*) &opt_quick, (gptr*) &opt_quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"repair", 'r',
   "Can fix almost anything except unique keys that aren't unique.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", (gptr*) &shared_memory_base_name, (gptr*) &shared_memory_base_name,
   0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"silent", 's', "Print only error messages.", (gptr*) &opt_silent,
   (gptr*) &opt_silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
   (gptr*) &opt_mysql_unix_port, (gptr*) &opt_mysql_unix_port, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include <sslopt-longopts.h>
  {"tables", OPT_TABLES, "Overrides option --databases (-B).", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", (gptr*) &current_user,
   (gptr*) &current_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"use-frm", OPT_FRM,
   "When used with REPAIR, get table structure from .frm file, so the table can be repaired even if .MYI header is corrupted.",
   (gptr*) &opt_frm, (gptr*) &opt_frm, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
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
static int use_db(char *database);
static int handle_request_for_tables(char *tables, uint length);
static int dbConnect(char *host, char *user,char *passwd);
static void dbDisconnect(char *host);
static void DBerror(MYSQL *mysql, const char *when);
static void safe_exit(int error);
static void print_result();
static char *fix_table_name(char *dest, char *src);
int what_to_do = 0;

#include <help_start.h>

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n", my_progname, CHECK_VERSION,
   MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  NETWARE_SET_SCREEN_MODE(1);
} /* print_version */


static void usage(void)
{
  print_version();
  puts("By Jani Tolonen, 2001-04-20, MySQL Development Team\n");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\n");
  puts("and you are welcome to modify and redistribute it under the GPL license.\n");
  puts("This program can be used to CHECK (-c,-m,-C), REPAIR (-r), ANALYZE (-a)");
  puts("or OPTIMIZE (-o) tables. Some of the options (like -e or -q) can be");
  puts("used at the same time. It works on MyISAM and in some cases on BDB tables.");
  puts("Please consult the MySQL manual for latest information about the");
  puts("above. The options -c,-r,-a and -o are exclusive to each other, which");
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

#include <help_end.h>

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
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
  case 'p':
    if (argument)
    {
      char *start = argument;
      my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
      opt_password = my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      if (*start)
	start[1] = 0;                             /* Cut length of argument */
    }
    else
      tty_password = 1;
    break;
  case 'r':
    what_to_do = DO_REPAIR;
    break;
  case 'W':
#ifdef __WIN__
    opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
    break;
  case '#':
    DBUG_PUSH(argument ? argument : "d:t:o");
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

  if (*argc == 1)
  {
    usage();
    exit(0);
  }

  load_defaults("my", load_default_groups, argc, argv);

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (!what_to_do)
  {
    int pnlen = strlen(my_progname);

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

  /* TODO: This variable is not yet used */
  if (strcmp(default_charset, charset_info->csname) &&
      !(charset_info= get_charset_by_csname(default_charset, 
  					    MY_CS_PRIMARY, MYF(MY_WME))))
      exit(1);
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
    if (process_all_tables_in_db(row[0]))
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
    if (process_all_tables_in_db(*db_names))
      result = 1;
  }
  return result;
} /* process_databases */


static int process_selected_tables(char *db, char **table_names, int tables)
{
  if (use_db(db))
    return 1;
  if (opt_all_in_1)
  {
    /* 
      We need table list in form `a`, `b`, `c`
      that's why we need 4 more chars added to to each table name
      space is for more readable output in logs and in case of error
    */	  
    char *table_names_comma_sep, *end;
    int i, tot_length = 0;

    for (i = 0; i < tables; i++)
      tot_length += strlen(*(table_names + i)) + 4;

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
    handle_request_for_tables(table_names_comma_sep + 1, tot_length - 1);
    my_free(table_names_comma_sep, MYF(0));
  }
  else
    for (; tables > 0; tables--, table_names++)
      handle_request_for_tables(*table_names, strlen(*table_names));
  return 0;
} /* process_selected_tables */


static char *fix_table_name(char *dest, char *src)
{
  char *db_sep;

  *dest++= '`';
  if ((db_sep= strchr(src, '.')))
  {
    dest= strmake(dest, src, (uint) (db_sep - src));
    dest= strmov(dest, "`.`");
    src= db_sep + 1;
  }
  dest= strxmov(dest, src, "`", NullS);
  return dest;
}


static int process_all_tables_in_db(char *database)
{
  MYSQL_RES *res;
  MYSQL_ROW row;

  LINT_INIT(res);
  if (use_db(database))
    return 1;
  if (mysql_query(sock, "SHOW TABLES") ||
	!((res= mysql_store_result(sock))))
    return 1;

  if (opt_all_in_1)
  {
    /*
      We need table list in form `a`, `b`, `c`
      that's why we need 4 more chars added to to each table name
      space is for more readable output in logs and in case of error
     */

    char *tables, *end;
    uint tot_length = 0;

    while ((row = mysql_fetch_row(res)))
      tot_length += strlen(row[0]) + 4;
    mysql_data_seek(res, 0);

    if (!(tables=(char *) my_malloc(sizeof(char)*tot_length+4, MYF(MY_WME))))
    {
      mysql_free_result(res);
      return 1;
    }
    for (end = tables + 1; (row = mysql_fetch_row(res)) ;)
    {
      end= fix_table_name(end, row[0]);
      *end++= ',';
    }
    *--end = 0;
    if (tot_length)
      handle_request_for_tables(tables + 1, tot_length - 1);
    my_free(tables, MYF(0));
  }
  else
  {
    while ((row = mysql_fetch_row(res)))
      handle_request_for_tables(row[0], strlen(row[0]));
  }
  mysql_free_result(res);
  return 0;
} /* process_all_tables_in_db */


static int use_db(char *database)
{
  if (mysql_select_db(sock, database))
  {
    DBerror(sock, "when selecting the database");
    return 1;
  }
  return 0;
} /* use_db */


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
    break;
  case DO_REPAIR:
    op = "REPAIR";
    if (opt_quick)              end = strmov(end, " QUICK");
    if (opt_extended)           end = strmov(end, " EXTENDED");
    if (opt_frm)                end = strmov(end, " USE_FRM");
    break;
  case DO_ANALYZE:
    op = "ANALYZE";
    break;
  case DO_OPTIMIZE:
    op = "OPTIMIZE";
    break;
  }

  if (!(query =(char *) my_malloc((sizeof(char)*(length+110)), MYF(MY_WME))))
    return 1;
  if (opt_all_in_1)
  {
    /* No backticks here as we added them before */
    query_length= my_sprintf(query,
			     (query, "%s TABLE %s %s", op, tables, options));
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
  my_free(query, MYF(0));
  return 0;
}


static void print_result()
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  char prev[NAME_LEN*2+2];
  uint i;
  my_bool found_error=0;

  res = mysql_use_result(sock);
  prev[0] = '\0';
  for (i = 0; (row = mysql_fetch_row(res)); i++)
  {
    int changed = strcmp(prev, row[0]);
    my_bool status = !strcmp(row[2], "status");

    if (status)
    {
      if (found_error && opt_auto_repair && what_to_do != DO_REPAIR &&
	  (!opt_fast || strcmp(row[3],"OK")))
	insert_dynamic(&tables4repair, prev);
      found_error=0;
      if (opt_silent)
	continue;
    }
    if (status && changed)
      printf("%-50s %s", row[0], row[3]);
    else if (!status && changed)
    {
      printf("%s\n%-9s: %s", row[0], row[2], row[3]);
      if (strcmp(row[2],"note"))
	found_error=1;
    }
    else
      printf("%-9s: %s", row[2], row[3]);
    strmov(prev, row[0]);
    putchar('\n');
  }
  if (found_error && opt_auto_repair && what_to_do != DO_REPAIR &&
      (!opt_fast || strcmp(row[3],"OK")))
    insert_dynamic(&tables4repair, prev);
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
    mysql_ssl_set(&mysql_connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
#endif
  if (opt_protocol)
    mysql_options(&mysql_connection,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    mysql_options(&mysql_connection,MYSQL_SHARED_MEMORY_BASE_NAME,shared_memory_base_name);
#endif
  if (!(sock = mysql_real_connect(&mysql_connection, host, user, passwd,
         NULL, opt_mysql_port, opt_mysql_unix_port, 0)))
  {
    DBerror(&mysql_connection, "when trying to connect");
    return 1;
  }
  return 0;
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
    my_end(0);
    exit(EX_USAGE);
  }
  if (dbConnect(current_host, current_user, opt_password))
    exit(EX_MYSQLERR);

  if (opt_auto_repair &&
      my_init_dynamic_array(&tables4repair, sizeof(char)*(NAME_LEN*2+2),16,64))
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

    if (!opt_silent && tables4repair.elements)
      puts("\nRepairing tables");
    what_to_do = DO_REPAIR;
    for (i = 0; i < tables4repair.elements ; i++)
    {
      char *name= (char*) dynamic_array_ptr(&tables4repair, i);
      handle_request_for_tables(name, strlen(name));
    }
  }
 end:
  dbDisconnect(current_host);
  if (opt_auto_repair)
    delete_dynamic(&tables4repair);
  my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
#ifdef HAVE_SMEM
  my_free(shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
#endif
  my_end(0);
  return(first_error!=0);
} /* main */
