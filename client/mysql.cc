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

/* mysql command tool
 * Commands compatible with mSQL by David J. Hughes
 *
 * Written by:
 *   Michael 'Monty' Widenius
 *   Andi Gutmans  <andi@zend.com>
 *   Zeev Suraski <zeev@zend.com>
 *   Jani Tolonen <jani@mysql.com>
 *
 **/

#include <global.h>
#include <my_sys.h> 
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "errmsg.h"
#include <my_dir.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__		      // Skip warnings in getopt.h
#endif
#include <getopt.h>
#include "my_readline.h"
#include <signal.h>

const char *VER="11.12";

gptr sql_alloc(unsigned size);	     // Don't use mysqld alloc for these
void sql_element_free(void *ptr);
#include "sql_string.h"

extern "C" {
#if defined(HAVE_CURSES_H) && defined(HAVE_TERM_H)
#include <curses.h>
#include <term.h>
#else
#if defined(HAVE_TERMIOS_H)
#include <termios.h>
#include <unistd.h>
#elif defined(HAVE_TERMBITS_H)
#include <termbits.h>
#elif defined(HAVE_ASM_TERMBITS_H) && (!defined __GLIBC__ || !(__GLIBC__ > 2 || __GLIBC__ == 2 && __GLIBC_MINOR__ > 0))
#include <asm/termbits.h>		// Standard linux
#endif
#undef VOID
#if defined(HAVE_TERMCAP_H)
#include <termcap.h>
#else
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#undef SYSV				// hack to avoid syntax error
#ifdef HAVE_TERM_H
#include <term.h>
#endif
#endif
#endif

#undef bcmp				// Fix problem with new readline
#undef bzero
#ifdef __WIN__
#include <conio.h>
#else
#include <readline/readline.h>
#define HAVE_READLINE
#endif
  //int vidattr(long unsigned int attrs);	// Was missing in sun curses
}

#if !defined(HAVE_VIDATTR)
#undef vidattr
#define vidattr(A) {}			// Can't get this to work
#endif

#ifdef __WIN__
#define cmp_database(A,B) my_strcasecmp((A),(B))
#else
#define cmp_database(A,B) strcmp((A),(B))
#endif

#include "completion_hash.h"

typedef struct st_status
{
  int exit_status;
  ulong query_start_line;
  char *file_name;
  LINE_BUFFER *line_buff;
  bool batch,add_to_history;
} STATUS;


static HashTable ht;

enum enum_info_type { INFO_INFO,INFO_ERROR,INFO_RESULT};
typedef enum enum_info_type INFO_TYPE;

static MYSQL mysql;			/* The connection */
static bool info_flag=0,ignore_errors=0,wait_flag=0,quick=0,
	    connected=0,opt_raw_data=0,unbuffered=0,output_tables=0,
	    no_rehash=0,skip_updates=0,safe_updates=0,one_database=0,
	    opt_compress=0,
	    vertical=0,skip_line_numbers=0,skip_column_names=0,opt_html=0,
	    opt_nopager=1, opt_outfile=0, no_named_cmds=1;
static uint verbose=0,opt_silent=0,opt_mysql_port=0;
static my_string opt_mysql_unix_port=0;
static int connect_flag=CLIENT_INTERACTIVE;
static char *current_host,*current_db,*current_user=0,*opt_password=0,
            *default_charset;
static char *histfile;
static String glob_buffer,old_buffer;
static STATUS status;
static ulong select_limit,max_join_size,opt_connect_timeout=0;
static char default_pager[FN_REFLEN];
char pager[FN_REFLEN], outfile[FN_REFLEN];
FILE *PAGER, *OUTFILE;

#include "sslopt-vars.h"

#ifndef DBUG_OFF
const char *default_dbug_option="d:t:o,/tmp/mysql.trace";
#endif

void tee_fprintf(FILE *file, const char *fmt, ...);
void tee_fputs(const char *s, FILE *file);
void tee_puts(const char *s, FILE *file);
void tee_putc(int c, FILE *file);
/* The names of functions that actually do the manipulation. */
static int get_options(int argc,char **argv);
static int com_quit(String *str,char*),
	   com_go(String *str,char*), com_ego(String *str,char*),
	   com_print(String *str,char*),
	   com_help(String *str,char*), com_clear(String *str,char*),
	   com_connect(String *str,char*), com_status(String *str,char*),
	   com_use(String *str,char*), com_source(String *str, char*),
	   com_rehash(String *str, char*), com_tee(String *str, char*),
           com_notee(String *str, char*);

#ifndef __WIN__
static int com_nopager(String *str, char*), com_pager(String *str, char*),
	   com_edit(String *str,char*);
#endif

static int read_lines(bool execute_commands);
static int sql_connect(char *host,char *database,char *user,char *password,
		       uint silent);
static int put_info(const char *str,INFO_TYPE info,uint error=0);
static void safe_put_field(const char *pos,ulong length);
static void init_pager();
static void end_pager();
static void init_tee();
static void end_tee();

/* A structure which contains information on the commands this program
   can understand. */

typedef struct {
  const char *name;		/* User printable name of the function. */
  char cmd_char;		/* msql command character */
  int (*func)(String *str,char *); /* Function to call to do the job. */
  bool takes_params;		/* Max parameters for command */
  const char *doc;		/* Documentation for this function.  */
} COMMANDS;

static COMMANDS commands[] = {
  { "help",   'h', com_help,   0, "Display this help." },
  { "?",      '?', com_help,   0, "Synonym for `help'." },
  { "clear",  'c', com_clear,  0, "Clear command."},
  { "connect",'r', com_connect,1,
    "Reconnect to the server. Optional arguments are db and host." },
#ifndef __WIN__
  { "edit",   'e', com_edit,   0, "Edit command with $EDITOR."},
#endif
  { "ego",    'G', com_ego,    0,
    "Send command to mysql server, display result vertically."},
  { "exit",   'q', com_quit,   0, "Exit mysql. Same as quit."},
  { "go",     'g', com_go,     0, "Send command to mysql server." },
#ifndef __WIN__
  { "nopager",'n', com_nopager,0, "Disable pager, print to stdout." },
#endif
  { "notee",  't', com_notee,  0, "Don't write into outfile." },
#ifndef __WIN__
  { "pager",  'P', com_pager,  1, 
    "Set PAGER [to_pager]. Print the query results via PAGER." },
#endif
  { "print",  'p', com_print,  0, "Print current command." },
  { "quit",   'q', com_quit,   0, "Quit mysql." },
  { "rehash", '#', com_rehash, 0, "Rebuild completion hash." },
  { "source", '.', com_source, 1,
    "Execute a SQL script file. Takes a file name as an argument."},
  { "status", 's', com_status, 0, "Get status information from the server."},
  { "tee",    'T', com_tee,    1, 
    "Set outfile [to_outfile]. Append everything into given outfile." },
  { "use",    'u', com_use,    1,
    "Use another database. Takes database name as argument." },

  /* Get bash-like expansion for some commands */
  { "create table",     0, 0, 0, ""},
  { "create database",  0, 0, 0, ""},
  { "drop",             0, 0, 0, ""},
  { "select",           0, 0, 0, ""},
  { "insert",           0, 0, 0, ""},
  { "replace",          0, 0, 0, ""},
  { "update",           0, 0, 0, ""},
  { "delete",           0, 0, 0, ""},
  { "explain",          0, 0, 0, ""},
  { "show databases",   0, 0, 0, ""},
  { "show fields from", 0, 0, 0, ""},
  { "show keys from",   0, 0, 0, ""},
  { "show tables",      0, 0, 0, ""},
  { "load data from",   0, 0, 0, ""},
  { "alter table",      0, 0, 0, ""},
  { "set option",       0, 0, 0, ""},
  { "lock tables",      0, 0, 0, ""},
  { "unlock tables",    0, 0, 0, ""},
  { (char *)NULL,       0, 0, 0, ""}
};

static const char *load_default_groups[]= { "mysql","client",0 };

#ifdef HAVE_READLINE
extern "C" void add_history(char *command); /* From readline directory */
extern "C" int read_history(char *command);
extern "C" int write_history(char *command);
static void initialize_readline (char *name);
#endif

static COMMANDS *find_command (char *name,char cmd_name);
static bool add_line(String &buffer,char *line,char *in_string);
static void remove_cntrl(String &buffer);
static void print_table_data(MYSQL_RES *result);
static void print_table_data_html(MYSQL_RES *result);
static void print_tab_data(MYSQL_RES *result);
static void print_table_data_vertically(MYSQL_RES *result);
static ulong start_timer(void);
static void end_timer(ulong start_time,char *buff);
static void mysql_end_timer(ulong start_time,char *buff);
static void nice_time(double sec,char *buff,bool part_second);
static sig_handler mysql_end(int sig);


int main(int argc,char *argv[])
{
  char buff[80];

  MY_INIT(argv[0]);
  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);

  strmov(outfile, "\0");   // no (default) outfile, unless given at least once
  strmov(pager, "stdout"); // the default, if --pager wasn't given
  if (!isatty(0) || !isatty(1))
  {
    status.batch=1; opt_silent=1;
    ignore_errors=0;
  }
  else
    status.add_to_history=1;
  status.exit_status=1;
  load_defaults("my",load_default_groups,&argc,&argv);
  if (get_options(argc,(char **) argv))
  {
    my_end(0);
    exit(1);
  }
  free_defaults(argv);
  if (status.batch && !status.line_buff &&
      !(status.line_buff=batch_readline_init(max_allowed_packet+512,stdin)))
    exit(1);
  glob_buffer.realloc(512);
  completion_hash_init(&ht,50);
  if (sql_connect(current_host,current_db,current_user,opt_password,
		  opt_silent))
  {
    quick=1;					// Avoid history
    status.exit_status=1;
    mysql_end(-1);
  }
  if (!status.batch)
    ignore_errors=1;				// Don't abort monitor
  signal(SIGINT, mysql_end);			// Catch SIGINT to clean up

  /*
  **  Run in interactive mode like the ingres/postgres monitor
  */

  put_info("Welcome to the MySQL monitor.  Commands end with ; or \\g.",
	   INFO_INFO);
  sprintf((char*) glob_buffer.ptr(),
	  "Your MySQL connection id is %ld to server version: %s\n",
	  mysql_thread_id(&mysql),mysql_get_server_info(&mysql));
  put_info((char*) glob_buffer.ptr(),INFO_INFO);

#ifdef HAVE_READLINE
  initialize_readline(my_progname);
  if (!status.batch && !quick && !opt_html)
  {
    /*read-history from file, default ~/.mysql_history*/
    if (getenv("MYSQL_HISTFILE"))
      histfile=my_strdup(getenv("MYSQL_HISTFILE"),MYF(MY_WME));
    else if (getenv("HOME"))
    {
      histfile=(char*) my_malloc((uint) strlen(getenv("HOME"))
				 + (uint) strlen("/.mysql_history")+2,
				 MYF(MY_WME));
      if (histfile)
	sprintf(histfile,"%s/.mysql_history",getenv("HOME"));
    }
    if (histfile)
    {
      if (verbose)
	tee_fprintf(stdout, "Reading history-file %s\n",histfile);
      read_history(histfile);
    }
  }
#endif
  sprintf(buff, 
	  "Type 'help;' or '\\h' for help. Type '\\c' to clear the buffer\n");
  put_info(buff,INFO_INFO);
  status.exit_status=read_lines(1);		// read lines and execute them
  if (opt_outfile)
    end_tee();
  mysql_end(0);
#ifndef _lint
  DBUG_RETURN(0);				// Keep compiler happy
#endif
}

sig_handler mysql_end(int sig)
{
  if (connected)
    mysql_close(&mysql);
#ifdef HAVE_READLINE
  if (!status.batch && !quick && ! opt_html)
  {
    /* write-history */
    if (verbose)
      tee_fprintf(stdout, "Writing history-file %s\n",histfile);
    write_history(histfile);
  }
  batch_readline_end(status.line_buff);
  completion_hash_free(&ht);
#endif
  if (sig >= 0)
    put_info(sig ? "Aborted" : "Bye", INFO_RESULT);
  glob_buffer.free();
  old_buffer.free();
  my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
  my_free(opt_mysql_unix_port,MYF(MY_ALLOW_ZERO_PTR));
  my_free(histfile,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_user,MYF(MY_ALLOW_ZERO_PTR));
  my_end(info_flag ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  exit(status.exit_status);
}

enum options {OPT_CHARSETS_DIR=256, OPT_DEFAULT_CHARSET,
	      OPT_PAGER, OPT_NOPAGER, OPT_TEE, OPT_NOTEE} ;


static struct option long_options[] =
{
  {"i-am-a-dummy",  optional_argument,	   0, 'U'},
  {"batch",	    no_argument,	   0, 'B'},
  {"character-sets-dir",required_argument, 0, OPT_CHARSETS_DIR},
  {"compress",	    no_argument,	   0, 'C'},
#ifndef DBUG_OFF
  {"debug",	    optional_argument,	   0, '#'},
#endif
  {"database",	    required_argument,     0, 'D'},
  {"debug-info",    no_argument,	   0, 'T'},
  {"default-character-set", required_argument,0, OPT_DEFAULT_CHARSET},
  {"enable-named-commands", no_argument,   0, 'G'},
  {"execute",	    required_argument,	   0, 'e'},
  {"force",	    no_argument,	   0, 'f'},
  {"help",	    no_argument,	   0, '?'},
  {"html",	    no_argument,	   0, 'H'},
  {"host",	    required_argument,	   0, 'h'},
  {"ignore-spaces", no_argument,	   0, 'i'},
  {"no-auto-rehash",no_argument,	   0, 'A'},
  {"no-named-commands", no_argument,       0, 'g'},
  {"no-tee",        no_argument,           0, OPT_NOTEE},
#ifndef __WIN__
  {"no-pager",      no_argument,           0, OPT_NOPAGER},
  {"nopager",       no_argument,           0, OPT_NOPAGER},  /* we are kind */
  {"pager",         optional_argument,     0, OPT_PAGER},
#endif
  {"notee",         no_argument,           0, OPT_NOTEE},    /* we are kind */
  {"tee",           required_argument,     0, OPT_TEE},
  {"one-database",  no_argument,	   0, 'o'},
  {"password",	    optional_argument,	   0, 'p'},
#ifdef __WIN__
  {"pipe",	    no_argument,	   0, 'W'},
#endif
  {"port",	    required_argument,	   0, 'P'},
  {"quick",	    no_argument,	   0, 'q'},
  {"set-variable",  required_argument,	   0, 'O'},
  {"raw",	    no_argument,	   0, 'r'},
  {"safe-updates",  optional_argument,	   0, 'U'},
  {"silent",	    no_argument,	   0, 's'},
  {"skip-column-names",no_argument,	   0, 'N'},
  {"skip-line-numbers",no_argument,	   0, 'L'},
  {"socket",	    required_argument,	   0, 'S'},
#include "sslopt-longopts.h"
  {"table",	    no_argument,	   0, 't'},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",	    required_argument,	   0, 'u'},
#endif
  {"unbuffered",    no_argument,	   0, 'n'},
  {"verbose",	    no_argument,	   0, 'v'},
  {"version",	    no_argument,	   0, 'V'},
  {"vertical",	    no_argument,	   0, 'E'},
  {"wait",	    no_argument,	   0, 'w'},
  {0, 0, 0, 0}
};


CHANGEABLE_VAR changeable_vars[] = {
  { "connect_timeout", (long*) &opt_connect_timeout, 0, 0, 3600*12, 0, 1},
  { "max_allowed_packet", (long*) &max_allowed_packet,16*1024L*1024L,4096,
    24*1024L*1024L, MALLOC_OVERHEAD,1024},
  { "net_buffer_length",(long*) &net_buffer_length,16384,1024,24*1024*1024L,
    MALLOC_OVERHEAD,1024},
  { "select_limit", (long*) &select_limit, 1000L, 1, ~0L, 0, 1},
  { "max_join_size", (long*) &max_join_size, 1000000L, 1, ~0L, 0, 1},
  { 0, 0, 0, 0, 0, 0, 0}
};


static void usage(int version)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",
	 my_progname, VER, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  if (version)
    return;
  puts("Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB");
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  printf("Usage: %s [OPTIONS] [database]\n", my_progname);
  printf("\n\
  -?, --help		Display this help and exit.\n\
  -A, --no-auto-rehash  No automatic rehashing. One has to use 'rehash' to\n\
			get table and field completion. This gives a quicker\n\
			start of mysql and disables rehashing on reconnect.\n\
  -B, --batch		Print results with a tab as separator, each row on\n\
			a new line. Doesn't use history file.\n\
  --character-sets-dir=...\n\
                        Directory where character sets are located.\n\
  -C, --compress	Use compression in server/client protocol.\n");
#ifndef DBUG_OFF
  printf("\
  -#, --debug[=...]     Debug log. Default is '%s'.\n",default_dbug_option);
#endif
  printf("\
  -D, --database=..	Database to use.\n\
  --default-character-set=...\n\
                        Set the default character set.\n\
  -e, --execute=...     Execute command and quit. (Output like with --batch)\n\
  -E, --vertical        Print the output of a query (rows) vertically.\n\
  -f, --force           Continue even if we get an sql error.\n\
  -g, --no-named-commands\n\
			Named commands are disabled. Use \\* form only, or\n\
                        use named commands only in the beginning of a line\n\
                        ending with a semicolon (;) Since version 10.9 the\n\
                        client now starts with this option ENABLED by\n\
                        default! Disable with '-G'. Long format commands\n\
                        still work from the first line.\n\
  -G, --enable-named-commands\n\
                        Named commands are enabled. Opposite to -g.\n\
  -i, --ignore-space	Ignore space after function names.\n\
  -h, --host=...	Connect to host.\n\
  -H, --html		Produce HTML output.\n\
  -L, --skip-line-numbers\n\
                        Don't write line number for errors.\n");
#ifndef __WIN__
  printf("\
  --no-pager            Disable pager and print to stdout. See interactive\n\
                        help (\\h) also.\n");
#endif
  printf("\
  --no-tee              Disable outfile. See interactive help (\\h) also.\n\
  -n, --unbuffered	Flush buffer after each query.\n\
  -N, --skip-column-names\n\
                        Don't write column names in results.\n\
  -O, --set-variable var=option\n\
			Give a variable an value. --help lists variables.\n\
  -o, --one-database	Only update the default database. This is useful\n\
			for skipping updates to other database in the update\n\
			log.\n");
#ifndef __WIN__
  printf("\
  --pager[=...]         Pager to use to display results. If you don't supply\n\
                        an option the default pager is taken from your ENV\n\
                        variable PAGER (%s).\n\
                        Valid pagers are less, more, cat [> filename], etc.\n\
                        See interactive help (\\h) also. This option does\n\
                        not work in batch mode.\n", getenv("PAGER") ? getenv("PAGER") : "");
#endif
  printf("\
  -p[password], --password[=...]\n\
			Password to use when connecting to server\n\
			If password is not given it's asked from the tty.\n");
#ifdef __WIN__
  puts("  -W, --pipe		Use named pipes to connect to server");
#endif
  printf("\n\
  -P, --port=...	Port number to use for connection.\n\
  -q, --quick		Don't cache result, print it row by row. This may\n\
			slow down the server if the output is suspended.\n\
			Doesn't use history file.\n\
  -r, --raw		Write fields without conversion. Used with --batch\n\
  -s, --silent		Be more silent.\n\
  -S  --socket=...	Socket file to use for connection.\n");
#include "sslopt-usage.h"
  printf("\
  -t, --table		Output in table format.\n\
  -T, --debug-info	Print some debug info at exit.\n\
  --tee=...             Append everything into outfile. See interactive help\n\
                        (\\h) also. Does not work in batch mode.\n");
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user.\n");
#endif
  printf("\
  -U, --safe-updates[=#], --i-am-a-dummy[=#]\n\
		        Only allow UPDATE and DELETE that uses keys.\n\
  -v, --verbose		Write more. (-v -v -v gives the table output format)\n\
  -V, --version		Output version information and exit.\n\
  -w, --wait		Wait and retry if connection is down.\n");
  print_defaults("my",load_default_groups);

  printf("\nPossible variables for option --set-variable (-O) are:\n");
  for (uint i=0 ; changeable_vars[i].name ; i++)
    printf("%-20s  current value: %lu\n",
	   changeable_vars[i].name,
	   (ulong) *changeable_vars[i].varptr);
}


static int get_options(int argc, char **argv)
{
  int c,option_index=0;
  bool tty_password=0;

  set_all_changeable_vars(changeable_vars);
  while ((c=getopt_long(argc,argv,
			"?ABCD:LfgGHinNoqrstTU::vVwWEe:h:O:P:S:u:#::p::",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
    case OPT_DEFAULT_CHARSET:
      default_charset= optarg;
      break;
    case OPT_CHARSETS_DIR:
      charsets_dir= optarg;
      break;
    case OPT_TEE:
      if (!opt_outfile && strlen(optarg))
      {
	strmov(outfile, optarg);
	opt_outfile=1;
	init_tee();
      }
      break;
    case OPT_NOTEE:
      if (opt_outfile)
	end_tee();
      opt_outfile=0;
      break;
    case OPT_PAGER:
      opt_nopager=0;
      if (optarg)
	strmov(pager, optarg);
      else
	strmov(pager, (char*) getenv("PAGER"));
      strmov(default_pager, pager);
      break;
    case OPT_NOPAGER:
      opt_nopager=1;
      break;
    case 'D':
      my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));      
      current_db=my_strdup(optarg,MYF(MY_WME));
      break;
    case 'e':
      status.batch=1;
      status.add_to_history=0;
      batch_readline_end(status.line_buff);	// If multiple -e
      if (!(status.line_buff=batch_readline_command(optarg)))
	return 1;
      ignore_errors=0;
      break;
    case 'f':
      ignore_errors=1;
      break;
    case 'h':
      my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
      current_host=my_strdup(optarg,MYF(MY_WME));
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      my_free(current_user,MYF(MY_ALLOW_ZERO_PTR));
      current_user= my_strdup(optarg,MYF(MY_WME));
      break;
#endif
    case 'U':
      if (!optarg)
	safe_updates=1;
      else
	safe_updates=atoi(optarg) != 0;
      break;
    case 'o':
      one_database=skip_updates=1;
      break;
    case 'O':
      if (set_changeable_var(optarg, changeable_vars))
      {
	usage(0);
	return(1);
      }
      break;
    case 'p':
      if (optarg)
      {
	char *start=optarg;
	my_free(opt_password,MYF(MY_ALLOW_ZERO_PTR));
	opt_password=my_strdup(optarg,MYF(MY_FAE));
	while (*optarg) *optarg++= 'x';		// Destroy argument
	if (*start)
	  start[1]=0;
      }
      else
	tty_password=1;
      break;
    case 't':
      output_tables=1;
      break;
    case 'r':
      opt_raw_data=1;
      break;
    case '#':
      DBUG_PUSH(optarg ? optarg : default_dbug_option);
      info_flag=1;
      break;
    case 'q': quick=1; break;
    case 's': opt_silent++; break;
    case 'T': info_flag=1; break;
    case 'n': unbuffered=1; break;
    case 'v': verbose++; break;
    case 'E': vertical=1; break;
    case 'w': wait_flag=1; break;
    case 'A': no_rehash=1; break;
    case 'G': no_named_cmds=0; break;
    case 'g': no_named_cmds=1; break;
    case 'H': opt_html=1; break;
    case 'i': connect_flag|= CLIENT_IGNORE_SPACE; break;
    case 'B':
      if (!status.batch)
      {
	status.batch=1;
	status.add_to_history=0;
	opt_silent++;				// more silent
      }
      break;
    case 'C':
      opt_compress=1;
      break;
    case 'L':
      skip_line_numbers=1;
      break;
    case 'N':
      skip_column_names=1;
      break;
    case 'P':
      opt_mysql_port= (unsigned int) atoi(optarg);
      break;
    case 'S':
      my_free(opt_mysql_unix_port,MYF(MY_ALLOW_ZERO_PTR));
      opt_mysql_unix_port= my_strdup(optarg,MYF(0));
      break;
    case 'W':
#ifdef __WIN__
      opt_mysql_unix_port=my_strdup(MYSQL_NAMEDPIPE,MYF(0));
#endif
      break;
    case 'V': usage(1); exit(0);
    case 'I':
    case '?':
      usage(0);
      exit(0);
#include "sslopt-case.h"
    default:
      tee_fprintf(stderr,"illegal option: -%c\n",opterr);
      usage(0);
      exit(1);
    }
  }
  if (status.batch) /* disable pager and outfile in this case */
  {
    strmov(default_pager, "stdout");
    strmov(pager, "stdout");
    opt_nopager=1;
    opt_outfile=0;
  }
  if (default_charset)
  {
    if (set_default_charset_by_name(default_charset, MYF(MY_WME)))
      exit(1);
  }
  argc-=optind;
  argv+=optind;
  if (argc > 1)
  {
    usage(0);
    exit(1);
  }
  if (argc == 1)
  {
    my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));
    current_db= my_strdup(*argv,MYF(MY_WME));
  }
  if (!current_host)
  {	/* If we don't have a hostname have a look at MYSQL_HOST */
    char *tmp=(char *) getenv("MYSQL_HOST");
    if (tmp)
      current_host = my_strdup(tmp,MYF(MY_WME));
  }
  if (tty_password)
    opt_password=get_tty_password(NullS);
  return(0);
}


static int read_lines(bool execute_commands)
{
#ifdef __WIN__
  char linebuffer[254];
#endif
  char	*line;
  char	in_string=0;
  ulong line_number=0;
  COMMANDS *com;
  status.exit_status=1;

  for (;;)
  {
    if (status.batch || !execute_commands)
    {
      line=batch_readline(status.line_buff);
      line_number++;
      if (!glob_buffer.length())
	status.query_start_line=line_number;
    }
    else
    {
#ifdef __WIN__
      if (opt_outfile && glob_buffer.is_empty())
	fflush(OUTFILE);
      tee_fputs(glob_buffer.is_empty() ? "mysql> " :
		!in_string ? "    -> " :
		in_string == '\'' ?
		"    '> " : "    \"> ",stdout);
      linebuffer[0]=(char) sizeof(linebuffer);
      line=_cgets(linebuffer);
#else
      if (opt_outfile)
      {
	if (glob_buffer.is_empty())
	  fflush(OUTFILE);
	fputs(glob_buffer.is_empty() ? "mysql> " :
	      !in_string ? "    -> " :
	      in_string == '\'' ?
	      "    '> " : "    \"> ", OUTFILE);
      }
      line=readline((char*) (glob_buffer.is_empty() ? "mysql> " :
			     !in_string ? "    -> " :
			     in_string == '\'' ?
			     "    '> " : "    \"> "));
#endif
      if (opt_outfile)
	fprintf(OUTFILE, "%s\n", line);
    }
    if (!line)					// End of file
    {
      status.exit_status=0;
      break;
    }
    if (!in_string && (line[0] == '#' ||
		       (line[0] == '-' && line[1] == '-') ||
		       line[0] == 0))
      continue;					// Skip comment lines

    /* Check if line is a mysql command line */
    /* (We want to allow help, print and clear anywhere at line start */
    if (execute_commands && (!no_named_cmds || glob_buffer.is_empty()) 
	&& !in_string && (com=find_command(line,0)))
    {
      if ((*com->func)(&glob_buffer,line) > 0)
	break;
      if (glob_buffer.is_empty())		// If buffer was emptied
	in_string=0;
#ifdef HAVE_READLINE
      if (status.add_to_history)
	add_history(line);
#endif
      continue;
    }
    if (add_line(glob_buffer,line,&in_string))
      break;
  }
  /* if in batch mode, send last query even if it doesn't end with \g or go */

  if ((status.batch || !execute_commands) && !status.exit_status)
  {
    remove_cntrl(glob_buffer);
    if (!glob_buffer.is_empty())
    {
      status.exit_status=1;
      if (com_go(&glob_buffer,line) <= 0)
	status.exit_status=0;
    }
  }
  return status.exit_status;
}


static COMMANDS *find_command (char *name,char cmd_char)
{
  uint len;
  char *end;

  if (!name)
  {
    len=0;
    end=0;
  }
  else
  {
    while (isspace(*name))
      name++;
    if (strchr(name,';') || strstr(name,"\\g"))
      return ((COMMANDS *) 0);
    if ((end=strcont(name," \t")))
    {
      len=(uint) (end - name);
      while (isspace(*end))
	end++;
      if (!*end)
	end=0;					// no arguments to function
    }
    else
      len=(uint) strlen(name);
  }

  for (uint i= 0; commands[i].name; i++)
  {
    if (commands[i].func &&
	((name && !my_casecmp(name,commands[i].name,len) &&
	  !commands[i].name[len] &&
	  (!end || (end && commands[i].takes_params))) ||
	 !name && commands[i].cmd_char == cmd_char))
      return (&commands[i]);
  }
  return ((COMMANDS *) 0);
}


static bool add_line(String &buffer,char *line,char *in_string)
{
  uchar inchar;
  char buff[80],*pos,*out;
  COMMANDS *com;

  if (!line[0] && buffer.is_empty())
    return 0;
#ifdef HAVE_READLINE
  if (status.add_to_history && line[0])
    add_history(line);
#endif
#ifdef USE_MB
  char *strend=line+(uint) strlen(line);
#endif

  for (pos=out=line ; (inchar= (uchar) *pos) ; pos++)
  {
    if (isspace(inchar) && out == line && buffer.is_empty())
      continue;
#ifdef USE_MB
    int l;
    if (use_mb(default_charset_info) &&
        (l = my_ismbchar(default_charset_info, pos, strend))) {
	while (l--)
	    *out++ = *pos++;
	pos--;
	continue;
    }
#endif
    if (inchar == '\\')
    {					// mSQL or postgreSQL style command ?
      if (!(inchar = (uchar) *++pos))
	break;				// readline adds one '\'
      if (*in_string || inchar == 'N')
      {					// Don't allow commands in string
	*out++='\\';
	*out++= (char) inchar;
	continue;
      }
      if ((com=find_command(NullS,(char) inchar)))
      {
	const String tmp(line,(uint) (out-line));
	buffer.append(tmp);
	if ((*com->func)(&buffer,pos-1) > 0)
	  return 1;				// Quit
	if (com->takes_params)
	{
	  for (pos++ ; *pos && *pos != ';' ; pos++) ;	// Remove parameters
	  if (!*pos)
	    pos--;
	}
	out=line;
      }
      else
      {
	sprintf(buff,"Unknown command '\\%c'.",inchar);
	if (put_info(buff,INFO_ERROR) > 0)
	  return 1;
	*out++='\\';
	*out++=(char) inchar;
	continue;
      }
    }
    else if (inchar == ';' && !*in_string)
    {						// ';' is end of command
      if (out != line)
	buffer.append(line,(uint) (out-line));	// Add this line
      if ((com=find_command(buffer.c_ptr(),0)))
      {
	if ((*com->func)(&buffer,buffer.c_ptr()) > 0)
	  return 1;				// Quit
      }
      else
      {
	int error=com_go(&buffer,0);
	if (error)
	{
	  return error < 0 ? 0 : 1;		// < 0 is not fatal
	}
      }
      buffer.length(0);
      out=line;
    }
    else if (!*in_string && (inchar == '#' ||
			     inchar == '-' && pos[1] == '-' &&
			     isspace(pos[2])))
      break;					// comment to end of line
    else
    {						// Add found char to buffer
      if (inchar == *in_string)
	*in_string=0;
      else if (!*in_string && (inchar == '\'' || inchar == '"'))
	*in_string=(char) inchar;
      *out++ = (char) inchar;
    }
  }
  if (out != line || !buffer.is_empty())
  {
    *out++='\n';
    uint length=(uint) (out-line);
    if (buffer.length() + length >= buffer.alloced_length())
      buffer.realloc(buffer.length()+length+IO_SIZE);
    if (buffer.append(line,length))
      return 1;
  }
  return 0;
}

/* **************************************************************** */
/*								    */
/*		    Interface to Readline Completion		    */
/*								    */
/* **************************************************************** */

#ifdef HAVE_READLINE

static char *new_command_generator(char *text, int);
static char **new_mysql_completion (char *text, int start, int end);

/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */

char **no_completion (char *text __attribute__ ((unused)),
		      char *word __attribute__ ((unused)))
{
  return 0;					/* No filename completion */
}

static void initialize_readline (char *name)
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name = name;

  /* Tell the completer that we want a crack first. */
  /* rl_attempted_completion_function = (CPPFunction *)mysql_completion;*/
  rl_attempted_completion_function = (CPPFunction *) new_mysql_completion;
  rl_completion_entry_function=(Function *) no_completion;
}

/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */


static char **new_mysql_completion (char *text,
				    int start __attribute__((unused)),
				    int end __attribute__((unused)))
{
  if (!status.batch && !quick)
    return completion_matches(text, (CPFunction*) new_command_generator);
  else
    return (char**) 0;
}

static char *new_command_generator(char *text,int state)
{
  static int textlen;
  char *ptr;
  static Bucket *b;
  static entry *e;
  static uint i;

  if (!state) {
    textlen=(uint) strlen(text);
  }

  if (textlen>0) { /* lookup in the hash */
    if (!state) {
      uint len;

      b = find_all_matches(&ht,text,(uint) strlen(text),&len);
      if (!b) {
	return NullS;
      }
      e = b->pData;
    }

    while (e) {
      ptr= strdup(e->str);
      e = e->pNext;
      return ptr;
    }
  } else { /* traverse the entire hash, ugly but works */

    if (!state) {
      i=0;
      /* find the first used bucket */
      while (i<ht.nTableSize) {
	if (ht.arBuckets[i]) {
	  b = ht.arBuckets[i];
	  e = b->pData;
	  break;
	}
	i++;
      }
    }
    ptr= NullS;
    while (e && !ptr) { /* find valid entry in bucket */
      if ((uint) strlen(e->str)==b->nKeyLength) {
	ptr = strdup(e->str);
      }
      /* find the next used entry */
      e = e->pNext;
      if (!e) { /* find the next used bucket */
	b = b->pNext;
	if (!b) {
	  i++;
	  while (i<ht.nTableSize) {
	    if (ht.arBuckets[i]) {
	      b = ht.arBuckets[i];
	      e = b->pData;
	      break;
	    }
	    i++;
	  }
	} else {
	  e = b->pData;
	}
      }
    }
    if (ptr) {
      return ptr;
    }
  }
  return NullS;
}


/* Build up the completion hash */

static void build_completion_hash(bool skip_rehash,bool write_info)
{
  COMMANDS *cmd=commands;
  static MYSQL_RES *databases=0,*tables=0,*fields;
  static char ***field_names= 0;
  MYSQL_ROW database_row,table_row;
  MYSQL_FIELD *sql_field;
  char buf[NAME_LEN*2+2];		 // table name plus field name plus 2
  int i,j,num_fields;
  DBUG_ENTER("build_completion_hash");

  if (status.batch || quick || !current_db)
    DBUG_VOID_RETURN;			// We don't need completion in batches

  completion_hash_clean(&ht);
  if (tables)
  {
    mysql_free_result(tables);
    tables=0;
  }
  if (databases) {
    mysql_free_result(databases);
    databases=0;
  }

  /* hash SQL commands */
  while (cmd->name) {
    add_word(&ht,(char*) cmd->name);
    cmd++;
  }
  if (skip_rehash)
    DBUG_VOID_RETURN;

  /* hash MySQL functions (to be implemented) */

  /* hash all database names */
  if (mysql_query(&mysql,"show databases")==0) {
    if (!(databases = mysql_store_result(&mysql)))
      put_info(mysql_error(&mysql),INFO_INFO);
    else
    {
      while ((database_row=mysql_fetch_row(databases)))
	add_word(&ht,(char*) database_row[0]);
    }
  }
  /* hash all table names */
  if (mysql_query(&mysql,"show tables")==0)
  {
    if (!(tables = mysql_store_result(&mysql)))
      put_info(mysql_error(&mysql),INFO_INFO);
    else
    {
      if (mysql_num_rows(tables) > 0 && !opt_silent && write_info)
      {
	tee_fprintf(stdout, "\
Reading table information for completion of table and column names\n\
You can turn off this feature to get a quicker startup with -A\n\n");
      }
      while ((table_row=mysql_fetch_row(tables)))
      {
	if (!completion_hash_exists(&ht,(char*) table_row[0],
				    (uint) strlen((const char*) table_row[0])))
	  add_word(&ht,table_row[0]);
      }
    }
  }
  if (field_names) {
    for (i=0; field_names[i]; i++) {
      for (j=0; field_names[i][j]; j++) {
	my_free(field_names[i][j],MYF(0));
      }
      my_free((gptr) field_names[i],MYF(0));
    }
    my_free((gptr) field_names,MYF(0));
  }
  field_names=0;

  /* hash all field names, both with the table prefix and without it */
  if (!tables)					/* no tables */
  {
    DBUG_VOID_RETURN;
  }
  mysql_data_seek(tables,0);
  field_names = (char ***) my_malloc(sizeof(char **) *
				     (uint) (mysql_num_rows(tables)+1),
				     MYF(MY_WME));
  if (!field_names)
    DBUG_VOID_RETURN;
  i=0;
  while ((table_row=mysql_fetch_row(tables)))
  {
    if ((fields=mysql_list_fields(&mysql,(const char*) table_row[0],NullS)))
    {
      num_fields=mysql_num_fields(fields);
      field_names[i] = (char **) my_malloc(sizeof(char *)*(num_fields*2+1),
					   MYF(0));
      if (!field_names[i])
      {
	continue;
      }
      field_names[i][num_fields*2]='\0';
      j=0;
      while ((sql_field=mysql_fetch_field(fields)))
      {
	sprintf(buf,"%s.%s",table_row[0],sql_field->name);
	field_names[i][j] = my_strdup(buf,MYF(0));
	add_word(&ht,field_names[i][j]);
	field_names[i][num_fields+j] = my_strdup(sql_field->name,MYF(0));
	if (!completion_hash_exists(&ht,field_names[i][num_fields+j],
				    (uint) strlen(field_names[i][num_fields+j])))
	  add_word(&ht,field_names[i][num_fields+j]);
	j++;
      }
    }
    else
    {
      tee_fprintf(stdout,
		  "Didn't find any fields in table '%s'\n",table_row[0]);
      field_names[i]=0;
    }
    i++;
  }
  field_names[i]=0;				// End pointer
  DBUG_VOID_RETURN;
}


	/* for gnu readline */

#ifndef HAVE_INDEX
#ifdef	__cplusplus
extern "C" {
#endif
extern char *index(const char *,pchar c),*rindex(const char *,pchar);

char *index(const char *s,pchar c)
{
  for (;;)
  {
     if (*s == (char) c) return (char*) s;
     if (!*s++) return NullS;
  }
}

char *rindex(const char *s,pchar c)
{
  reg3 char *t;

  t = NullS;
  do if (*s == (char) c) t = (char*) s; while (*s++);
  return (char*) t;
}
#ifdef	__cplusplus
}
#endif
#endif
#endif /* HAVE_READLINE */

static int reconnect(void)
{
  if (!status.batch)
  {
    put_info("No connection. Trying to reconnect...",INFO_INFO);
    (void) com_connect((String *) 0, 0);
    if(!no_rehash) com_rehash(NULL, NULL);
  }
  if (!connected)
    return put_info("Can't connect to the server\n",INFO_ERROR);
  return 0;
}


/***************************************************************************
 The different commands
***************************************************************************/

static int
com_help (String *buffer __attribute__((unused)),
	  char *line __attribute__((unused)))
{
  reg1 int i;

  put_info("\nMySQL commands:",INFO_INFO);
  if (no_named_cmds)
    put_info("Note that all text commands must be first on line and end with ';'",INFO_INFO);
  for (i = 0; commands[i].name; i++)
  {
    if (commands[i].func)
      tee_fprintf(stdout, "%s\t(\\%c)\t%s\n", commands[i].name,
		  commands[i].cmd_char, commands[i].doc);
  }
  if (connected)
    tee_fprintf(stdout,
		"\nConnection id: %ld  (Can be used with mysqladmin kill)\n\n",
		mysql_thread_id(&mysql));
  else
    tee_fprintf(stdout, "Not connected!  Reconnect with 'connect'!\n\n");
  return 0;
}


	/* ARGSUSED */
static int
com_clear(String *buffer,char *line __attribute__((unused)))
{
  buffer->length(0);
  return 0;
}


/*
** Execute command
** Returns: 0  if ok
**	    -1 if not fatal error
**	    1  if fatal error
*/


static int
com_go(String *buffer,char *line __attribute__((unused)))
{
  char		buff[160],time_buff[32];
  MYSQL_RES	*result;
  ulong		timer;
  uint		error=0;

  if (!status.batch)
  {
    old_buffer= *buffer;			// Save for edit command
    old_buffer.copy();
  }

	/* Remove garbage for nicer messages */
  LINT_INIT(buff[0]);
  remove_cntrl(*buffer);

  if (buffer->is_empty())
  {
    if (status.batch)				// Ignore empty quries
      return 0;
    return put_info("No query specified\n",INFO_ERROR);

  }
  if (!connected && reconnect())
  {
    buffer->length(0);				// Remove query on error
    return status.batch ? 1 : -1;		// Fatal error
  }
  if (verbose)
    (void) com_print(buffer,0);

  if (skip_updates &&
      (buffer->length() < 4 || my_sortcmp(buffer->ptr(),"SET ",4)))
  {
    (void) put_info("Ignoring query to other database",INFO_INFO);
    return 0;
  }

  timer=start_timer();
  for (uint retry=0;; retry++)
  {
    if (!mysql_real_query(&mysql,buffer->ptr(),buffer->length()))
      break;
    error=put_info(mysql_error(&mysql),INFO_ERROR, mysql_errno(&mysql));
    if (mysql_errno(&mysql) != CR_SERVER_GONE_ERROR || retry > 1 
	|| status.batch)
    {
      buffer->length(0);			// Remove query on error
      return error;
    }
    if (reconnect())
    {
      buffer->length(0);			// Remove query on error
      return error;
    }
  }
  error=0;
  buffer->length(0);

  if (quick)
  {
    if (!(result=mysql_use_result(&mysql)) && mysql_field_count(&mysql))
    {
      return put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
    }
  }
  else
  {
    if (!(result=mysql_store_result(&mysql)))
    {
      if (mysql_error(&mysql)[0])
      {
	return put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
      }
    }
  }

  if (verbose >= 3 || !opt_silent)
    mysql_end_timer(timer,time_buff);
  else
    time_buff[0]=0;
  if (result)
  {
    if (!mysql_num_rows(result) && ! quick)
    {
      sprintf(buff,"Empty set%s",time_buff);
    }
    else
    {
      init_pager();
      if (opt_html)
	print_table_data_html(result);
      else if (vertical)
	print_table_data_vertically(result);
      else if (opt_silent && verbose <= 2 && !output_tables)
	print_tab_data(result);
      else
	print_table_data(result);
      sprintf(buff,"%ld %s in set%s",
	      (long) mysql_num_rows(result),
	      (long) mysql_num_rows(result) == 1 ? "row" : "rows",
	      time_buff);
      end_pager();
    }
  }
  else if (mysql_affected_rows(&mysql) == ~(ulonglong) 0)
    sprintf(buff,"Query OK%s",time_buff);
  else
    sprintf(buff,"Query OK, %ld %s affected%s",
	    (long) mysql_affected_rows(&mysql),
	    (long) mysql_affected_rows(&mysql) == 1 ? "row" : "rows",
	    time_buff);
  put_info(buff,INFO_RESULT);
  if (mysql_info(&mysql))
    put_info(mysql_info(&mysql),INFO_RESULT);
  put_info("",INFO_RESULT);			// Empty row

  if (result && !mysql_eof(result))	/* Something wrong when using quick */
    error=put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
  else if (unbuffered)
    fflush(stdout);
  mysql_free_result(result);
  return error;				/* New command follows */
}


static void init_pager()
{
#ifndef __WIN__
  if (!opt_nopager)
  {
    if (!(PAGER= popen(pager, "w")))
    {
      tee_fprintf(stdout, "popen() failed! defaulting PAGER to stdout!\n");
      PAGER= stdout;
    }
  }
  else
#endif
    PAGER= stdout;
}

static void end_pager()
{
#ifndef __WIN__
  if (!opt_nopager)
    pclose(PAGER);
#endif
}

static void init_tee()
{
  if (!(OUTFILE= my_fopen(outfile, O_APPEND | O_WRONLY, MYF(MY_WME))))
  {
    opt_outfile=0;
    init_pager();
    return;
  }
}

static void end_tee()
{
  my_fclose(OUTFILE, MYF(0));
  return;
}

static int
com_ego(String *buffer,char *line)
{
  int result;
  bool oldvertical=vertical;
  vertical=1;
  result=com_go(buffer,line);
  vertical=oldvertical;
  return result;
}


static void
print_table_data(MYSQL_RES *result)
{
  String separator(256);
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;
  bool		*num_flag;

  num_flag=(bool*) my_alloca(sizeof(bool)*mysql_num_fields(result));
  separator.copy("+",1);
  while ((field = mysql_fetch_field(result)))
  {
    uint length=skip_column_names ? 0 : (uint) strlen(field->name);
    if (quick)
      length=max(length,field->length);
    else
      length=max(length,field->max_length);
    if (length < 4 && !IS_NOT_NULL(field->flags))
      length=4;					// Room for "NULL"
    field->max_length=length+1;
    separator.fill(separator.length()+length+2,'-');
    separator.append('+');
  }
  tee_puts(separator.c_ptr(), PAGER);
  if (!skip_column_names)
  {
    mysql_field_seek(result,0);
    (void) tee_fputs("|", PAGER);
    for (uint off=0; (field = mysql_fetch_field(result)) ; off++)
    {
      tee_fprintf(PAGER, " %-*s|",field->max_length,field->name);
      num_flag[off]= IS_NUM(field->type);
    }
    (void) tee_fputs("\n", PAGER);
    tee_puts(separator.c_ptr(), PAGER);
  }

  while ((cur = mysql_fetch_row(result)))
  {
    (void) tee_fputs("|", PAGER);
    mysql_field_seek(result,0);
    for (uint off=0 ; off < mysql_num_fields(result); off++)
    {
      field = mysql_fetch_field(result);
      uint length=field->max_length;
      tee_fprintf(PAGER, num_flag[off] ? "%*s |" : " %-*s|",
		  length,cur[off] ? (char*) cur[off] : "NULL");
    }
    (void) tee_fputs("\n", PAGER);
  }
  tee_puts(separator.c_ptr(), PAGER);
  my_afree((gptr) num_flag);
}

static void
print_table_data_html(MYSQL_RES *result)
{
  MYSQL_ROW   cur;
  MYSQL_FIELD *field;

  mysql_field_seek(result,0);
  (void) tee_fputs("<TABLE BORDER=1><TR>", PAGER);
  if (!skip_column_names)
  {
    while((field = mysql_fetch_field(result)))
    {
      tee_fprintf(PAGER, "<TH>%s</TH>", (field->name ? 
					 (field->name[0] ? field->name : 
					  " &nbsp; ") : "NULL"));
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  while ((cur = mysql_fetch_row(result)))
  {
    (void) tee_fputs("<TR>", PAGER);
    for (uint i=0; i < mysql_num_fields(result); i++)
    {
      ulong *lengths=mysql_fetch_lengths(result);
      (void) tee_fputs("<TD>", PAGER);
      safe_put_field(cur[i],lengths[i]);
      (void) tee_fputs("</TD>", PAGER);
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  (void) tee_fputs("</TABLE>", PAGER);
}



static void
print_table_data_vertically(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  uint		max_length=0;
  MYSQL_FIELD	*field;

  while ((field = mysql_fetch_field(result)))
  {
    uint length=(uint) strlen(field->name);
    if (length > max_length)
      max_length= length;
    field->max_length=length;
  }

  mysql_field_seek(result,0);
  for (uint row_count=1; (cur= mysql_fetch_row(result)); row_count++)
  {
    mysql_field_seek(result,0);
    tee_fprintf(PAGER, 
		"*************************** %d. row ***************************\n", row_count);
    for (uint off=0; off < mysql_num_fields(result); off++)
    {
      field= mysql_fetch_field(result);
      tee_fprintf(PAGER, "%*s: ",(int) max_length,field->name);
      tee_fprintf(PAGER, "%s\n",cur[off] ? (char*) cur[off] : "NULL");
    }
  }
}


static void
safe_put_field(const char *pos,ulong length)
{
  if (!pos)
    tee_fputs("NULL", PAGER);
  else
  {
    if (opt_raw_data)
      tee_fputs(pos, PAGER);
    else for (const char *end=pos+length ; pos != end ; pos++)
    {
#ifdef USE_MB
      int l;
      if (use_mb(default_charset_info) &&
          (l = my_ismbchar(default_charset_info, pos, end))) {
	  while (l--)
	    tee_putc(*pos++, PAGER);
	  pos--;
	  continue;
      }
#endif
      if (!*pos)
	tee_fputs("\\0", PAGER); // This makes everything hard
      else if (*pos == '\t')
	tee_fputs("\\t", PAGER); // This would destroy tab format
      else if (*pos == '\n')
	tee_fputs("\\n", PAGER); // This too
      else if (*pos == '\\')
	tee_fputs("\\\\", PAGER);
      else
	tee_putc(*pos, PAGER);
    }
  }
}


static void
print_tab_data(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;
  ulong		*lengths;

  if (opt_silent < 2 && !skip_column_names)
  {
    int first=0;
    while ((field = mysql_fetch_field(result)))
    {
      if (first++)
	(void) tee_fputs("\t", PAGER);
      (void) tee_fputs(field->name, PAGER);
    }
    (void) tee_fputs("\n", PAGER);
  }
  while ((cur = mysql_fetch_row(result)))
  {
    lengths=mysql_fetch_lengths(result);
    safe_put_field(cur[0],lengths[0]);
    for (uint off=1 ; off < mysql_num_fields(result); off++)
    {
      (void) tee_fputs("\t", PAGER);
      safe_put_field(cur[off],lengths[off]);
    }
    (void) tee_fputs("\n", PAGER);
  }
}

static int
com_tee(String *buffer, char *line __attribute__((unused)))
{
  char file_name[FN_REFLEN], *end, *param;

  if (status.batch)
    return 0;
  while (isspace(*line))
    line++;
  if (!(param = strchr(line, ' '))) // if outfile wasn't given, use the default
  {
    if (!strlen(outfile))
    {
      printf("No previous outfile available, you must give the filename!\n");
      opt_outfile=0;
      return 0;
    }
  }
  else
  {
    while (isspace(*param))
      param++;
    end=strmake(file_name, param, sizeof(file_name)-1);
    while (end > file_name && (isspace(end[-1]) || iscntrl(end[-1])))
      end--;
    end[0]=0;
    strmov(outfile, file_name);
  }
  if (!strlen(outfile))
  {
    printf("No outfile specified!\n");
    return 0;
  }
  if (!opt_outfile)
  {
    init_tee();
    opt_outfile=1;
  }
  tee_fprintf(stdout, "Logging to file '%s'\n", outfile);
  return 0;
}

static int
com_notee(String *buffer __attribute__((unused)),
	  char *line __attribute__((unused)))
{
  if (opt_outfile)
    end_tee();
  opt_outfile=0;
  tee_fprintf(stdout, "Outfile disabled.\n");
  return 0;
}

/*
** Sorry, this command is not available in Windows.
*/

#ifndef __WIN__
static int
com_pager(String *buffer, char *line __attribute__((unused)))
{
  char pager_name[FN_REFLEN], *end, *param;

  if (status.batch)
    return 0;
  /* Skip space from file name */
  while (isspace(*line))
    line++;
  if (!(param = strchr(line, ' '))) // if pager was not given, use the default
  {
    if (!strlen(default_pager))
    {
      tee_fprintf(stdout, "Default pager wasn't set, using stdout.\n");
      opt_nopager=1;
      strmov(pager, "stdout");
      PAGER= stdout;
      return 0;
    }
    strmov(pager, default_pager);
  }
  else
  {
    while (isspace(*param))
      param++;
    end=strmake(pager_name, param, sizeof(pager_name)-1);
    while (end > pager_name && (isspace(end[-1]) || iscntrl(end[-1])))
      end--;
    end[0]=0;
    strmov(pager, pager_name);
  }
  opt_nopager=0;
  tee_fprintf(stdout, "PAGER set to %s\n", pager);
  return 0;
}


static int
com_nopager(String *buffer __attribute__((unused)),
	    char *line __attribute__((unused)))
{
  strmov(pager, "stdout");
  opt_nopager=1;
  tee_fprintf(stdout, "PAGER set to stdout\n");
  return 0;
}
#endif


/*
** Sorry, you can't send the result to an editor in Win32
*/

#ifndef __WIN__
static int
com_edit(String *buffer,char *line __attribute__((unused)))
{
  char	filename[FN_REFLEN],buff[160];
  int	fd,tmp;
  const char *editor;

  if ((fd=create_temp_file(filename,NullS,"sql", O_CREAT | O_WRONLY,
			   MYF(MY_WME))) < 0)
    goto err;
  if (buffer->is_empty() && !old_buffer.is_empty())
    (void) my_write(fd,(byte*) old_buffer.ptr(),old_buffer.length(),
		    MYF(MY_WME));
  else
    (void) my_write(fd,(byte*) buffer->ptr(),buffer->length(),MYF(MY_WME));
  (void) my_close(fd,MYF(0));

  if (!(editor = (char *)getenv("EDITOR")) &&
      !(editor = (char *)getenv("VISUAL")))
    editor = "vi";
  strxmov(buff,editor," ",filename,NullS);
  (void) system(buff);

  MY_STAT stat_arg;
  if (!my_stat(filename,&stat_arg,MYF(MY_WME)))
    goto err;
  if ((fd = my_open(filename,O_RDONLY, MYF(MY_WME))) < 0)
    goto err;
  (void) buffer->alloc((uint) stat_arg.st_size);
  if ((tmp=read(fd,(char*) buffer->ptr(),buffer->alloced_length())) >= 0L)
    buffer->length((uint) tmp);
  else
    buffer->length(0);
  (void) my_close(fd,MYF(0));
  (void) my_delete(filename,MYF(MY_WME));
err:
  return 0;
}
#endif


/* If arg is given, exit without errors. This happens on command 'quit' */

static int
com_quit(String *buffer __attribute__((unused)),
	 char *line __attribute__((unused)))
{
  status.exit_status=0;
  return 1;
}

static int
com_rehash(String *buffer __attribute__((unused)),
	 char *line __attribute__((unused)))
{
#ifdef HAVE_READLINE
  build_completion_hash(0,0);
#endif
  return 0;
}

static int
com_print(String *buffer,char *line __attribute__((unused)))
{
  tee_puts("--------------", stdout);
  (void) tee_fputs(buffer->c_ptr(), stdout);
  if (!buffer->length() || (*buffer)[buffer->length()-1] != '\n')
    tee_putc('\n', stdout);
  tee_puts("--------------\n", stdout);
  return 0;					/* If empty buffer */
}

	/* ARGSUSED */
static int
com_connect(String *buffer, char *line)
{
  char *tmp,buff[256];
  bool save_rehash=no_rehash;
  int error;

  if (buffer)
  {
    while (isspace(*line))
      line++;
    strnmov(buff,line,sizeof(buff)-1);		// Don't destroy history
    if (buff[0] == '\\')			// Short command
      buff[1]=' ';
    tmp=(char *) strtok(buff," \t");		// Skip connect command
    if (tmp && (tmp=(char *) strtok(NullS," \t;")))
    {
      my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));
      current_db=my_strdup(tmp,MYF(MY_WME));
      if ((tmp=(char *) strtok(NullS," \t;")))
      {
	my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
	current_host=my_strdup(tmp,MYF(MY_WME));
      }
    }
    else
      no_rehash=1;				// Quick re-connect
    buffer->length(0);				// command used
  }
  else
    no_rehash=1;
  error=sql_connect(current_host,current_db,current_user,opt_password,0);
  no_rehash=save_rehash;

  if (connected)
  {
    sprintf(buff,"Connection id:    %ld",mysql_thread_id(&mysql));
    put_info(buff,INFO_INFO);
    sprintf(buff,"Current database: %s\n",
	    current_db ? current_db : "*** NONE ***");
    put_info(buff,INFO_INFO);
  }
  return error;
}


static int com_source(String *buffer, char *line)
{
  char source_name[FN_REFLEN], *end, *param;
  LINE_BUFFER *line_buff;
  int error;
  STATUS old_status;
  FILE *sql_file;

  /* Skip space from file name */
  while (isspace(*line))
    line++;
  if (!(param = strchr(line, ' ')))		// Skip command name
    return put_info("Usage: \\. <filename> | source <filename>", 
		    INFO_ERROR, 0);
  while (isspace(*param))
    param++;
  end=strmake(source_name,param,sizeof(source_name)-1);
  while (end > source_name && (isspace(end[-1]) || iscntrl(end[-1])))
    end--;
  end[0]=0;
  unpack_filename(source_name,source_name);
  /* open file name */
  if (!(sql_file = my_fopen(source_name, O_RDONLY | O_BINARY,MYF(0))))
  {
    char buff[FN_REFLEN+60];
    sprintf(buff,"Failed to open file '%s', error: %d", source_name,errno);
    return put_info(buff, INFO_ERROR, 0);
  }

  if (!(line_buff=batch_readline_init(max_allowed_packet+512,sql_file)))
  {
    my_fclose(sql_file,MYF(0));
    return put_info("Can't initialize batch_readline", INFO_ERROR, 0);
  }

  /* Save old status */
  old_status=status;
  bfill((char*) &status,sizeof(status),(char) 0);

  status.batch=old_status.batch;		// Run in batch mode
  status.line_buff=line_buff;
  status.file_name=source_name;
  glob_buffer.length(0);			// Empty command buffer
  error=read_lines(0);				// Read lines from file
  status=old_status;				// Continue as before
  my_fclose(sql_file,MYF(0));
  batch_readline_end(line_buff);
  return error;
}


	/* ARGSUSED */
static int
com_use(String *buffer __attribute__((unused)), char *line)
{
  char *tmp;
  char buff[256];

  while (isspace(*line))
    line++;
  strnmov(buff,line,sizeof(buff)-1);		// Don't destroy history
  if (buff[0] == '\\')				// Short command
    buff[1]=' ';
  tmp=(char *) strtok(buff," \t;");		// Skip connect command
  if (!tmp || !(tmp=(char *) strtok(NullS," \t;")))
  {
    put_info("USE must be followed by a database name",INFO_ERROR);
    return 0;
  }
  if (!current_db || cmp_database(current_db,tmp))
  {
    if (one_database)
      skip_updates=1;
    else
    {
      /*
	reconnect once if connection is down or if connection was found to
	be down during query
      */
      if (!connected && reconnect())
	return status.batch ? 1 : -1;			// Fatal error
      if (mysql_select_db(&mysql,tmp))
      {
	if (mysql_errno(&mysql) != CR_SERVER_GONE_ERROR)
	  return put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));

	if (reconnect())
	  return status.batch ? 1 : -1;			// Fatal error
	if (mysql_select_db(&mysql,tmp))
	  return put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
      }
      my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));
      current_db=my_strdup(tmp,MYF(MY_WME));
#ifdef HAVE_READLINE
      build_completion_hash(no_rehash,1);
#endif
    }
  }
  else
    skip_updates=0;
  put_info("Database changed",INFO_INFO);
  return 0;
}


static int
sql_real_connect(char *host,char *database,char *user,char *password,
		 uint silent)
{
  if (connected)
  {					/* if old is open, close it first */
    mysql_close(&mysql);
    connected= 0;
  }
  mysql_init(&mysql);
  if (opt_connect_timeout)
  {
    uint timeout=opt_connect_timeout;
    mysql_options(&mysql,MYSQL_OPT_CONNECT_TIMEOUT,
		  (char*) &timeout);
  }
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);
#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
    mysql_ssl_set(&mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath);
#endif
  if (safe_updates)
  {
    char init_command[100];
    sprintf(init_command,
	    "SET SQL_SAFE_UPDATES=1,SQL_SELECT_LIMIT=%lu,SQL_MAX_JOIN_SIZE=%lu",
	    select_limit,max_join_size);
    mysql_options(&mysql, MYSQL_INIT_COMMAND, init_command);
  }
  if (!mysql_real_connect(&mysql,host,user,password,
			  database,opt_mysql_port,opt_mysql_unix_port,
			  connect_flag))
  {
    if (!silent ||
	(mysql_errno(&mysql) != CR_CONN_HOST_ERROR &&
	 mysql_errno(&mysql) != CR_CONNECTION_ERROR))
    {
      put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
      (void) fflush(stdout);
      return ignore_errors ? -1 : 1;		// Abort
    }
    return -1;					// Retryable
  }
  connected=1;
  mysql.reconnect=info_flag ? 1 : 0; // We want to know if this happens
#ifdef HAVE_READLINE
  build_completion_hash(no_rehash,1);
#endif
  return 0;
}


static int
sql_connect(char *host,char *database,char *user,char *password,uint silent)
{
  bool message=0;
  uint count=0;
  int error;
  for (;;)
  {
    if ((error=sql_real_connect(host,database,user,password,wait_flag)) >= 0)
    {
      if (count)
      {
	tee_fputs("\n", stderr);
	(void) fflush(stderr);
      }
      return error;
    }
    if (!wait_flag)
      return ignore_errors ? -1 : 1;
    if (!message && !silent)
    {
      message=1;
      tee_fputs("Waiting",stderr); (void) fflush(stderr);
    }
    (void) sleep(5);
    if (!silent)
    {
      putc('.',stderr); (void) fflush(stderr);
      count++;
    }
  }
}



static int
com_status(String *buffer __attribute__((unused)),
	   char *line __attribute__((unused)))
{
  char *status;
  tee_puts("--------------", stdout);
  usage(1);					/* Print version */
  if (connected)
  {
    MYSQL_RES *result;
    LINT_INIT(result);
    tee_fprintf(stdout, "\nConnection id:\t\t%ld\n",mysql_thread_id(&mysql));
    if (!mysql_query(&mysql,"select DATABASE(),USER()") &&
	(result=mysql_use_result(&mysql)))
    {
      MYSQL_ROW cur=mysql_fetch_row(result);
      tee_fprintf(stdout, "Current database:\t%s\n",cur[0]);
      tee_fprintf(stdout, "Current user:\t\t%s\n",cur[1]);
      (void) mysql_fetch_row(result);		// Read eof
    }
  }
  else
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, "\nNo connection\n");
    vidattr(A_NORMAL);
    return 0;
  }
  if (skip_updates)
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, "\nAll updates ignored to this database\n");
    vidattr(A_NORMAL);
  }
#ifndef __WIN__
  tee_fprintf(stdout, "Current pager:\t\t%s\n", pager);
  tee_fprintf(stdout, "Using outfile:\t\t'%s'\n", opt_outfile ? outfile : "");
#endif
  tee_fprintf(stdout, "Server version:\t\t%s\n", mysql_get_server_info(&mysql));
  tee_fprintf(stdout, "Protocol version:\t%d\n", mysql_get_proto_info(&mysql));
  tee_fprintf(stdout, "Connection:\t\t%s\n", mysql_get_host_info(&mysql));
  tee_fprintf(stdout, "Client characterset:\t%s\n",
	      default_charset_info->name);
  tee_fprintf(stdout, "Server characterset:\t%s\n", mysql.charset->name);
  if (strstr(mysql_get_host_info(&mysql),"TCP/IP") || ! mysql.unix_socket)
    tee_fprintf(stdout, "TCP port:\t\t%d\n", mysql.port);
  else
    tee_fprintf(stdout, "UNIX socket:\t\t%s\n", mysql.unix_socket);
  if (mysql.net.compress)
    tee_fprintf(stdout, "Protocol:\t\tCompressed\n");

  if ((status=mysql_stat(&mysql)) && !mysql_error(&mysql)[0])
  {
    char *pos,buff[40];
    ulong sec;
    pos=strchr(status,' ');
    *pos++=0;
    tee_fprintf(stdout, "%s\t\t\t", status);	/* print label */
    if ((status=str2int(pos,10,0,LONG_MAX,(long*) &sec)))
    {
      nice_time((double) sec,buff,0);
      tee_puts(buff, stdout);			/* print nice time */
      while (*status == ' ') status++;		/* to next info */
    }
    if (status)
    {
      tee_putc('\n', stdout);
      tee_puts(status, stdout);
    }
  }
  if (safe_updates)
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, "\nNote that we are running in safe_update_mode:\n");
    vidattr(A_NORMAL);
    tee_fprintf(stdout, "\
UPDATE and DELETE that doesn't use a key in the WHERE clause are not allowed\n\
(One can force UPDATE/DELETE by adding LIMIT # at the end of the command)\n\
SELECT has an automatic 'LIMIT %lu' if LIMIT is not used\n\
Max number of examined row combination in a join is set to: %lu\n\n",
select_limit,max_join_size);
  }
  tee_puts("--------------\n", stdout);
  return 0;
}


static int
put_info(const char *str,INFO_TYPE info_type,uint error)
{
  static int inited=0;
  
  if (status.batch)
  {
    if (info_type == INFO_ERROR)
    {
      (void) fflush(stdout);
      fprintf(stderr,"ERROR");
      if (error)
	(void) fprintf(stderr," %d",error);
      if (status.query_start_line && ! skip_line_numbers)
      {
	(void) fprintf(stderr," at line %lu",status.query_start_line);
	if (status.file_name)
	  (void) fprintf(stderr," in file: '%s'", status.file_name);
      }
      (void) fprintf(stderr,": %s\n",str);
      (void) fflush(stderr);
      if (!ignore_errors)
	return 1;
    }
    else if (info_type == INFO_RESULT && verbose > 1)
      tee_puts(str, stdout);
    if (unbuffered)
      fflush(stdout);
    return info_type == INFO_ERROR ? -1 : 0;
  }
  if (!opt_silent || info_type == INFO_ERROR)
  {
    if (!inited)
    {
      inited=1;
#ifdef HAVE_SETUPTERM
      (void) setupterm((char *)0, 1, (int *) 0);
#endif
    }
    if (info_type == INFO_ERROR)
    {
      putchar('\007');				/* This should make a bell */
      vidattr(A_STANDOUT);
      if (error)
        (void) tee_fprintf(stderr, "ERROR %d: ", error);
      else
        tee_puts("ERROR: ", stdout);
    }
    else
      vidattr(A_BOLD);
    (void) tee_puts(str, stdout);
    vidattr(A_NORMAL);
  }
  if (unbuffered)
    fflush(stdout);
  return info_type == INFO_ERROR ? -1 : 0;
}


static void remove_cntrl(String &buffer)
{
  char *start,*end;
  end=(start=(char*) buffer.ptr())+buffer.length();
  while (start < end && !isgraph(end[-1]))
    end--;
  buffer.length((uint) (end-start));
}


void tee_fprintf(FILE *file, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  (void) vfprintf(file, fmt, args);
  if (opt_outfile)
    (void) vfprintf(OUTFILE, fmt, args);
  va_end(args);
}


void tee_fputs(const char *s, FILE *file)
{
  fputs(s, file);
  if (opt_outfile)
    fputs(s, OUTFILE);
}


void tee_puts(const char *s, FILE *file)
{
  fputs(s, file);
  fputs("\n", file);
  if (opt_outfile)
  {
    fputs(s, OUTFILE);
    fputs("\n", OUTFILE);
  }
}

void tee_putc(int c, FILE *file)
{
  putc(c, file);
  if (opt_outfile)
    putc(c, OUTFILE);
}

#ifdef __WIN__
#include <time.h>
#else
#include <sys/times.h>
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC (sysconf(_SC_CLK_TCK))
#endif

static ulong start_timer(void)
{
#ifdef __WIN__
 return clock();
#else
  struct tms tms_tmp;
  return times(&tms_tmp);
#endif
}


static void nice_time(double sec,char *buff,bool part_second)
{
  ulong tmp;
  if (sec >= 3600.0*24)
  {
    tmp=(ulong) floor(sec/(3600.0*24));
    sec-=3600.0*24*tmp;
    buff=int2str((long) tmp,buff,10);
    buff=strmov(buff,tmp > 1 ? " days " : " day ");
  }
  if (sec >= 3600.0)
  {
    tmp=(ulong) floor(sec/3600.0);
    sec-=3600.0*tmp;
    buff=int2str((long) tmp,buff,10);
    buff=strmov(buff,tmp > 1 ? " hours " : " hour ");
  }
  if (sec >= 60.0)
  {
    tmp=(ulong) floor(sec/60.0);
    sec-=60.0*tmp;
    buff=int2str((long) tmp,buff,10);
    buff=strmov(buff," min ");
  }
  if (part_second)
    sprintf(buff,"%.2f sec",sec);
  else
    sprintf(buff,"%d sec",(int) sec);
}


static void end_timer(ulong start_time,char *buff)
{
  nice_time((double) (start_timer() - start_time) /
	    CLOCKS_PER_SEC,buff,1);
}


static void mysql_end_timer(ulong start_time,char *buff)
{
  buff[0]=' ';
  buff[1]='(';
  end_timer(start_time,buff+2);
  strmov(strend(buff),")");
}

/* Keep sql_string library happy */

gptr sql_alloc(unsigned int Size)
{
  return my_malloc(Size,MYF(MY_WME));
}

void sql_element_free(void *ptr)
{
  my_free((gptr) ptr,MYF(0));
}
