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

/* mysqldump.c  - Dump a tables contents and format to an ASCII file
**
** The author's original notes follow :-
**
**		******************************************************
**		*						     *
**		* AUTHOR: Igor Romanenko (igor@frog.kiev.ua)	     *
**		* DATE:   December 3, 1994			     *
**		* WARRANTY: None, expressed, impressed, implied      *
**		*	    or other				     *
**		* STATUS: Public domain				     *
**		* Adapted and optimized for MySQL by		     *
**		* Michael Widenius, Sinisa Milivojevic, Jani Tolonen *
**		* -w --where added 9/10/98 by Jim Faucette	     *
**		* slave code by David Saez Padros <david@ols.es>     *
**		*						     *
**		******************************************************
*/
/* SSL by
**   Andrei Errapart <andreie@no.spam.ee>
**   Tõnu Samuel  <tonu@please.do.not.remove.this.spam.ee>
**/

#define DUMP_VERSION "8.14"

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>

#include "mysql.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include <getopt.h>

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

static char *add_load_option(char *ptr, const char *object,
			     const char *statement);

static char *field_escape(char *to,const char *from,uint length);
static my_bool  verbose=0,tFlag=0,cFlag=0,dFlag=0,quick=0, extended_insert = 0,
		lock_tables=0,ignore_errors=0,flush_logs=0,replace=0,
		ignore=0,opt_drop=0,opt_keywords=0,opt_lock=0,opt_compress=0,
                opt_delayed=0,create_options=0,opt_quoted=0,opt_databases=0,
	        opt_alldbs=0,opt_create_db=0,opt_first_slave=0;
static MYSQL  mysql_connection,*sock=0;
static char  insert_pat[12 * 1024],*opt_password=0,*current_user=0,
             *current_host=0,*path=0,*fields_terminated=0,
             *lines_terminated=0, *enclosed=0, *opt_enclosed=0, *escaped=0,
             *where=0, *default_charset;
static uint     opt_mysql_port=0;
static my_string opt_mysql_unix_port=0;
static int   first_error=0;
extern ulong net_buffer_length;
static DYNAMIC_STRING extended_row;
#include "sslopt-vars.h"
FILE  *md_result_file;

enum md_options {OPT_FTB=256, OPT_LTB, OPT_ENC, OPT_O_ENC, OPT_ESC,
		 OPT_KEYWORDS, OPT_LOCKS, OPT_DROP, OPT_OPTIMIZE, OPT_DELAYED,
		 OPT_TABLES, MD_OPT_CHARSETS_DIR, MD_OPT_DEFAULT_CHARSET};

static struct option long_options[] =
{
  {"all-databases",     no_argument,    0,      'A'},
  {"all",		no_argument,    0, 	'a'},
  {"add-drop-table",	no_argument,    0, 	OPT_DROP},
  {"add-locks",    	no_argument,    0,	OPT_LOCKS},
  {"allow-keywords",	no_argument,    0, 	OPT_KEYWORDS},
  {"character-sets-dir",required_argument,0,    MD_OPT_CHARSETS_DIR},
  {"complete-insert",	no_argument,    0, 	'c'},
  {"compress",          no_argument,    0, 	'C'},
  {"databases",         no_argument,    0,      'B'},
  {"debug",		optional_argument, 	0, '#'},
  {"default-character-set", required_argument,  0, MD_OPT_DEFAULT_CHARSET},
  {"delayed-insert",	no_argument,    0, 	OPT_DELAYED},
  {"extended-insert",   no_argument,    0, 	'e'},
  {"fields-terminated-by", required_argument,   0, (int) OPT_FTB},
  {"fields-enclosed-by", required_argument,	0, (int) OPT_ENC},
  {"fields-optionally-enclosed-by", required_argument, 0, (int) OPT_O_ENC},
  {"fields-escaped-by", required_argument,	0, (int) OPT_ESC},
  {"first-slave",	no_argument,    0,	'x'},
  {"flush-logs",	no_argument,    0,	'F'},
  {"force",    		no_argument,    0,	'f'},
  {"help",   		no_argument,    0,	'?'},
  {"host",    		required_argument,	0, 'h'},
  {"lines-terminated-by", required_argument,    0, (int) OPT_LTB},
  {"lock-tables",  	no_argument,    0, 	'l'},
  {"no-create-db",      no_argument,    0,      'n'},
  {"no-create-info", 	no_argument,    0, 	't'},
  {"no-data",  		no_argument,    0, 	'd'},
  {"opt",   		no_argument,    0, 	OPT_OPTIMIZE},
  {"password",  	optional_argument, 	0, 'p'},
#ifdef __WIN__
  {"pipe",		no_argument,	   	0, 'W'},
#endif
  {"port",    		required_argument,	0, 'P'},
  {"quick",    		no_argument,		0, 'q'},
  {"quote-names",	no_argument,		0, 'Q'},
  {"result-file",       required_argument,      0, 'r'},
  {"set-variable",	required_argument,	0, 'O'},
  {"socket",   		required_argument,	0, 'S'},
#include "sslopt-longopts.h"
  {"tab",    		required_argument,	0, 'T'},
  {"tables",            no_argument,            0, OPT_TABLES},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",    		required_argument,	0, 'u'},
#endif
  {"verbose",    	no_argument,		0, 'v'},
  {"version",    	no_argument,    	0, 'V'},
  {"where",		required_argument, 	0, 'w'},
  {0, 0, 0, 0}
};

static const char *load_default_groups[]= { "mysqldump","client",0 };

CHANGEABLE_VAR md_changeable_vars[] = {
  { "max_allowed_packet", (long*) &max_allowed_packet,24*1024*1024,4096,
    24*1024L*1024L,MALLOC_OVERHEAD,1024},
  { "net_buffer_length", (long*) &net_buffer_length,1024*1024L-1025,4096,
    24*1024L*1024L,MALLOC_OVERHEAD,1024},
  { 0, 0, 0, 0, 0, 0, 0}
};

static void safe_exit(int error);
static void write_heder(FILE *sql_file, char *db_name);
static void print_value(FILE *file, MYSQL_RES  *result, MYSQL_ROW row,
			const char *prefix,const char *name,
			int string_value);
static int dump_selected_tables(char *db, char **table_names, int tables);
static int dump_all_tables_in_db(char *db);
static int init_dumping(char *);
static int dump_databases(char **);
static int dump_all_databases();
static char *quote_name(char *name, char *buff);

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,DUMP_VERSION,
   MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
} /* print_version */


static void usage(void)
{
  uint i;
  print_version();
  puts("By Igor Romanenko, Monty, Jani & Sinisa");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Dumping definition and data mysql database or table");
  printf("Usage: %s [OPTIONS] database [tables]\n", my_progname);
  printf("OR     %s [OPTIONS] --databases [OPTIONS] DB1 [DB2 DB3...]\n",
	 my_progname);
  printf("OR     %s [OPTIONS] --all-databases [OPTIONS]\n", my_progname);
  printf("\n\
  -A, --all-databases   Dump all the databases. This will be same as\n\
		        --databases with all databases selected.\n\
  -a, --all		Include all MySQL specific create options.\n\
  -#, --debug=...       Output debug log. Often this is 'd:t:o,filename`.\n\
  --character-sets-dir=...\n\
                        Directory where character sets are\n\
  -?, --help		Display this help message and exit.\n\
  -B, --databases       To dump several databases. Note the difference in\n\
			usage; In this case no tables are given. All name\n\
			arguments are regarded as databasenames.\n\
			'USE db_name;' will be included in the output\n\
  -c, --complete-insert Use complete insert statements.\n\
  -C, --compress        Use compression in server/client protocol.\n\
  --default-character-set=...\n\
                        Set the default character set\n\
  -e, --extended-insert Allows utilization of the new, much faster\n\
                        INSERT syntax.\n\
  --add-drop-table	Add a 'drop table' before each create.\n\
  --add-locks		Add locks around insert statements.\n\
  --allow-keywords	Allow creation of column names that are keywords.\n\
  --delayed-insert      Insert rows with INSERT DELAYED.\n\
  -F, --flush-logs	Flush logs file in server before starting dump.\n\
  -f, --force		Continue even if we get an sql-error.\n\
  -h, --host=...	Connect to host.\n");
puts("\
  -l, --lock-tables     Lock all tables for read.\n\
  -n, --no-create-db    'CREATE DATABASE /*!32312 IF NOT EXISTS*/ db_name;'\n\
                        will not be put in the output. The above line will\n\
                        be added otherwise, if --databases or\n\
                        --all-databases option was given.\n\
  -t, --no-create-info	Don't write table creation info.\n\
  -d, --no-data		No row information.\n\
  -O, --set-variable var=option\n\
                        give a variable a value. --help lists variables\n\
  --opt			Same as --add-drop-table --add-locks --all\n\
                        --extended-insert --quick --lock-tables\n\
  -p, --password[=...]	Password to use when connecting to server.\n\
                        If password is not given it's solicited on the tty.\n");
#ifdef __WIN__
  puts("-W, --pipe		Use named pipes to connect to server");
#endif
  printf("\
  -P, --port=...	Port number to use for connection.\n\
  -q, --quick		Don't buffer query, dump directly to stdout.\n\
  -Q, --quote-names	Quote table and column names with `\n\
  -r, --result-file=... Direct output to a given file. This option should be\n\
                        used in MSDOS, because it prevents new line '\\n'\n\
                        from being converted to '\\n\\r' (newline + carriage\n\
                        return).\n\
  -S, --socket=...	Socket file to use for connection.\n\
  --tables              Overrides option --databases (-B).\n");
#include "sslopt-usage.h"
  printf("\
  -T, --tab=...         Creates tab separated textfile for each table to\n\
                        given path. (creates .sql and .txt files).\n\
                        NOTE: This only works if mysqldump is run on\n\
                              the same machine as the mysqld daemon.\n");
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user.\n");
#endif
  printf("\
  -v, --verbose		Print info about the various stages.\n\
  -V, --version		Output version information and exit.\n\
  -w, --where=		dump only selected records; QUOTES mandatory!\n\
  EXAMPLES: \"--where=user=\'jimf\'\" \"-wuserid>1\" \"-wuserid<1\"\n\
  Use -T (--tab=...) with --fields-...\n\
  --fields-terminated-by=...\n\
                        Fields in the textfile are terminated by ...\n\
  --fields-enclosed-by=...\n\
                        Fields in the importfile are enclosed by ...\n\
  --fields-optionally-enclosed-by=...\n\
                        Fields in the i.file are opt. enclosed by ...\n\
  --fields-escaped-by=...\n\
                        Fields in the i.file are escaped by ...\n\
  --lines-terminated-by=...\n\
                        Lines in the i.file are terminated by ...\n\
");
  print_defaults("my",load_default_groups);

  printf("\nPossible variables for option --set-variable (-O) are:\n");
  for (i=0 ; md_changeable_vars[i].name ; i++)
    printf("%-20s  current value: %lu\n",
     md_changeable_vars[i].name,
     (ulong) *md_changeable_vars[i].varptr);
} /* usage */


static void write_heder(FILE *sql_file, char *db_name)
{
  fprintf(sql_file, "# MySQL dump %s\n#\n", DUMP_VERSION);
  fprintf(sql_file, "# Host: %s    Database: %s\n",
	  current_host ? current_host : "localhost", db_name ? db_name : "");
  fputs("#--------------------------------------------------------\n",
  sql_file);
  fprintf(sql_file, "# Server version\t%s\n",
	  mysql_get_server_info(&mysql_connection));
  return;
} /* write_heder */


static int get_options(int *argc,char ***argv)
{
  int c,option_index;
  my_bool tty_password=0;

  md_result_file=stdout;
  load_defaults("my",load_default_groups,argc,argv);
  set_all_changeable_vars(md_changeable_vars);
  while ((c=getopt_long(*argc,*argv,
			"#::p::h:u:O:P:r:S:T:EBaAcCdefFlnqtvVw:?Ix",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
    case 'a':
      create_options=1;
      break;
    case 'e':
      extended_insert=1;
      break;
    case 'A':
      opt_alldbs=1;
      break;
    case MD_OPT_DEFAULT_CHARSET:
      default_charset= optarg;
      break;
    case MD_OPT_CHARSETS_DIR:
      charsets_dir= optarg;
      break;
    case 'f':
      ignore_errors=1;
      break;
    case 'F':
      flush_logs=1;
      break;
    case 'h':
      my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
      current_host=my_strdup(optarg,MYF(MY_WME));
      break;
    case 'n':
      opt_create_db = 1;
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      current_user=optarg;
      break;
#endif
    case 'O':
      if (set_changeable_var(optarg, md_changeable_vars))
      {
	usage();
	return(1);
      }
      break;
    case 'p':
      if (optarg)
      {
	char *start=optarg;
	my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
	opt_password=my_strdup(optarg,MYF(MY_FAE));
	while (*optarg) *optarg++= 'x';		/* Destroy argument */
	if (*start)
	  start[1]=0;				/* Cut length of argument */
      }
      else
	tty_password=1;
      break;
    case 'P':
      opt_mysql_port= (unsigned int) atoi(optarg);
      break;
    case 'r':
      if (!(md_result_file = my_fopen(optarg, O_WRONLY | O_BINARY,
				   MYF(MY_WME))))
	exit(1);
      break;
    case 'S':
      opt_mysql_unix_port= optarg;
      break;
    case 'W':
#ifdef __WIN__
      opt_mysql_unix_port=MYSQL_NAMEDPIPE;
#endif
      break;
    case 'T':
      path= optarg;
      break;
    case 'B':
      opt_databases = 1;
      break;
    case '#':
      DBUG_PUSH(optarg ? optarg : "d:t:o");
      break;
    case 'c': cFlag=1; break;
    case 'C':
      opt_compress=1;
      break;
    case 'd': dFlag=1; break;
    case 'l': lock_tables=1; break;
    case 'q': quick=1; break;
    case 'Q': opt_quoted=1; break;
    case 't': tFlag=1;  break;
    case 'v': verbose=1; break;
    case 'V': print_version(); exit(0);
    case 'w':
      where=optarg;
      break;
    case 'x':
      opt_first_slave=1;
      break;
    default:
      fprintf(stderr,"%s: Illegal option character '%c'\n",my_progname,opterr);
      /* Fall throught */
    case 'I':
    case '?':
      usage();
      exit(0);
    case (int) OPT_FTB:
      fields_terminated= optarg;
      break;
    case (int) OPT_LTB:
      lines_terminated= optarg;
      break;
    case (int) OPT_ENC:
      enclosed= optarg;
      break;
    case (int) OPT_O_ENC:
      opt_enclosed= optarg;
      break;
    case (int) OPT_ESC:
      escaped= optarg;
      break;
    case (int) OPT_DROP:
      opt_drop=1;
      break;
    case (int) OPT_KEYWORDS:
      opt_keywords=1;
      break;
    case (int) OPT_LOCKS:
      opt_lock=1;
      break;
    case (int) OPT_OPTIMIZE:
      extended_insert=opt_drop=opt_lock=lock_tables=quick=create_options=1;
      break;
    case (int) OPT_DELAYED:
      opt_delayed=1;
      break;
    case (int) OPT_TABLES:
      opt_databases=0;
      break;
#include "sslopt-case.h"
    }
  }
  if (opt_delayed)
    opt_lock=0;				/* Can't have lock with delayed */
  if (!path && (enclosed || opt_enclosed || escaped || lines_terminated ||
		fields_terminated))
  {
    fprintf(stderr, "%s: You must use option --tab with --fields-...\n", my_progname);
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
  (*argc)-=optind;
  (*argv)+=optind;
  if ((*argc < 1 && !opt_alldbs) || (*argc > 0 && opt_alldbs))
  {
    usage();
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
    fprintf(stderr, "# Connecting to %s...\n", host ? host : "localhost");
  }
  mysql_init(&mysql_connection);
  if (opt_compress)
    mysql_options(&mysql_connection,MYSQL_OPT_COMPRESS,NullS);
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
    mysql_ssl_set(&mysql_connection, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath);
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
    fprintf(stderr, "# Disconnecting from %s...\n", host ? host : "localhost");
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
  mysql_real_escape_string(&mysql_connection,tmp, pos, length);
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

static char *quote_name(char *name, char *buff)
{
  char *end;
  if (!opt_quoted && !test_if_special_chars(name))
    return name;
  buff[0]=QUOTE_CHAR;
  end=strmov(buff+1,name);
  end[0]=QUOTE_CHAR;
  end[1]=0;
  return buff;
} /* quote_name */


/*
** getStructure -- retrievs database structure, prints out corresponding
**       CREATE statement and fills out insert_pat.
** Return values:  number of fields in table, 0 if error
*/
static uint getTableStructure(char *table, char* db)
{
  MYSQL_RES  *tableRes;
  MYSQL_ROW  row;
  my_bool    init=0;
  uint       numFields;
  char 	     *strpos, *table_name;
  const char *delayed;
  char 	     name_buff[NAME_LEN+3],table_buff[NAME_LEN+3];
  FILE       *sql_file = md_result_file;
  DBUG_ENTER("getTableStructure");

  delayed= opt_delayed ? " DELAYED " : "";

  if (verbose)
    fprintf(stderr, "# Retrieving table structure for table %s...\n", table);

  sprintf(insert_pat,"SET OPTION SQL_QUOTE_SHOW_CREATE=%d", opt_quoted);
  table_name=quote_name(table,table_buff);
  if (!mysql_query(sock,insert_pat))
  {
    /* using SHOW CREATE statement */
    if (!tFlag)
    {
      /* Make an sql-file, if path was given iow. option -T was given */
      char buff[20+FN_REFLEN];

      sprintf(buff,"show create table %s",table_name);
      if (mysql_query(sock, buff))
      {
        fprintf(stderr, "%s: Can't get CREATE TABLE for table '%s' (%s)\n",
  		      my_progname, table, mysql_error(sock));
        safe_exit(EX_MYSQLERR);
        DBUG_RETURN(0);
      }

      if (path)
      {
        char filename[FN_REFLEN], tmp_path[FN_REFLEN];
        strmov(tmp_path,path);
        convert_dirname(tmp_path);
        sql_file= my_fopen(fn_format(filename, table, tmp_path, ".sql", 4),
  				 O_WRONLY, MYF(MY_WME));
        if (!sql_file)			/* If file couldn't be opened */
        {
	  safe_exit(EX_MYSQLERR);
	  DBUG_RETURN(0);
        }
        write_heder(sql_file, db);
      }
      fprintf(sql_file, "\n#\n# Table structure for table '%s'\n#\n\n", table);
      if (opt_drop)
        fprintf(sql_file, "DROP TABLE IF EXISTS %s;\n",table_name);

      tableRes=mysql_store_result(sock);
      row=mysql_fetch_row(tableRes);
      fprintf(sql_file, "%s;\n", row[1]);
      mysql_free_result(tableRes);
    }
    sprintf(insert_pat,"show fields from %s",table_name);
    if (mysql_query(sock,insert_pat) || !(tableRes=mysql_store_result(sock)))
    {
      fprintf(stderr, "%s: Can't get info about table: '%s'\nerror: %s\n",
  		    my_progname, table, mysql_error(sock));
      safe_exit(EX_MYSQLERR);
      DBUG_RETURN(0);
    }

    if (cFlag)
      sprintf(insert_pat, "INSERT %sINTO %s (", delayed, table_name);
    else
    {
      sprintf(insert_pat, "INSERT %sINTO %s VALUES ", delayed, table_name);
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
        strpos=strmov(strpos,quote_name(row[SHOW_FIELDNAME],name_buff));
    }
    numFields = (uint) mysql_num_rows(tableRes);
    mysql_free_result(tableRes);
  }
  else
  {
  /*  fprintf(stderr, "%s: Can't set SQL_QUOTE_SHOW_CREATE option (%s)\n",
      my_progname, mysql_error(sock)); */

    sprintf(insert_pat,"show fields from %s",table_name);
    if (mysql_query(sock,insert_pat) || !(tableRes=mysql_store_result(sock)))
    {
      fprintf(stderr, "%s: Can't get info about table: '%s'\nerror: %s\n",
		    my_progname, table, mysql_error(sock));
      safe_exit(EX_MYSQLERR);
      DBUG_RETURN(0);
    }

    /* Make an sql-file, if path was given iow. option -T was given */
    if (!tFlag)
    {
      if (path)
      {
        char filename[FN_REFLEN], tmp_path[FN_REFLEN];
        strmov(tmp_path,path);
        convert_dirname(tmp_path);
        sql_file= my_fopen(fn_format(filename, table, tmp_path, ".sql", 4),
				 O_WRONLY, MYF(MY_WME));
        if (!sql_file)			/* If file couldn't be opened */
        {
		safe_exit(EX_MYSQLERR);
		DBUG_RETURN(0);
        }
        write_heder(sql_file, db);
      }
      fprintf(sql_file, "\n#\n# Table structure for table '%s'\n#\n\n", table);
      if (opt_drop)
        fprintf(sql_file, "DROP TABLE IF EXISTS %s;\n",table_name);
      fprintf(sql_file, "CREATE TABLE %s (\n", table_name);
    }
    if (cFlag)
      sprintf(insert_pat, "INSERT %sINTO %s (", delayed, table_name);
    else
    {
      sprintf(insert_pat, "INSERT %sINTO %s VALUES ", delayed, table_name);
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
        strpos=strmov(strpos,quote_name(row[SHOW_FIELDNAME],name_buff));
      if (!tFlag)
      {
        if (opt_keywords)
	  fprintf(sql_file, "  %s.%s %s", table_name,
		  quote_name(row[SHOW_FIELDNAME],name_buff), row[SHOW_TYPE]);
        else
	  fprintf(sql_file, "  %s %s", quote_name(row[SHOW_FIELDNAME],
						  name_buff), row[SHOW_TYPE]);
        if (row[SHOW_DEFAULT])
        {
	  fputs(" DEFAULT ", sql_file);
	  unescape(sql_file,row[SHOW_DEFAULT],lengths[SHOW_DEFAULT]);
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
      sprintf(buff,"show keys from %s",table_name);
      if (mysql_query(sock, buff))
      {
        fprintf(stderr, "%s: Can't get keys for table '%s' (%s)\n",
		my_progname, table, mysql_error(sock));
        if (sql_file != stdout)
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
	    fprintf(sql_file, ",\n  KEY %s (",quote_name(row[2],name_buff));
	  else if (keynr == primary_key)
	    fputs(",\n  PRIMARY KEY (",sql_file); /* First UNIQUE is primary */
	  else
	    fprintf(sql_file, ",\n  UNIQUE %s (",quote_name(row[2],name_buff));
        }
        else
	  putc(',', sql_file);
        fputs(quote_name(row[4],name_buff), sql_file);
        if (row[7])
	  fprintf(sql_file, " (%s)",row[7]);      /* Sub key */
      }
      if (keynr)
        putc(')', sql_file);
      fputs("\n)",sql_file);

      /* Get MySQL specific create options */
      if (create_options)
      {
        sprintf(buff,"show table status like '%s'",table);
        if (mysql_query(sock, buff))
        {
	  if (mysql_errno(sock) != ER_PARSE_ERROR)
	  {					/* If old MySQL version */
	    if (verbose)
	      fprintf(stderr,
		      "# Warning: Couldn't get status information for table '%s' (%s)\n",
		      table,mysql_error(sock));
	  }
        }
        else if (!(tableRes=mysql_store_result(sock)) ||
		 !(row=mysql_fetch_row(tableRes)))
        {
	  fprintf(stderr,
		  "Error: Couldn't read status information for table '%s' (%s)\n",
		  table,mysql_error(sock));
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


/*
** dumpTable saves database contents as a series of INSERT statements.
*/
static void dumpTable(uint numFields, char *table)
{
  char query[1024], *end, buff[256],table_buff[NAME_LEN+3];
  MYSQL_RES	*res;
  MYSQL_FIELD  *field;
  MYSQL_ROW    row;
  ulong		rownr, row_break, total_length, init_length;

  if (verbose)
    fprintf(stderr, "# Sending SELECT query...\n");
  if (path)
  {
    char filename[FN_REFLEN], tmp_path[FN_REFLEN];
    strmov(tmp_path, path);
    convert_dirname(tmp_path);
    my_load_path(tmp_path, tmp_path, NULL);
    fn_format(filename, table, tmp_path, ".txt", 4);
    my_delete(filename, MYF(0)); /* 'INTO OUTFILE' doesn't work, if
				    filename wasn't deleted */
    to_unix_path(filename);
    sprintf(query, "SELECT * INTO OUTFILE '%s'", filename);
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

    sprintf(buff," FROM %s",table);
    end= strmov(end,buff);
    if (where)
      end= strxmov(end, " WHERE ",where,NullS);
    if (mysql_query(sock, query))
    {
      DBerror(sock, "when executing 'SELECT INTO OUTFILE'");
      return;
    }
  }
  else
  {
    fprintf(md_result_file,"\n#\n# Dumping data for table '%s'\n", table);
    sprintf(query, "SELECT * FROM %s", quote_name(table,table_buff));
    if (where)
    {
      fprintf(md_result_file,"# WHERE:  %s\n",where);
      strxmov(strend(query), " WHERE ",where,NullS);
    }
    fputs("#\n\n", md_result_file);

    if (mysql_query(sock, query))
    {
      DBerror(sock, "when retrieving data from server");
      return;
    }
    if (quick)
      res=mysql_use_result(sock);
    else
      res=mysql_store_result(sock);
    if (!res)
    {
      DBerror(sock, "when retrieving data from server");
      return;
    }
    if (verbose)
      fprintf(stderr, "# Retrieving rows...\n");
    if (mysql_num_fields(res) != numFields)
    {
      fprintf(stderr,"%s: Error in field count for table: '%s' !  Aborting.\n",
	      my_progname,table);
      safe_exit(EX_CONSCHECK);
      return;
    }

    if (opt_lock)
      fprintf(md_result_file,"LOCK TABLES %s WRITE;\n",
	      quote_name(table,table_buff));

    total_length=net_buffer_length;		/* Force row break */
    row_break=0;
    rownr=0;
    init_length=(uint) strlen(insert_pat)+4;

    while ((row=mysql_fetch_row(res)))
    {
      uint i;
      ulong *lengths=mysql_fetch_lengths(res);
      rownr++;
      if (!extended_insert)
	fputs(insert_pat,md_result_file);
      mysql_field_seek(res,0);

      for (i = 0; i < mysql_num_fields(res); i++)
      {
	if (!(field = mysql_fetch_field(res)))
	{
	  sprintf(query,"%s: Not enough fields from table '%s'! Aborting.\n",
		  my_progname,table);
	  fputs(query,stderr);
	  safe_exit(EX_CONSCHECK);
	  return;
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
		  safe_exit(EX_EOM);
		}
		dynstr_append(&extended_row,"\'");
		extended_row.length +=
		  mysql_real_escape_string(&mysql_connection,
					   &extended_row.str[extended_row.length],row[i],length);
		extended_row.str[extended_row.length]='\0';
		dynstr_append(&extended_row,"\'");
	      }
	      else
		dynstr_append(&extended_row,row[i]);
	    }
	    else
	      dynstr_append(&extended_row,"\'\'");
	  }
	  else if (dynstr_append(&extended_row,"NULL"))
	  {
	    fputs("Aborting dump (out of memory)",stderr);
	    safe_exit(EX_EOM);
	  }
	}
	else
	{
	  if (i)
	    fputc(',',md_result_file);
	  if (row[i])
	  {
	    if (!IS_NUM_FIELD(field))
	      unescape(md_result_file, row[i], lengths[i]);
	    else
	      fputs(row[i],md_result_file);
	  }
	  else
	  {
	    fputs("NULL",md_result_file);
	  }
	}
      }

      if (extended_insert)
      {
	ulong row_length;
	dynstr_append(&extended_row,")");
        row_length = 2 + extended_row.length;
        if (total_length + row_length < net_buffer_length)
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
      }
      else
	fputs(");\n", md_result_file);
    }
    if (extended_insert && row_break)
      fputs(";\n", md_result_file);		/* If not empty table */
    fflush(md_result_file);
    if (mysql_errno(sock))
    {
      sprintf(query,"%s: Error %d: %s when dumping table '%s' at row: %ld\n",
	      my_progname,
	      mysql_errno(sock),
	      mysql_error(sock),
	      table,
	      rownr);
      fputs(query,stderr);
      safe_exit(EX_CONSCHECK);
      return;
    }
    if (opt_lock)
      fputs("UNLOCK TABLES;\n", md_result_file);
    mysql_free_result(res);
  }
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
  if (!path)
  {
    if (opt_databases || opt_alldbs)
    {
      fprintf(md_result_file,"\n#\n# Current Database: %s\n#\n", database);
      if (!opt_create_db)
	fprintf(md_result_file,"\nCREATE DATABASE /*!32312 IF NOT EXISTS*/ %s;\n",
		database);
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

  if (init_dumping(database))
    return 1;
  if (lock_tables)
  {
    DYNAMIC_STRING query;
    init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
    for (numrows=0 ; (table = getTableName(1)) ; numrows++)
    {
      dynstr_append(&query, table);
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
  if (lock_tables)
    mysql_query(sock,"UNLOCK_TABLES");
  return 0;
} /* dump_all_tables_in_db */



static int dump_selected_tables(char *db, char **table_names, int tables)
{
  uint numrows;

  if (init_dumping(db))
    return 1;
  if (lock_tables)
  {
    DYNAMIC_STRING query;
    int i;

    init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
    for (i=0 ; i < tables ; i++)
    {
      dynstr_append(&query, table_names[i]);
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
  for (; tables > 0 ; tables-- , table_names++)
  {
    numrows = getTableStructure(*table_names, db);
    if (!dFlag && numrows > 0)
      dumpTable(numrows, *table_names);
  }
  if (lock_tables)
    mysql_query(sock,"UNLOCK_TABLES");
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
  if (!path)
    write_heder(md_result_file, *argv);

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
  if (opt_alldbs)
    dump_all_databases();
  /* Only one database and selected table(s) */
  else if (argc > 1 && !opt_databases)
    dump_selected_tables(*argv, (argv + 1), (argc - 1));
  /* One or more databases, all tables */
  else
    dump_databases(argv);

  if (opt_first_slave)
  {
    if (mysql_query(sock, "FLUSH MASTER"))
    {
      my_printf_error(0, "Error: Couldn't execute 'FLUSH MASTER': %s",
		      MYF(0), mysql_error(sock));
    }
    if (mysql_query(sock, "UNLOCK TABLES"))
    {
      my_printf_error(0, "Error: Couldn't execute 'UNLOCK TABLES': %s",
		      MYF(0), mysql_error(sock));
   }
  }
  dbDisconnect(current_host);
  fputs("\n", md_result_file);
  if (md_result_file != stdout)
    my_fclose(md_result_file, MYF(0));
  my_free(opt_password, MYF(MY_ALLOW_ZERO_PTR));
  if (extended_insert)
    dynstr_free(&extended_row);
  my_end(0);
  return(first_error);
} /* main */
