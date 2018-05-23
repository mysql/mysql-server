/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/* mysql command tool
 * Commands compatible with mSQL by David J. Hughes
 *
 * Written by:
 *   Michael 'Monty' Widenius
 *   Andi Gutmans  <andi@zend.com>
 *   Zeev Suraski  <zeev@zend.com>
 *   Jani Tolonen  <jani@mysql.com>
 *   Matt Wagner   <matt@mysql.com>
 *   Jeremy Cole   <jcole@mysql.com>
 *   Tonu Samuel   <tonu@mysql.com>
 *   Harrison Fisk <harrison@mysql.com>
 *
 **/

#include "client_priv.h"
#include <m_ctype.h>
#include <stdarg.h>
#include <my_dir.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__		      // Skip warnings in getopt.h
#endif
#include "my_readline.h"
#include <signal.h>
#include <violite.h>

#if defined(USE_LIBEDIT_INTERFACE) && defined(HAVE_LOCALE_H)
#include <locale.h>
#endif

const char *VER= "14.14";

/* Don't try to make a nice table if the data is too big */
#define MAX_COLUMN_LENGTH	     1024

/* Buffer to hold 'version' and 'version_comment' */
static char *server_version= NULL;

/* Array of options to pass to libemysqld */
#define MAX_SERVER_ARGS               64

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

#if defined(__WIN__)
#include <conio.h>
#else
#include <readline/readline.h>
#define HAVE_READLINE
#define USE_POPEN
#endif
  //int vidattr(long unsigned int attrs);	// Was missing in sun curses
}

#if !defined(HAVE_VIDATTR)
#undef vidattr
#define vidattr(A) {}			// Can't get this to work
#endif

#ifdef FN_NO_CASE_SENSE
#define cmp_database(cs,A,B) my_strcasecmp((cs), (A), (B))
#else
#define cmp_database(cs,A,B) strcmp((A),(B))
#endif

#include "completion_hash.h"
#include <welcome_copyright_notice.h> // ORACLE_WELCOME_COPYRIGHT_NOTICE

#define PROMPT_CHAR '\\'
#define DEFAULT_DELIMITER ";"

#define MAX_BATCH_BUFFER_SIZE (1024L * 1024L * 1024L)

typedef struct st_status
{
  int exit_status;
  ulong query_start_line;
  char *file_name;
  LINE_BUFFER *line_buff;
  bool batch,add_to_history;
} STATUS;


static HashTable ht;
static char **defaults_argv;

enum enum_info_type { INFO_INFO,INFO_ERROR,INFO_RESULT};
typedef enum enum_info_type INFO_TYPE;

static MYSQL mysql;			/* The connection */
static my_bool ignore_errors=0,wait_flag=0,quick=0,
               connected=0,opt_raw_data=0,unbuffered=0,output_tables=0,
	       opt_rehash=1,skip_updates=0,safe_updates=0,one_database=0,
	       opt_compress=0, using_opt_local_infile=0,
	       vertical=0, line_numbers=1, column_names=1,opt_html=0,
               opt_xml=0,opt_nopager=1, opt_outfile=0, named_cmds= 0,
	       tty_password= 0, opt_nobeep=0, opt_reconnect=1,
	       opt_secure_auth= 0,
               default_pager_set= 0, opt_sigint_ignore= 0,
               auto_vertical_output= 0,
               show_warnings= 0, executing_query= 0, interrupted_query= 0,
               ignore_spaces= 0, opt_binhex= 0;
static my_bool debug_info_flag, debug_check_flag;
static my_bool column_types_flag;
static my_bool preserve_comments= 0;
static ulong opt_max_allowed_packet, opt_net_buffer_length;
static uint verbose=0,opt_silent=0,opt_mysql_port=0, opt_local_infile=0;
static uint opt_enable_cleartext_plugin= 0;
static my_bool using_opt_enable_cleartext_plugin= 0;
static uint my_end_arg;
static char * opt_mysql_unix_port=0;
static int connect_flag=CLIENT_INTERACTIVE;
static char *current_host,*current_db,*current_user=0,*opt_password=0,
            *current_prompt=0, *delimiter_str= 0,
            *default_charset= (char*) MYSQL_AUTODETECT_CHARSET_NAME,
            *opt_init_command= 0;
static char *histfile;
static char *histfile_tmp;
static String glob_buffer,old_buffer;
static String processed_prompt;
static char *full_username=0,*part_username=0,*default_prompt=0;
static int wait_time = 5;
static STATUS status;
static ulong select_limit,max_join_size,opt_connect_timeout=0;
static char mysql_charsets_dir[FN_REFLEN+1];
static char *opt_plugin_dir= 0, *opt_default_auth= 0;
static const char *xmlmeta[] = {
  "&", "&amp;",
  "<", "&lt;",
  ">", "&gt;",
  "\"", "&quot;",
  /* Turn \0 into a space. Why not &#0;? That's not valid XML or HTML. */
  "\0", " ",
  0, 0
};
static const char *day_names[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *month_names[]={"Jan","Feb","Mar","Apr","May","Jun","Jul",
			    "Aug","Sep","Oct","Nov","Dec"};
static char default_pager[FN_REFLEN];
static char pager[FN_REFLEN], outfile[FN_REFLEN];
static FILE *PAGER, *OUTFILE;
static MEM_ROOT hash_mem_root;
static uint prompt_counter;
static char delimiter[16]= DEFAULT_DELIMITER;
static uint delimiter_length= 1;
unsigned short terminal_width= 80;

#ifdef HAVE_SMEM
static char *shared_memory_base_name=0;
#endif
static uint opt_protocol=0;
static CHARSET_INFO *charset_info= &my_charset_latin1;

#include "sslopt-vars.h"

const char *default_dbug_option="d:t:o,/tmp/mysql.trace";

void tee_fprintf(FILE *file, const char *fmt, ...);
void tee_fputs(const char *s, FILE *file);
void tee_puts(const char *s, FILE *file);
void tee_putc(int c, FILE *file);
static void tee_print_sized_data(const char *, unsigned int, unsigned int, bool);
/* The names of functions that actually do the manipulation. */
static int get_options(int argc,char **argv);
extern "C" my_bool get_one_option(int optid, const struct my_option *opt,
                                  char *argument);
static int com_quit(String *str,char*),
	   com_go(String *str,char*), com_ego(String *str,char*),
	   com_print(String *str,char*),
	   com_help(String *str,char*), com_clear(String *str,char*),
	   com_connect(String *str,char*), com_status(String *str,char*),
	   com_use(String *str,char*), com_source(String *str, char*),
	   com_rehash(String *str, char*), com_tee(String *str, char*),
           com_notee(String *str, char*), com_charset(String *str,char*),
           com_prompt(String *str, char*), com_delimiter(String *str, char*),
     com_warnings(String *str, char*), com_nowarnings(String *str, char*);

#ifdef USE_POPEN
static int com_nopager(String *str, char*), com_pager(String *str, char*),
           com_edit(String *str,char*), com_shell(String *str, char *);
#endif

static int read_and_execute(bool interactive);
static int sql_connect(char *host,char *database,char *user,char *password,
		       uint silent);
static const char *server_version_string(MYSQL *mysql);
static int put_info(const char *str,INFO_TYPE info,uint error=0,
		    const char *sql_state=0);
static int put_error(MYSQL *mysql);
static void safe_put_field(const char *pos,ulong length);
static void xmlencode_print(const char *src, uint length);
static void init_pager();
static void end_pager();
static void init_tee(const char *);
static void end_tee();
static const char* construct_prompt();
static char *get_arg(char *line, my_bool get_next_arg);
static void init_username();
static void add_int_to_prompt(int toadd);
static int get_result_width(MYSQL_RES *res);
static int get_field_disp_length(MYSQL_FIELD * field);
static int normalize_dbname(const char *line, char *buff, uint buff_size);
static int get_quote_count(const char *line);

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
  { "?",      '?', com_help,   1, "Synonym for `help'." },
  { "clear",  'c', com_clear,  0, "Clear the current input statement."},
  { "connect",'r', com_connect,1,
    "Reconnect to the server. Optional arguments are db and host." },
  { "delimiter", 'd', com_delimiter,    1,
    "Set statement delimiter." },
#ifdef USE_POPEN
  { "edit",   'e', com_edit,   0, "Edit command with $EDITOR."},
#endif
  { "ego",    'G', com_ego,    0,
    "Send command to mysql server, display result vertically."},
  { "exit",   'q', com_quit,   0, "Exit mysql. Same as quit."},
  { "go",     'g', com_go,     0, "Send command to mysql server." },
  { "help",   'h', com_help,   1, "Display this help." },
#ifdef USE_POPEN
  { "nopager",'n', com_nopager,0, "Disable pager, print to stdout." },
#endif
  { "notee",  't', com_notee,  0, "Don't write into outfile." },
#ifdef USE_POPEN
  { "pager",  'P', com_pager,  1, 
    "Set PAGER [to_pager]. Print the query results via PAGER." },
#endif
  { "print",  'p', com_print,  0, "Print current command." },
  { "prompt", 'R', com_prompt, 1, "Change your mysql prompt."},
  { "quit",   'q', com_quit,   0, "Quit mysql." },
  { "rehash", '#', com_rehash, 0, "Rebuild completion hash." },
  { "source", '.', com_source, 1,
    "Execute an SQL script file. Takes a file name as an argument."},
  { "status", 's', com_status, 0, "Get status information from the server."},
#ifdef USE_POPEN
  { "system", '!', com_shell,  1, "Execute a system shell command."},
#endif
  { "tee",    'T', com_tee,    1, 
    "Set outfile [to_outfile]. Append everything into given outfile." },
  { "use",    'u', com_use,    1,
    "Use another database. Takes database name as argument." },
  { "charset",    'C', com_charset,    1,
    "Switch to another charset. Might be needed for processing binlog with multi-byte charsets." },
  { "warnings", 'W', com_warnings,  0,
    "Show warnings after every statement." },
  { "nowarning", 'w', com_nowarnings, 0,
    "Don't show warnings after every statement." },
  /* Get bash-like expansion for some commands */
  { "create table",     0, 0, 0, ""},
  { "create database",  0, 0, 0, ""},
  { "show databases",   0, 0, 0, ""},
  { "show fields from", 0, 0, 0, ""},
  { "show keys from",   0, 0, 0, ""},
  { "show tables",      0, 0, 0, ""},
  { "load data from",   0, 0, 0, ""},
  { "alter table",      0, 0, 0, ""},
  { "set option",       0, 0, 0, ""},
  { "lock tables",      0, 0, 0, ""},
  { "unlock tables",    0, 0, 0, ""},
  /* generated 2006-12-28.  Refresh occasionally from lexer. */
  { "ACTION", 0, 0, 0, ""},
  { "ADD", 0, 0, 0, ""},
  { "AFTER", 0, 0, 0, ""},
  { "AGAINST", 0, 0, 0, ""},
  { "AGGREGATE", 0, 0, 0, ""},
  { "ALL", 0, 0, 0, ""},
  { "ALGORITHM", 0, 0, 0, ""},
  { "ALTER", 0, 0, 0, ""},
  { "ANALYZE", 0, 0, 0, ""},
  { "AND", 0, 0, 0, ""},
  { "ANY", 0, 0, 0, ""},
  { "AS", 0, 0, 0, ""},
  { "ASC", 0, 0, 0, ""},
  { "ASCII", 0, 0, 0, ""},
  { "ASENSITIVE", 0, 0, 0, ""},
  { "AUTO_INCREMENT", 0, 0, 0, ""},
  { "AVG", 0, 0, 0, ""},
  { "AVG_ROW_LENGTH", 0, 0, 0, ""},
  { "BACKUP", 0, 0, 0, ""},
  { "BDB", 0, 0, 0, ""},
  { "BEFORE", 0, 0, 0, ""},
  { "BEGIN", 0, 0, 0, ""},
  { "BERKELEYDB", 0, 0, 0, ""},
  { "BETWEEN", 0, 0, 0, ""},
  { "BIGINT", 0, 0, 0, ""},
  { "BINARY", 0, 0, 0, ""},
  { "BINLOG", 0, 0, 0, ""},
  { "BIT", 0, 0, 0, ""},
  { "BLOB", 0, 0, 0, ""},
  { "BOOL", 0, 0, 0, ""},
  { "BOOLEAN", 0, 0, 0, ""},
  { "BOTH", 0, 0, 0, ""},
  { "BTREE", 0, 0, 0, ""},
  { "BY", 0, 0, 0, ""},
  { "BYTE", 0, 0, 0, ""},
  { "CACHE", 0, 0, 0, ""},
  { "CALL", 0, 0, 0, ""},
  { "CASCADE", 0, 0, 0, ""},
  { "CASCADED", 0, 0, 0, ""},
  { "CASE", 0, 0, 0, ""},
  { "CHAIN", 0, 0, 0, ""},
  { "CHANGE", 0, 0, 0, ""},
  { "CHANGED", 0, 0, 0, ""},
  { "CHAR", 0, 0, 0, ""},
  { "CHARACTER", 0, 0, 0, ""},
  { "CHARSET", 0, 0, 0, ""},
  { "CHECK", 0, 0, 0, ""},
  { "CHECKSUM", 0, 0, 0, ""},
  { "CIPHER", 0, 0, 0, ""},
  { "CLIENT", 0, 0, 0, ""},
  { "CLOSE", 0, 0, 0, ""},
  { "CODE", 0, 0, 0, ""},
  { "COLLATE", 0, 0, 0, ""},
  { "COLLATION", 0, 0, 0, ""},
  { "COLUMN", 0, 0, 0, ""},
  { "COLUMNS", 0, 0, 0, ""},
  { "COMMENT", 0, 0, 0, ""},
  { "COMMIT", 0, 0, 0, ""},
  { "COMMITTED", 0, 0, 0, ""},
  { "COMPACT", 0, 0, 0, ""},
  { "COMPRESSED", 0, 0, 0, ""},
  { "CONCURRENT", 0, 0, 0, ""},
  { "CONDITION", 0, 0, 0, ""},
  { "CONNECTION", 0, 0, 0, ""},
  { "CONSISTENT", 0, 0, 0, ""},
  { "CONSTRAINT", 0, 0, 0, ""},
  { "CONTAINS", 0, 0, 0, ""},
  { "CONTINUE", 0, 0, 0, ""},
  { "CONVERT", 0, 0, 0, ""},
  { "CREATE", 0, 0, 0, ""},
  { "CROSS", 0, 0, 0, ""},
  { "CUBE", 0, 0, 0, ""},
  { "CURRENT_DATE", 0, 0, 0, ""},
  { "CURRENT_TIME", 0, 0, 0, ""},
  { "CURRENT_TIMESTAMP", 0, 0, 0, ""},
  { "CURRENT_USER", 0, 0, 0, ""},
  { "CURSOR", 0, 0, 0, ""},
  { "DATA", 0, 0, 0, ""},
  { "DATABASE", 0, 0, 0, ""},
  { "DATABASES", 0, 0, 0, ""},
  { "DATE", 0, 0, 0, ""},
  { "DATETIME", 0, 0, 0, ""},
  { "DAY", 0, 0, 0, ""},
  { "DAY_HOUR", 0, 0, 0, ""},
  { "DAY_MICROSECOND", 0, 0, 0, ""},
  { "DAY_MINUTE", 0, 0, 0, ""},
  { "DAY_SECOND", 0, 0, 0, ""},
  { "DEALLOCATE", 0, 0, 0, ""},     
  { "DEC", 0, 0, 0, ""},
  { "DECIMAL", 0, 0, 0, ""},
  { "DECLARE", 0, 0, 0, ""},
  { "DEFAULT", 0, 0, 0, ""},
  { "DEFINER", 0, 0, 0, ""},
  { "DELAYED", 0, 0, 0, ""},
  { "DELAY_KEY_WRITE", 0, 0, 0, ""},
  { "DELETE", 0, 0, 0, ""},
  { "DESC", 0, 0, 0, ""},
  { "DESCRIBE", 0, 0, 0, ""},
  { "DES_KEY_FILE", 0, 0, 0, ""},
  { "DETERMINISTIC", 0, 0, 0, ""},
  { "DIRECTORY", 0, 0, 0, ""},
  { "DISABLE", 0, 0, 0, ""},
  { "DISCARD", 0, 0, 0, ""},
  { "DISTINCT", 0, 0, 0, ""},
  { "DISTINCTROW", 0, 0, 0, ""},
  { "DIV", 0, 0, 0, ""},
  { "DO", 0, 0, 0, ""},
  { "DOUBLE", 0, 0, 0, ""},
  { "DROP", 0, 0, 0, ""},
  { "DUAL", 0, 0, 0, ""},
  { "DUMPFILE", 0, 0, 0, ""},
  { "DUPLICATE", 0, 0, 0, ""},
  { "DYNAMIC", 0, 0, 0, ""},
  { "EACH", 0, 0, 0, ""},
  { "ELSE", 0, 0, 0, ""},
  { "ELSEIF", 0, 0, 0, ""},
  { "ENABLE", 0, 0, 0, ""},
  { "ENCLOSED", 0, 0, 0, ""},
  { "END", 0, 0, 0, ""},
  { "ENGINE", 0, 0, 0, ""},
  { "ENGINES", 0, 0, 0, ""},
  { "ENUM", 0, 0, 0, ""},
  { "ERRORS", 0, 0, 0, ""},
  { "ESCAPE", 0, 0, 0, ""},
  { "ESCAPED", 0, 0, 0, ""},
  { "EVENTS", 0, 0, 0, ""},
  { "EXECUTE", 0, 0, 0, ""},
  { "EXISTS", 0, 0, 0, ""},
  { "EXIT", 0, 0, 0, ""},
  { "EXPANSION", 0, 0, 0, ""},
  { "EXPLAIN", 0, 0, 0, ""},
  { "EXTENDED", 0, 0, 0, ""},
  { "FALSE", 0, 0, 0, ""},
  { "FAST", 0, 0, 0, ""},
  { "FETCH", 0, 0, 0, ""},
  { "FIELDS", 0, 0, 0, ""},
  { "FILE", 0, 0, 0, ""},
  { "FIRST", 0, 0, 0, ""},
  { "FIXED", 0, 0, 0, ""},
  { "FLOAT", 0, 0, 0, ""},
  { "FLOAT4", 0, 0, 0, ""},
  { "FLOAT8", 0, 0, 0, ""},
  { "FLUSH", 0, 0, 0, ""},
  { "FOR", 0, 0, 0, ""},
  { "FORCE", 0, 0, 0, ""},
  { "FOREIGN", 0, 0, 0, ""},
  { "FOUND", 0, 0, 0, ""},
  { "FROM", 0, 0, 0, ""},
  { "FULL", 0, 0, 0, ""},
  { "FULLTEXT", 0, 0, 0, ""},
  { "FUNCTION", 0, 0, 0, ""},
  { "GEOMETRY", 0, 0, 0, ""},
  { "GEOMETRYCOLLECTION", 0, 0, 0, ""},
  { "GET_FORMAT", 0, 0, 0, ""},
  { "GLOBAL", 0, 0, 0, ""},
  { "GRANT", 0, 0, 0, ""},
  { "GRANTS", 0, 0, 0, ""},
  { "GROUP", 0, 0, 0, ""},
  { "HANDLER", 0, 0, 0, ""},
  { "HASH", 0, 0, 0, ""},
  { "HAVING", 0, 0, 0, ""},
  { "HELP", 0, 0, 0, ""},
  { "HIGH_PRIORITY", 0, 0, 0, ""},
  { "HOSTS", 0, 0, 0, ""},
  { "HOUR", 0, 0, 0, ""},
  { "HOUR_MICROSECOND", 0, 0, 0, ""},
  { "HOUR_MINUTE", 0, 0, 0, ""},
  { "HOUR_SECOND", 0, 0, 0, ""},
  { "IDENTIFIED", 0, 0, 0, ""},
  { "IF", 0, 0, 0, ""},
  { "IGNORE", 0, 0, 0, ""},
  { "IMPORT", 0, 0, 0, ""},
  { "IN", 0, 0, 0, ""},
  { "INDEX", 0, 0, 0, ""},
  { "INDEXES", 0, 0, 0, ""},
  { "INFILE", 0, 0, 0, ""},
  { "INNER", 0, 0, 0, ""},
  { "INNOBASE", 0, 0, 0, ""},
  { "INNODB", 0, 0, 0, ""},
  { "INOUT", 0, 0, 0, ""},
  { "INSENSITIVE", 0, 0, 0, ""},
  { "INSERT", 0, 0, 0, ""},
  { "INSERT_METHOD", 0, 0, 0, ""},
  { "INT", 0, 0, 0, ""},
  { "INT1", 0, 0, 0, ""},
  { "INT2", 0, 0, 0, ""},
  { "INT3", 0, 0, 0, ""},
  { "INT4", 0, 0, 0, ""},
  { "INT8", 0, 0, 0, ""},
  { "INTEGER", 0, 0, 0, ""},
  { "INTERVAL", 0, 0, 0, ""},
  { "INTO", 0, 0, 0, ""},
  { "IO_THREAD", 0, 0, 0, ""},
  { "IS", 0, 0, 0, ""},
  { "ISOLATION", 0, 0, 0, ""},
  { "ISSUER", 0, 0, 0, ""},
  { "ITERATE", 0, 0, 0, ""},
  { "INVOKER", 0, 0, 0, ""},
  { "JOIN", 0, 0, 0, ""},
  { "KEY", 0, 0, 0, ""},
  { "KEYS", 0, 0, 0, ""},
  { "KILL", 0, 0, 0, ""},
  { "LANGUAGE", 0, 0, 0, ""},
  { "LAST", 0, 0, 0, ""},
  { "LEADING", 0, 0, 0, ""},
  { "LEAVE", 0, 0, 0, ""},
  { "LEAVES", 0, 0, 0, ""},
  { "LEFT", 0, 0, 0, ""},
  { "LEVEL", 0, 0, 0, ""},
  { "LIKE", 0, 0, 0, ""},
  { "LIMIT", 0, 0, 0, ""},
  { "LINES", 0, 0, 0, ""},
  { "LINESTRING", 0, 0, 0, ""},
  { "LOAD", 0, 0, 0, ""},
  { "LOCAL", 0, 0, 0, ""},
  { "LOCALTIME", 0, 0, 0, ""},
  { "LOCALTIMESTAMP", 0, 0, 0, ""},
  { "LOCK", 0, 0, 0, ""},
  { "LOCKS", 0, 0, 0, ""},
  { "LOGS", 0, 0, 0, ""},
  { "LONG", 0, 0, 0, ""},
  { "LONGBLOB", 0, 0, 0, ""},
  { "LONGTEXT", 0, 0, 0, ""},
  { "LOOP", 0, 0, 0, ""},
  { "LOW_PRIORITY", 0, 0, 0, ""},
  { "MASTER", 0, 0, 0, ""},
  { "MASTER_CONNECT_RETRY", 0, 0, 0, ""},
  { "MASTER_HOST", 0, 0, 0, ""},
  { "MASTER_LOG_FILE", 0, 0, 0, ""},
  { "MASTER_LOG_POS", 0, 0, 0, ""},
  { "MASTER_PASSWORD", 0, 0, 0, ""},
  { "MASTER_PORT", 0, 0, 0, ""},
  { "MASTER_SERVER_ID", 0, 0, 0, ""},
  { "MASTER_SSL", 0, 0, 0, ""},
  { "MASTER_SSL_CA", 0, 0, 0, ""},
  { "MASTER_SSL_CAPATH", 0, 0, 0, ""},
  { "MASTER_SSL_CERT", 0, 0, 0, ""},
  { "MASTER_SSL_CIPHER", 0, 0, 0, ""},
  { "MASTER_SSL_KEY", 0, 0, 0, ""},
  { "MASTER_USER", 0, 0, 0, ""},
  { "MATCH", 0, 0, 0, ""},
  { "MAX_CONNECTIONS_PER_HOUR", 0, 0, 0, ""},
  { "MAX_QUERIES_PER_HOUR", 0, 0, 0, ""},
  { "MAX_ROWS", 0, 0, 0, ""},
  { "MAX_UPDATES_PER_HOUR", 0, 0, 0, ""},
  { "MAX_USER_CONNECTIONS", 0, 0, 0, ""},
  { "MEDIUM", 0, 0, 0, ""},
  { "MEDIUMBLOB", 0, 0, 0, ""},
  { "MEDIUMINT", 0, 0, 0, ""},
  { "MEDIUMTEXT", 0, 0, 0, ""},
  { "MERGE", 0, 0, 0, ""},
  { "MICROSECOND", 0, 0, 0, ""},
  { "MIDDLEINT", 0, 0, 0, ""},
  { "MIGRATE", 0, 0, 0, ""},
  { "MINUTE", 0, 0, 0, ""},
  { "MINUTE_MICROSECOND", 0, 0, 0, ""},
  { "MINUTE_SECOND", 0, 0, 0, ""},
  { "MIN_ROWS", 0, 0, 0, ""},
  { "MOD", 0, 0, 0, ""},
  { "MODE", 0, 0, 0, ""},
  { "MODIFIES", 0, 0, 0, ""},
  { "MODIFY", 0, 0, 0, ""},
  { "MONTH", 0, 0, 0, ""},
  { "MULTILINESTRING", 0, 0, 0, ""},
  { "MULTIPOINT", 0, 0, 0, ""},
  { "MULTIPOLYGON", 0, 0, 0, ""},
  { "MUTEX", 0, 0, 0, ""},
  { "NAME", 0, 0, 0, ""},
  { "NAMES", 0, 0, 0, ""},
  { "NATIONAL", 0, 0, 0, ""},
  { "NATURAL", 0, 0, 0, ""},
  { "NDB", 0, 0, 0, ""},
  { "NDBCLUSTER", 0, 0, 0, ""},
  { "NCHAR", 0, 0, 0, ""},
  { "NEW", 0, 0, 0, ""},
  { "NEXT", 0, 0, 0, ""},
  { "NO", 0, 0, 0, ""},
  { "NONE", 0, 0, 0, ""},
  { "NOT", 0, 0, 0, ""},
  { "NO_WRITE_TO_BINLOG", 0, 0, 0, ""},
  { "NULL", 0, 0, 0, ""},
  { "NUMERIC", 0, 0, 0, ""},
  { "NVARCHAR", 0, 0, 0, ""},
  { "OFFSET", 0, 0, 0, ""},
  { "OLD_PASSWORD", 0, 0, 0, ""},
  { "ON", 0, 0, 0, ""},
  { "ONE", 0, 0, 0, ""},
  { "ONE_SHOT", 0, 0, 0, ""},
  { "OPEN", 0, 0, 0, ""},
  { "OPTIMIZE", 0, 0, 0, ""},
  { "OPTION", 0, 0, 0, ""},
  { "OPTIONALLY", 0, 0, 0, ""},
  { "OR", 0, 0, 0, ""},
  { "ORDER", 0, 0, 0, ""},
  { "OUT", 0, 0, 0, ""},
  { "OUTER", 0, 0, 0, ""},
  { "OUTFILE", 0, 0, 0, ""},
  { "PACK_KEYS", 0, 0, 0, ""},
  { "PARTIAL", 0, 0, 0, ""},
  { "PASSWORD", 0, 0, 0, ""},
  { "PHASE", 0, 0, 0, ""},
  { "POINT", 0, 0, 0, ""},
  { "POLYGON", 0, 0, 0, ""},
  { "PRECISION", 0, 0, 0, ""},
  { "PREPARE", 0, 0, 0, ""},
  { "PREV", 0, 0, 0, ""},
  { "PRIMARY", 0, 0, 0, ""},
  { "PRIVILEGES", 0, 0, 0, ""},
  { "PROCEDURE", 0, 0, 0, ""},
  { "PROCESS", 0, 0, 0, ""},
  { "PROCESSLIST", 0, 0, 0, ""},
  { "PURGE", 0, 0, 0, ""},
  { "QUARTER", 0, 0, 0, ""},
  { "QUERY", 0, 0, 0, ""},
  { "QUICK", 0, 0, 0, ""},
  { "READ", 0, 0, 0, ""},
  { "READS", 0, 0, 0, ""},
  { "REAL", 0, 0, 0, ""},
  { "RECOVER", 0, 0, 0, ""},
  { "REDUNDANT", 0, 0, 0, ""},
  { "REFERENCES", 0, 0, 0, ""},
  { "REGEXP", 0, 0, 0, ""},
  { "RELAY_LOG_FILE", 0, 0, 0, ""},
  { "RELAY_LOG_POS", 0, 0, 0, ""},
  { "RELAY_THREAD", 0, 0, 0, ""},
  { "RELEASE", 0, 0, 0, ""},
  { "RELOAD", 0, 0, 0, ""},
  { "RENAME", 0, 0, 0, ""},
  { "REPAIR", 0, 0, 0, ""},
  { "REPEATABLE", 0, 0, 0, ""},
  { "REPLACE", 0, 0, 0, ""},
  { "REPLICATION", 0, 0, 0, ""},
  { "REPEAT", 0, 0, 0, ""},
  { "REQUIRE", 0, 0, 0, ""},
  { "RESET", 0, 0, 0, ""},
  { "RESTORE", 0, 0, 0, ""},
  { "RESTRICT", 0, 0, 0, ""},
  { "RESUME", 0, 0, 0, ""},
  { "RETURN", 0, 0, 0, ""},
  { "RETURNS", 0, 0, 0, ""},
  { "REVOKE", 0, 0, 0, ""},
  { "RIGHT", 0, 0, 0, ""},
  { "RLIKE", 0, 0, 0, ""},
  { "ROLLBACK", 0, 0, 0, ""},
  { "ROLLUP", 0, 0, 0, ""},
  { "ROUTINE", 0, 0, 0, ""},
  { "ROW", 0, 0, 0, ""},
  { "ROWS", 0, 0, 0, ""},
  { "ROW_FORMAT", 0, 0, 0, ""},
  { "RTREE", 0, 0, 0, ""},
  { "SAVEPOINT", 0, 0, 0, ""},
  { "SCHEMA", 0, 0, 0, ""},
  { "SCHEMAS", 0, 0, 0, ""},
  { "SECOND", 0, 0, 0, ""},
  { "SECOND_MICROSECOND", 0, 0, 0, ""},
  { "SECURITY", 0, 0, 0, ""},
  { "SELECT", 0, 0, 0, ""},
  { "SENSITIVE", 0, 0, 0, ""},
  { "SEPARATOR", 0, 0, 0, ""},
  { "SERIAL", 0, 0, 0, ""},
  { "SERIALIZABLE", 0, 0, 0, ""},
  { "SESSION", 0, 0, 0, ""},
  { "SET", 0, 0, 0, ""},
  { "SHARE", 0, 0, 0, ""},
  { "SHOW", 0, 0, 0, ""},
  { "SHUTDOWN", 0, 0, 0, ""},
  { "SIGNED", 0, 0, 0, ""},
  { "SIMPLE", 0, 0, 0, ""},
  { "SLAVE", 0, 0, 0, ""},
  { "SNAPSHOT", 0, 0, 0, ""},
  { "SMALLINT", 0, 0, 0, ""},
  { "SOME", 0, 0, 0, ""},
  { "SONAME", 0, 0, 0, ""},
  { "SOUNDS", 0, 0, 0, ""},
  { "SPATIAL", 0, 0, 0, ""},
  { "SPECIFIC", 0, 0, 0, ""},
  { "SQL", 0, 0, 0, ""},
  { "SQLEXCEPTION", 0, 0, 0, ""},
  { "SQLSTATE", 0, 0, 0, ""},
  { "SQLWARNING", 0, 0, 0, ""},
  { "SQL_BIG_RESULT", 0, 0, 0, ""},
  { "SQL_BUFFER_RESULT", 0, 0, 0, ""},
  { "SQL_CACHE", 0, 0, 0, ""},
  { "SQL_CALC_FOUND_ROWS", 0, 0, 0, ""},
  { "SQL_NO_CACHE", 0, 0, 0, ""},
  { "SQL_SMALL_RESULT", 0, 0, 0, ""},
  { "SQL_THREAD", 0, 0, 0, ""},
  { "SQL_TSI_SECOND", 0, 0, 0, ""},
  { "SQL_TSI_MINUTE", 0, 0, 0, ""},
  { "SQL_TSI_HOUR", 0, 0, 0, ""},
  { "SQL_TSI_DAY", 0, 0, 0, ""},
  { "SQL_TSI_WEEK", 0, 0, 0, ""},
  { "SQL_TSI_MONTH", 0, 0, 0, ""},
  { "SQL_TSI_QUARTER", 0, 0, 0, ""},
  { "SQL_TSI_YEAR", 0, 0, 0, ""},
  { "SSL", 0, 0, 0, ""},
  { "START", 0, 0, 0, ""},
  { "STARTING", 0, 0, 0, ""},
  { "STATUS", 0, 0, 0, ""},
  { "STOP", 0, 0, 0, ""},
  { "STORAGE", 0, 0, 0, ""},
  { "STRAIGHT_JOIN", 0, 0, 0, ""},
  { "STRING", 0, 0, 0, ""},
  { "STRIPED", 0, 0, 0, ""},
  { "SUBJECT", 0, 0, 0, ""},
  { "SUPER", 0, 0, 0, ""},
  { "SUSPEND", 0, 0, 0, ""},
  { "TABLE", 0, 0, 0, ""},
  { "TABLES", 0, 0, 0, ""},
  { "TABLESPACE", 0, 0, 0, ""},
  { "TEMPORARY", 0, 0, 0, ""},
  { "TEMPTABLE", 0, 0, 0, ""},
  { "TERMINATED", 0, 0, 0, ""},
  { "TEXT", 0, 0, 0, ""},
  { "THEN", 0, 0, 0, ""},
  { "TIME", 0, 0, 0, ""},
  { "TIMESTAMP", 0, 0, 0, ""},
  { "TIMESTAMPADD", 0, 0, 0, ""},
  { "TIMESTAMPDIFF", 0, 0, 0, ""},
  { "TINYBLOB", 0, 0, 0, ""},
  { "TINYINT", 0, 0, 0, ""},
  { "TINYTEXT", 0, 0, 0, ""},
  { "TO", 0, 0, 0, ""},
  { "TRAILING", 0, 0, 0, ""},
  { "TRANSACTION", 0, 0, 0, ""},
  { "TRIGGER", 0, 0, 0, ""},
  { "TRIGGERS", 0, 0, 0, ""},
  { "TRUE", 0, 0, 0, ""},
  { "TRUNCATE", 0, 0, 0, ""},
  { "TYPE", 0, 0, 0, ""},
  { "TYPES", 0, 0, 0, ""},
  { "UNCOMMITTED", 0, 0, 0, ""},
  { "UNDEFINED", 0, 0, 0, ""},
  { "UNDO", 0, 0, 0, ""},
  { "UNICODE", 0, 0, 0, ""},
  { "UNION", 0, 0, 0, ""},
  { "UNIQUE", 0, 0, 0, ""},
  { "UNKNOWN", 0, 0, 0, ""},
  { "UNLOCK", 0, 0, 0, ""},
  { "UNSIGNED", 0, 0, 0, ""},
  { "UNTIL", 0, 0, 0, ""},
  { "UPDATE", 0, 0, 0, ""},
  { "UPGRADE", 0, 0, 0, ""},
  { "USAGE", 0, 0, 0, ""},
  { "USE", 0, 0, 0, ""},
  { "USER", 0, 0, 0, ""},
  { "USER_RESOURCES", 0, 0, 0, ""},
  { "USE_FRM", 0, 0, 0, ""},
  { "USING", 0, 0, 0, ""},
  { "UTC_DATE", 0, 0, 0, ""},
  { "UTC_TIME", 0, 0, 0, ""},
  { "UTC_TIMESTAMP", 0, 0, 0, ""},
  { "VALUE", 0, 0, 0, ""},
  { "VALUES", 0, 0, 0, ""},
  { "VARBINARY", 0, 0, 0, ""},
  { "VARCHAR", 0, 0, 0, ""},
  { "VARCHARACTER", 0, 0, 0, ""},
  { "VARIABLES", 0, 0, 0, ""},
  { "VARYING", 0, 0, 0, ""},
  { "WARNINGS", 0, 0, 0, ""},
  { "WEEK", 0, 0, 0, ""},
  { "WHEN", 0, 0, 0, ""},
  { "WHERE", 0, 0, 0, ""},
  { "WHILE", 0, 0, 0, ""},
  { "VIEW", 0, 0, 0, ""},
  { "WITH", 0, 0, 0, ""},
  { "WORK", 0, 0, 0, ""},
  { "WRITE", 0, 0, 0, ""},
  { "X509", 0, 0, 0, ""},
  { "XOR", 0, 0, 0, ""},
  { "XA", 0, 0, 0, ""},
  { "YEAR", 0, 0, 0, ""},
  { "YEAR_MONTH", 0, 0, 0, ""},
  { "ZEROFILL", 0, 0, 0, ""},
  { "ABS", 0, 0, 0, ""},
  { "ACOS", 0, 0, 0, ""},
  { "ADDDATE", 0, 0, 0, ""},
  { "ADDTIME", 0, 0, 0, ""},
  { "AES_ENCRYPT", 0, 0, 0, ""},
  { "AES_DECRYPT", 0, 0, 0, ""},
  { "AREA", 0, 0, 0, ""},
  { "ASIN", 0, 0, 0, ""},
  { "ASBINARY", 0, 0, 0, ""},
  { "ASTEXT", 0, 0, 0, ""},
  { "ASWKB", 0, 0, 0, ""},
  { "ASWKT", 0, 0, 0, ""},
  { "ATAN", 0, 0, 0, ""},
  { "ATAN2", 0, 0, 0, ""},
  { "BENCHMARK", 0, 0, 0, ""},
  { "BIN", 0, 0, 0, ""},
  { "BIT_COUNT", 0, 0, 0, ""},
  { "BIT_OR", 0, 0, 0, ""},
  { "BIT_AND", 0, 0, 0, ""},
  { "BIT_XOR", 0, 0, 0, ""},
  { "CAST", 0, 0, 0, ""},
  { "CEIL", 0, 0, 0, ""},
  { "CEILING", 0, 0, 0, ""},
  { "BIT_LENGTH", 0, 0, 0, ""},
  { "CENTROID", 0, 0, 0, ""},
  { "CHAR_LENGTH", 0, 0, 0, ""},
  { "CHARACTER_LENGTH", 0, 0, 0, ""},
  { "COALESCE", 0, 0, 0, ""},
  { "COERCIBILITY", 0, 0, 0, ""},
  { "COMPRESS", 0, 0, 0, ""},
  { "CONCAT", 0, 0, 0, ""},
  { "CONCAT_WS", 0, 0, 0, ""},
  { "CONNECTION_ID", 0, 0, 0, ""},
  { "CONV", 0, 0, 0, ""},
  { "CONVERT_TZ", 0, 0, 0, ""},
  { "COUNT", 0, 0, 0, ""},
  { "COS", 0, 0, 0, ""},
  { "COT", 0, 0, 0, ""},
  { "CRC32", 0, 0, 0, ""},
  { "CROSSES", 0, 0, 0, ""},
  { "CURDATE", 0, 0, 0, ""},
  { "CURTIME", 0, 0, 0, ""},
  { "DATE_ADD", 0, 0, 0, ""},
  { "DATEDIFF", 0, 0, 0, ""},
  { "DATE_FORMAT", 0, 0, 0, ""},
  { "DATE_SUB", 0, 0, 0, ""},
  { "DAYNAME", 0, 0, 0, ""},
  { "DAYOFMONTH", 0, 0, 0, ""},
  { "DAYOFWEEK", 0, 0, 0, ""},
  { "DAYOFYEAR", 0, 0, 0, ""},
  { "DECODE", 0, 0, 0, ""},
  { "DEGREES", 0, 0, 0, ""},
  { "DES_ENCRYPT", 0, 0, 0, ""},
  { "DES_DECRYPT", 0, 0, 0, ""},
  { "DIMENSION", 0, 0, 0, ""},
  { "DISJOINT", 0, 0, 0, ""},
  { "ELT", 0, 0, 0, ""},
  { "ENCODE", 0, 0, 0, ""},
  { "ENCRYPT", 0, 0, 0, ""},
  { "ENDPOINT", 0, 0, 0, ""},
  { "ENVELOPE", 0, 0, 0, ""},
  { "EQUALS", 0, 0, 0, ""},
  { "EXTERIORRING", 0, 0, 0, ""},
  { "EXTRACT", 0, 0, 0, ""},
  { "EXP", 0, 0, 0, ""},
  { "EXPORT_SET", 0, 0, 0, ""},
  { "FIELD", 0, 0, 0, ""},
  { "FIND_IN_SET", 0, 0, 0, ""},
  { "FLOOR", 0, 0, 0, ""},
  { "FORMAT", 0, 0, 0, ""},
  { "FOUND_ROWS", 0, 0, 0, ""},
  { "FROM_DAYS", 0, 0, 0, ""},
  { "FROM_UNIXTIME", 0, 0, 0, ""},
  { "GET_LOCK", 0, 0, 0, ""},
  { "GEOMETRYN", 0, 0, 0, ""},
  { "GEOMETRYTYPE", 0, 0, 0, ""},
  { "GEOMCOLLFROMTEXT", 0, 0, 0, ""},
  { "GEOMCOLLFROMWKB", 0, 0, 0, ""},
  { "GEOMETRYCOLLECTIONFROMTEXT", 0, 0, 0, ""},
  { "GEOMETRYCOLLECTIONFROMWKB", 0, 0, 0, ""},
  { "GEOMETRYFROMTEXT", 0, 0, 0, ""},
  { "GEOMETRYFROMWKB", 0, 0, 0, ""},
  { "GEOMFROMTEXT", 0, 0, 0, ""},
  { "GEOMFROMWKB", 0, 0, 0, ""},
  { "GLENGTH", 0, 0, 0, ""},
  { "GREATEST", 0, 0, 0, ""},
  { "GROUP_CONCAT", 0, 0, 0, ""},
  { "GROUP_UNIQUE_USERS", 0, 0, 0, ""},
  { "HEX", 0, 0, 0, ""},
  { "IFNULL", 0, 0, 0, ""},
  { "INET_ATON", 0, 0, 0, ""},
  { "INET_NTOA", 0, 0, 0, ""},
  { "INSTR", 0, 0, 0, ""},
  { "INTERIORRINGN", 0, 0, 0, ""},
  { "INTERSECTS", 0, 0, 0, ""},
  { "ISCLOSED", 0, 0, 0, ""},
  { "ISEMPTY", 0, 0, 0, ""},
  { "ISNULL", 0, 0, 0, ""},
  { "IS_FREE_LOCK", 0, 0, 0, ""},
  { "IS_USED_LOCK", 0, 0, 0, ""},
  { "LAST_INSERT_ID", 0, 0, 0, ""},
  { "ISSIMPLE", 0, 0, 0, ""},
  { "LAST_DAY", 0, 0, 0, ""},
  { "LCASE", 0, 0, 0, ""},
  { "LEAST", 0, 0, 0, ""},
  { "LENGTH", 0, 0, 0, ""},
  { "LN", 0, 0, 0, ""},
  { "LINEFROMTEXT", 0, 0, 0, ""},
  { "LINEFROMWKB", 0, 0, 0, ""},
  { "LINESTRINGFROMTEXT", 0, 0, 0, ""},
  { "LINESTRINGFROMWKB", 0, 0, 0, ""},
  { "LOAD_FILE", 0, 0, 0, ""},
  { "LOCATE", 0, 0, 0, ""},
  { "LOG", 0, 0, 0, ""},
  { "LOG2", 0, 0, 0, ""},
  { "LOG10", 0, 0, 0, ""},
  { "LOWER", 0, 0, 0, ""},
  { "LPAD", 0, 0, 0, ""},
  { "LTRIM", 0, 0, 0, ""},
  { "MAKE_SET", 0, 0, 0, ""},
  { "MAKEDATE", 0, 0, 0, ""},
  { "MAKETIME", 0, 0, 0, ""},
  { "MASTER_POS_WAIT", 0, 0, 0, ""},
  { "MAX", 0, 0, 0, ""},
  { "MBRCONTAINS", 0, 0, 0, ""},
  { "MBRDISJOINT", 0, 0, 0, ""},
  { "MBREQUAL", 0, 0, 0, ""},
  { "MBRINTERSECTS", 0, 0, 0, ""},
  { "MBROVERLAPS", 0, 0, 0, ""},
  { "MBRTOUCHES", 0, 0, 0, ""},
  { "MBRWITHIN", 0, 0, 0, ""},
  { "MD5", 0, 0, 0, ""},
  { "MID", 0, 0, 0, ""},
  { "MIN", 0, 0, 0, ""},
  { "MLINEFROMTEXT", 0, 0, 0, ""},
  { "MLINEFROMWKB", 0, 0, 0, ""},
  { "MPOINTFROMTEXT", 0, 0, 0, ""},
  { "MPOINTFROMWKB", 0, 0, 0, ""},
  { "MPOLYFROMTEXT", 0, 0, 0, ""},
  { "MPOLYFROMWKB", 0, 0, 0, ""},
  { "MONTHNAME", 0, 0, 0, ""},
  { "MULTILINESTRINGFROMTEXT", 0, 0, 0, ""},
  { "MULTILINESTRINGFROMWKB", 0, 0, 0, ""},
  { "MULTIPOINTFROMTEXT", 0, 0, 0, ""},
  { "MULTIPOINTFROMWKB", 0, 0, 0, ""},
  { "MULTIPOLYGONFROMTEXT", 0, 0, 0, ""},
  { "MULTIPOLYGONFROMWKB", 0, 0, 0, ""},
  { "NAME_CONST", 0, 0, 0, ""},
  { "NOW", 0, 0, 0, ""},
  { "NULLIF", 0, 0, 0, ""},
  { "NUMGEOMETRIES", 0, 0, 0, ""},
  { "NUMINTERIORRINGS", 0, 0, 0, ""},
  { "NUMPOINTS", 0, 0, 0, ""},
  { "OCTET_LENGTH", 0, 0, 0, ""},
  { "OCT", 0, 0, 0, ""},
  { "ORD", 0, 0, 0, ""},
  { "OVERLAPS", 0, 0, 0, ""},
  { "PERIOD_ADD", 0, 0, 0, ""},
  { "PERIOD_DIFF", 0, 0, 0, ""},
  { "PI", 0, 0, 0, ""},
  { "POINTFROMTEXT", 0, 0, 0, ""},
  { "POINTFROMWKB", 0, 0, 0, ""},
  { "POINTN", 0, 0, 0, ""},
  { "POLYFROMTEXT", 0, 0, 0, ""},
  { "POLYFROMWKB", 0, 0, 0, ""},
  { "POLYGONFROMTEXT", 0, 0, 0, ""},
  { "POLYGONFROMWKB", 0, 0, 0, ""},
  { "POSITION", 0, 0, 0, ""},
  { "POW", 0, 0, 0, ""},
  { "POWER", 0, 0, 0, ""},
  { "QUOTE", 0, 0, 0, ""},
  { "RADIANS", 0, 0, 0, ""},
  { "RAND", 0, 0, 0, ""},
  { "RELEASE_LOCK", 0, 0, 0, ""},
  { "REVERSE", 0, 0, 0, ""},
  { "ROUND", 0, 0, 0, ""},
  { "ROW_COUNT", 0, 0, 0, ""},
  { "RPAD", 0, 0, 0, ""},
  { "RTRIM", 0, 0, 0, ""},
  { "SEC_TO_TIME", 0, 0, 0, ""},
  { "SESSION_USER", 0, 0, 0, ""},
  { "SUBDATE", 0, 0, 0, ""},
  { "SIGN", 0, 0, 0, ""},
  { "SIN", 0, 0, 0, ""},
  { "SHA", 0, 0, 0, ""},
  { "SHA1", 0, 0, 0, ""},
  { "SLEEP", 0, 0, 0, ""},
  { "SOUNDEX", 0, 0, 0, ""},
  { "SPACE", 0, 0, 0, ""},
  { "SQRT", 0, 0, 0, ""},
  { "SRID", 0, 0, 0, ""},
  { "STARTPOINT", 0, 0, 0, ""},
  { "STD", 0, 0, 0, ""},
  { "STDDEV", 0, 0, 0, ""},
  { "STDDEV_POP", 0, 0, 0, ""},
  { "STDDEV_SAMP", 0, 0, 0, ""},
  { "STR_TO_DATE", 0, 0, 0, ""},
  { "STRCMP", 0, 0, 0, ""},
  { "SUBSTR", 0, 0, 0, ""},
  { "SUBSTRING", 0, 0, 0, ""},
  { "SUBSTRING_INDEX", 0, 0, 0, ""},
  { "SUBTIME", 0, 0, 0, ""},
  { "SUM", 0, 0, 0, ""},
  { "SYSDATE", 0, 0, 0, ""},
  { "SYSTEM_USER", 0, 0, 0, ""},
  { "TAN", 0, 0, 0, ""},
  { "TIME_FORMAT", 0, 0, 0, ""},
  { "TIME_TO_SEC", 0, 0, 0, ""},
  { "TIMEDIFF", 0, 0, 0, ""},
  { "TO_DAYS", 0, 0, 0, ""},
  { "TOUCHES", 0, 0, 0, ""},
  { "TRIM", 0, 0, 0, ""},
  { "UCASE", 0, 0, 0, ""},
  { "UNCOMPRESS", 0, 0, 0, ""},
  { "UNCOMPRESSED_LENGTH", 0, 0, 0, ""},
  { "UNHEX", 0, 0, 0, ""},
  { "UNIQUE_USERS", 0, 0, 0, ""},
  { "UNIX_TIMESTAMP", 0, 0, 0, ""},
  { "UPPER", 0, 0, 0, ""},
  { "UUID", 0, 0, 0, ""},
  { "VARIANCE", 0, 0, 0, ""},
  { "VAR_POP", 0, 0, 0, ""},
  { "VAR_SAMP", 0, 0, 0, ""},
  { "VERSION", 0, 0, 0, ""},
  { "WEEKDAY", 0, 0, 0, ""},
  { "WEEKOFYEAR", 0, 0, 0, ""},
  { "WITHIN", 0, 0, 0, ""},
  { "X", 0, 0, 0, ""},
  { "Y", 0, 0, 0, ""},
  { "YEARWEEK", 0, 0, 0, ""},
  /* end sentinel */
  { (char *)NULL,       0, 0, 0, ""}
};

static const char *load_default_groups[]= { "mysql","client",0 };

static int         embedded_server_arg_count= 0;
static char       *embedded_server_args[MAX_SERVER_ARGS];
static const char *embedded_server_groups[]=
{ "server", "embedded", "mysql_SERVER", 0 };

#ifdef HAVE_READLINE
/*
 HIST_ENTRY is defined for libedit, but not for the real readline
 Need to redefine it for real readline to find it
*/
#if !defined(HAVE_HIST_ENTRY)
typedef struct _hist_entry {
  const char      *line;
  const char      *data;
} HIST_ENTRY; 
#endif

extern "C" int add_history(const char *command); /* From readline directory */
extern "C" int read_history(const char *command);
extern "C" int write_history(const char *command);
extern "C" HIST_ENTRY *history_get(int num);
extern "C" int history_length;
static int not_in_history(const char *line);
static void initialize_readline (char *name);
static void fix_history(String *final_command);
#endif

static COMMANDS *find_command(char *name,char cmd_name);
static bool add_line(String &buffer,char *line,char *in_string,
                     bool *ml_comment, bool truncated);
static void remove_cntrl(String &buffer);
static void print_table_data(MYSQL_RES *result);
static void print_table_data_html(MYSQL_RES *result);
static void print_table_data_xml(MYSQL_RES *result);
static void print_tab_data(MYSQL_RES *result);
static void print_table_data_vertically(MYSQL_RES *result);
static void print_warnings(void);
static ulong start_timer(void);
static void end_timer(ulong start_time,char *buff);
static void mysql_end_timer(ulong start_time,char *buff);
static void nice_time(double sec,char *buff,bool part_second);
extern "C" sig_handler mysql_end(int sig);
extern "C" sig_handler handle_sigint(int sig);
#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
static sig_handler window_resize(int sig);
#endif


int main(int argc,char *argv[])
{
  char buff[80];

  MY_INIT(argv[0]);
  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);
  
  delimiter_str= delimiter;
  default_prompt = my_strdup(getenv("MYSQL_PS1") ? 
			     getenv("MYSQL_PS1") : 
			     "mysql> ",MYF(MY_WME));
  current_prompt = my_strdup(default_prompt,MYF(MY_WME));
  prompt_counter=0;

  outfile[0]=0;			// no (default) outfile
  strmov(pager, "stdout");	// the default, if --pager wasn't given
  {
    char *tmp=getenv("PAGER");
    if (tmp && strlen(tmp))
    {
      default_pager_set= 1;
      strmov(default_pager, tmp);
    }
  }
  if (!isatty(0) || !isatty(1))
  {
    status.batch=1; opt_silent=1;
    ignore_errors=0;
  }
  else
    status.add_to_history=1;
  status.exit_status=1;

  {
    /* 
     The file descriptor-layer may be out-of-sync with the file-number layer,
     so we make sure that "stdout" is really open.  If its file is closed then
     explicitly close the FD layer. 
    */
    int stdout_fileno_copy;
    stdout_fileno_copy= dup(fileno(stdout)); /* Okay if fileno fails. */
    if (stdout_fileno_copy == -1)
      fclose(stdout);
    else
      close(stdout_fileno_copy);             /* Clean up dup(). */
  }

  if (load_defaults("my",load_default_groups,&argc,&argv))
  {
    my_end(0);
    exit(1);
  }
  defaults_argv=argv;
  if (get_options(argc, (char **) argv))
  {
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }
  if (status.batch && !status.line_buff &&
      !(status.line_buff= batch_readline_init(MAX_BATCH_BUFFER_SIZE, stdin)))
  {
    put_info("Can't initialize batch_readline - may be the input source is "
             "a directory or a block device.", INFO_ERROR, 0);
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }
  if (mysql_server_init(embedded_server_arg_count, embedded_server_args, 
                        (char**) embedded_server_groups))
  {
    put_error(NULL);
    free_defaults(defaults_argv);
    my_end(0);
    exit(1);
  }
  glob_buffer.realloc(512);
  completion_hash_init(&ht, 128);
  init_alloc_root(&hash_mem_root, 16384, 0);
  bzero((char*) &mysql, sizeof(mysql));
  if (sql_connect(current_host,current_db,current_user,opt_password,
		  opt_silent))
  {
    quick= 1;					// Avoid history
    status.exit_status= 1;
    mysql_end(-1);
  }
  if (!status.batch)
    ignore_errors=1;				// Don't abort monitor

  if (opt_sigint_ignore)
    signal(SIGINT, SIG_IGN);
  else
    signal(SIGINT, handle_sigint);              // Catch SIGINT to clean up
  signal(SIGQUIT, mysql_end);			// Catch SIGQUIT to clean up

#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
  /* Readline will call this if it installs a handler */
  signal(SIGWINCH, window_resize);
  /* call the SIGWINCH handler to get the default term width */
  window_resize(0);
#endif

  put_info("Welcome to the MySQL monitor.  Commands end with ; or \\g.",
	   INFO_INFO);
  snprintf((char*) glob_buffer.ptr(), glob_buffer.alloced_length(),
	   "Your MySQL connection id is %lu\nServer version: %s\n",
	   mysql_thread_id(&mysql), server_version_string(&mysql));
  put_info((char*) glob_buffer.ptr(),INFO_INFO);

  put_info(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"), INFO_INFO);

#ifdef HAVE_READLINE
  initialize_readline((char*) my_progname);
  if (!status.batch && !quick && !opt_html && !opt_xml)
  {
    /* read-history from file, default ~/.mysql_history*/
    if (getenv("MYSQL_HISTFILE"))
      histfile=my_strdup(getenv("MYSQL_HISTFILE"),MYF(MY_WME));
    else if (getenv("HOME"))
    {
      histfile=(char*) my_malloc((uint) strlen(getenv("HOME"))
				 + (uint) strlen("/.mysql_history")+2,
				 MYF(MY_WME));
      if (histfile)
	sprintf(histfile,"%s/.mysql_history",getenv("HOME"));
      char link_name[FN_REFLEN];
      if (my_readlink(link_name, histfile, 0) == 0 &&
          strncmp(link_name, "/dev/null", 10) == 0)
      {
        /* The .mysql_history file is a symlink to /dev/null, don't use it */
        my_free(histfile);
        histfile= 0;
      }
    }

    /* We used to suggest setting MYSQL_HISTFILE=/dev/null. */
    if (histfile && strncmp(histfile, "/dev/null", 10) == 0)
      histfile= NULL;

    if (histfile && histfile[0])
    {
      if (verbose)
	tee_fprintf(stdout, "Reading history-file %s\n",histfile);
      read_history(histfile);
      if (!(histfile_tmp= (char*) my_malloc((uint) strlen(histfile) + 5,
					    MYF(MY_WME))))
      {
	fprintf(stderr, "Couldn't allocate memory for temp histfile!\n");
	exit(1);
      }
      sprintf(histfile_tmp, "%s.TMP", histfile);
    }
  }

#endif

  sprintf(buff, "%s",
	  "Type 'help;' or '\\h' for help. Type '\\c' to clear the current input statement.\n");
  put_info(buff,INFO_INFO);
  status.exit_status= read_and_execute(!status.batch);
  if (opt_outfile)
    end_tee();
  mysql_end(0);
#ifndef _lint
  DBUG_RETURN(0);				// Keep compiler happy
#endif
}

sig_handler mysql_end(int sig)
{
#ifndef _WIN32
  /*
    Ingnoring SIGQUIT and SIGINT signals when cleanup process starts.
    This will help in resolving the double free issues, which occures in case
    the signal handler function is started in between the clean up function.
  */
  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);
#endif

  mysql_close(&mysql);
#ifdef HAVE_READLINE
  if (!status.batch && !quick && !opt_html && !opt_xml &&
      histfile && histfile[0])
  {
    /* write-history */
    if (verbose)
      tee_fprintf(stdout, "Writing history-file %s\n",histfile);
    if (!write_history(histfile_tmp))
      my_rename(histfile_tmp, histfile, MYF(MY_WME));
  }
  batch_readline_end(status.line_buff);
  completion_hash_free(&ht);
  free_root(&hash_mem_root,MYF(0));

#endif
  if (sig >= 0)
    put_info(sig ? "Aborted" : "Bye", INFO_RESULT);
  glob_buffer.free();
  old_buffer.free();
  processed_prompt.free();
  my_free(server_version);
  my_free(opt_password);
  my_free(opt_mysql_unix_port);
  my_free(histfile);
  my_free(histfile_tmp);
  my_free(current_db);
  my_free(current_host);
  my_free(current_user);
  my_free(full_username);
  my_free(part_username);
  my_free(default_prompt);
#ifdef HAVE_SMEM
  my_free(shared_memory_base_name);
#endif
  my_free(current_prompt);
  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count]);
  mysql_server_end();
  free_defaults(defaults_argv);
  my_end(my_end_arg);
  exit(status.exit_status);
}


/*
  This function handles sigint calls
  If query is in process, kill query
  no query in process, terminate like previous behavior
 */
sig_handler handle_sigint(int sig)
{
  char kill_buffer[40];
  MYSQL *kill_mysql= NULL;

  /* terminate if no query being executed, or we already tried interrupting */
  /* terminate if no query being executed, or we already tried interrupting */
  if (!executing_query || (interrupted_query == 2))
  {
    tee_fprintf(stdout, "Ctrl-C -- exit!\n");
    goto err;
  }

  kill_mysql= mysql_init(kill_mysql);
  if (!mysql_connect_ssl_check(kill_mysql, current_host, current_user, opt_password,
                               "", opt_mysql_port, opt_mysql_unix_port, 0,
                               opt_ssl_mode == SSL_MODE_REQUIRED))
  {
    tee_fprintf(stdout, "Ctrl-C -- sorry, cannot connect to server to kill query, giving up ...\n");
    goto err;
  }

  interrupted_query++;

  /* mysqld < 5 does not understand KILL QUERY, skip to KILL CONNECTION */
  if ((interrupted_query == 1) && (mysql_get_server_version(&mysql) < 50000))
    interrupted_query= 2;

  /* kill_buffer is always big enough because max length of %lu is 15 */
  sprintf(kill_buffer, "KILL %s%lu",
          (interrupted_query == 1) ? "QUERY " : "",
          mysql_thread_id(&mysql));
  tee_fprintf(stdout, "Ctrl-C -- sending \"%s\" to server ...\n", kill_buffer);
  mysql_real_query(kill_mysql, kill_buffer, (uint) strlen(kill_buffer));
  mysql_close(kill_mysql);
  tee_fprintf(stdout, "Ctrl-C -- query aborted.\n");

  return;

err:
#ifdef _WIN32
  /*
   When SIGINT is raised on Windows, the OS creates a new thread to handle the
   interrupt. Once that thread completes, the main thread continues running 
   only to find that it's resources have already been free'd when the sigint 
   handler called mysql_end(). 
  */
  mysql_thread_end();
  return;
#else
  mysql_end(sig);
#endif  
}


#if defined(HAVE_TERMIOS_H) && defined(GWINSZ_IN_SYS_IOCTL)
sig_handler window_resize(int sig)
{
  struct winsize window_size;

  if (ioctl(fileno(stdin), TIOCGWINSZ, &window_size) == 0)
    terminal_width= window_size.ws_col;
}
#endif

static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"help", 'I', "Synonym for -?", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"auto-rehash", OPT_AUTO_REHASH,
   "Enable automatic rehashing. One doesn't need to use 'rehash' to get table "
   "and field completion, but startup and reconnecting may take a longer time. "
   "Disable with --disable-auto-rehash.",
   &opt_rehash, &opt_rehash, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0,
   0, 0},
  {"no-auto-rehash", 'A',
   "No automatic rehashing. One has to use 'rehash' to get table and field "
   "completion. This gives a quicker start of mysql and disables rehashing "
   "on reconnect.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
   {"auto-vertical-output", OPT_AUTO_VERTICAL_OUTPUT,
    "Automatically switch to vertical output mode if the result is wider "
    "than the terminal width.",
    &auto_vertical_output, &auto_vertical_output, 0, GET_BOOL, NO_ARG, 0,
    0, 0, 0, 0, 0},
  {"batch", 'B',
   "Don't use history file. Disable interactive behavior. (Enables --silent.)",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"binary-as-hex", 'b', "Print binary data as hex", &opt_binhex, &opt_binhex,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory for character set files.", &charsets_dir,
   &charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"column-type-info", OPT_COLUMN_TYPES, "Display column type information.",
   &column_types_flag, &column_types_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"comments", 'c', "Preserve comments. Send comments to the server."
   " The default is --skip-comments (discard comments), enable with --comments.",
   &preserve_comments, &preserve_comments,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use compression in server/client protocol.",
   &opt_compress, &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit.",
   0,0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log.", &default_dbug_option,
   &default_dbug_option, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit.",
   &debug_check_flag, &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", 'T', "Print some debug info at exit.", &debug_info_flag,
   &debug_info_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"database", 'D', "Database to use.", &current_db,
   &current_db, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default-character-set", OPT_DEFAULT_CHARSET,
   "Set the default character set.", &default_charset,
   &default_charset, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"delimiter", OPT_DELIMITER, "Delimiter to be used.", &delimiter_str,
   &delimiter_str, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"enable_cleartext_plugin", OPT_ENABLE_CLEARTEXT_PLUGIN, 
    "Enable/disable the clear text authentication plugin.",
   &opt_enable_cleartext_plugin, &opt_enable_cleartext_plugin, 
   0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"execute", 'e', "Execute command and quit. (Disables --force and history file.)", 0,
   0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"vertical", 'E', "Print the output of a query (rows) vertically.",
   &vertical, &vertical, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0,
   0},
  {"force", 'f', "Continue even if we get an SQL error.",
   &ignore_errors, &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"named-commands", 'G',
   "Enable named commands. Named commands mean this program's internal "
   "commands; see mysql> help . When enabled, the named commands can be "
   "used from any line of the query, otherwise only from the first line, "
   "before an enter. Disable with --disable-named-commands. This option "
   "is disabled by default.",
   &named_cmds, &named_cmds, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0,
   0, 0},
  {"ignore-spaces", 'i', "Ignore space after function names.",
   &ignore_spaces, &ignore_spaces, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"init-command", OPT_INIT_COMMAND,
   "SQL Command to execute when connecting to MySQL server. Will "
   "automatically be re-executed when reconnecting.",
   &opt_init_command, &opt_init_command, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"local-infile", OPT_LOCAL_INFILE, "Enable/disable LOAD DATA LOCAL INFILE.",
   &opt_local_infile, &opt_local_infile, 0, GET_BOOL, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"no-beep", 'b', "Turn off beep on error.", &opt_nobeep,
   &opt_nobeep, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", &current_host,
   &current_host, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"html", 'H', "Produce HTML output.", &opt_html, &opt_html,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"xml", 'X', "Produce XML output.", &opt_xml, &opt_xml, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"line-numbers", OPT_LINE_NUMBERS, "Write line numbers for errors.",
   &line_numbers, &line_numbers, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
  {"skip-line-numbers", 'L', "Don't write line number for errors.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"unbuffered", 'n', "Flush buffer after each query.", &unbuffered,
   &unbuffered, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"column-names", OPT_COLUMN_NAMES, "Write column names in results.",
   &column_names, &column_names, 0, GET_BOOL,
   NO_ARG, 1, 0, 0, 0, 0, 0},
  {"skip-column-names", 'N',
   "Don't write column names in results.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sigint-ignore", OPT_SIGINT_IGNORE, "Ignore SIGINT (CTRL-C).",
   &opt_sigint_ignore,  &opt_sigint_ignore, 0, GET_BOOL,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"one-database", 'o',
   "Ignore statements except those that occur while the default "
   "database is the one named at the command line.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef USE_POPEN
  {"pager", OPT_PAGER,
   "Pager to use to display results. If you don't supply an option, the "
   "default pager is taken from your ENV variable PAGER. Valid pagers are "
   "less, more, cat [> filename], etc. See interactive help (\\h) also. "
   "This option does not work in batch mode. Disable with --disable-pager. "
   "This option is disabled by default.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"password", 'p',
   "Password to use when connecting to server. If password is not given it's asked from the tty.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef __WIN__
  {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &opt_mysql_port,
   &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0,  0},
  {"prompt", OPT_PROMPT, "Set the mysql prompt to this value.",
   &current_prompt, &current_prompt, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol to use for connection (tcp, socket, pipe, memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"quick", 'q',
   "Don't cache result, print it row by row. This may slow down the server "
   "if the output is suspended. Doesn't use history file.",
   &quick, &quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"raw", 'r', "Write fields without conversion. Used with --batch.",
   &opt_raw_data, &opt_raw_data, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"reconnect", OPT_RECONNECT, "Reconnect if the connection is lost. Disable "
   "with --disable-reconnect. This option is enabled by default.",
   &opt_reconnect, &opt_reconnect, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0, 0},
  {"silent", 's', "Be more silent. Print results with a tab as separator, "
   "each row on new line.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef HAVE_SMEM
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", &shared_memory_base_name,
   &shared_memory_base_name, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"socket", 'S', "The socket file to use for connection.",
   &opt_mysql_unix_port, &opt_mysql_unix_port, 0, GET_STR_ALLOC,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#include "sslopt-longopts.h"
  {"table", 't', "Output in table format.", &output_tables,
   &output_tables, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"tee", OPT_TEE,
   "Append everything into outfile. See interactive help (\\h) also. "
   "Does not work in batch mode. Disable with --disable-tee. "
   "This option is disabled by default.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user", 'u', "User for login if not current user.", &current_user,
   &current_user, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"safe-updates", 'U', "Only allow UPDATE and DELETE that uses keys.",
   &safe_updates, &safe_updates, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"i-am-a-dummy", 'U', "Synonym for option --safe-updates, -U.",
   &safe_updates, &safe_updates, 0, GET_BOOL, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {"verbose", 'v', "Write more. (-v -v -v gives the table output format).", 0,
   0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"wait", 'w', "Wait and retry if connection is down.", 0, 0, 0, GET_NO_ARG,
   NO_ARG, 0, 0, 0, 0, 0, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT,
   "Number of seconds before connection timeout.",
   &opt_connect_timeout, &opt_connect_timeout, 0, GET_ULONG, REQUIRED_ARG,
   0, 0, 3600*12, 0, 0, 0},
  {"max_allowed_packet", OPT_MAX_ALLOWED_PACKET,
   "The maximum packet length to send to or receive from server.",
   &opt_max_allowed_packet, &opt_max_allowed_packet, 0,
   GET_ULONG, REQUIRED_ARG, 16 *1024L*1024L, 4096,
   (longlong) 2*1024L*1024L*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"net_buffer_length", OPT_NET_BUFFER_LENGTH,
   "The buffer size for TCP/IP and socket communication.",
   &opt_net_buffer_length, &opt_net_buffer_length, 0, GET_ULONG,
   REQUIRED_ARG, 16384, 1024, 512*1024*1024L, MALLOC_OVERHEAD, 1024, 0},
  {"select_limit", OPT_SELECT_LIMIT,
   "Automatic limit for SELECT when using --safe-updates.",
   &select_limit, &select_limit, 0, GET_ULONG, REQUIRED_ARG, 1000L,
   1, ULONG_MAX, 0, 1, 0},
  {"max_join_size", OPT_MAX_JOIN_SIZE,
   "Automatic limit for rows in a join when using --safe-updates.",
   &max_join_size, &max_join_size, 0, GET_ULONG, REQUIRED_ARG, 1000000L,
   1, ULONG_MAX, 0, 1, 0},
  {"secure-auth", OPT_SECURE_AUTH, "Refuse client connecting to server if it"
    " uses old (pre-4.1.1) protocol.", &opt_secure_auth,
    &opt_secure_auth, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"server-arg", OPT_SERVER_ARG, "Send embedded server this as a parameter.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"show-warnings", OPT_SHOW_WARNINGS, "Show warnings after every statement.",
    &show_warnings, &show_warnings, 0, GET_BOOL, NO_ARG,
    0, 0, 0, 0, 0, 0},
  {"plugin_dir", OPT_PLUGIN_DIR, "Directory for client-side plugins.",
    &opt_plugin_dir, &opt_plugin_dir, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"default_auth", OPT_DEFAULT_AUTH,
    "Default authentication client-side plugin to use.",
    &opt_default_auth, &opt_default_auth, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void usage(int version)
{
#if defined(USE_LIBEDIT_INTERFACE)
  const char* readline= "";
#else
  const char* readline= "readline";
#endif

#ifdef HAVE_READLINE
  printf("%s  Ver %s Distrib %s, for %s (%s) using %s %s\n",
	 my_progname, VER, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE,
         readline, rl_library_version);
#else
  printf("%s  Ver %s Distrib %s, for %s (%s)\n", my_progname, VER,
	MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
#endif

  if (version)
    return;
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  printf("Usage: %s [OPTIONS] [database]\n", my_progname);
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}


my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case OPT_CHARSETS_DIR:
    strmake(mysql_charsets_dir, argument, sizeof(mysql_charsets_dir) - 1);
    charsets_dir = mysql_charsets_dir;
    break;
  case OPT_DELIMITER:
    if (argument == disabled_my_option) 
    {
      strmov(delimiter, DEFAULT_DELIMITER);
    }
    else 
    {
      /* Check that delimiter does not contain a backslash */
      if (!strstr(argument, "\\")) 
      {
        strmake(delimiter, argument, sizeof(delimiter) - 1);
      }
      else 
      {
        put_info("DELIMITER cannot contain a backslash character", INFO_ERROR);
        return 0;
      } 
    }
    delimiter_length= (uint)strlen(delimiter);
    delimiter_str= delimiter;
    break;
  case OPT_LOCAL_INFILE:
    using_opt_local_infile=1;
    break;
  case OPT_ENABLE_CLEARTEXT_PLUGIN:
    using_opt_enable_cleartext_plugin= TRUE;
    break;
  case OPT_TEE:
    if (argument == disabled_my_option)
    {
      if (opt_outfile)
	end_tee();
    }
    else
      init_tee(argument);
    break;
  case OPT_PAGER:
    if (argument == disabled_my_option)
      opt_nopager= 1;
    else
    {
      opt_nopager= 0;
      if (argument && strlen(argument))
      {
	default_pager_set= 1;
	strmake(pager, argument, sizeof(pager) - 1);
	strmov(default_pager, pager);
      }
      else if (default_pager_set)
	strmov(pager, default_pager);
      else
	opt_nopager= 1;
    }
    break;
  case OPT_MYSQL_PROTOCOL:
#ifndef EMBEDDED_LIBRARY
    opt_protocol= find_type_or_exit(argument, &sql_protocol_typelib,
                                    opt->name);
#endif
    break;
  case OPT_SERVER_ARG:
#ifdef EMBEDDED_LIBRARY
    /*
      When the embedded server is being tested, the client needs to be
      able to pass command-line arguments to the embedded server so it can
      locate the language files and data directory.
    */
    if (!embedded_server_arg_count)
    {
      embedded_server_arg_count= 1;
      embedded_server_args[0]= (char*) "";
    }
    if (embedded_server_arg_count == MAX_SERVER_ARGS-1 ||
        !(embedded_server_args[embedded_server_arg_count++]=
          my_strdup(argument, MYF(MY_FAE))))
    {
        put_info("Can't use server argument", INFO_ERROR);
        return 0;
    }
#else /*EMBEDDED_LIBRARY */
    printf("WARNING: --server-arg option not supported in this configuration.\n");
#endif
    break;
  case 'A':
    opt_rehash= 0;
    break;
  case 'N':
    column_names= 0;
    break;
  case 'e':
    status.batch= 1;
    status.add_to_history= 0;
    if (!status.line_buff)
      ignore_errors= 0;                         // do it for the first -e only
    if (!(status.line_buff= batch_readline_command(status.line_buff, argument)))
      return 1;
    break;
  case 'o':
    if (argument == disabled_my_option)
      one_database= 0;
    else
      one_database= skip_updates= 1;
    break;
  case 'p':
    if (argument == disabled_my_option)
      argument= (char*) "";			// Don't require password
    if (argument)
    {
      char *start= argument;
      my_free(opt_password);
      opt_password= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		// Destroy argument
      if (*start)
	start[1]=0 ;
      tty_password= 0;
    }
    else
      tty_password= 1;
    break;
  case '#':
    DBUG_PUSH(argument ? argument : default_dbug_option);
    debug_info_flag= 1;
    break;
  case 's':
    if (argument == disabled_my_option)
      opt_silent= 0;
    else
      opt_silent++;
    break;
  case 'v':
    if (argument == disabled_my_option)
      verbose= 0;
    else
      verbose++;
    break;
  case 'B':
    status.batch= 1;
    status.add_to_history= 0;
    set_if_bigger(opt_silent,1);                         // more silent
    break;
  case 'W':
#ifdef __WIN__
    opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
    break;
#include <sslopt-case.h>
  case 'V':
    usage(1);
    exit(0);
  case 'I':
  case '?':
    usage(0);
    exit(0);
  }
  return 0;
}


static int get_options(int argc, char **argv)
{
  char *tmp, *pagpoint;
  int ho_error;
  MYSQL_PARAMETERS *mysql_params= mysql_get_parameters();

  tmp= (char *) getenv("MYSQL_HOST");
  if (tmp)
    current_host= my_strdup(tmp, MYF(MY_WME));

  pagpoint= getenv("PAGER");
  if (!((char*) (pagpoint)))
  {
    strmov(pager, "stdout");
    opt_nopager= 1;
  }
  else
    strmov(pager, pagpoint);
  strmov(default_pager, pager);

  opt_max_allowed_packet= *mysql_params->p_max_allowed_packet;
  opt_net_buffer_length= *mysql_params->p_net_buffer_length;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  *mysql_params->p_max_allowed_packet= opt_max_allowed_packet;
  *mysql_params->p_net_buffer_length= opt_net_buffer_length;

  if (status.batch) /* disable pager and outfile in this case */
  {
    strmov(default_pager, "stdout");
    strmov(pager, "stdout");
    opt_nopager= 1;
    default_pager_set= 0;
    opt_outfile= 0;
    opt_reconnect= 0;
    connect_flag= 0; /* Not in interactive mode */
  }
  
  if (argc > 1)
  {
    usage(0);
    exit(1);
  }
  if (argc == 1)
  {
    skip_updates= 0;
    my_free(current_db);
    current_db= my_strdup(*argv, MYF(MY_WME));
  }
  if (tty_password)
    opt_password= get_tty_password(NullS);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;

  if (ignore_spaces)
    connect_flag|= CLIENT_IGNORE_SPACE;

  return(0);
}

static int read_and_execute(bool interactive)
{
#if defined(__WIN__)
  String tmpbuf;
  String buffer;
#endif

  char	*line= NULL;
  char	in_string=0;
  ulong line_number=0;
  bool ml_comment= 0;  
  COMMANDS *com;
  status.exit_status=1;

  for (;;)
  {
    if (!interactive)
    {
      line=batch_readline(status.line_buff);
      /*
        Skip UTF8 Byte Order Marker (BOM) 0xEFBBBF.
        Editors like "notepad" put this marker in
        the very beginning of a text file when
        you save the file using "Unicode UTF-8" format.
      */
      if (line && !line_number &&
           (uchar) line[0] == 0xEF &&
           (uchar) line[1] == 0xBB &&
           (uchar) line[2] == 0xBF)
        line+= 3;
      line_number++;
      if (!glob_buffer.length())
	status.query_start_line=line_number;
    }
    else
    {
      char *prompt= (char*) (ml_comment ? "   /*> " :
                             glob_buffer.is_empty() ?  construct_prompt() :
			     !in_string ? "    -> " :
			     in_string == '\'' ?
			     "    '> " : (in_string == '`' ?
			     "    `> " :
			     "    \"> "));
      if (opt_outfile && glob_buffer.is_empty())
	fflush(OUTFILE);

#if defined(__WIN__)
      tee_fputs(prompt, stdout);
      if (!tmpbuf.is_alloced())
        tmpbuf.alloc(65535);
      tmpbuf.length(0);
      buffer.length(0);
      size_t clen;
      do
      {
	line= my_cgets((char*)tmpbuf.ptr(), tmpbuf.alloced_length()-1, &clen);
        buffer.append(line, clen);
        /* 
           if we got buffer fully filled than there is a chance that
           something else is still in console input buffer
        */
      } while (tmpbuf.alloced_length() <= clen);
      /* 
        An empty line is returned from my_cgets when there's error reading :
        Ctrl-c for example
      */
      if (line)
        line= buffer.c_ptr();
#else
      if (opt_outfile)
	fputs(prompt, OUTFILE);
      /*
        free the previous entered line.
        Note: my_free() cannot be used here as the memory was allocated under
        the readline/libedit library.
      */
      if (line)
        free(line);
      line= readline(prompt);
#endif /* defined(__WIN__) */

      /*
        When Ctrl+d or Ctrl+z is pressed, the line may be NULL on some OS
        which may cause coredump.
      */
      if (opt_outfile && line)
	fprintf(OUTFILE, "%s\n", line);
    }
    // End of file or system error
    if (!line)
    {
      if (status.line_buff && status.line_buff->error)
        status.exit_status= 1;
      else
        status.exit_status= 0;
      break;
    }

    /*
      Check if line is a mysql command line
      (We want to allow help, print and clear anywhere at line start
    */
    if ((named_cmds || glob_buffer.is_empty())
	&& !ml_comment && !in_string && (com=find_command(line,0)))
    {
      if ((*com->func)(&glob_buffer,line) > 0)
	break;
      if (glob_buffer.is_empty())		// If buffer was emptied
	in_string=0;
#ifdef HAVE_READLINE
      if (interactive && status.add_to_history && not_in_history(line))
	add_history(line);
#endif
      continue;
    }
    if (add_line(glob_buffer, line, &in_string, &ml_comment,
                 status.line_buff ? status.line_buff->truncated : 0))
      break;
  }
  /* if in batch mode, send last query even if it doesn't end with \g or go */

  if (!interactive && !status.exit_status)
  {
    remove_cntrl(glob_buffer);
    if (!glob_buffer.is_empty())
    {
      status.exit_status=1;
      if (com_go(&glob_buffer,line) <= 0)
	status.exit_status=0;
    }
  }

#if defined(__WIN__)
  buffer.free();
  tmpbuf.free();
#else
  if (interactive)
    /*
      free the last entered line.
      Note: my_free() cannot be used here as the memory was allocated under
      the readline/libedit library.
    */
    free(line);
#endif


  return status.exit_status;
}


static COMMANDS *find_command(char *name,char cmd_char)
{
  uint len;
  char *end;
  DBUG_ENTER("find_command");
  DBUG_PRINT("enter",("name: '%s'  char: %d", name ? name : "NULL", cmd_char));

  if (!name)
  {
    len=0;
    end=0;
  }
  else
  {
    while (my_isspace(charset_info,*name))
      name++;
    /*
      If there is an \\g in the row or if the row has a delimiter but
      this is not a delimiter command, let add_line() take care of
      parsing the row and calling find_command()
    */
    if (strstr(name, "\\g") || (strstr(name, delimiter) &&
                                !(strlen(name) >= 9 &&
                                  !my_strnncoll(&my_charset_latin1,
                                                (uchar*) name, 9,
                                                (const uchar*) "delimiter",
                                                9))))
      DBUG_RETURN((COMMANDS *) 0);
    if ((end=strcont(name," \t")))
    {
      len=(uint) (end - name);
      while (my_isspace(charset_info,*end))
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
	((name &&
	  !my_strnncoll(&my_charset_latin1, (uchar*)name, len,
				     (uchar*)commands[i].name,len) &&
	  !commands[i].name[len] &&
	  (!end || (end && commands[i].takes_params))) ||
	 (!name && commands[i].cmd_char == cmd_char)))
    {
      DBUG_PRINT("exit",("found command: %s", commands[i].name));
      DBUG_RETURN(&commands[i]);
    }
  }
  DBUG_RETURN((COMMANDS *) 0);
}


static bool add_line(String &buffer,char *line,char *in_string,
                     bool *ml_comment, bool truncated)
{
  uchar inchar;
  char buff[80], *pos, *out;
  COMMANDS *com;
  bool need_space= 0;
  bool ss_comment= 0;
  DBUG_ENTER("add_line");

  if (!line[0] && buffer.is_empty())
    DBUG_RETURN(0);
#ifdef HAVE_READLINE
  if (status.add_to_history && line[0] && not_in_history(line))
    add_history(line);
#endif
  char *end_of_line=line+(uint) strlen(line);

  for (pos=out=line ; (inchar= (uchar) *pos) ; pos++)
  {
    if (!preserve_comments)
    {
      // Skip spaces at the beginning of a statement
      if (my_isspace(charset_info,inchar) && (out == line) &&
          buffer.is_empty())
        continue;
    }
        
#ifdef USE_MB
    // Accept multi-byte characters as-is
    int length;
    if (use_mb(charset_info) &&
        (length= my_ismbchar(charset_info, pos, end_of_line)))
    {
      if (!*ml_comment || preserve_comments)
      {
        while (length--)
          *out++ = *pos++;
        pos--;
      }
      else
        pos+= length - 1;
      continue;
    }
#endif
    if (!*ml_comment && inchar == '\\' &&
        !(*in_string && 
          (mysql.server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES)))
    {
      // Found possbile one character command like \c

      if (!(inchar = (uchar) *++pos))
	break;				// readline adds one '\'
      if (*in_string || inchar == 'N')	// \N is short for NULL
      {					// Don't allow commands in string
	*out++='\\';
        if ((inchar == '`') && (*in_string == inchar))
          pos--;
        else
	  *out++= (char) inchar;
	continue;
      }
      if ((com=find_command(NullS,(char) inchar)))
      {
        // Flush previously accepted characters
        if (out != line)
        {
          buffer.append(line, (uint) (out-line));
          out= line;
        }
        
        if ((*com->func)(&buffer,pos-1) > 0)
          DBUG_RETURN(1);                       // Quit
        if (com->takes_params)
        {
          if (ss_comment)
          {
            /*
              If a client-side macro appears inside a server-side comment,
              discard all characters in the comment after the macro (that is,
              until the end of the comment rather than the next delimiter)
            */
            for (pos++; *pos && (*pos != '*' || *(pos + 1) != '/'); pos++)
              ;
            pos--;
          }
          else
          {
            for (pos++ ;
                 *pos && (*pos != *delimiter ||
                          !is_prefix(pos + 1, delimiter + 1)) ; pos++)
              ;	// Remove parameters
            if (!*pos)
              pos--;
            else 
              pos+= delimiter_length - 1; // Point at last delim char
          }
        }
      }
      else
      {
	sprintf(buff,"Unknown command '\\%c'.",inchar);
	if (put_info(buff,INFO_ERROR) > 0)
	  DBUG_RETURN(1);
	*out++='\\';
	*out++=(char) inchar;
	continue;
      }
    }
    else if (!*ml_comment && !*in_string && is_prefix(pos, delimiter))
    {
      // Found a statement. Continue parsing after the delimiter
      pos+= delimiter_length;

      if (preserve_comments)
      {
        while (my_isspace(charset_info, *pos))
          *out++= *pos++;
      }
      // Flush previously accepted characters
      if (out != line)
      {
        buffer.append(line, (uint32) (out-line));
        out= line;
      }

      if (preserve_comments && ((*pos == '#') ||
                                ((*pos == '-') &&
                                 (pos[1] == '-') &&
                                 my_isspace(charset_info, pos[2]))))
      {
        // Add trailing single line comments to this statement
        buffer.append(pos);
        pos+= strlen(pos);
      }

      pos--;

      if ((com= find_command(buffer.c_ptr(), 0)))
      {
          
        if ((*com->func)(&buffer, buffer.c_ptr()) > 0)
          DBUG_RETURN(1);                       // Quit 
      }
      else
      {
        if (com_go(&buffer, 0) > 0)             // < 0 is not fatal
          DBUG_RETURN(1);
      }
      buffer.length(0);
    }
    else if (!*ml_comment && (!*in_string && (inchar == '#' ||
                                              (inchar == '-' && pos[1] == '-' &&
                              /*
                                The third byte is either whitespace or is the
                                end of the line -- which would occur only
                                because of the user sending newline -- which is
                                itself whitespace and should also match.
                              */
			      (my_isspace(charset_info,pos[2]) ||
                               !pos[2])))))
    {
      // Flush previously accepted characters
      if (out != line)
      {
        buffer.append(line, (uint32) (out - line));
        out= line;
      }

      // comment to end of line
      if (preserve_comments)
      {
        bool started_with_nothing= !buffer.length();

        buffer.append(pos);

        /*
          A single-line comment by itself gets sent immediately so that
          client commands (delimiter, status, etc) will be interpreted on
          the next line.
        */
        if (started_with_nothing)
        {
          if (com_go(&buffer, 0) > 0)             // < 0 is not fatal
            DBUG_RETURN(1);
          buffer.length(0);
        }
      }

      break;
    }
    else if (!*in_string && inchar == '/' && *(pos+1) == '*' &&
	     *(pos+2) != '!')
    {
      if (preserve_comments)
      {
        *out++= *pos++;                       // copy '/'
        *out++= *pos;                         // copy '*'
      }
      else
        pos++;
      *ml_comment= 1;
      if (out != line)
      {
        buffer.append(line,(uint) (out-line));
        out=line;
      }
    }
    else if (*ml_comment && !ss_comment && inchar == '*' && *(pos + 1) == '/')
    {
      if (preserve_comments)
      {
        *out++= *pos++;                       // copy '*'
        *out++= *pos;                         // copy '/'
      }
      else
        pos++;
      *ml_comment= 0;
      if (out != line)
      {
        buffer.append(line, (uint32) (out - line));
        out= line;
      }
      // Consumed a 2 chars or more, and will add 1 at most,
      // so using the 'line' buffer to edit data in place is ok.
      need_space= 1;
    }      
    else
    {						// Add found char to buffer
      if (!*in_string && inchar == '/' && *(pos + 1) == '*' &&
          *(pos + 2) == '!')
        ss_comment= 1;
      else if (!*in_string && ss_comment && inchar == '*' && *(pos + 1) == '/')
        ss_comment= 0;
      if (inchar == *in_string)
	*in_string= 0;
      else if (!*ml_comment && !*in_string &&
	       (inchar == '\'' || inchar == '"' || inchar == '`'))
	*in_string= (char) inchar;
      if (!*ml_comment || preserve_comments)
      {
        if (need_space && !my_isspace(charset_info, (char)inchar))
          *out++= ' ';
        need_space= 0;
        *out++= (char) inchar;
      }
    }
  }
  if (out != line || !buffer.is_empty())
  {
    uint length=(uint) (out-line);

    if (!truncated && (length < 9 ||
                       my_strnncoll (charset_info, (uchar *)line, 9,
                                     (const uchar *) "delimiter", 9) ||
                       (*in_string || *ml_comment)))
    {
      /* 
        Don't add a new line in case there's a DELIMITER command to be 
        added to the glob buffer (e.g. on processing a line like 
        "<command>;DELIMITER <non-eof>") : similar to how a new line is 
        not added in the case when the DELIMITER is the first command 
        entered with an empty glob buffer. However, if the delimiter is
        part of a string or a comment, the new line should be added. (e.g.
        SELECT '\ndelimiter\n';\n)
      */
      *out++='\n';
      length++;
    }
    if (buffer.length() + length >= buffer.alloced_length())
      buffer.realloc(buffer.length()+length+IO_SIZE);
    if ((!*ml_comment || preserve_comments) && buffer.append(line, length))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

/*****************************************************************
	    Interface to Readline Completion
******************************************************************/

#ifdef HAVE_READLINE

C_MODE_START
static char *new_command_generator(const char *text, int);
static char **new_mysql_completion(const char *text, int start, int end);
C_MODE_END

/*
  Tell the GNU Readline library how to complete.  We want to try to complete
  on command names if this is the first word in the line, or on filenames
  if not.
*/

#if defined(USE_NEW_READLINE_INTERFACE) 
static int fake_magic_space(int, int);
extern "C" char *no_completion(const char*,int)
#elif defined(USE_LIBEDIT_INTERFACE)
static int fake_magic_space(const char *, int);
extern "C" int no_completion(const char*,int)
#else
extern "C" char *no_completion()
#endif
{
  return 0;					/* No filename completion */
}

/*	glues pieces of history back together if in pieces   */
static void fix_history(String *final_command) 
{
  int total_lines = 1;
  char *ptr = final_command->c_ptr();
  String fixed_buffer; 	/* Converted buffer */
  char str_char = '\0';  /* Character if we are in a string or not */
  
  /* find out how many lines we have and remove newlines */
  while (*ptr != '\0') 
  {
    switch (*ptr) {
      /* string character */
    case '"':
    case '\'':
    case '`':
      if (str_char == '\0')	/* open string */
	str_char = *ptr;
      else if (str_char == *ptr)   /* close string */
	str_char = '\0';
      fixed_buffer.append(ptr,1);
      break;
    case '\n':
      /* 
	 not in string, change to space
	 if in string, leave it alone 
      */
      fixed_buffer.append(str_char == '\0' ? " " : "\n");
      total_lines++;
      break;
    case '\\':
      fixed_buffer.append('\\');
      /* need to see if the backslash is escaping anything */
      if (str_char) 
      {
	ptr++;
	/* special characters that need escaping */
	if (*ptr == '\'' || *ptr == '"' || *ptr == '\\')
	  fixed_buffer.append(ptr,1);
	else
	  ptr--;
      }
      break;
      
    default:
      fixed_buffer.append(ptr,1);
    }
    ptr++;
  }
  if (total_lines > 1)			
    add_history(fixed_buffer.ptr());
}

/*	
  returns 0 if line matches the previous history entry
  returns 1 if the line doesn't match the previous history entry
*/
static int not_in_history(const char *line) 
{
  HIST_ENTRY *oldhist = history_get(history_length);
  
  if (oldhist == 0)
    return 1;
  if (strcmp(oldhist->line,line) == 0)
    return 0;
  return 1;
}


#if defined(USE_NEW_READLINE_INTERFACE)
static int fake_magic_space(int, int)
#else
static int fake_magic_space(const char *, int)
#endif
{
  rl_insert(1, ' ');
  return 0;
}


static void initialize_readline (char *name)
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name = name;

  /* Tell the completer that we want a crack first. */
#if defined(USE_NEW_READLINE_INTERFACE)
  rl_attempted_completion_function= (rl_completion_func_t*)&new_mysql_completion;
  rl_completion_entry_function= (rl_compentry_func_t*)&no_completion;

  rl_add_defun("magic-space", (rl_command_func_t *)&fake_magic_space, -1);
#elif defined(USE_LIBEDIT_INTERFACE)
#ifdef HAVE_LOCALE_H
  setlocale(LC_ALL,""); /* so as libedit use isprint */
#endif
  rl_attempted_completion_function= (CPPFunction*)&new_mysql_completion;
  rl_completion_entry_function= &no_completion;
  rl_add_defun("magic-space", (Function*)&fake_magic_space, -1);
#else
  rl_attempted_completion_function= (CPPFunction*)&new_mysql_completion;
  rl_completion_entry_function= &no_completion;
#endif
}

/*
  Attempt to complete on the contents of TEXT.  START and END show the
  region of TEXT that contains the word to complete.  We can use the
  entire line in case we want to do some simple parsing.  Return the
  array of matches, or NULL if there aren't any.
*/

static char **new_mysql_completion(const char *text,
                                   int start __attribute__((unused)),
                                   int end __attribute__((unused)))
{
  if (!status.batch && !quick)
#if defined(USE_NEW_READLINE_INTERFACE)
    return rl_completion_matches(text, new_command_generator);
#else
    return completion_matches((char *)text, (CPFunction *)new_command_generator);
#endif
  else
    return (char**) 0;
}

static char *new_command_generator(const char *text,int state)
{
  static int textlen;
  char *ptr;
  static Bucket *b;
  static entry *e;
  static uint i;

  if (!state)
    textlen=(uint) strlen(text);

  if (textlen>0)
  {						/* lookup in the hash */
    if (!state)
    {
      uint len;

      b = find_all_matches(&ht,text,(uint) strlen(text),&len);
      if (!b)
	return NullS;
      e = b->pData;
    }

    if (e)
    {
      ptr= strdup(e->str);
      e = e->pNext;
      return ptr;
    }
  }
  else
  { /* traverse the entire hash, ugly but works */

    if (!state)
    {
      /* find the first used bucket */
      for (i=0 ; i < ht.nTableSize ; i++)
      {
	if (ht.arBuckets[i])
	{
	  b = ht.arBuckets[i];
	  e = b->pData;
	  break;
	}
      }
    }
    ptr= NullS;
    while (e && !ptr)
    {					/* find valid entry in bucket */
      if ((uint) strlen(e->str) == b->nKeyLength)
	ptr = strdup(e->str);
      /* find the next used entry */
      e = e->pNext;
      if (!e)
      { /* find the next used bucket */
	b = b->pNext;
	if (!b)
	{
	  for (i++ ; i<ht.nTableSize; i++)
	  {
	    if (ht.arBuckets[i])
	    {
	      b = ht.arBuckets[i];
	      e = b->pData;
	      break;
	    }
	  }
	}
	else
	  e = b->pData;
      }
    }
    if (ptr)
      return ptr;
  }
  return NullS;
}


/* Build up the completion hash */

static void build_completion_hash(bool rehash, bool write_info)
{
  COMMANDS *cmd=commands;
  MYSQL_RES *databases=0,*tables=0;
  MYSQL_RES *fields;
  static char ***field_names= 0;
  MYSQL_ROW database_row,table_row;
  MYSQL_FIELD *sql_field;
  char buf[NAME_LEN*2+2];		 // table name plus field name plus 2
  int i,j,num_fields;
  DBUG_ENTER("build_completion_hash");

  if (status.batch || quick || !current_db)
    DBUG_VOID_RETURN;			// We don't need completion in batches
  if (!rehash)
    DBUG_VOID_RETURN;

  /* Free old used memory */
  if (field_names)
    field_names=0;
  completion_hash_clean(&ht);
  free_root(&hash_mem_root,MYF(0));

  /* hash this file's known subset of SQL commands */
  while (cmd->name) {
    add_word(&ht,(char*) cmd->name);
    cmd++;
  }

  /* hash MySQL functions (to be implemented) */

  /* hash all database names */
  if (mysql_query(&mysql,"show databases") == 0)
  {
    if (!(databases = mysql_store_result(&mysql)))
      put_info(mysql_error(&mysql),INFO_INFO);
    else
    {
      while ((database_row=mysql_fetch_row(databases)))
      {
	char *str=strdup_root(&hash_mem_root, (char*) database_row[0]);
	if (str)
	  add_word(&ht,(char*) str);
      }
      mysql_free_result(databases);
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
	char *str=strdup_root(&hash_mem_root, (char*) table_row[0]);
	if (str &&
	    !completion_hash_exists(&ht,(char*) str, (uint) strlen(str)))
	  add_word(&ht,str);
      }
    }
  }

  /* hash all field names, both with the table prefix and without it */
  if (!tables)					/* no tables */
  {
    DBUG_VOID_RETURN;
  }
  mysql_data_seek(tables,0);
  if (!(field_names= (char ***) alloc_root(&hash_mem_root,sizeof(char **) *
					   (uint) (mysql_num_rows(tables)+1))))
  {
    mysql_free_result(tables);
    DBUG_VOID_RETURN;
  }
  i=0;
  while ((table_row=mysql_fetch_row(tables)))
  {
    if ((fields=mysql_list_fields(&mysql,(const char*) table_row[0],NullS)))
    {
      num_fields=mysql_num_fields(fields);
      if (!(field_names[i] = (char **) alloc_root(&hash_mem_root,
						  sizeof(char *) *
						  (num_fields*2+1))))
      {
        mysql_free_result(fields);
        break;
      }
      field_names[i][num_fields*2]= NULL;
      j=0;
      while ((sql_field=mysql_fetch_field(fields)))
      {
	sprintf(buf,"%.64s.%.64s",table_row[0],sql_field->name);
	field_names[i][j] = strdup_root(&hash_mem_root,buf);
	add_word(&ht,field_names[i][j]);
	field_names[i][num_fields+j] = strdup_root(&hash_mem_root,
						   sql_field->name);
	if (!completion_hash_exists(&ht,field_names[i][num_fields+j],
				    (uint) strlen(field_names[i][num_fields+j])))
	  add_word(&ht,field_names[i][num_fields+j]);
	j++;
      }
      mysql_free_result(fields);
    }
    else
      field_names[i]= 0;

    i++;
  }
  mysql_free_result(tables);
  field_names[i]=0;				// End pointer
  DBUG_VOID_RETURN;
}

	/* for gnu readline */

#ifndef HAVE_INDEX
extern "C" {
extern char *index(const char *,int c),*rindex(const char *,int);

char *index(const char *s,int c)
{
  for (;;)
  {
     if (*s == (char) c) return (char*) s;
     if (!*s++) return NullS;
  }
}

char *rindex(const char *s,int c)
{
  reg3 char *t;

  t = NullS;
  do if (*s == (char) c) t = (char*) s; while (*s++);
  return (char*) t;
}
}
#endif
#endif /* HAVE_READLINE */


static int reconnect(void)
{
  /* purecov: begin tested */
  if (opt_reconnect)
  {
    put_info("No connection. Trying to reconnect...",INFO_INFO);
    (void) com_connect((String *) 0, 0);
    if (opt_rehash)
      com_rehash(NULL, NULL);
  }
  if (!connected)
    return put_info("Can't connect to the server\n",INFO_ERROR);
  /* purecov: end */
  return 0;
}

static void get_current_db()
{
  MYSQL_RES *res;

  /* If one_database is set, current_db is not supposed to change. */
  if (one_database)
    return;

  my_free(current_db);
  current_db= NULL;
  /* In case of error below current_db will be NULL */
  if (!mysql_query(&mysql, "SELECT DATABASE()") &&
      (res= mysql_use_result(&mysql)))
  {
    MYSQL_ROW row= mysql_fetch_row(res);
    if (row && row[0])
      current_db= my_strdup(row[0], MYF(MY_WME));
    mysql_free_result(res);
  }
}

/***************************************************************************
 The different commands
***************************************************************************/

int mysql_real_query_for_lazy(const char *buf, int length)
{
  for (uint retry=0;; retry++)
  {
    int error;
    if (!mysql_real_query(&mysql,buf,length))
      return 0;
    error= put_error(&mysql);
    if (mysql_errno(&mysql) != CR_SERVER_GONE_ERROR || retry > 1 ||
        !opt_reconnect)
      return error;
    if (reconnect())
      return error;
  }
}

int mysql_store_result_for_lazy(MYSQL_RES **result)
{
  if ((*result=mysql_store_result(&mysql)))
    return 0;

  if (mysql_error(&mysql)[0])
    return put_error(&mysql);
  return 0;
}

static void print_help_item(MYSQL_ROW *cur, int num_name, int num_cat, char *last_char)
{
  char ccat= (*cur)[num_cat][0];
  if (*last_char != ccat)
  {
    put_info(ccat == 'Y' ? "categories:" : "topics:", INFO_INFO);
    *last_char= ccat;
  }
  tee_fprintf(PAGER, "   %s\n", (*cur)[num_name]);
}


static int com_server_help(String *buffer __attribute__((unused)),
			   char *line __attribute__((unused)), char *help_arg)
{
  MYSQL_ROW cur;
  const char *server_cmd;
  char cmd_buf[100 + 1];
  MYSQL_RES *result;
  int error;
  
  if (help_arg[0] != '\'')
  {
	char *end_arg= strend(help_arg);
	if(--end_arg)
	{
		while (my_isspace(charset_info,*end_arg))
          end_arg--;
		*++end_arg= '\0';
	}
	(void) strxnmov(cmd_buf, sizeof(cmd_buf), "help '", help_arg, "'", NullS);
  }
  else
    (void) strxnmov(cmd_buf, sizeof(cmd_buf), "help ", help_arg, NullS);

  server_cmd= cmd_buf;

  if (!status.batch)
  {
    old_buffer= *buffer;
    old_buffer.copy();
  }

  if (!connected && reconnect())
    return 1;

  if ((error= mysql_real_query_for_lazy(server_cmd,(int)strlen(server_cmd))) ||
      (error= mysql_store_result_for_lazy(&result)))
    return error;

  if (result)
  {
    unsigned int num_fields= mysql_num_fields(result);
    my_ulonglong num_rows= mysql_num_rows(result);
    mysql_fetch_fields(result);
    if (num_fields==3 && num_rows==1)
    {
      if (!(cur= mysql_fetch_row(result)))
      {
	error= -1;
	goto err;
      }

      init_pager();
      tee_fprintf(PAGER,   "Name: \'%s\'\n", cur[0]);
      tee_fprintf(PAGER,   "Description:\n%s", cur[1]);
      if (cur[2] && *((char*)cur[2]))
	tee_fprintf(PAGER, "Examples:\n%s", cur[2]);
      tee_fprintf(PAGER,   "\n");
      end_pager();
    }
    else if (num_fields >= 2 && num_rows)
    {
      init_pager();
      char last_char= 0;

      int num_name= 0, num_cat= 0;
      LINT_INIT(num_name);
      LINT_INIT(num_cat);

      if (num_fields == 2)
      {
	put_info("Many help items for your request exist.", INFO_INFO);
	put_info("To make a more specific request, please type 'help <item>',\nwhere <item> is one of the following", INFO_INFO);
	num_name= 0;
	num_cat= 1;
      }
      else if ((cur= mysql_fetch_row(result)))
      {
	tee_fprintf(PAGER, "You asked for help about help category: \"%s\"\n", cur[0]);
	put_info("For more information, type 'help <item>', where <item> is one of the following", INFO_INFO);
	num_name= 1;
	num_cat= 2;
	print_help_item(&cur,1,2,&last_char);
      }

      while ((cur= mysql_fetch_row(result)))
	print_help_item(&cur,num_name,num_cat,&last_char);
      tee_fprintf(PAGER, "\n");
      end_pager();
    }
    else
    {
      put_info("\nNothing found", INFO_INFO);
      if (strncasecmp(server_cmd, "help 'contents'", 15) == 0)
      {
         put_info("\nPlease check if 'help tables' are loaded.\n", INFO_INFO); 
         goto err;
      }
      put_info("Please try to run 'help contents' for a list of all accessible topics\n", INFO_INFO);
    }
  }

err:
  mysql_free_result(result);
  return error;
}

static int
com_help(String *buffer __attribute__((unused)),
	 char *line __attribute__((unused)))
{
  reg1 int i, j;
  char * help_arg= strchr(line,' '), buff[32], *end;
  if (help_arg)
  {
    while (my_isspace(charset_info,*help_arg))
      help_arg++;
	if (*help_arg)	  
	  return com_server_help(buffer,line,help_arg);
  }

  put_info("\nFor information about MySQL products and services, visit:\n"
           "   http://www.mysql.com/\n"
           "For developer information, including the MySQL Reference Manual, "
           "visit:\n"
           "   http://dev.mysql.com/\n"
           "To buy MySQL Enterprise support, training, or other products, visit:\n"
           "   https://shop.mysql.com/\n", INFO_INFO);
  put_info("List of all MySQL commands:", INFO_INFO);
  if (!named_cmds)
    put_info("Note that all text commands must be first on line and end with ';'",INFO_INFO);
  for (i = 0; commands[i].name; i++)
  {
    end= strmov(buff, commands[i].name);
    for (j= (int)strlen(commands[i].name); j < 10; j++)
      end= strmov(end, " ");
    if (commands[i].func)
      tee_fprintf(stdout, "%s(\\%c) %s\n", buff,
		  commands[i].cmd_char, commands[i].doc);
  }
  if (connected && mysql_get_server_version(&mysql) >= 40100)
    put_info("\nFor server side help, type 'help contents'\n", INFO_INFO);
  return 0;
}


	/* ARGSUSED */
static int
com_clear(String *buffer,char *line __attribute__((unused)))
{
#ifdef HAVE_READLINE
  if (status.add_to_history)
    fix_history(buffer);
#endif
  buffer->length(0);
  return 0;
}

	/* ARGSUSED */
static int
com_charset(String *buffer __attribute__((unused)), char *line)
{
  char buff[256], *param;
  CHARSET_INFO * new_cs;
  strmake(buff, line, sizeof(buff) - 1);
  param= get_arg(buff, 0);
  if (!param || !*param)
  {
    return put_info("Usage: \\C charset_name | charset charset_name", 
		    INFO_ERROR, 0);
  }
  new_cs= get_charset_by_csname(param, MY_CS_PRIMARY, MYF(MY_WME));
  if (new_cs)
  {
    charset_info= new_cs;
    mysql_set_character_set(&mysql, charset_info->csname);
    default_charset= (char *)charset_info->csname;
    put_info("Charset changed", INFO_INFO);
  }
  else put_info("Charset is not found", INFO_INFO);
  return 0;
}

/*
  Execute command
  Returns: 0  if ok
          -1 if not fatal error
	  1  if fatal error
*/


static int
com_go(String *buffer,char *line __attribute__((unused)))
{
  char		buff[200]; /* about 110 chars used so far */
  char		time_buff[52+3+1]; /* time max + space&parens + NUL */
  MYSQL_RES	*result;
  ulong		timer, warnings= 0;
  uint		error= 0;
  int           err= 0;

  interrupted_query= 0;
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
    return opt_reconnect ? -1 : 1;          // Fatal error
  }
  if (verbose)
    (void) com_print(buffer,0);

  if (skip_updates &&
      (buffer->length() < 4 || my_strnncoll(charset_info,
					    (const uchar*)buffer->ptr(),4,
					    (const uchar*)"SET ",4)))
  {
    (void) put_info("Ignoring query to other database",INFO_INFO);
    return 0;
  }

  timer=start_timer();
  executing_query= 1;
  error= mysql_real_query_for_lazy(buffer->ptr(),buffer->length());

#ifdef HAVE_READLINE
  if (status.add_to_history) 
  {  
    buffer->append(vertical ? "\\G" : delimiter);
    /* Append final command onto history */
    fix_history(buffer);
  }
#endif

  buffer->length(0);

  if (error)
    goto end;

  do
  {
    char *pos;

    if (quick)
    {
      if (!(result=mysql_use_result(&mysql)) && mysql_field_count(&mysql))
      {
        error= put_error(&mysql);
        goto end;
      }
    }
    else
    {
      error= mysql_store_result_for_lazy(&result);
      if (error)
        goto end;
    }

    if (verbose >= 3 || !opt_silent)
      mysql_end_timer(timer,time_buff);
    else
      time_buff[0]= '\0';

    /* Every branch must truncate  buff . */
    if (result)
    {
      if (!mysql_num_rows(result) && ! quick && !column_types_flag)
      {
	strmov(buff, "Empty set");
        if (opt_xml)
        { 
          /*
            We must print XML header and footer
            to produce a well-formed XML even if
            the result set is empty (Bug#27608).
          */
          init_pager();
          print_table_data_xml(result);
          end_pager();
        }
      }
      else
      {
	init_pager();
	if (opt_html)
	  print_table_data_html(result);
	else if (opt_xml)
	  print_table_data_xml(result);
        else if (vertical || (auto_vertical_output &&
                (terminal_width < get_result_width(result))))
	  print_table_data_vertically(result);
	else if (opt_silent && verbose <= 2 && !output_tables)
	  print_tab_data(result);
	else
	  print_table_data(result);
	sprintf(buff,"%ld %s in set",
		(long) mysql_num_rows(result),
		(long) mysql_num_rows(result) == 1 ? "row" : "rows");
	end_pager();
        if (mysql_errno(&mysql))
          error= put_error(&mysql);
      }
    }
    else if (mysql_affected_rows(&mysql) == ~(ulonglong) 0)
      strmov(buff,"Query OK");
    else
      sprintf(buff,"Query OK, %ld %s affected",
	      (long) mysql_affected_rows(&mysql),
	      (long) mysql_affected_rows(&mysql) == 1 ? "row" : "rows");

    pos=strend(buff);
    if ((warnings= mysql_warning_count(&mysql)))
    {
      *pos++= ',';
      *pos++= ' ';
      pos=int10_to_str(warnings, pos, 10);
      pos=strmov(pos, " warning");
      if (warnings != 1)
	*pos++= 's';
    }
    strmov(pos, time_buff);
    put_info(buff,INFO_RESULT);
    if (mysql_info(&mysql))
      put_info(mysql_info(&mysql),INFO_RESULT);
    put_info("",INFO_RESULT);			// Empty row

    if (result && !mysql_eof(result))	/* Something wrong when using quick */
      error= put_error(&mysql);
    else if (unbuffered)
      fflush(stdout);
    mysql_free_result(result);
  } while (!(err= mysql_next_result(&mysql)));
  if (err >= 1)
    error= put_error(&mysql);

end:

 /* Show warnings if any or error occured */
  if (show_warnings == 1 && (warnings >= 1 || error))
    print_warnings();

  if (!error && !status.batch && 
      (mysql.server_status & SERVER_STATUS_DB_DROPPED))
    get_current_db();

  executing_query= 0;
  return error;				/* New command follows */
}


static void init_pager()
{
#ifdef USE_POPEN
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
#ifdef USE_POPEN
  if (!opt_nopager)
    pclose(PAGER);
#endif
}


static void init_tee(const char *file_name)
{
  FILE* new_outfile;
  if (opt_outfile)
    end_tee();
  if (!(new_outfile= my_fopen(file_name, O_APPEND | O_WRONLY, MYF(MY_WME))))
  {
    tee_fprintf(stdout, "Error logging to file '%s'\n", file_name);
    return;
  }
  OUTFILE = new_outfile;
  strmake(outfile, file_name, FN_REFLEN-1);
  tee_fprintf(stdout, "Logging to file '%s'\n", file_name);
  opt_outfile= 1;
  return;
}


static void end_tee()
{
  my_fclose(OUTFILE, MYF(0));
  OUTFILE= 0;
  opt_outfile= 0;
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


static const char *fieldtype2str(enum enum_field_types type)
{
  switch (type) {
    case MYSQL_TYPE_BIT:         return "BIT";
    case MYSQL_TYPE_BLOB:        return "BLOB";
    case MYSQL_TYPE_DATE:        return "DATE";
    case MYSQL_TYPE_DATETIME:    return "DATETIME";
    case MYSQL_TYPE_NEWDECIMAL:  return "NEWDECIMAL";
    case MYSQL_TYPE_DECIMAL:     return "DECIMAL";
    case MYSQL_TYPE_DOUBLE:      return "DOUBLE";
    case MYSQL_TYPE_ENUM:        return "ENUM";
    case MYSQL_TYPE_FLOAT:       return "FLOAT";
    case MYSQL_TYPE_GEOMETRY:    return "GEOMETRY";
    case MYSQL_TYPE_INT24:       return "INT24";
    case MYSQL_TYPE_LONG:        return "LONG";
    case MYSQL_TYPE_LONGLONG:    return "LONGLONG";
    case MYSQL_TYPE_LONG_BLOB:   return "LONG_BLOB";
    case MYSQL_TYPE_MEDIUM_BLOB: return "MEDIUM_BLOB";
    case MYSQL_TYPE_NEWDATE:     return "NEWDATE";
    case MYSQL_TYPE_NULL:        return "NULL";
    case MYSQL_TYPE_SET:         return "SET";
    case MYSQL_TYPE_SHORT:       return "SHORT";
    case MYSQL_TYPE_STRING:      return "STRING";
    case MYSQL_TYPE_TIME:        return "TIME";
    case MYSQL_TYPE_TIMESTAMP:   return "TIMESTAMP";
    case MYSQL_TYPE_TINY:        return "TINY";
    case MYSQL_TYPE_TINY_BLOB:   return "TINY_BLOB";
    case MYSQL_TYPE_VAR_STRING:  return "VAR_STRING";
    case MYSQL_TYPE_YEAR:        return "YEAR";
    default:                     return "?-unknown-?";
  }
}

static char *fieldflags2str(uint f) {
  static char buf[1024];
  char *s=buf;
  *s=0;
#define ff2s_check_flag(X) \
                if (f & X ## _FLAG) { s=strmov(s, # X " "); f &= ~ X ## _FLAG; }
  ff2s_check_flag(NOT_NULL);
  ff2s_check_flag(PRI_KEY);
  ff2s_check_flag(UNIQUE_KEY);
  ff2s_check_flag(MULTIPLE_KEY);
  ff2s_check_flag(BLOB);
  ff2s_check_flag(UNSIGNED);
  ff2s_check_flag(ZEROFILL);
  ff2s_check_flag(BINARY);
  ff2s_check_flag(ENUM);
  ff2s_check_flag(AUTO_INCREMENT);
  ff2s_check_flag(TIMESTAMP);
  ff2s_check_flag(SET);
  ff2s_check_flag(NO_DEFAULT_VALUE);
  ff2s_check_flag(NUM);
  ff2s_check_flag(PART_KEY);
  ff2s_check_flag(GROUP);
  ff2s_check_flag(UNIQUE);
  ff2s_check_flag(BINCMP);
  ff2s_check_flag(ON_UPDATE_NOW);
#undef ff2s_check_flag
  if (f)
    sprintf(s, " unknows=0x%04x", f);
  return buf;
}

static void
print_field_types(MYSQL_RES *result)
{
  MYSQL_FIELD   *field;
  uint i=0;

  while ((field = mysql_fetch_field(result)))
  {
    tee_fprintf(PAGER, "Field %3u:  `%s`\n"
                       "Catalog:    `%s`\n"
                       "Database:   `%s`\n"
                       "Table:      `%s`\n"
                       "Org_table:  `%s`\n"
                       "Type:       %s\n"
                       "Collation:  %s (%u)\n"
                       "Length:     %lu\n"
                       "Max_length: %lu\n"
                       "Decimals:   %u\n"
                       "Flags:      %s\n\n",
                ++i,
                field->name, field->catalog, field->db, field->table,
                field->org_table, fieldtype2str(field->type),
                get_charset_name(field->charsetnr), field->charsetnr,
                field->length, field->max_length, field->decimals,
                fieldflags2str(field->flags));
  }
  tee_puts("", PAGER);
}


/* Used to determine if we should invoke print_as_hex for this field */

static bool
is_binary_field(MYSQL_FIELD *field)
{
  if ((field->charsetnr == 63) &&
      (field->type == MYSQL_TYPE_BIT ||
       field->type == MYSQL_TYPE_BLOB ||
       field->type == MYSQL_TYPE_LONG_BLOB ||
       field->type == MYSQL_TYPE_MEDIUM_BLOB ||
      field->type == MYSQL_TYPE_TINY_BLOB ||
       field->type == MYSQL_TYPE_VAR_STRING ||
       field->type == MYSQL_TYPE_STRING ||
       field->type == MYSQL_TYPE_VARCHAR ||
       field->type == MYSQL_TYPE_GEOMETRY))
    return 1;
  return 0;
}


/* Print binary value as hex literal (0x ...) */

static void
print_as_hex(FILE *output_file, const char *str, ulong len, ulong total_bytes_to_send)
{
  const char *ptr= str, *end= ptr+len;
  ulong i;
  fprintf(output_file, "0x");
  for(; ptr < end; ptr++)
    fprintf(output_file, "%02X", *((uchar*)ptr));
  for (i= 2*len+2; i < total_bytes_to_send; i++)
    tee_putc((int)' ', output_file);
}


static void
print_table_data(MYSQL_RES *result)
{
  String separator(256);
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;
  bool		*num_flag;

  num_flag=(bool*) my_alloca(sizeof(bool)*mysql_num_fields(result));
  if (column_types_flag)
  {
    print_field_types(result);
    if (!mysql_num_rows(result))
      return;
    mysql_field_seek(result,0);
  }
  separator.copy("+",1,charset_info);
  while ((field = mysql_fetch_field(result)))
  {
    uint length= column_names ? field->name_length : 0;
    if (quick)
      length=max(length,field->length);
    else
      length=max(length,field->max_length);
    if (length < 4 && !IS_NOT_NULL(field->flags))
      length=4;					// Room for "NULL"
    if (opt_binhex && is_binary_field(field))
      length= 2 + length * 2;
    field->max_length=(ulong) length;
    separator.fill(separator.length()+length+2,'-');
    separator.append('+');
  }
  separator.append('\0');                       // End marker for \0
  tee_puts((char*) separator.ptr(), PAGER);
  if (column_names)
  {
    mysql_field_seek(result,0);
    (void) tee_fputs("|", PAGER);
    for (uint off=0; (field = mysql_fetch_field(result)) ; off++)
    {
      uint name_length= (uint) strlen(field->name);
      uint numcells= charset_info->cset->numcells(charset_info,
                                                  field->name,
                                                  field->name + name_length);
      uint display_length= field->max_length + name_length - numcells;
      tee_fprintf(PAGER, " %-*s |",(int) min(display_length,
                                            MAX_COLUMN_LENGTH),
                  field->name);
      num_flag[off]= IS_NUM(field->type);
    }
    (void) tee_fputs("\n", PAGER);
    tee_puts((char*) separator.ptr(), PAGER);
  }

  while ((cur= mysql_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    ulong *lengths= mysql_fetch_lengths(result);
    (void) tee_fputs("| ", PAGER);
    mysql_field_seek(result, 0);
    for (uint off= 0; off < mysql_num_fields(result); off++)
    {
      const char *buffer;
      uint data_length;
      uint field_max_length;
      uint visible_length;
      uint extra_padding;

      if (off)
        (void) tee_fputs(" ", PAGER);

      if (cur[off] == NULL)
      {
        buffer= "NULL";
        data_length= 4;
      } 
      else 
      {
        buffer= cur[off];
        data_length= (uint) lengths[off];
      }

      field= mysql_fetch_field(result);
      field_max_length= field->max_length;

      /* 
       How many text cells on the screen will this string span?  If it contains
       multibyte characters, then the number of characters we occupy on screen
       will be fewer than the number of bytes we occupy in memory.

       We need to find how much screen real-estate we will occupy to know how 
       many extra padding-characters we should send with the printing function.
      */
      visible_length= charset_info->cset->numcells(charset_info, buffer, buffer + data_length);
      extra_padding= (uint) (data_length - visible_length);

      if (opt_binhex && is_binary_field(field))
        print_as_hex(PAGER, cur[off], lengths[off], field_max_length);
      else if (field_max_length > MAX_COLUMN_LENGTH)
        tee_print_sized_data(buffer, data_length, MAX_COLUMN_LENGTH+extra_padding, FALSE);
      else
      {
        if (num_flag[off] != 0) /* if it is numeric, we right-justify it */
          tee_print_sized_data(buffer, data_length, field_max_length+extra_padding, TRUE);
        else 
          tee_print_sized_data(buffer, data_length, field_max_length+extra_padding, FALSE);
      }
      tee_fputs(" |", PAGER);
    }
    (void) tee_fputs("\n", PAGER);
  }
  tee_puts((char*) separator.ptr(), PAGER);
  my_afree((uchar*) num_flag);
}

/**
  Return the length of a field after it would be rendered into text.

  This doesn't know or care about multibyte characters.  Assume we're
  using such a charset.  We can't know that all of the upcoming rows 
  for this column will have bytes that each render into some fraction
  of a character.  It's at least possible that a row has bytes that 
  all render into one character each, and so the maximum length is 
  still the number of bytes.  (Assumption 1:  This can't be better 
  because we can never know the number of characters that the DB is 
  going to send -- only the number of bytes.  2: Chars <= Bytes.)

  @param  field  Pointer to a field to be inspected

  @returns  number of character positions to be used, at most
*/
static int get_field_disp_length(MYSQL_FIELD *field)
{
  uint length= column_names ? field->name_length : 0;

  if (quick)
    length= max(length, field->length);
  else
    length= max(length, field->max_length);

  if (length < 4 && !IS_NOT_NULL(field->flags))
    length= 4;				/* Room for "NULL" */

  return length;
}

/**
  For a new result, return the max number of characters that any
  upcoming row may return.

  @param  result  Pointer to the result to judge

  @returns  The max number of characters in any row of this result
*/
static int get_result_width(MYSQL_RES *result)
{
  unsigned int len= 0;
  MYSQL_FIELD *field;
  MYSQL_FIELD_OFFSET offset;
  
#ifndef DBUG_OFF
  offset= mysql_field_tell(result);
  DBUG_ASSERT(offset == 0);
#else
  offset= 0;
#endif

  while ((field= mysql_fetch_field(result)) != NULL)
    len+= get_field_disp_length(field) + 3; /* plus bar, space, & final space */

  (void) mysql_field_seek(result, offset);	

  return len + 1; /* plus final bar. */
}

static void
tee_print_sized_data(const char *data, unsigned int data_length, unsigned int total_bytes_to_send, bool right_justified)
{
  /* 
    For '\0's print ASCII spaces instead, as '\0' is eaten by (at
    least my) console driver, and that messes up the pretty table
    grid.  (The \0 is also the reason we can't use fprintf() .) 
  */
  unsigned int i;
  const char *p;

  if (right_justified) 
    for (i= data_length; i < total_bytes_to_send; i++)
      tee_putc((int)' ', PAGER);

  for (i= 0, p= data; i < data_length; i+= 1, p+= 1)
  {
    if (*p == '\0')
      tee_putc((int)' ', PAGER);
    else
      tee_putc((int)*p, PAGER);
  }

  if (! right_justified) 
    for (i= data_length; i < total_bytes_to_send; i++)
      tee_putc((int)' ', PAGER);
}



static void
print_table_data_html(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;

  mysql_field_seek(result,0);
  (void) tee_fputs("<TABLE BORDER=1><TR>", PAGER);
  if (column_names)
  {
    while((field = mysql_fetch_field(result)))
    {
      tee_fputs("<TH>", PAGER);
      if (field->name && field->name[0])
        xmlencode_print(field->name, field->name_length);
      else
        tee_fputs(field->name ? " &nbsp; " : "NULL", PAGER);
      tee_fputs("</TH>", PAGER);
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  while ((cur = mysql_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    ulong *lengths=mysql_fetch_lengths(result);
    field= mysql_fetch_fields(result);
    (void) tee_fputs("<TR>", PAGER);
    for (uint i=0; i < mysql_num_fields(result); i++)
    {
      (void) tee_fputs("<TD>", PAGER);
      if (opt_binhex && is_binary_field(&field[i]))
        print_as_hex(PAGER, cur[i], lengths[i], lengths[i]);
      else
        xmlencode_print(cur[i], lengths[i]);
      (void) tee_fputs("</TD>", PAGER);
    }
    (void) tee_fputs("</TR>", PAGER);
  }
  (void) tee_fputs("</TABLE>", PAGER);
}


static void
print_table_data_xml(MYSQL_RES *result)
{
  MYSQL_ROW   cur;
  MYSQL_FIELD *fields;

  mysql_field_seek(result,0);

  tee_fputs("<?xml version=\"1.0\"?>\n\n<resultset statement=\"", PAGER);
  xmlencode_print(glob_buffer.ptr(), (int)strlen(glob_buffer.ptr()));
  tee_fputs("\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">",
            PAGER);

  fields = mysql_fetch_fields(result);
  while ((cur = mysql_fetch_row(result)))
  {
    if (interrupted_query)
      break;
    ulong *lengths=mysql_fetch_lengths(result);
    (void) tee_fputs("\n  <row>\n", PAGER);
    for (uint i=0; i < mysql_num_fields(result); i++)
    {
      tee_fprintf(PAGER, "\t<field name=\"");
      xmlencode_print(fields[i].name, (uint) strlen(fields[i].name));
      if (cur[i])
      {
        tee_fprintf(PAGER, "\">");
        if (opt_binhex && is_binary_field(&fields[i]))
          print_as_hex(PAGER, cur[i], lengths[i], lengths[i]);
        else
          xmlencode_print(cur[i], lengths[i]);
        tee_fprintf(PAGER, "</field>\n");
      }
      else
        tee_fprintf(PAGER, "\" xsi:nil=\"true\" />\n");
    }
    (void) tee_fputs("  </row>\n", PAGER);
  }
  (void) tee_fputs("</resultset>\n", PAGER);
}


static void
print_table_data_vertically(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  uint		max_length=0;
  MYSQL_FIELD	*field;

  while ((field = mysql_fetch_field(result)))
  {
    uint length= field->name_length;
    if (length > max_length)
      max_length= length;
    field->max_length=length;
  }

  mysql_field_seek(result,0);
  for (uint row_count=1; (cur= mysql_fetch_row(result)); row_count++)
  {
    if (interrupted_query)
      break;
    mysql_field_seek(result,0);
    tee_fprintf(PAGER, 
		"*************************** %d. row ***************************\n", row_count);

    ulong *lengths= mysql_fetch_lengths(result);

    for (uint off=0; off < mysql_num_fields(result); off++)
    {
      field= mysql_fetch_field(result);
      if (column_names)
        tee_fprintf(PAGER, "%*s: ",(int) max_length,field->name);
      if (cur[off])
      {
        unsigned int i;
        const char *p;
        if (opt_binhex && is_binary_field(field))
           fprintf(PAGER, "0x");
        for (i= 0, p= cur[off]; i < lengths[off]; i+= 1, p+= 1)
        {
          if (opt_binhex && is_binary_field(field))
            fprintf(PAGER, "%02X", *((uchar*)p));
          else
          {
            if (*p == '\0')
              tee_putc((int)' ', PAGER);
            else
              tee_putc((int)*p, PAGER);
          }
        }
        tee_putc('\n', PAGER);
      }
       else
        tee_fprintf(PAGER, "NULL\n");
    }
  }
}

/* print_warnings should be called right after executing a statement */

static void print_warnings()
{
  const char   *query;
  MYSQL_RES    *result;
  MYSQL_ROW    cur;
  my_ulonglong num_rows;
  
  /* Save current error before calling "show warnings" */
  uint error= mysql_errno(&mysql);

  /* Get the warnings */
  query= "show warnings";
  mysql_real_query_for_lazy(query, strlen(query));
  mysql_store_result_for_lazy(&result);

  /* Bail out when no warnings */
  if (!result || !(num_rows= mysql_num_rows(result)))
    goto end;

  cur= mysql_fetch_row(result);

  /*
    Don't print a duplicate of the current error.  It is possible for SHOW
    WARNINGS to return multiple errors with the same code, but different
    messages.  To be safe, skip printing the duplicate only if it is the only
    warning.
  */
  if (!cur || (num_rows == 1 && error == (uint) strtoul(cur[1], NULL, 10)))
    goto end;

  /* Print the warnings */
  init_pager();
  do
  {
    tee_fprintf(PAGER, "%s (Code %s): %s\n", cur[0], cur[1], cur[2]);
  } while ((cur= mysql_fetch_row(result)));
  end_pager();

end:
  mysql_free_result(result);
}


static const char *array_value(const char **array, char key)
{
  for (; *array; array+= 2)
    if (**array == key)
      return array[1];
  return 0;
}


static void
xmlencode_print(const char *src, uint length)
{
  if (!src)
    tee_fputs("NULL", PAGER);
  else
  {
    for (const char *p = src; length; p++, length--)
    {
      const char *t;
      if ((t = array_value(xmlmeta, *p)))
	tee_fputs(t, PAGER);
      else
	tee_putc(*p, PAGER);
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
    {
      unsigned long i;
      /* Can't use tee_fputs(), it stops with NUL characters. */
      for (i= 0; i < length; i++, pos++)
        tee_putc(*pos, PAGER);
    }
    else for (const char *end=pos+length ; pos != end ; pos++)
    {
#ifdef USE_MB
      int l;
      if (use_mb(charset_info) &&
          (l = my_ismbchar(charset_info, pos, end)))
      {
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

  if (opt_silent < 2 && column_names)
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
    field= mysql_fetch_fields(result);
    if (opt_binhex && is_binary_field(&field[0]))
      print_as_hex(PAGER, cur[0], lengths[0], lengths[0]);
    else
      safe_put_field(cur[0],lengths[0]);

    for (uint off=1 ; off < mysql_num_fields(result); off++)
    {
      (void) tee_fputs("\t", PAGER);
      if (opt_binhex && field && is_binary_field(&field[off]))
        print_as_hex(PAGER, cur[off], lengths[off], lengths[off]);
      else
        safe_put_field(cur[off], lengths[off]);
    }
    (void) tee_fputs("\n", PAGER);
  }
}

static int
com_tee(String *buffer __attribute__((unused)),
        char *line __attribute__((unused)))
{
  char file_name[FN_REFLEN], *end, *param;

  if (status.batch)
    return 0;
  while (my_isspace(charset_info,*line))
    line++;
  if (!(param = strchr(line, ' '))) // if outfile wasn't given, use the default
  {
    if (!strlen(outfile))
    {
      printf("No previous outfile available, you must give a filename!\n");
      return 0;
    }
    else if (opt_outfile)
    {
      tee_fprintf(stdout, "Currently logging to file '%s'\n", outfile);
      return 0;
    }
    else
      param = outfile;			//resume using the old outfile
  }

  /* eliminate the spaces before the parameters */
  while (my_isspace(charset_info,*param))
    param++;
  end= strmake(file_name, param, sizeof(file_name) - 1);
  /* remove end space from command line */
  while (end > file_name && (my_isspace(charset_info,end[-1]) || 
			     my_iscntrl(charset_info,end[-1])))
    end--;
  end[0]= 0;
  if (end == file_name)
  {
    printf("No outfile specified!\n");
    return 0;
  }
  init_tee(file_name);
  return 0;
}


static int
com_notee(String *buffer __attribute__((unused)),
	  char *line __attribute__((unused)))
{
  if (opt_outfile)
    end_tee();
  tee_fprintf(stdout, "Outfile disabled.\n");
  return 0;
}

/*
  Sorry, this command is not available in Windows.
*/

#ifdef USE_POPEN
static int
com_pager(String *buffer __attribute__((unused)),
          char *line __attribute__((unused)))
{
  char pager_name[FN_REFLEN], *end, *param;

  if (status.batch)
    return 0;
  /* Skip spaces in front of the pager command */
  while (my_isspace(charset_info, *line))
    line++;
  /* Skip the pager command */
  param= strchr(line, ' ');
  /* Skip the spaces between the command and the argument */
  while (param && my_isspace(charset_info, *param))
    param++;
  if (!param || !strlen(param)) // if pager was not given, use the default
  {
    if (!default_pager_set)
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
    end= strmake(pager_name, param, sizeof(pager_name)-1);
    while (end > pager_name && (my_isspace(charset_info,end[-1]) || 
                                my_iscntrl(charset_info,end[-1])))
      end--;
    end[0]=0;
    strmov(pager, pager_name);
    strmov(default_pager, pager_name);
  }
  opt_nopager=0;
  tee_fprintf(stdout, "PAGER set to '%s'\n", pager);
  return 0;
}


static int
com_nopager(String *buffer __attribute__((unused)),
	    char *line __attribute__((unused)))
{
  strmov(pager, "stdout");
  opt_nopager=1;
  PAGER= stdout;
  tee_fprintf(stdout, "PAGER set to stdout\n");
  return 0;
}
#endif


/*
  Sorry, you can't send the result to an editor in Win32
*/

#ifdef USE_POPEN
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
    (void) my_write(fd,(uchar*) old_buffer.ptr(),old_buffer.length(),
		    MYF(MY_WME));
  else
    (void) my_write(fd,(uchar*) buffer->ptr(),buffer->length(),MYF(MY_WME));
  (void) my_close(fd,MYF(0));

  if (!(editor = (char *)getenv("EDITOR")) &&
      !(editor = (char *)getenv("VISUAL")))
    editor = "vi";
  strxmov(buff,editor," ",filename,NullS);
  if(system(buff) == -1)
    goto err;

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
  build_completion_hash(1, 0);
#endif
  return 0;
}


#ifdef USE_POPEN
static int
com_shell(String *buffer __attribute__((unused)),
          char *line __attribute__((unused)))
{
  char *shell_cmd;

  /* Skip space from line begin */
  while (my_isspace(charset_info, *line))
    line++;
  if (!(shell_cmd = strchr(line, ' ')))
  {
    put_info("Usage: \\! shell-command", INFO_ERROR);
    return -1;
  }
  /*
    The output of the shell command does not
    get directed to the pager or the outfile
  */
  if (system(shell_cmd) == -1)
  {
    put_info(strerror(errno), INFO_ERROR, errno);
    return -1;
  }
  return 0;
}
#endif


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
  char *tmp, buff[256];
  bool save_rehash= opt_rehash;
  int error;

  bzero(buff, sizeof(buff));
  if (buffer)
  {
    /*
      Two null bytes are needed in the end of buff to allow
      get_arg to find end of string the second time it's called.
    */
    tmp= strmake(buff, line, sizeof(buff)-2);
#ifdef EXTRA_DEBUG
    tmp[1]= 0;
#endif
    tmp= get_arg(buff, 0);
    if (tmp && *tmp)
    {
      my_free(current_db);
      current_db= my_strdup(tmp, MYF(MY_WME));
      tmp= get_arg(buff, 1);
      if (tmp)
      {
	my_free(current_host);
	current_host=my_strdup(tmp,MYF(MY_WME));
      }
    }
    else
    {
      /* Quick re-connect */
      opt_rehash= 0;                            /* purecov: tested */
    }
    buffer->length(0);				// command used
  }
  else
    opt_rehash= 0;
  error=sql_connect(current_host,current_db,current_user,opt_password,0);
  opt_rehash= save_rehash;

  if (connected)
  {
    sprintf(buff,"Connection id:    %lu",mysql_thread_id(&mysql));
    put_info(buff,INFO_INFO);
    sprintf(buff,"Current database: %.128s\n",
	    current_db ? current_db : "*** NONE ***");
    put_info(buff,INFO_INFO);
  }
  return error;
}


static int com_source(String *buffer __attribute__((unused)),
                      char *line)
{
  char source_name[FN_REFLEN], *end, *param;
  LINE_BUFFER *line_buff;
  int error;
  STATUS old_status;
  FILE *sql_file;

  /* Skip space from file name */
  while (my_isspace(charset_info,*line))
    line++;
  if (!(param = strchr(line, ' ')))		// Skip command name
    return put_info("Usage: \\. <filename> | source <filename>", 
		    INFO_ERROR, 0);
  while (my_isspace(charset_info,*param))
    param++;
  end=strmake(source_name,param,sizeof(source_name)-1);
  while (end > source_name && (my_isspace(charset_info,end[-1]) || 
                               my_iscntrl(charset_info,end[-1])))
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

  if (!(line_buff= batch_readline_init(MAX_BATCH_BUFFER_SIZE, sql_file)))
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
  error= read_and_execute(false);
  status=old_status;				// Continue as before
  my_fclose(sql_file,MYF(0));
  batch_readline_end(line_buff);
  return error;
}


	/* ARGSUSED */
static int
com_delimiter(String *buffer __attribute__((unused)), char *line)
{
  char buff[256], *tmp;

  strmake(buff, line, sizeof(buff) - 1);
  tmp= get_arg(buff, 0);

  if (!tmp || !*tmp)
  {
    put_info("DELIMITER must be followed by a 'delimiter' character or string",
	     INFO_ERROR);
    return 0;
  }
  else
  {
    if (strstr(tmp, "\\")) 
    {
      put_info("DELIMITER cannot contain a backslash character", INFO_ERROR);
      return 0;
    }
  }
  strmake(delimiter, tmp, sizeof(delimiter) - 1);
  delimiter_length= (int)strlen(delimiter);
  delimiter_str= delimiter;
  return 0;
}

	/* ARGSUSED */
static int
com_use(String *buffer __attribute__((unused)), char *line)
{
  char *tmp, buff[FN_REFLEN + 1];
  int select_db;

  bzero(buff, sizeof(buff));

  /*
    In case of quotes used, try to get the normalized db name.
  */
  if (get_quote_count(line) > 0)
  {
    if (normalize_dbname(line, buff, sizeof(buff)))
      return put_error(&mysql);
    tmp= buff;
  }
  else
  {
    strmake(buff, line, sizeof(buff) - 1);
    tmp= get_arg(buff, 0);
  }

  if (!tmp || !*tmp)
  {
    put_info("USE must be followed by a database name", INFO_ERROR);
    return 0;
  }
  /*
    We need to recheck the current database, because it may change
    under our feet, for example if DROP DATABASE or RENAME DATABASE
    (latter one not yet available by the time the comment was written)
  */
  get_current_db();

  if (!current_db || cmp_database(charset_info, current_db,tmp))
  {
    if (one_database)
    {
      skip_updates= 1;
      select_db= 0;    // don't do mysql_select_db()
    }
    else
      select_db= 2;    // do mysql_select_db() and build_completion_hash()
  }
  else
  {
    /*
      USE to the current db specified.
      We do need to send mysql_select_db() to make server
      update database level privileges, which might
      change since last USE (see bug#10979).
      For performance purposes, we'll skip rebuilding of completion hash.
    */
    skip_updates= 0;
    select_db= 1;      // do only mysql_select_db(), without completion
  }

  if (select_db)
  {
    /*
      reconnect once if connection is down or if connection was found to
      be down during query
    */
    if (!connected && reconnect())
      return opt_reconnect ? -1 : 1;                        // Fatal error
    if (mysql_select_db(&mysql,tmp))
    {
      if (mysql_errno(&mysql) != CR_SERVER_GONE_ERROR)
        return put_error(&mysql);

      if (reconnect())
        return opt_reconnect ? -1 : 1;                      // Fatal error
      if (mysql_select_db(&mysql,tmp))
        return put_error(&mysql);
    }
    my_free(current_db);
    current_db=my_strdup(tmp,MYF(MY_WME));
#ifdef HAVE_READLINE
    if (select_db > 1)
      build_completion_hash(opt_rehash, 1);
#endif
  }

  put_info("Database changed",INFO_INFO);
  return 0;
}

/**
  Normalize database name.

  @param line [IN]          The command.
  @param buff [OUT]         Normalized db name.
  @param buff_size [IN]     Buffer size.

  @return Operation status
      @retval 0    Success
      @retval 1    Failure

  @note Sometimes server normilizes the database names
        & APIs like mysql_select_db() expect normalized
        database names. Since it is difficult to perform
        the name conversion/normalization on the client
        side, this function tries to get the normalized
        dbname (indirectly) from the server.
*/

static int
normalize_dbname(const char *line, char *buff, uint buff_size)
{
  MYSQL_RES *res= NULL;

  /* Send the "USE db" commmand to the server. */
  if (mysql_query(&mysql, line))
    return 1;

  /*
    Now, get the normalized database name and store it
    into the buff.
  */
  if (!mysql_query(&mysql, "SELECT DATABASE()") &&
      (res= mysql_use_result(&mysql)))
  {
    MYSQL_ROW row= mysql_fetch_row(res);
    if (row && row[0])
    {
      size_t len= strlen(row[0]);
      /* Make sure there is enough room to store the dbname. */
      if ((len > buff_size) || ! memcpy(buff, row[0], len))
      {
        mysql_free_result(res);
        return 1;
      }
    }
    mysql_free_result(res);
  }

  /* Restore the original database. */
  if (current_db && mysql_select_db(&mysql, current_db))
    return 1;

  return 0;
}

static int
com_warnings(String *buffer __attribute__((unused)),
   char *line __attribute__((unused)))
{
  show_warnings = 1;
  put_info("Show warnings enabled.",INFO_INFO);
  return 0;
}

static int
com_nowarnings(String *buffer __attribute__((unused)),
   char *line __attribute__((unused)))
{
  show_warnings = 0;
  put_info("Show warnings disabled.",INFO_INFO);
  return 0;
}

/*
  Gets argument from a command on the command line. If get_next_arg is
  not defined, skips the command and returns the first argument. The
  line is modified by adding zero to the end of the argument. If
  get_next_arg is defined, then the function searches for end of string
  first, after found, returns the next argument and adds zero to the
  end. If you ever wish to use this feature, remember to initialize all
  items in the array to zero first.
*/

char *get_arg(char *line, my_bool get_next_arg)
{
  char *ptr, *start;
  my_bool quoted= 0, valid_arg= 0;
  char qtype= 0;

  ptr= line;
  if (get_next_arg)
  {
    for (; *ptr; ptr++) ;
    if (*(ptr + 1))
      ptr++;
  }
  else
  {
    /* skip leading white spaces */
    while (my_isspace(charset_info, *ptr))
      ptr++;
    if (*ptr == '\\') // short command was used
      ptr+= 2;
    else
      while (*ptr &&!my_isspace(charset_info, *ptr)) // skip command
        ptr++;
  }
  if (!*ptr)
    return NullS;
  while (my_isspace(charset_info, *ptr))
    ptr++;
  if (*ptr == '\'' || *ptr == '\"' || *ptr == '`')
  {
    qtype= *ptr;
    quoted= 1;
    ptr++;
  }
  for (start=ptr ; *ptr; ptr++)
  {
    if (*ptr == '\\' && ptr[1]) // escaped character
    {
      // Remove the backslash
      strmov_overlapp(ptr, ptr+1);
    }
    else if ((!quoted && *ptr == ' ') || (quoted && *ptr == qtype))
    {
      *ptr= 0;
      break;
    }
  }
  valid_arg= ptr != start;
  return valid_arg ? start : NullS;
}

/*
  Number of quotes present in the command's argument.
*/
static int
get_quote_count(const char *line)
{
  int quote_count= 0;
  const char *quote= line;

  while ((quote= strpbrk(quote, "'`\"")) != NULL) {
    quote_count++;
    quote++;
  }

  return quote_count;
}

static int
sql_real_connect(char *host,char *database,char *user,char *password,
		 uint silent)
{
  if (connected)
  {
    connected= 0;
    mysql_close(&mysql);
  }
  mysql_init(&mysql);
  if (opt_init_command)
    mysql_options(&mysql, MYSQL_INIT_COMMAND, opt_init_command);
  if (opt_connect_timeout)
  {
    uint timeout=opt_connect_timeout;
    mysql_options(&mysql,MYSQL_OPT_CONNECT_TIMEOUT,
		  (char*) &timeout);
  }
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);
  if (opt_secure_auth)
    mysql_options(&mysql, MYSQL_SECURE_AUTH, (char *) &opt_secure_auth);
  if (using_opt_local_infile)
    mysql_options(&mysql,MYSQL_OPT_LOCAL_INFILE, (char*) &opt_local_infile);
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  if (opt_use_ssl)
    mysql_ssl_set(&mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
  mysql_options(&mysql,MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                (char*)&opt_ssl_verify_server_cert);
#endif
  if (opt_protocol)
    mysql_options(&mysql,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
#ifdef HAVE_SMEM
  if (shared_memory_base_name)
    mysql_options(&mysql,MYSQL_SHARED_MEMORY_BASE_NAME,shared_memory_base_name);
#endif
  if (safe_updates)
  {
    char init_command[100];
    sprintf(init_command,
	    "SET SQL_SAFE_UPDATES=1,SQL_SELECT_LIMIT=%lu,MAX_JOIN_SIZE=%lu",
	    select_limit,max_join_size);
    mysql_options(&mysql, MYSQL_INIT_COMMAND, init_command);
  }

  mysql_options(&mysql, MYSQL_SET_CHARSET_NAME, default_charset);
  
  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(&mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(&mysql, MYSQL_DEFAULT_AUTH, opt_default_auth);

  if (using_opt_enable_cleartext_plugin)
    mysql_options(&mysql, MYSQL_ENABLE_CLEARTEXT_PLUGIN, 
                  (char*) &opt_enable_cleartext_plugin);

  if (!mysql_connect_ssl_check(&mysql, host, user, password,
                               database, opt_mysql_port, opt_mysql_unix_port,
                               connect_flag | CLIENT_MULTI_STATEMENTS,
                               opt_ssl_mode == SSL_MODE_REQUIRED))
  {
    if (!silent ||
	(mysql_errno(&mysql) != CR_CONN_HOST_ERROR &&
	 mysql_errno(&mysql) != CR_CONNECTION_ERROR))
    {
      (void) put_error(&mysql);
      (void) fflush(stdout);
      return ignore_errors ? -1 : 1;		// Abort
    }
    return -1;					// Retryable
  }
  
  charset_info= mysql.charset;
  
  connected=1;
#ifndef EMBEDDED_LIBRARY
  mysql.reconnect= debug_info_flag; // We want to know if this happens
#else
  mysql.reconnect= 1;
#endif
#ifdef HAVE_READLINE
  build_completion_hash(opt_rehash, 1);
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
    (void) sleep(wait_time);
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
  const char *status_str;
  char buff[40];
  ulonglong id;
  MYSQL_RES *result;
  LINT_INIT(result);

  if (mysql_real_query_for_lazy(
        C_STRING_WITH_LEN("select DATABASE(), USER() limit 1")))
    return 0;

  tee_puts("--------------", stdout);
  usage(1);					/* Print version */
  tee_fprintf(stdout, "\nConnection id:\t\t%lu\n",mysql_thread_id(&mysql));
  /*
    Don't remove "limit 1",
    it is protection againts SQL_SELECT_LIMIT=0
  */
  if (!mysql_store_result_for_lazy(&result))
  {
    MYSQL_ROW cur=mysql_fetch_row(result);
    if (cur)
    {
      tee_fprintf(stdout, "Current database:\t%s\n", cur[0] ? cur[0] : "");
      tee_fprintf(stdout, "Current user:\t\t%s\n", cur[1]);
    }
    mysql_free_result(result);
  }

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  if ((status_str= mysql_get_ssl_cipher(&mysql)))
    tee_fprintf(stdout, "SSL:\t\t\tCipher in use is %s\n",
                status_str);
  else
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */
    tee_puts("SSL:\t\t\tNot in use", stdout);

  if (skip_updates)
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, "\nAll updates ignored to this database\n");
    vidattr(A_NORMAL);
  }
#ifdef USE_POPEN
  tee_fprintf(stdout, "Current pager:\t\t%s\n", pager);
  tee_fprintf(stdout, "Using outfile:\t\t'%s'\n", opt_outfile ? outfile : "");
#endif
  tee_fprintf(stdout, "Using delimiter:\t%s\n", delimiter);
  tee_fprintf(stdout, "Server version:\t\t%s\n", server_version_string(&mysql));
  tee_fprintf(stdout, "Protocol version:\t%d\n", mysql_get_proto_info(&mysql));
  tee_fprintf(stdout, "Connection:\t\t%s\n", mysql_get_host_info(&mysql));
  if ((id= mysql_insert_id(&mysql)))
    tee_fprintf(stdout, "Insert id:\t\t%s\n", llstr(id, buff));

  /* "limit 1" is protection against SQL_SELECT_LIMIT=0 */
  if (mysql_real_query_for_lazy(C_STRING_WITH_LEN(
        "select @@character_set_client, @@character_set_connection, "
        "@@character_set_server, @@character_set_database limit 1")))
  {
    if (mysql_errno(&mysql) == CR_SERVER_GONE_ERROR)
      return 0;
  }
  if (!mysql_store_result_for_lazy(&result))
  {
    MYSQL_ROW cur=mysql_fetch_row(result);
    if (cur)
    {
      tee_fprintf(stdout, "Server characterset:\t%s\n", cur[2] ? cur[2] : "");
      tee_fprintf(stdout, "Db     characterset:\t%s\n", cur[3] ? cur[3] : "");
      tee_fprintf(stdout, "Client characterset:\t%s\n", cur[0] ? cur[0] : "");
      tee_fprintf(stdout, "Conn.  characterset:\t%s\n", cur[1] ? cur[1] : "");
    }
    mysql_free_result(result);
  }
  else
  {
    /* Probably pre-4.1 server */
    tee_fprintf(stdout, "Client characterset:\t%s\n", charset_info->csname);
    tee_fprintf(stdout, "Server characterset:\t%s\n", mysql.charset->csname);
  }

#ifndef EMBEDDED_LIBRARY
  if (strstr(mysql_get_host_info(&mysql),"TCP/IP") || ! mysql.unix_socket)
    tee_fprintf(stdout, "TCP port:\t\t%d\n", mysql.port);
  else
    tee_fprintf(stdout, "UNIX socket:\t\t%s\n", mysql.unix_socket);
  if (mysql.net.compress)
    tee_fprintf(stdout, "Protocol:\t\tCompressed\n");
#endif

  if ((status_str= mysql_stat(&mysql)) && !mysql_error(&mysql)[0])
  {
    ulong sec;
    const char *pos= strchr(status_str,' ');
    /* print label */
    tee_fprintf(stdout, "%.*s\t\t\t", (int) (pos-status_str), status_str);
    if ((status_str= str2int(pos,10,0,LONG_MAX,(long*) &sec)))
    {
      nice_time((double) sec,buff,0);
      tee_puts(buff, stdout);			/* print nice time */
      while (*status_str == ' ')
        status_str++;  /* to next info */
      tee_putc('\n', stdout);
      tee_puts(status_str, stdout);
    }
  }
  if (safe_updates)
  {
    vidattr(A_BOLD);
    tee_fprintf(stdout, "\nNote that you are running in safe_update_mode:\n");
    vidattr(A_NORMAL);
    tee_fprintf(stdout, "\
UPDATEs and DELETEs that don't use a key in the WHERE clause are not allowed.\n\
(One can force an UPDATE/DELETE by adding LIMIT # at the end of the command.)\n\
SELECT has an automatic 'LIMIT %lu' if LIMIT is not used.\n\
Max number of examined row combination in a join is set to: %lu\n\n",
select_limit, max_join_size);
  }
  tee_puts("--------------\n", stdout);
  return 0;
}

static const char *
server_version_string(MYSQL *con)
{
  /* Only one thread calls this, so no synchronization is needed */
  if (server_version == NULL)
  {
    MYSQL_RES *result;

    /* "limit 1" is protection against SQL_SELECT_LIMIT=0 */
    if (!mysql_query(con, "select @@version_comment limit 1") &&
        (result = mysql_use_result(con)))
    {
      MYSQL_ROW cur = mysql_fetch_row(result);
      if (cur && cur[0])
      {
        /* version, space, comment, \0 */
        size_t len= strlen(mysql_get_server_info(con)) + strlen(cur[0]) + 2;

        if ((server_version= (char *) my_malloc(len, MYF(MY_WME))))
        {
          char *bufp;
          bufp = strmov(server_version, mysql_get_server_info(con));
          bufp = strmov(bufp, " ");
          (void) strmov(bufp, cur[0]);
        }
      }
      mysql_free_result(result);
    }

    /*
      If for some reason we didn't get a version_comment, we'll
      keep things simple.
    */

    if (server_version == NULL)
      server_version= my_strdup(mysql_get_server_info(con), MYF(MY_WME));
  }

  return server_version ? server_version : "";
}

static int
put_info(const char *str,INFO_TYPE info_type, uint error, const char *sqlstate)
{
  FILE *file= (info_type == INFO_ERROR ? stderr : stdout);
  static int inited=0;

  if (status.batch)
  {
    if (info_type == INFO_ERROR)
    {
      (void) fflush(file);
      fprintf(file,"ERROR");
      if (error)
      {
	if (sqlstate)
	  (void) fprintf(file," %d (%s)",error, sqlstate);
        else
	  (void) fprintf(file," %d",error);
      }
      if (status.query_start_line && line_numbers)
      {
	(void) fprintf(file," at line %lu",status.query_start_line);
	if (status.file_name)
	  (void) fprintf(file," in file: '%s'", status.file_name);
      }
      (void) fprintf(file,": %s\n",str);
      (void) fflush(file);
      if (!ignore_errors)
	return 1;
    }
    else if (info_type == INFO_RESULT && verbose > 1)
      tee_puts(str, file);
    if (unbuffered)
      fflush(file);
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
      if (!opt_nobeep)
        putchar('\a');		      	/* This should make a bell */
      vidattr(A_STANDOUT);
      if (error)
      {
	if (sqlstate)
          (void) tee_fprintf(file, "ERROR %d (%s): ", error, sqlstate);
        else
          (void) tee_fprintf(file, "ERROR %d: ", error);
      }
      else
        tee_puts("ERROR: ", file);
    }
    else
      vidattr(A_BOLD);
    (void) tee_puts(str, file);
    vidattr(A_NORMAL);
  }
  if (unbuffered)
    fflush(file);
  return info_type == INFO_ERROR ? -1 : 0;
}


static int
put_error(MYSQL *con)
{
  return put_info(mysql_error(con), INFO_ERROR, mysql_errno(con),
		  mysql_sqlstate(con));
}  


static void remove_cntrl(String &buffer)
{
  char *start,*end;
  end=(start=(char*) buffer.ptr())+buffer.length();
  while (start < end && !my_isgraph(charset_info,end[-1]))
    end--;
  buffer.length((uint) (end-start));
}


void tee_fprintf(FILE *file, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  (void) vfprintf(file, fmt, args);
  va_end(args);

  if (opt_outfile)
  {
    va_start(args, fmt);
    (void) vfprintf(OUTFILE, fmt, args);
    va_end(args);
  }
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
  fputc('\n', file);
  if (opt_outfile)
  {
    fputs(s, OUTFILE);
    fputc('\n', OUTFILE);
  }
}

void tee_putc(int c, FILE *file)
{
  putc(c, file);
  if (opt_outfile)
    putc(c, OUTFILE);
}

#if defined(__WIN__)
#include <time.h>
#else
#include <sys/times.h>
#ifdef _SC_CLK_TCK				// For mit-pthreads
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC (sysconf(_SC_CLK_TCK))
#endif
#endif

static ulong start_timer(void)
{
#if defined(__WIN__)
  return clock();
#else
  struct tms tms_tmp;
  return times(&tms_tmp);
#endif
}


/** 
  Write as many as 52+1 bytes to buff, in the form of a legible duration of time.

  len("4294967296 days, 23 hours, 59 minutes, 60.00 seconds")  ->  52
*/
static void nice_time(double sec,char *buff,bool part_second)
{
  ulong tmp;
  if (sec >= 3600.0*24)
  {
    tmp=(ulong) floor(sec/(3600.0*24));
    sec-=3600.0*24*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
    buff=strmov(buff,tmp > 1 ? " days " : " day ");
  }
  if (sec >= 3600.0)
  {
    tmp=(ulong) floor(sec/3600.0);
    sec-=3600.0*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
    buff=strmov(buff,tmp > 1 ? " hours " : " hour ");
  }
  if (sec >= 60.0)
  {
    tmp=(ulong) floor(sec/60.0);
    sec-=60.0*tmp;
    buff=int10_to_str((long) tmp, buff, 10);
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

static const char* construct_prompt()
{
  processed_prompt.free();			// Erase the old prompt
  time_t  lclock = time(NULL);			// Get the date struct
  struct tm *t = localtime(&lclock);

  /* parse thru the settings for the prompt */
  for (char *c = current_prompt; *c ; c++)
  {
    if (*c != PROMPT_CHAR)
	processed_prompt.append(*c);
    else
    {
      switch (*++c) {
      case '\0':
	c--;			// stop it from going beyond if ends with %
	break;
      case 'c':
	add_int_to_prompt(++prompt_counter);
	break;
      case 'v':
	if (connected)
	  processed_prompt.append(mysql_get_server_info(&mysql));
	else
	  processed_prompt.append("not_connected");
	break;
      case 'd':
	processed_prompt.append(current_db ? current_db : "(none)");
	break;
      case 'h':
      {
	const char *prompt;
	prompt= connected ? mysql_get_host_info(&mysql) : "not_connected";
	if (strstr(prompt, "Localhost"))
	  processed_prompt.append("localhost");
	else
	{
	  const char *end=strcend(prompt,' ');
	  processed_prompt.append(prompt, (uint) (end-prompt));
	}
	break;
      }
      case 'p':
      {
#ifndef EMBEDDED_LIBRARY
	if (!connected)
	{
	  processed_prompt.append("not_connected");
	  break;
	}

	const char *host_info = mysql_get_host_info(&mysql);
	if (strstr(host_info, "memory")) 
	{
		processed_prompt.append( mysql.host );
	}
	else if (strstr(host_info,"TCP/IP") ||
	    !mysql.unix_socket)
	  add_int_to_prompt(mysql.port);
	else
	{
	  char *pos=strrchr(mysql.unix_socket,'/');
 	  processed_prompt.append(pos ? pos+1 : mysql.unix_socket);
	}
#endif
      }
	break;
      case 'U':
	if (!full_username)
	  init_username();
        processed_prompt.append(full_username ? full_username :
                                (current_user ?  current_user : "(unknown)"));
	break;
      case 'u':
	if (!full_username)
	  init_username();
        processed_prompt.append(part_username ? part_username :
                                (current_user ?  current_user : "(unknown)"));
	break;
      case PROMPT_CHAR:
	processed_prompt.append(PROMPT_CHAR);
	break;
      case 'n':
	processed_prompt.append('\n');
	break;
      case ' ':
      case '_':
	processed_prompt.append(' ');
	break;
      case 'R':
	if (t->tm_hour < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(t->tm_hour);
	break;
      case 'r':
	int getHour;
	getHour = t->tm_hour % 12;
	if (getHour == 0)
	  getHour=12;
	if (getHour < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(getHour);
	break;
      case 'm':
	if (t->tm_min < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(t->tm_min);
	break;
      case 'y':
	int getYear;
	getYear = t->tm_year % 100;
	if (getYear < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(getYear);
	break;
      case 'Y':
	add_int_to_prompt(t->tm_year+1900);
	break;
      case 'D':
	char* dateTime;
	dateTime = ctime(&lclock);
	processed_prompt.append(strtok(dateTime,"\n"));
	break;
      case 's':
	if (t->tm_sec < 10)
	  processed_prompt.append('0');
	add_int_to_prompt(t->tm_sec);
	break;
      case 'w':
	processed_prompt.append(day_names[t->tm_wday]);
	break;
      case 'P':
	processed_prompt.append(t->tm_hour < 12 ? "am" : "pm");
	break;
      case 'o':
	add_int_to_prompt(t->tm_mon+1);
	break;
      case 'O':
	processed_prompt.append(month_names[t->tm_mon]);
	break;
      case '\'':
	processed_prompt.append("'");
	break;
      case '"':
	processed_prompt.append('"');
	break;
      case 'S':
	processed_prompt.append(';');
	break;
      case 't':
	processed_prompt.append('\t');
	break;
      case 'l':
	processed_prompt.append(delimiter_str);
	break;
      default:
	processed_prompt.append(c);
      }
    }
  }
  processed_prompt.append('\0');
  return processed_prompt.ptr();
}


static void add_int_to_prompt(int toadd)
{
  char buffer[16];
  int10_to_str(toadd,buffer,10);
  processed_prompt.append(buffer);
}

static void init_username()
{
  my_free(full_username);
  my_free(part_username);

  MYSQL_RES *result;
  LINT_INIT(result);
  if (!mysql_query(&mysql,"select USER()") &&
      (result=mysql_use_result(&mysql)))
  {
    MYSQL_ROW cur=mysql_fetch_row(result);
    full_username=my_strdup(cur[0],MYF(MY_WME));
    part_username=my_strdup(strtok(cur[0],"@"),MYF(MY_WME));
    (void) mysql_fetch_row(result);		// Read eof
  }
}

static int com_prompt(String *buffer __attribute__((unused)),
                      char *line)
{
  char *ptr=strchr(line, ' ');
  prompt_counter = 0;
  my_free(current_prompt);
  current_prompt=my_strdup(ptr ? ptr+1 : default_prompt,MYF(MY_WME));
  if (!ptr)
    tee_fprintf(stdout, "Returning to default PROMPT of %s\n", default_prompt);
  else
    tee_fprintf(stdout, "PROMPT set to '%s'\n", current_prompt);
  return 0;
}
