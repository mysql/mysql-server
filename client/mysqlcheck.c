/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#define CHECK_VERSION "1.02"

#include "client_priv.h"
#include <m_ctype.h>
#include "mysql_version.h"
#include "mysqld_error.h"
#include "sslopt-vars.h"

/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2

static MYSQL mysql_connection, *sock = 0;
static my_bool opt_alldbs = 0, opt_check_only_changed = 0, opt_extended = 0,
               opt_compress = 0, opt_databases = 0, opt_fast = 0,
               opt_medium_check = 0, opt_quick = 0, opt_all_in_1 = 0,
               opt_silent = 0, opt_auto_repair = 0, ignore_errors = 0;
static uint verbose = 0, opt_mysql_port=0;
static my_string opt_mysql_unix_port = 0;
static char *opt_password = 0, *current_user = 0, *default_charset = 0,
            *current_host = 0;
static int first_error = 0;
DYNAMIC_ARRAY tables4repair;

enum operations {DO_CHECK, DO_REPAIR, DO_ANALYZE, DO_OPTIMIZE};

static struct option long_options[] =
{
  {"all-databases",         no_argument,       0, 'A'},
  {"all-in-1",              no_argument,       0, '1'},
  {"auto-repair",           no_argument,       0, OPT_AUTO_REPAIR},
  {"analyze",		    no_argument,       0, 'a'},
  {"character-sets-dir",    required_argument, 0, OPT_CHARSETS_DIR},
  {"check",	            no_argument,       0, 'c'},
  {"check-only-changed",    no_argument,       0, 'C'},
  {"compress",              no_argument,       0, OPT_COMPRESS},
  {"databases",             no_argument,       0, 'B'},
  {"debug",		    optional_argument, 0, '#'},
  {"default-character-set", required_argument, 0, OPT_DEFAULT_CHARSET},
  {"fast",	            no_argument,       0, 'F'},
  {"force",                 no_argument,       0, 'f'},
  {"extended",              no_argument,       0, 'e'},
  {"help",   		    no_argument,       0, '?'},
  {"host",    		    required_argument, 0, 'h'},
  {"medium-check",          no_argument,       0, 'm'},
  {"optimize",              no_argument,       0, 'o'},
  {"password",  	    optional_argument, 0, 'p'},
#ifdef __WIN__
  {"pipe",		    no_argument,       0, 'W'},
#endif
  {"port",    		    required_argument, 0, 'P'},
  {"quick",    		    no_argument,       0, 'q'},
  {"repair",	            no_argument,       0, 'r'},
  {"silent",                no_argument,       0, 's'},
  {"socket",   		    required_argument, 0, 'S'},
#include "sslopt-longopts.h"
  {"tables",                no_argument,       0, OPT_TABLES},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",    		    required_argument, 0, 'u'},
#endif
  {"verbose",    	    no_argument,       0, 'v'},
  {"version",    	    no_argument,       0, 'V'},
  {0, 0, 0, 0}
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
int what_to_do = 0;

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n", my_progname, CHECK_VERSION,
   MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
} /* print_version */


static void usage(void)
{
  print_version();
  puts("By Jani Tolonen, 2001-04-20, MySQL Development Team\n");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free");
  puts("software and you are welcome to modify and redistribute it");
  puts("under the GPL license.\n");
  puts("This program can be used to CHECK (-c,-m,-C), REPAIR (-r), ANALYZE (-a)");
  puts("or OPTIMIZE (-o) tables. Some of the options (like -e or -q) can be");
  puts("used same time. It works on MyISAM and in some cases on BDB tables.");
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
  printf("\
  -A, --all-databases   Check all the databases. This will be same as\n\
		        --databases with all databases selected\n\
  -1, --all-in-1        Instead of making one query for each table, execute\n\
                        all queries in 1 query separately for each database.\n\
                        Table names will be in a comma separeted list.\n\
  -a, --analyze         Analyze given tables.\n\
  --auto-repair         If a checked table is corrupted, automatically fix\n\
                        it. Repairing will be done after all tables have\n\
                        been checked, if corrupted ones were found.\n\
  -#, --debug=...       Output debug log. Often this is 'd:t:o,filename'\n\
  --character-sets-dir=...\n\
                        Directory where character sets are\n\
  -c, --check           Check table for errors\n\
  -C, --check-only-changed\n\
		        Check only tables that have changed since last check\n\
                        or haven't been closed properly.\n\
  --compress            Use compression in server/client protocol.\n\
  -?, --help		Display this help message and exit.\n\
  -B, --databases       To check several databases. Note the difference in\n\
 			usage; In this case no tables are given. All name\n\
			arguments are regarded as databasenames.\n\
  --default-character-set=...\n\
                        Set the default character set\n\
  -F, --fast	        Check only tables that hasn't been closed properly\n\
  -f, --force		Continue even if we get an sql-error.\n\
  -e, --extended        If you are using this option with CHECK TABLE,\n\
                        it will ensure that the table is 100 percent\n\
                        consistent, but will take a long time.\n\n");
printf("\
                        If you are using this option with REPAIR TABLE,\n\
                        it will run an extended repair on the table, which\n\
                        may not only take a long time to execute, but\n\
                        may produce a lot of garbage rows also!\n\
  -h, --host=...	Connect to host.\n\
  -m, --medium-check    Faster than extended-check, but only finds 99.99 percent\n\
                        of all errors.  Should be good enough for most cases.\n\
  -o, --optimize        Optimize table\n\
  -p, --password[=...]	Password to use when connecting to server.\n\
                        If password is not given it's solicited on the tty.\n");
#ifdef __WIN__
  puts("-W, --pipe	Use named pipes to connect to server");
#endif
  printf("\
  -P, --port=...	Port number to use for connection.\n\
  -q, --quick		If you are using this option with CHECK TABLE, it\n\
                        prevents the check from scanning the rows to check\n\
                        for wrong links. This is the fastest check.\n\n\
                        If you are using this option with REPAIR TABLE, it\n\
                        will try to repair only the index tree. This is\n\
                        the fastest repair method for a table.\n\
  -r, --repair          Can fix almost anything except unique keys that aren't\n\
                        unique.\n\
  -s, --silent          Print only error messages.\n\
  -S, --socket=...	Socket file to use for connection.\n\
  --tables              Overrides option --databases (-B).\n");
#include "sslopt-usage.h"
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user.\n");
#endif
  printf("\
  -v, --verbose		Print info about the various stages.\n\
  -V, --version		Output version information and exit.\n");
  print_defaults("my", load_default_groups);
} /* usage */

 
static int get_options(int *argc, char ***argv)
{
  int c, option_index;
  my_bool tty_password = 0;

  if (*argc == 1)
  {
    usage();
    exit(0);
  }

  load_defaults("my", load_default_groups, argc, argv);
  while ((c = getopt_long(*argc, *argv, "#::p::h:u:P:S:BaAcCdeFfmqorsvVw:?I1",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
    case 'a':
      what_to_do = DO_ANALYZE;
      break;
    case '1':
      opt_all_in_1 = 1;
      break;
    case 'A':
      opt_alldbs = 1;
      break;
    case OPT_AUTO_REPAIR:
      opt_auto_repair = 1;
      break;
    case OPT_DEFAULT_CHARSET:
      default_charset = optarg;
      break;
    case OPT_CHARSETS_DIR:
      charsets_dir = optarg;
      break;
    case 'c':
      what_to_do = DO_CHECK;
      break;
    case 'C':
      what_to_do = DO_CHECK;
      opt_check_only_changed = 1;
      break;
    case 'e':
      opt_extended = 1;
      break;
    case OPT_COMPRESS:
      opt_compress = 1;
      break;
    case 'B':
      opt_databases = 1;
      break;
    case 'F':
      opt_fast = 1;
      break;
    case 'f':
      ignore_errors = 1;
      break;
    case 'I': /* Fall through */
    case '?':
      usage();
      exit(0);
    case 'h':
      my_free(current_host, MYF(MY_ALLOW_ZERO_PTR));
      current_host = my_strdup(optarg, MYF(MY_WME));
      break;
    case 'm':
      what_to_do = DO_CHECK;
      opt_medium_check = 1;
      break;
    case 'o':
      what_to_do = DO_OPTIMIZE;
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      current_user = optarg;
      break;
#endif
    case 'p':
      if (optarg)
      {
	char *start = optarg;
	my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
	opt_password = my_strdup(optarg, MYF(MY_FAE));
	while (*optarg) *optarg++= 'x';		/* Destroy argument */
	if (*start)
	  start[1] = 0;	         		/* Cut length of argument */
      }
      else
	tty_password = 1;
      break;
    case 'P':
      opt_mysql_port = (unsigned int) atoi(optarg);
      break;
    case 'q':
      opt_quick = 1;
      break;
    case 'r':
      what_to_do = DO_REPAIR;
      break;
    case 'S':
      opt_mysql_unix_port = optarg;
     break;
    case 's':
      opt_silent = 1;
      break;
    case 'W':
#ifdef __WIN__
      opt_mysql_unix_port = MYSQL_NAMEDPIPE;
#endif
      break;
    case '#':
      DBUG_PUSH(optarg ? optarg : "d:t:o");
      break;
    case OPT_TABLES:
      opt_databases = 0;
      break;
    case 'v':
      verbose++;
      break;
    case 'V': print_version(); exit(0);
    default:
      fprintf(stderr, "%s: Illegal option character '%c'\n", my_progname,
	      opterr);
#include "sslopt-case.h"
    }
  }
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
  if (default_charset)
  {
    if (set_default_charset_by_name(default_charset, MYF(MY_WME)))
      exit(1);
  }
  (*argc) -= optind;
  (*argv) += optind;
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
    char *table_names_comma_sep, *end;
    int i, tot_length = 0;

    for (i = 0; i < tables; i++)
      tot_length += strlen(*(table_names + i)) + 1;
    
    if (!(table_names_comma_sep = (char *)
	  my_malloc((sizeof(char) * tot_length) + 1, MYF(MY_WME))))
      return 1;

    for (end = table_names_comma_sep + 1; tables > 0;
	 tables--, table_names++)
    {
      end = strmov(end, *table_names);
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


static int process_all_tables_in_db(char *database)
{
  MYSQL_RES *res;
  MYSQL_ROW row;

  LINT_INIT(res);
  if (use_db(database))
    return 1;
  if (!(mysql_query(sock, "SHOW TABLES") ||
	(res = mysql_store_result(sock))))
    return 1;
  
  if (opt_all_in_1)
  {
    char *tables, *end;
    uint tot_length = 0;

    while ((row = mysql_fetch_row(res)))
      tot_length += strlen(row[0]) + 1;
    mysql_data_seek(res, 0);
    
    if (!(tables=(char *) my_malloc(sizeof(char)*tot_length+1, MYF(MY_WME))))
    {
      mysql_free_result(res);
      return 1;
    }
    for (end = tables + 1; (row = mysql_fetch_row(res)) ;)
    {
      end = strmov(end, row[0]);
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
  sprintf(query, "%s TABLE %s %s", op, tables, options);
  if (mysql_query(sock, query))
  {
    sprintf(message, "when executing '%s TABLE ... %s", op, options);
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
      if (found_error)
      {
	if (what_to_do != DO_REPAIR && opt_auto_repair &&
	    (!opt_fast || strcmp(row[3],"OK")))
	  insert_dynamic(&tables4repair, row[0]);
      }
      found_error=0;
      if (opt_silent)
	continue;
    }
    if (status && changed)
      printf("%-50s %s", row[0], row[3]);
    else if (!status && changed)
    {
      printf("%s\n%-9s: %s", row[0], row[2], row[3]);
      found_error=1;
    }
    else
      printf("%-9s: %s", row[2], row[3]);
    strmov(prev, row[0]);
    putchar('\n');
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
    mysql_ssl_set(&mysql_connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath);
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
      init_dynamic_array(&tables4repair, sizeof(char)*(NAME_LEN*2+2),16,64))
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
  my_end(0);
  return(first_error!=0);
} /* main */
