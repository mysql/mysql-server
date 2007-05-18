/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  mysqltest

  Tool used for executing a .test file

  See the "MySQL Test framework manual" for more information
  http://dev.mysql.com/doc/mysqltest/en/index.html

  Please keep the test framework tools identical in all versions!

  Written by:
  Sasha Pachev <sasha@mysql.com>
  Matt Wagner  <matt@mysql.com>
  Monty
  Jani
  Holyfoot
*/

#define MTEST_VERSION "3.2"

#include "client_priv.h"
#include <mysql_version.h>
#include <mysqld_error.h>
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include <stdarg.h>
#include <violite.h>
#include "my_regex.h" /* Our own version of regex */
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/* Use cygwin for --exec and --system before 5.0 */
#if MYSQL_VERSION_ID < 50000
#define USE_CYGWIN
#endif

#define MAX_VAR_NAME_LENGTH    256
#define MAX_COLUMNS            256
#define MAX_EMBEDDED_SERVER_ARGS 64
#define MAX_DELIMITER_LENGTH 16

/* Flags controlling send and reap */
#define QUERY_SEND_FLAG  1
#define QUERY_REAP_FLAG  2

                           enum {
   RESULT_OK= 0,
   RESULT_CONTENT_MISMATCH= 1,
   RESULT_LENGTH_MISMATCH= 2
 };

enum {
  OPT_SKIP_SAFEMALLOC=OPT_MAX_CLIENT_OPTION,
  OPT_PS_PROTOCOL, OPT_SP_PROTOCOL, OPT_CURSOR_PROTOCOL, OPT_VIEW_PROTOCOL,
  OPT_MAX_CONNECT_RETRIES, OPT_MARK_PROGRESS, OPT_LOG_DIR
};

static int record= 0, opt_sleep= -1;
static char *opt_db= 0, *opt_pass= 0;
const char *opt_user= 0, *opt_host= 0, *unix_sock= 0, *opt_basedir= "./";
const char *opt_logdir= "";
const char *opt_include= 0, *opt_charsets_dir;
static int opt_port= 0;
static int opt_max_connect_retries;
static my_bool opt_compress= 0, silent= 0, verbose= 0;
static my_bool tty_password= 0;
static my_bool opt_mark_progress= 0;
static my_bool ps_protocol= 0, ps_protocol_enabled= 0;
static my_bool sp_protocol= 0, sp_protocol_enabled= 0;
static my_bool view_protocol= 0, view_protocol_enabled= 0;
static my_bool cursor_protocol= 0, cursor_protocol_enabled= 0;
static my_bool parsing_disabled= 0;
static my_bool display_result_vertically= FALSE,
  display_metadata= FALSE, display_result_sorted= FALSE;
static my_bool disable_query_log= 0, disable_result_log= 0;
static my_bool disable_warnings= 0;
static my_bool disable_info= 1;
static my_bool abort_on_error= 1;
static my_bool server_initialized= 0;
static my_bool is_windows= 0;
static char **default_argv;
static const char *load_default_groups[]= { "mysqltest", "client", 0 };
static char line_buffer[MAX_DELIMITER_LENGTH], *line_buffer_pos= line_buffer;

static uint start_lineno= 0; /* Start line of current command */

static char delimiter[MAX_DELIMITER_LENGTH]= ";";
static uint delimiter_length= 1;

static char TMPDIR[FN_REFLEN];

/* Block stack */
enum block_cmd {
  cmd_none,
  cmd_if,
  cmd_while
};

struct st_block
{
  int             line; /* Start line of block */
  my_bool         ok;   /* Should block be executed */
  enum block_cmd  cmd;  /* Command owning the block */
};

static struct st_block block_stack[32];
static struct st_block *cur_block, *block_stack_end;

/* Open file stack */
struct st_test_file
{
  FILE* file;
  const char *file_name;
  uint lineno; /* Current line in file */
};

static struct st_test_file file_stack[16];
static struct st_test_file* cur_file;
static struct st_test_file* file_stack_end;


static CHARSET_INFO *charset_info= &my_charset_latin1; /* Default charset */

static const char *embedded_server_groups[]=
{
  "server",
  "embedded",
  "mysqltest_SERVER",
  NullS
};

static int embedded_server_arg_count=0;
static char *embedded_server_args[MAX_EMBEDDED_SERVER_ARGS];

/*
  Timer related variables
  See the timer_output() definition for details
*/
static char *timer_file = NULL;
static ulonglong timer_start;
static void timer_output(void);
static ulonglong timer_now(void);

static ulonglong progress_start= 0;

/* Precompiled re's */
static my_regex_t ps_re;     /* the query can be run using PS protocol */
static my_regex_t sp_re;     /* the query can be run as a SP */
static my_regex_t view_re;   /* the query can be run as a view*/

static void init_re(void);
static int match_re(my_regex_t *, char *);
static void free_re(void);

DYNAMIC_ARRAY q_lines;

#include "sslopt-vars.h"

struct
{
  int read_lines,current_line;
} parser;

struct
{
  char file[FN_REFLEN];
  ulong pos;
} master_pos;

/* if set, all results are concated and compared against this file */
const char *result_file_name= 0;

typedef struct st_var
{
  char *name;
  int name_len;
  char *str_val;
  int str_val_len;
  int int_val;
  int alloced_len;
  int int_dirty; /* do not update string if int is updated until first read */
  int alloced;
  char *env_s;
} VAR;

/*Perl/shell-like variable registers */
VAR var_reg[10];

HASH var_hash;

struct st_connection
{
  MYSQL mysql;
  /* Used when creating views and sp, to avoid implicit commit */
  MYSQL* util_mysql;
  char *name;
  MYSQL_STMT* stmt;

#ifdef EMBEDDED_LIBRARY
  const char *cur_query;
  int cur_query_len;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int query_done;
#endif /*EMBEDDED_LIBRARY*/
};
struct st_connection connections[128];
struct st_connection* cur_con, *next_con, *connections_end;

/*
  List of commands in mysqltest
  Must match the "command_names" array
  Add new commands before Q_UNKNOWN!
*/
enum enum_commands {
  Q_CONNECTION=1,     Q_QUERY,
  Q_CONNECT,	    Q_SLEEP, Q_REAL_SLEEP,
  Q_INC,		    Q_DEC,
  Q_SOURCE,	    Q_DISCONNECT,
  Q_LET,		    Q_ECHO,
  Q_WHILE,	    Q_END_BLOCK,
  Q_SYSTEM,	    Q_RESULT,
  Q_REQUIRE,	    Q_SAVE_MASTER_POS,
  Q_SYNC_WITH_MASTER,
  Q_SYNC_SLAVE_WITH_MASTER,
  Q_ERROR,
  Q_SEND,		    Q_REAP,
  Q_DIRTY_CLOSE,	    Q_REPLACE, Q_REPLACE_COLUMN,
  Q_PING,		    Q_EVAL,
  Q_RPL_PROBE,	    Q_ENABLE_RPL_PARSE,
  Q_DISABLE_RPL_PARSE, Q_EVAL_RESULT,
  Q_ENABLE_QUERY_LOG, Q_DISABLE_QUERY_LOG,
  Q_ENABLE_RESULT_LOG, Q_DISABLE_RESULT_LOG,
  Q_WAIT_FOR_SLAVE_TO_STOP,
  Q_ENABLE_WARNINGS, Q_DISABLE_WARNINGS,
  Q_ENABLE_INFO, Q_DISABLE_INFO,
  Q_ENABLE_METADATA, Q_DISABLE_METADATA,
  Q_EXEC, Q_DELIMITER,
  Q_DISABLE_ABORT_ON_ERROR, Q_ENABLE_ABORT_ON_ERROR,
  Q_DISPLAY_VERTICAL_RESULTS, Q_DISPLAY_HORIZONTAL_RESULTS,
  Q_QUERY_VERTICAL, Q_QUERY_HORIZONTAL, Q_SORTED_RESULT,
  Q_START_TIMER, Q_END_TIMER,
  Q_CHARACTER_SET, Q_DISABLE_PS_PROTOCOL, Q_ENABLE_PS_PROTOCOL,
  Q_DISABLE_RECONNECT, Q_ENABLE_RECONNECT,
  Q_IF,
  Q_DISABLE_PARSING, Q_ENABLE_PARSING,
  Q_REPLACE_REGEX, Q_REMOVE_FILE, Q_FILE_EXIST,
  Q_WRITE_FILE, Q_COPY_FILE, Q_PERL, Q_DIE, Q_EXIT, Q_SKIP,
  Q_CHMOD_FILE, Q_APPEND_FILE, Q_CAT_FILE, Q_DIFF_FILES,

  Q_UNKNOWN,			       /* Unknown command.   */
  Q_COMMENT,			       /* Comments, ignored. */
  Q_COMMENT_WITH_COMMAND
};


const char *command_names[]=
{
  "connection",
  "query",
  "connect",
  "sleep",
  "real_sleep",
  "inc",
  "dec",
  "source",
  "disconnect",
  "let",
  "echo",
  "while",
  "end",
  "system",
  "result",
  "require",
  "save_master_pos",
  "sync_with_master",
  "sync_slave_with_master",
  "error",
  "send",
  "reap",
  "dirty_close",
  "replace_result",
  "replace_column",
  "ping",
  "eval",
  "rpl_probe",
  "enable_rpl_parse",
  "disable_rpl_parse",
  "eval_result",
  /* Enable/disable that the _query_ is logged to result file */
  "enable_query_log",
  "disable_query_log",
  /* Enable/disable that the _result_ from a query is logged to result file */
  "enable_result_log",
  "disable_result_log",
  "wait_for_slave_to_stop",
  "enable_warnings",
  "disable_warnings",
  "enable_info",
  "disable_info",
  "enable_metadata",
  "disable_metadata",
  "exec",
  "delimiter",
  "disable_abort_on_error",
  "enable_abort_on_error",
  "vertical_results",
  "horizontal_results",
  "query_vertical",
  "query_horizontal",
  "sorted_result",
  "start_timer",
  "end_timer",
  "character_set",
  "disable_ps_protocol",
  "enable_ps_protocol",
  "disable_reconnect",
  "enable_reconnect",
  "if",
  "disable_parsing",
  "enable_parsing",
  "replace_regex",
  "remove_file",
  "file_exists",
  "write_file",
  "copy_file",
  "perl",
  "die",
               
  /* Don't execute any more commands, compare result */
  "exit",
  "skip",
  "chmod",
  "append_file",
  "cat_file",
  "diff_files",
  0
};


/*
  The list of error codes to --error are stored in an internal array of
  structs. This struct can hold numeric SQL error codes, error names or
  SQLSTATE codes as strings. The element next to the last active element
  in the list is set to type ERR_EMPTY. When an SQL statement returns an
  error, we use this list to check if this is an expected error.
*/
enum match_err_type
{
  ERR_EMPTY= 0,
  ERR_ERRNO,
  ERR_SQLSTATE
};

struct st_match_err
{
  enum match_err_type type;
  union
  {
    uint errnum;
    char sqlstate[SQLSTATE_LENGTH+1];  /* \0 terminated string */
  } code;
};

struct st_expected_errors
{
  struct st_match_err err[10];
  uint count;
};
static struct st_expected_errors saved_expected_errors;

struct st_command
{
  char *query, *query_buf,*first_argument,*last_argument,*end;
  int first_word_len, query_len;
  my_bool abort_on_error;
  struct st_expected_errors expected_errors;
  char require_file[FN_REFLEN];
  enum enum_commands type;
};

TYPELIB command_typelib= {array_elements(command_names),"",
			  command_names, 0};

DYNAMIC_STRING ds_res, ds_progress, ds_warning_messages;

char builtin_echo[FN_REFLEN];

void die(const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);
void abort_not_supported_test(const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);
void verbose_msg(const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);
void warning_msg(const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);
void log_msg(const char *fmt, ...)
  ATTRIBUTE_FORMAT(printf, 1, 2);

VAR* var_from_env(const char *, const char *);
VAR* var_init(VAR* v, const char *name, int name_len, const char *val,
              int val_len);
void var_free(void* v);
VAR* var_get(const char *var_name, const char** var_name_end,
             my_bool raw, my_bool ignore_not_existing);
void eval_expr(VAR* v, const char *p, const char** p_end);
my_bool match_delimiter(int c, const char *delim, uint length);
void dump_result_to_reject_file(char *buf, int size);
void dump_result_to_log_file(char *buf, int size);
void dump_warning_messages();
void dump_progress();

void do_eval(DYNAMIC_STRING *query_eval, const char *query,
             const char *query_end, my_bool pass_through_escape_chars);
void str_to_file(const char *fname, char *str, int size);
void str_to_file2(const char *fname, char *str, int size, my_bool append);

#ifdef __WIN__
void free_tmp_sh_file();
void free_win_path_patterns();
#endif

static int eval_result = 0;

/* For replace_column */
static char *replace_column[MAX_COLUMNS];
static uint max_replace_column= 0;
void do_get_replace_column(struct st_command*);
void free_replace_column();

/* For replace */
void do_get_replace(struct st_command *command);
void free_replace();

/* For replace_regex */
void do_get_replace_regex(struct st_command *command);
void free_replace_regex();


void free_all_replace(){
  free_replace();
  free_replace_regex();
  free_replace_column();
}


/* Disable functions that only exist in MySQL 4.0 */
#if MYSQL_VERSION_ID < 40000
void mysql_enable_rpl_parse(MYSQL* mysql __attribute__((unused))) {}
void mysql_disable_rpl_parse(MYSQL* mysql __attribute__((unused))) {}
int mysql_rpl_parse_enabled(MYSQL* mysql __attribute__((unused))) { return 1; }
my_bool mysql_rpl_probe(MYSQL *mysql __attribute__((unused))) { return 1; }
#endif
void replace_dynstr_append_mem(DYNAMIC_STRING *ds, const char *val,
                               int len);
void replace_dynstr_append(DYNAMIC_STRING *ds, const char *val);
void replace_dynstr_append_uint(DYNAMIC_STRING *ds, uint val);
void dynstr_append_sorted(DYNAMIC_STRING* ds, DYNAMIC_STRING* ds_input);

void handle_error(struct st_command*,
                  unsigned int err_errno, const char *err_error,
                  const char *err_sqlstate, DYNAMIC_STRING *ds);
void handle_no_error(struct st_command*);

#ifdef EMBEDDED_LIBRARY
/*
  send_one_query executes query in separate thread what is
  necessary in embedded library to run 'send' in proper way.
  This implementation doesn't handle errors returned
  by mysql_send_query. It's technically possible, though
  i don't see where it is needed.
*/
pthread_handler_t send_one_query(void *arg)
{
  struct st_connection *cn= (struct st_connection*)arg;

  mysql_thread_init();
  VOID(mysql_send_query(&cn->mysql, cn->cur_query, cn->cur_query_len));

  mysql_thread_end();
  pthread_mutex_lock(&cn->mutex);
  cn->query_done= 1;
  VOID(pthread_cond_signal(&cn->cond));
  pthread_mutex_unlock(&cn->mutex);
  pthread_exit(0);
  return 0;
}

static int do_send_query(struct st_connection *cn, const char *q, int q_len,
                         int flags)
{
  pthread_t tid;

  if (flags & QUERY_REAP_FLAG)
    return mysql_send_query(&cn->mysql, q, q_len);

  if (pthread_mutex_init(&cn->mutex, NULL) ||
      pthread_cond_init(&cn->cond, NULL))
    die("Error in the thread library");

  cn->cur_query= q;
  cn->cur_query_len= q_len;
  cn->query_done= 0;
  if (pthread_create(&tid, NULL, send_one_query, (void*)cn))
    die("Cannot start new thread for query");

  return 0;
}

#else /*EMBEDDED_LIBRARY*/

#define do_send_query(cn,q,q_len,flags) mysql_send_query(&cn->mysql, q, q_len)

#endif /*EMBEDDED_LIBRARY*/

void do_eval(DYNAMIC_STRING *query_eval, const char *query,
             const char *query_end, my_bool pass_through_escape_chars)
{
  const char *p;
  register char c, next_c;
  register int escaped = 0;
  VAR *v;
  DBUG_ENTER("do_eval");

  for (p= query; (c= *p) && p < query_end; ++p)
  {
    switch(c) {
    case '$':
      if (escaped)
      {
	escaped= 0;
	dynstr_append_mem(query_eval, p, 1);
      }
      else
      {
	if (!(v= var_get(p, &p, 0, 0)))
	  die("Bad variable in eval");
	dynstr_append_mem(query_eval, v->str_val, v->str_val_len);
      }
      break;
    case '\\':
      next_c= *(p+1);
      if (escaped)
      {
	escaped= 0;
	dynstr_append_mem(query_eval, p, 1);
      }
      else if (next_c == '\\' || next_c == '$' || next_c == '"')
      {
        /* Set escaped only if next char is \, " or $ */
	escaped= 1;

        if (pass_through_escape_chars)
        {
          /* The escape char should be added to the output string. */
          dynstr_append_mem(query_eval, p, 1);
        }
      }
      else
	dynstr_append_mem(query_eval, p, 1);
      break;
    default:
      escaped= 0;
      dynstr_append_mem(query_eval, p, 1);
      break;
    }
  }
  DBUG_VOID_RETURN;
}


enum arg_type
{
  ARG_STRING,
  ARG_REST
};

struct command_arg {
  const char *argname;       /* Name of argument   */
  enum arg_type type;        /* Type of argument   */
  my_bool required;          /* Argument required  */
  DYNAMIC_STRING *ds;        /* Storage for argument */
  const char *description;   /* Description of the argument */
};


void check_command_args(struct st_command *command,
                        const char *arguments,
                        const struct command_arg *args,
                        int num_args, const char delimiter_arg)
{
  int i;
  const char *ptr= arguments;
  const char *start;
  DBUG_ENTER("check_command_args");
  DBUG_PRINT("enter", ("num_args: %d", num_args));

  for (i= 0; i < num_args; i++)
  {
    const struct command_arg *arg= &args[i];

    switch (arg->type) {
      /* A string */
    case ARG_STRING:
      /* Skip leading spaces */
      while (*ptr && *ptr == ' ')
        ptr++;
      start= ptr;
      /* Find end of arg, terminated by "delimiter_arg" */
      while (*ptr && *ptr != delimiter_arg)
        ptr++;
      if (ptr > start)
      {
        init_dynamic_string(arg->ds, 0, ptr-start, 32);
        do_eval(arg->ds, start, ptr, FALSE);
      }
      else
      {
        /* Empty string */
        init_dynamic_string(arg->ds, "", 0, 0);
      }
      command->last_argument= (char*)ptr;

      /* Step past the delimiter */
      if (*ptr && *ptr == delimiter_arg)
        ptr++;
      DBUG_PRINT("info", ("val: %s", arg->ds->str));
      break;

      /* Rest of line */
    case ARG_REST:
      start= ptr;
      init_dynamic_string(arg->ds, 0, command->query_len, 256);
      do_eval(arg->ds, start, command->end, FALSE);
      command->last_argument= command->end;
      DBUG_PRINT("info", ("val: %s", arg->ds->str));
      break;

    default:
      DBUG_ASSERT("Unknown argument type");
      break;
    }

    /* Check required arg */
    if (arg->ds->length == 0 && arg->required)
      die("Missing required argument '%s' to command '%.*s'", arg->argname,
          command->first_word_len, command->query);

  }
  DBUG_VOID_RETURN;
}


void handle_command_error(struct st_command *command, uint error)
{
  DBUG_ENTER("handle_command_error");
  DBUG_PRINT("enter", ("error: %d", error));
  if (error != 0)
  {
    uint i;

    if (command->abort_on_error)
      die("command \"%.*s\" failed with error %d",
          command->first_word_len, command->query, error);
    for (i= 0; i < command->expected_errors.count; i++)
    {
      DBUG_PRINT("info", ("expected error: %d",
                          command->expected_errors.err[i].code.errnum));
      if ((command->expected_errors.err[i].type == ERR_ERRNO) &&
          (command->expected_errors.err[i].code.errnum == error))
      {
        DBUG_PRINT("info", ("command \"%.*s\" failed with expected error: %d",
                            command->first_word_len, command->query, error));
        DBUG_VOID_RETURN;
      }
    }
    die("command \"%.*s\" failed with wrong error: %d",
        command->first_word_len, command->query, error);
  }
  else if (command->expected_errors.err[0].type == ERR_ERRNO &&
           command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    die("command \"%.*s\" succeeded - should have failed with errno %d...",
        command->first_word_len, command->query,
        command->expected_errors.err[0].code.errnum);
  }
  DBUG_VOID_RETURN;
}


void close_connections()
{
  DBUG_ENTER("close_connections");
  for (--next_con; next_con >= connections; --next_con)
  {
    if (next_con->stmt)
      mysql_stmt_close(next_con->stmt);
    next_con->stmt= 0;
    mysql_close(&next_con->mysql);
    if (next_con->util_mysql)
      mysql_close(next_con->util_mysql);
    my_free(next_con->name, MYF(MY_ALLOW_ZERO_PTR));
  }
  DBUG_VOID_RETURN;
}


void close_statements()
{
  struct st_connection *con;
  DBUG_ENTER("close_statements");
  for (con= connections; con < next_con; con++)
  {
    if (con->stmt)
      mysql_stmt_close(con->stmt);
    con->stmt= 0;
  }
  DBUG_VOID_RETURN;
}


void close_files()
{
  DBUG_ENTER("close_files");
  for (; cur_file >= file_stack; cur_file--)
  {
    if (cur_file->file && cur_file->file != stdin)
    {
      DBUG_PRINT("info", ("closing file: %s", cur_file->file_name));
      my_fclose(cur_file->file, MYF(0));
    }
    my_free((gptr)cur_file->file_name, MYF(MY_ALLOW_ZERO_PTR));
    cur_file->file_name= 0;
  }
  DBUG_VOID_RETURN;
}


void free_used_memory()
{
  uint i;
  DBUG_ENTER("free_used_memory");

  close_connections();
  close_files();
  hash_free(&var_hash);

  for (i= 0 ; i < q_lines.elements ; i++)
  {
    struct st_command **q= dynamic_element(&q_lines, i, struct st_command**);
    my_free((gptr) (*q)->query_buf,MYF(MY_ALLOW_ZERO_PTR));
    my_free((gptr) (*q),MYF(0));
  }
  for (i= 0; i < 10; i++)
  {
    if (var_reg[i].alloced_len)
      my_free(var_reg[i].str_val, MYF(MY_WME));
  }
  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count],MYF(0));
  delete_dynamic(&q_lines);
  dynstr_free(&ds_res);
  dynstr_free(&ds_progress);
  dynstr_free(&ds_warning_messages);
  free_all_replace();
  my_free(opt_pass,MYF(MY_ALLOW_ZERO_PTR));
  free_defaults(default_argv);
  free_re();
#ifdef __WIN__
  free_tmp_sh_file();
  free_win_path_patterns();
#endif

  /* Only call mysql_server_end if mysql_server_init has been called */
  if (server_initialized)
    mysql_server_end();

  /* Don't use DBUG after mysql_server_end() */
  return;
}


static void cleanup_and_exit(int exit_code)
{
  free_used_memory();
  my_end(MY_CHECK_ERROR);

  if (!silent)
  {
    switch (exit_code)
    {
    case 1:
      printf("not ok\n");
      break;
    case 0:
      printf("ok\n");
      break;
    case 62:
      printf("skipped\n");
    break;
    default:
      printf("unknown exit code: %d\n", exit_code);
      DBUG_ASSERT(0);
    }
  }

  exit(exit_code);
}

void die(const char *fmt, ...)
{
  static int dying= 0;
  va_list args;
  DBUG_ENTER("die");
  DBUG_PRINT("enter", ("start_lineno: %d", start_lineno));

  /*
    Protect against dying twice
    first time 'die' is called, try to write log files
    second time, just exit
  */
  if (dying)
    cleanup_and_exit(1);
  dying= 1;

  /* Print the error message */
  fprintf(stderr, "mysqltest: ");
  if (cur_file && cur_file != file_stack)
    fprintf(stderr, "In included file \"%s\": ",
            cur_file->file_name);
  if (start_lineno > 0)
    fprintf(stderr, "At line %u: ", start_lineno);
  if (fmt)
  {
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
  else
    fprintf(stderr, "unknown error");
  fprintf(stderr, "\n");
  fflush(stderr);

  /* Dump the result that has been accumulated so far to .log file */
  if (result_file_name && ds_res.length)
    dump_result_to_log_file(ds_res.str, ds_res.length);

  /* Dump warning messages */
  if (result_file_name && ds_warning_messages.length)
    dump_warning_messages();

  cleanup_and_exit(1);
}


void abort_not_supported_test(const char *fmt, ...)
{
  va_list args;
  struct st_test_file* err_file= cur_file;
  DBUG_ENTER("abort_not_supported_test");

  /* Print include filestack */
  fprintf(stderr, "The test '%s' is not supported by this installation\n",
          file_stack->file_name);
  fprintf(stderr, "Detected in file %s at line %d\n",
          err_file->file_name, err_file->lineno);
  while (err_file != file_stack)
  {
    err_file--;
    fprintf(stderr, "included from %s at line %d\n",
            err_file->file_name, err_file->lineno);
  }

  /* Print error message */
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "reason: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);

  cleanup_and_exit(62);
}


void abort_not_in_this_version()
{
  die("Not available in this version of mysqltest");
}


void verbose_msg(const char *fmt, ...)
{
  va_list args;
  DBUG_ENTER("verbose_msg");
  if (!verbose)
    DBUG_VOID_RETURN;

  va_start(args, fmt);
  fprintf(stderr, "mysqltest: ");
  if (cur_file && cur_file != file_stack)
    fprintf(stderr, "In included file \"%s\": ",
            cur_file->file_name);
  if (start_lineno != 0)
    fprintf(stderr, "At line %u: ", start_lineno);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);

  DBUG_VOID_RETURN;
}


void warning_msg(const char *fmt, ...)
{
  va_list args;
  char buff[512];
  size_t len;
  DBUG_ENTER("warning_msg");

  va_start(args, fmt);
  dynstr_append(&ds_warning_messages, "mysqltest: ");
  if (start_lineno != 0)
  {
    dynstr_append(&ds_warning_messages, "Warning detected ");
    if (cur_file && cur_file != file_stack)
    {
      len= my_snprintf(buff, sizeof(buff), "in included file %s ",
                       cur_file->file_name);
      dynstr_append_mem(&ds_warning_messages,
                        buff, len);
    }
    len= my_snprintf(buff, sizeof(buff), "at line %d: ",
                     start_lineno);
    dynstr_append_mem(&ds_warning_messages,
                      buff, len);
  }

  len= my_vsnprintf(buff, sizeof(buff), fmt, args);
  dynstr_append_mem(&ds_warning_messages, buff, len);

  dynstr_append(&ds_warning_messages, "\n");
  va_end(args);

  DBUG_VOID_RETURN;
}


void log_msg(const char *fmt, ...)
{
  va_list args;
  char buff[512];
  size_t len;
  DBUG_ENTER("log_msg");

  memset(buff, 0, sizeof(buff));
  va_start(args, fmt);
  len= my_vsnprintf(buff, sizeof(buff)-1, fmt, args);
  va_end(args);

  dynstr_append_mem(&ds_res, buff, len);
  dynstr_append(&ds_res, "\n");

  DBUG_VOID_RETURN;
}


/*
  Compare content of the string ds to content of file fname
*/

int dyn_string_cmp(DYNAMIC_STRING* ds, const char *fname)
{
  MY_STAT stat_info;
  char *tmp, *res_ptr;
  char eval_file[FN_REFLEN];
  int res;
  uint res_len;
  int fd;
  DYNAMIC_STRING res_ds;
  DBUG_ENTER("dyn_string_cmp");

  if (!test_if_hard_path(fname))
  {
    strxmov(eval_file, opt_basedir, fname, NullS);
    fn_format(eval_file, eval_file, "", "", MY_UNPACK_FILENAME);
  }
  else
    fn_format(eval_file, fname, "", "", MY_UNPACK_FILENAME);

  if (!my_stat(eval_file, &stat_info, MYF(MY_WME)))
    die(NullS);
  if (!eval_result && (uint) stat_info.st_size != ds->length)
  {
    DBUG_PRINT("info",("Size differs:  result size: %u  file size: %lu",
		       ds->length, (ulong) stat_info.st_size));
    DBUG_PRINT("info",("result: '%s'", ds->str));
    DBUG_RETURN(RESULT_LENGTH_MISMATCH);
  }
  if (!(tmp = (char*) my_malloc(stat_info.st_size + 1, MYF(MY_WME))))
    die("Out of memory");

  if ((fd = my_open(eval_file, O_RDONLY, MYF(MY_WME))) < 0)
    die("Failed to open file %s", eval_file);
  if (my_read(fd, (byte*)tmp, stat_info.st_size, MYF(MY_WME|MY_NABP)))
    die("Failed to read from file %s, errno: %d", eval_file, errno);
  tmp[stat_info.st_size] = 0;
  init_dynamic_string(&res_ds, "", stat_info.st_size+256, 256);
  if (eval_result)
  {
    do_eval(&res_ds, tmp, tmp + stat_info.st_size, FALSE);
    res_ptr= res_ds.str;
    res_len= res_ds.length;
    if (res_len != ds->length)
    {
      res= RESULT_LENGTH_MISMATCH;
      goto err;
    }
  }
  else
  {
    res_ptr = tmp;
    res_len = stat_info.st_size;
  }

  res= (memcmp(res_ptr, ds->str, res_len)) ?
    RESULT_CONTENT_MISMATCH : RESULT_OK;

err:
  if (res && eval_result)
    str_to_file(fn_format(eval_file, fname, "", ".eval",
                          MY_REPLACE_EXT),
                res_ptr, res_len);

  dynstr_free(&res_ds);
  my_free((gptr) tmp, MYF(0));
  my_close(fd, MYF(MY_WME));

  DBUG_RETURN(res);
}


/*
  Check the content of ds against result file

  SYNOPSIS
  check_result
  ds - content to be checked

  RETURN VALUES
  error - the function will not return

*/

void check_result(DYNAMIC_STRING* ds)
{
  DBUG_ENTER("check_result");
  DBUG_ASSERT(result_file_name);

  switch (dyn_string_cmp(ds, result_file_name))
  {
  case RESULT_OK:
    break; /* ok */
  case RESULT_LENGTH_MISMATCH:
    dump_result_to_reject_file(ds->str, ds->length);
    die("Result length mismatch");
    break;
  case RESULT_CONTENT_MISMATCH:
    dump_result_to_reject_file(ds->str, ds->length);
    die("Result content mismatch");
    break;
  default: /* impossible */
    die("Unknown error code from dyn_string_cmp()");
  }

  DBUG_VOID_RETURN;
}


/*
  Check the content of ds against a require file
  If match fails, abort the test with special error code
  indicating that test is not supported

  SYNOPSIS
  check_result
  ds - content to be checked
  fname - name of file to check against

  RETURN VALUES
  error - the function will not return

*/

void check_require(DYNAMIC_STRING* ds, const char *fname)
{
  DBUG_ENTER("check_require");

  if (dyn_string_cmp(ds, fname))
  {
    char reason[FN_REFLEN];
    fn_format(reason, fname, "", "", MY_REPLACE_EXT | MY_REPLACE_DIR);
    abort_not_supported_test("Test requires: '%s'", reason);
  }
  DBUG_VOID_RETURN;
}


static byte *get_var_key(const byte* var, uint* len,
                  my_bool __attribute__((unused)) t)
{
  register char* key;
  key = ((VAR*)var)->name;
  *len = ((VAR*)var)->name_len;
  return (byte*)key;
}


VAR *var_init(VAR *v, const char *name, int name_len, const char *val,
              int val_len)
{
  int val_alloc_len;
  VAR *tmp_var;
  if (!name_len && name)
    name_len = strlen(name);
  if (!val_len && val)
    val_len = strlen(val) ;
  val_alloc_len = val_len + 16; /* room to grow */
  if (!(tmp_var=v) && !(tmp_var = (VAR*)my_malloc(sizeof(*tmp_var)
                                                  + name_len+1, MYF(MY_WME))))
    die("Out of memory");

  tmp_var->name = (name) ? (char*) tmp_var + sizeof(*tmp_var) : 0;
  tmp_var->alloced = (v == 0);

  if (!(tmp_var->str_val = my_malloc(val_alloc_len+1, MYF(MY_WME))))
    die("Out of memory");

  memcpy(tmp_var->name, name, name_len);
  if (val)
  {
    memcpy(tmp_var->str_val, val, val_len);
    tmp_var->str_val[val_len]= 0;
  }
  tmp_var->name_len = name_len;
  tmp_var->str_val_len = val_len;
  tmp_var->alloced_len = val_alloc_len;
  tmp_var->int_val = (val) ? atoi(val) : 0;
  tmp_var->int_dirty = 0;
  tmp_var->env_s = 0;
  return tmp_var;
}


void var_free(void *v)
{
  my_free(((VAR*) v)->str_val, MYF(MY_WME));
  if (((VAR*)v)->alloced)
    my_free((char*) v, MYF(MY_WME));
}


VAR* var_from_env(const char *name, const char *def_val)
{
  const char *tmp;
  VAR *v;
  if (!(tmp = getenv(name)))
    tmp = def_val;

  v = var_init(0, name, strlen(name), tmp, strlen(tmp));
  my_hash_insert(&var_hash, (byte*)v);
  return v;
}


VAR* var_get(const char *var_name, const char **var_name_end, my_bool raw,
	     my_bool ignore_not_existing)
{
  int digit;
  VAR *v;
  DBUG_ENTER("var_get");
  DBUG_PRINT("enter", ("var_name: %s",var_name));

  if (*var_name != '$')
    goto err;
  digit = *++var_name - '0';
  if (digit < 0 || digit >= 10)
  {
    const char *save_var_name = var_name, *end;
    uint length;
    end = (var_name_end) ? *var_name_end : 0;
    while (my_isvar(charset_info,*var_name) && var_name != end)
      var_name++;
    if (var_name == save_var_name)
    {
      if (ignore_not_existing)
	DBUG_RETURN(0);
      die("Empty variable");
    }
    length= (uint) (var_name - save_var_name);
    if (length >= MAX_VAR_NAME_LENGTH)
      die("Too long variable name: %s", save_var_name);

    if (!(v = (VAR*) hash_search(&var_hash, save_var_name, length)))
    {
      char buff[MAX_VAR_NAME_LENGTH+1];
      strmake(buff, save_var_name, length);
      v= var_from_env(buff, "");
    }
    var_name--;	/* Point at last character */
  }
  else
    v = var_reg + digit;

  if (!raw && v->int_dirty)
  {
    sprintf(v->str_val, "%d", v->int_val);
    v->int_dirty = 0;
    v->str_val_len = strlen(v->str_val);
  }
  if (var_name_end)
    *var_name_end = var_name  ;
  DBUG_RETURN(v);
err:
  if (var_name_end)
    *var_name_end = 0;
  die("Unsupported variable name: %s", var_name);
  DBUG_RETURN(0);
}


VAR *var_obtain(const char *name, int len)
{
  VAR* v;
  if ((v = (VAR*)hash_search(&var_hash, name, len)))
    return v;
  v = var_init(0, name, len, "", 0);
  my_hash_insert(&var_hash, (byte*)v);
  return v;
}


/*
  - if variable starts with a $ it is regarded as a local test varable
  - if not it is treated as a environment variable, and the corresponding
  environment variable will be updated
*/

void var_set(const char *var_name, const char *var_name_end,
             const char *var_val, const char *var_val_end)
{
  int digit, env_var= 0;
  VAR *v;
  DBUG_ENTER("var_set");
  DBUG_PRINT("enter", ("var_name: '%.*s' = '%.*s' (length: %d)",
                       (int) (var_name_end - var_name), var_name,
                       (int) (var_val_end - var_val), var_val,
                       (int) (var_val_end - var_val)));

  if (*var_name != '$')
    env_var= 1;
  else
    var_name++;

  digit= *var_name - '0';
  if (!(digit < 10 && digit >= 0))
  {
    v= var_obtain(var_name, (uint) (var_name_end - var_name));
  }
  else
    v= var_reg + digit;

  eval_expr(v, var_val, (const char**) &var_val_end);

  if (env_var)
  {
    char buf[1024], *old_env_s= v->env_s;
    if (v->int_dirty)
    {
      sprintf(v->str_val, "%d", v->int_val);
      v->int_dirty= 0;
      v->str_val_len= strlen(v->str_val);
    }
    my_snprintf(buf, sizeof(buf), "%.*s=%.*s",
                v->name_len, v->name,
                v->str_val_len, v->str_val);
    if (!(v->env_s= my_strdup(buf, MYF(MY_WME))))
      die("Out of memory");
    putenv(v->env_s);
    my_free((gptr)old_env_s, MYF(MY_ALLOW_ZERO_PTR));
  }
  DBUG_VOID_RETURN;
}


void var_set_string(const char* name, const char* value)
{
  var_set(name, name + strlen(name), value, value + strlen(value));
}


void var_set_int(const char* name, int value)
{
  char buf[21];
  my_snprintf(buf, sizeof(buf), "%d", value);
  var_set_string(name, buf);
}


/*
  Store an integer (typically the returncode of the last SQL)
  statement in the mysqltest builtin variable $mysql_errno
*/

void var_set_errno(int sql_errno)
{
  var_set_int("$mysql_errno", sql_errno);
}

/*
  Set variable from the result of a query

  SYNOPSIS
  var_query_set()
  var	        variable to set from query
  query       start of query string to execute
  query_end   end of the query string to execute


  DESCRIPTION
  let @<var_name> = `<query>`

  Execute the query and assign the first row of result to var as
  a tab separated strings

  Also assign each column of the result set to
  variable "$<var_name>_<column_name>"
  Thus the tab separated output can be read from $<var_name> and
  and each individual column can be read as $<var_name>_<col_name>

*/

void var_query_set(VAR *var, const char *query, const char** query_end)
{
  char *end = (char*)((query_end && *query_end) ?
		      *query_end : query + strlen(query));
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL* mysql = &cur_con->mysql;
  DYNAMIC_STRING ds_query;
  DBUG_ENTER("var_query_set");
  LINT_INIT(res);

  while (end > query && *end != '`')
    --end;
  if (query == end)
    die("Syntax error in query, missing '`'");
  ++query;

  /* Eval the query, thus replacing all environment variables */
  init_dynamic_string(&ds_query, 0, (end - query) + 32, 256);
  do_eval(&ds_query, query, end, FALSE);

  if (mysql_real_query(mysql, ds_query.str, ds_query.length) ||
      !(res = mysql_store_result(mysql)))
  {
    die("Error running query '%s': %d %s", ds_query.str,
	mysql_errno(mysql), mysql_error(mysql));
  }
  dynstr_free(&ds_query);

  if ((row = mysql_fetch_row(res)) && row[0])
  {
    /*
      Concatenate all row results with tab in between to allow us to work
      with results from many columns (for example from SHOW VARIABLES)
    */
    DYNAMIC_STRING result;
    uint i;
    ulong *lengths;
#ifdef NOT_YET
    MYSQL_FIELD *fields= mysql_fetch_fields(res);
#endif

    init_dynamic_string(&result, "", 2048, 2048);
    lengths= mysql_fetch_lengths(res);
    for (i=0; i < mysql_num_fields(res); i++)
    {
      if (row[0])
      {
#ifdef NOT_YET
	/* Add to <var_name>_<col_name> */
	uint j;
	char var_col_name[MAX_VAR_NAME_LENGTH];
	uint length= snprintf(var_col_name, MAX_VAR_NAME_LENGTH,
			      "$%s_%s", var->name, fields[i].name);
	/* Convert characters not allowed in variable names to '_' */
	for (j= 1; j < length; j++)
	{
	  if (!my_isvar(charset_info,var_col_name[j]))
            var_col_name[j]= '_';
        }
	var_set(var_col_name,  var_col_name + length,
		row[i], row[i] + lengths[i]);
#endif
        /* Add column to tab separated string */
	dynstr_append_mem(&result, row[i], lengths[i]);
      }
      dynstr_append_mem(&result, "\t", 1);
    }
    end= result.str + result.length-1;
    eval_expr(var, result.str, (const char**) &end);
    dynstr_free(&result);
  }
  else
    eval_expr(var, "", 0);

  mysql_free_result(res);
  DBUG_VOID_RETURN;
}


void var_copy(VAR *dest, VAR *src)
{
  dest->int_val= src->int_val;
  dest->int_dirty= src->int_dirty;

  /* Alloc/realloc data for str_val in dest */
  if (dest->alloced_len < src->alloced_len &&
      !(dest->str_val= dest->str_val
        ? my_realloc(dest->str_val, src->alloced_len, MYF(MY_WME))
        : my_malloc(src->alloced_len, MYF(MY_WME))))
    die("Out of memory");
  else
    dest->alloced_len= src->alloced_len;

  /* Copy str_val data to dest */
  dest->str_val_len= src->str_val_len;
  if (src->str_val_len)
    memcpy(dest->str_val, src->str_val, src->str_val_len);
}


void eval_expr(VAR *v, const char *p, const char **p_end)
{
  static int MIN_VAR_ALLOC= 32; /* MASV why 32? */
  VAR *vp;
  if (*p == '$')
  {
    if ((vp= var_get(p, p_end, 0, 0)))
    {
      var_copy(v, vp);
      return;
    }
  }
  else if (*p == '`')
  {
    var_query_set(v, p, p_end);
  }
  else
  {
    int new_val_len = (p_end && *p_end) ?
      (int) (*p_end - p) : (int) strlen(p);
    if (new_val_len + 1 >= v->alloced_len)
    {
      v->alloced_len = (new_val_len < MIN_VAR_ALLOC - 1) ?
        MIN_VAR_ALLOC : new_val_len + 1;
      if (!(v->str_val =
            v->str_val ? my_realloc(v->str_val, v->alloced_len+1,
                                    MYF(MY_WME)) :
            my_malloc(v->alloced_len+1, MYF(MY_WME))))
        die("Out of memory");
    }
    v->str_val_len = new_val_len;
    memcpy(v->str_val, p, new_val_len);
    v->str_val[new_val_len] = 0;
    v->int_val=atoi(p);
    v->int_dirty=0;
  }
  return;
}


int open_file(const char *name)
{
  char buff[FN_REFLEN];
  DBUG_ENTER("open_file");
  DBUG_PRINT("enter", ("name: %s", name));
  if (!test_if_hard_path(name))
  {
    strxmov(buff, opt_basedir, name, NullS);
    name=buff;
  }
  fn_format(buff, name, "", "", MY_UNPACK_FILENAME);

  if (cur_file == file_stack_end)
    die("Source directives are nesting too deep");
  cur_file++;
  if (!(cur_file->file = my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(0))))
  {
    cur_file--;
    die("Could not open file %s", buff);
  }
  cur_file->file_name= my_strdup(buff, MYF(MY_FAE));
  cur_file->lineno=1;
  DBUG_RETURN(0);
}


/*
  Source and execute the given file

  SYNOPSIS
  do_source()
  query	called command

  DESCRIPTION
  source <file_name>

  Open the file <file_name> and execute it

*/

void do_source(struct st_command *command)
{
  static DYNAMIC_STRING ds_filename;
  const struct command_arg source_args[] = {
    { "filename", ARG_STRING, TRUE, &ds_filename, "File to source" }
  };
  DBUG_ENTER("do_source");

  check_command_args(command, command->first_argument, source_args,
                     sizeof(source_args)/sizeof(struct command_arg),
                     ' ');

  /*
    If this file has already been sourced, don't source it again.
    It's already available in the q_lines cache.
  */
  if (parser.current_line < (parser.read_lines - 1))
    ; /* Do nothing */
  else
  {
    DBUG_PRINT("info", ("sourcing file: %s", ds_filename.str));
    open_file(ds_filename.str);
  }

  dynstr_free(&ds_filename);
  return;
}


#if defined __WIN__

#ifdef USE_CYGWIN
/* Variables used for temporary sh files used for emulating Unix on Windows */
char tmp_sh_name[64], tmp_sh_cmd[70];
#endif

void init_tmp_sh_file()
{
#ifdef USE_CYGWIN
  /* Format a name for the tmp sh file that is unique for this process */
  my_snprintf(tmp_sh_name, sizeof(tmp_sh_name), "tmp_%d.sh", getpid());
  /* Format the command to execute in order to run the script */
  my_snprintf(tmp_sh_cmd, sizeof(tmp_sh_cmd), "sh %s", tmp_sh_name);
#endif
}


void free_tmp_sh_file()
{
#ifdef USE_CYGWIN
  my_delete(tmp_sh_name, MYF(0));
#endif
}
#endif


FILE* my_popen(DYNAMIC_STRING *ds_cmd, const char *mode)
{
#if defined __WIN__ && defined USE_CYGWIN
  /* Dump the command into a sh script file and execute with popen */
  str_to_file(tmp_sh_name, ds_cmd->str, ds_cmd->length);
  return popen(tmp_sh_cmd, mode);
#else
  return popen(ds_cmd->str, mode);
#endif
}


static void init_builtin_echo(void)
{
#ifdef __WIN__

  /* Look for "echo.exe" in same dir as mysqltest was started from */
  dirname_part(builtin_echo, my_progname);
  fn_format(builtin_echo, ".\\echo.exe",
            builtin_echo, "", MYF(MY_REPLACE_DIR));

  /* Make sure echo.exe exists */
  if (access(builtin_echo, F_OK) != 0)
    builtin_echo[0]= 0;
  return;

#else

  builtin_echo[0]= 0;
  return;

#endif
}


/*
  Replace a substring

  SYNOPSIS
    replace
    ds_str      The string to search and perform the replace in
    search_str  The string to search for
    search_len  Length of the string to search for
    replace_str The string to replace with
    replace_len Length of the string to replace with

  RETURN
    0 String replaced
    1 Could not find search_str in str
*/

static int replace(DYNAMIC_STRING *ds_str,
                   const char *search_str, ulong search_len,
                   const char *replace_str, ulong replace_len)
{
  DYNAMIC_STRING ds_tmp;
  const char *start= strstr(ds_str->str, search_str);
  if (!start)
    return 1;
  init_dynamic_string(&ds_tmp, "",
                      ds_str->length + replace_len, 256);
  dynstr_append_mem(&ds_tmp, ds_str->str, start - ds_str->str);
  dynstr_append_mem(&ds_tmp, replace_str, replace_len);
  dynstr_append(&ds_tmp, start + search_len);
  dynstr_set(ds_str, ds_tmp.str);
  dynstr_free(&ds_tmp);
  return 0;
}


/*
  Execute given command.

  SYNOPSIS
  do_exec()
  query	called command

  DESCRIPTION
  exec <command>

  Execute the text between exec and end of line in a subprocess.
  The error code returned from the subprocess is checked against the
  expected error array, previously set with the --error command.
  It can thus be used to execute a command that shall fail.

  NOTE
  Although mysqltest is executed from cygwin shell, the command will be
  executed in "cmd.exe". Thus commands like "rm" etc can NOT be used, use
  mysqltest commmand(s) like "remove_file" for that
*/

void do_exec(struct st_command *command)
{
  int error;
  char buf[512];
  FILE *res_file;
  char *cmd= command->first_argument;
  DYNAMIC_STRING ds_cmd;
  DBUG_ENTER("do_exec");
  DBUG_PRINT("enter", ("cmd: '%s'", cmd));

  /* Skip leading space */
  while (*cmd && my_isspace(charset_info, *cmd))
    cmd++;
  if (!*cmd)
    die("Missing argument in exec");
  command->last_argument= command->end;

  init_dynamic_string(&ds_cmd, 0, command->query_len+256, 256);
  /* Eval the command, thus replacing all environment variables */
  do_eval(&ds_cmd, cmd, command->end, !is_windows);

  /* Check if echo should be replaced with "builtin" echo */
  if (builtin_echo[0] && strncmp(cmd, "echo", 4) == 0)
  {
    /* Replace echo with our "builtin" echo */
    replace(&ds_cmd, "echo", 4, builtin_echo, strlen(builtin_echo));
  }

#ifdef __WIN__
#ifndef USE_CYGWIN
  /* Replace /dev/null with NUL */
  while(replace(&ds_cmd, "/dev/null", 9, "NUL", 3) == 0)
    ;
  /* Replace "closed stdout" with non existing output fd */
  while(replace(&ds_cmd, ">&-", 3, ">&4", 3) == 0)
    ;
#endif
#endif

  DBUG_PRINT("info", ("Executing '%s' as '%s'",
                      command->first_argument, ds_cmd.str));

  if (!(res_file= my_popen(&ds_cmd, "r")) && command->abort_on_error)
    die("popen(\"%s\", \"r\") failed", command->first_argument);

  while (fgets(buf, sizeof(buf), res_file))
  {
    if (disable_result_log)
    {
      buf[strlen(buf)-1]=0;
      DBUG_PRINT("exec_result",("%s", buf));
    }
    else
    {
      replace_dynstr_append(&ds_res, buf);
    }
  }
  error= pclose(res_file);
  if (error > 0)
  {
    uint status= WEXITSTATUS(error), i;
    my_bool ok= 0;

    if (command->abort_on_error)
    {
      log_msg("exec of '%s failed, error: %d, status: %d, errno: %d",
              ds_cmd.str, error, status, errno);
      die("command \"%s\" failed", command->first_argument);
    }

    DBUG_PRINT("info",
               ("error: %d, status: %d", error, status));
    for (i= 0; i < command->expected_errors.count; i++)
    {
      DBUG_PRINT("info", ("expected error: %d",
                          command->expected_errors.err[i].code.errnum));
      if ((command->expected_errors.err[i].type == ERR_ERRNO) &&
          (command->expected_errors.err[i].code.errnum == status))
      {
        ok= 1;
        DBUG_PRINT("info", ("command \"%s\" failed with expected error: %d",
                            command->first_argument, status));
      }
    }
    if (!ok)
      die("command \"%s\" failed with wrong error: %d",
          command->first_argument, status);
  }
  else if (command->expected_errors.err[0].type == ERR_ERRNO &&
           command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    log_msg("exec of '%s failed, error: %d, errno: %d",
            ds_cmd.str, error, errno);
    die("command \"%s\" succeeded - should have failed with errno %d...",
        command->first_argument, command->expected_errors.err[0].code.errnum);
  }

  dynstr_free(&ds_cmd);
  DBUG_VOID_RETURN;
}

enum enum_operator
{
  DO_DEC,
  DO_INC
};


/*
  Decrease or increase the value of a variable

  SYNOPSIS
  do_modify_var()
  query	called command
  operator    operation to perform on the var

  DESCRIPTION
  dec $var_name
  inc $var_name

*/

int do_modify_var(struct st_command *command,
                  enum enum_operator operator)
{
  const char *p= command->first_argument;
  VAR* v;
  if (!*p)
    die("Missing argument to %.*s", command->first_word_len, command->query);
  if (*p != '$')
    die("The argument to %.*s must be a variable (start with $)",
        command->first_word_len, command->query);
  v= var_get(p, &p, 1, 0);
  switch (operator) {
  case DO_DEC:
    v->int_val--;
    break;
  case DO_INC:
    v->int_val++;
    break;
  default:
    die("Invalid operator to do_modify_var");
    break;
  }
  v->int_dirty= 1;
  command->last_argument= (char*)++p;
  return 0;
}


/*
  Wrapper for 'system' function

  NOTE
  If mysqltest is executed from cygwin shell, the command will be
  executed in the "windows command interpreter" cmd.exe and we prepend "sh"
  to make it be executed by cygwins "bash". Thus commands like "rm",
  "mkdir" as well as shellscripts can executed by "system" in Windows.

*/

int my_system(DYNAMIC_STRING* ds_cmd)
{
#if defined __WIN__ && defined USE_CYGWIN
  /* Dump the command into a sh script file and execute with system */
  str_to_file(tmp_sh_name, ds_cmd->str, ds_cmd->length);
  return system(tmp_sh_cmd);
#else
  return system(ds_cmd->str);
#endif
}


/*
  SYNOPSIS
  do_system
  command	called command

  DESCRIPTION
  system <command>

  Eval the query to expand any $variables in the command.
  Execute the command with the "system" command.

*/

void do_system(struct st_command *command)
{
  DYNAMIC_STRING ds_cmd;
  DBUG_ENTER("do_system");

  if (strlen(command->first_argument) == 0)
    die("Missing arguments to system, nothing to do!");

  init_dynamic_string(&ds_cmd, 0, command->query_len + 64, 256);

  /* Eval the system command, thus replacing all environment variables */
  do_eval(&ds_cmd, command->first_argument, command->end, !is_windows);

#ifdef __WIN__
#ifndef USE_CYGWIN
   /* Replace /dev/null with NUL */
   while(replace(&ds_cmd, "/dev/null", 9, "NUL", 3) == 0)
     ;
#endif
#endif


  DBUG_PRINT("info", ("running system command '%s' as '%s'",
                      command->first_argument, ds_cmd.str));
  if (my_system(&ds_cmd))
  {
    if (command->abort_on_error)
      die("system command '%s' failed", command->first_argument);

    /* If ! abort_on_error, log message and continue */
    dynstr_append(&ds_res, "system command '");
    replace_dynstr_append(&ds_res, command->first_argument);
    dynstr_append(&ds_res, "' failed\n");
  }

  command->last_argument= command->end;
  dynstr_free(&ds_cmd);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  do_remove_file
  command	called command

  DESCRIPTION
  remove_file <file_name>
  Remove the file <file_name>
*/

void do_remove_file(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_filename;
  const struct command_arg rm_args[] = {
    { "filename", ARG_STRING, TRUE, &ds_filename, "File to delete" }
  };
  DBUG_ENTER("do_remove_file");

  check_command_args(command, command->first_argument,
                     rm_args, sizeof(rm_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("info", ("removing file: %s", ds_filename.str));
  error= my_delete(ds_filename.str, MYF(0)) != 0;
  handle_command_error(command, error);
  dynstr_free(&ds_filename);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  do_copy_file
  command	command handle

  DESCRIPTION
  copy_file <from_file> <to_file>
  Copy <from_file> to <to_file>

  NOTE! Will fail if <to_file> exists
*/

void do_copy_file(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_from_file;
  static DYNAMIC_STRING ds_to_file;
  const struct command_arg copy_file_args[] = {
    { "from_file", ARG_STRING, TRUE, &ds_from_file, "Filename to copy from" },
    { "to_file", ARG_STRING, TRUE, &ds_to_file, "Filename to copy to" }
  };
  DBUG_ENTER("do_copy_file");

  check_command_args(command, command->first_argument,
                     copy_file_args,
                     sizeof(copy_file_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("info", ("Copy %s to %s", ds_from_file.str, ds_to_file.str));
  error= (my_copy(ds_from_file.str, ds_to_file.str,
                  MYF(MY_DONT_OVERWRITE_FILE)) != 0);
  handle_command_error(command, error);
  dynstr_free(&ds_from_file);
  dynstr_free(&ds_to_file);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  do_chmod_file
  command	command handle

  DESCRIPTION
  chmod <octal> <file>
  Change file permission of <file>

*/

void do_chmod_file(struct st_command *command)
{
  long mode= 0;
  static DYNAMIC_STRING ds_mode;
  static DYNAMIC_STRING ds_file;
  const struct command_arg chmod_file_args[] = {
    { "mode", ARG_STRING, TRUE, &ds_mode, "Mode of file" },
    { "file", ARG_STRING, TRUE, &ds_file, "Filename of file to modify" }
  };
  DBUG_ENTER("do_chmod_file");

  check_command_args(command, command->first_argument,
                     chmod_file_args,
                     sizeof(chmod_file_args)/sizeof(struct command_arg),
                     ' ');

  /* Parse what mode to set */
  if (ds_mode.length != 4 ||
      str2int(ds_mode.str, 8, 0, INT_MAX, &mode) == NullS)
    die("You must write a 4 digit octal number for mode");

  DBUG_PRINT("info", ("chmod %o %s", (uint)mode, ds_file.str));
  handle_command_error(command, chmod(ds_file.str, mode));
  dynstr_free(&ds_mode);
  dynstr_free(&ds_file);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  do_file_exists
  command	called command

  DESCRIPTION
  fiile_exist <file_name>
  Check if file <file_name> exists
*/

void do_file_exist(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_filename;
  const struct command_arg file_exist_args[] = {
    { "filename", ARG_STRING, TRUE, &ds_filename, "File to check if it exist" }
  };
  DBUG_ENTER("do_file_exist");

  check_command_args(command, command->first_argument,
                     file_exist_args,
                     sizeof(file_exist_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("info", ("Checking for existence of file: %s", ds_filename.str));
  error= (access(ds_filename.str, F_OK) != 0);
  handle_command_error(command, error);
  dynstr_free(&ds_filename);
  DBUG_VOID_RETURN;
}


/*
  Read characters from line buffer or file. This is needed to allow
  my_ungetc() to buffer MAX_DELIMITER_LENGTH characters for a file

  NOTE:
  This works as long as one doesn't change files (with 'source file_name')
  when there is things pushed into the buffer.  This should however not
  happen for any tests in the test suite.
*/

int my_getc(FILE *file)
{
  if (line_buffer_pos == line_buffer)
    return fgetc(file);
  return *--line_buffer_pos;
}


void my_ungetc(int c)
{
  *line_buffer_pos++= (char) c;
}


void read_until_delimiter(DYNAMIC_STRING *ds,
                          DYNAMIC_STRING *ds_delimiter)
{
  char c;
  DBUG_ENTER("read_until_delimiter");
  DBUG_PRINT("enter", ("delimiter: %s, length: %d",
                       ds_delimiter->str, ds_delimiter->length));

  if (ds_delimiter->length > MAX_DELIMITER_LENGTH)
    die("Max delimiter length(%d) exceeded", MAX_DELIMITER_LENGTH);

  /* Read from file until delimiter is found */
  while (1)
  {
    c= my_getc(cur_file->file);

    if (c == '\n')
      cur_file->lineno++;

    if (feof(cur_file->file))
      die("End of file encountered before '%s' delimiter was found",
          ds_delimiter->str);

    if (match_delimiter(c, ds_delimiter->str, ds_delimiter->length))
    {
      DBUG_PRINT("exit", ("Found delimiter '%s'", ds_delimiter->str));
      break;
    }
    dynstr_append_mem(ds, (const char*)&c, 1);
  }
  DBUG_PRINT("exit", ("ds: %s", ds->str));
  DBUG_VOID_RETURN;
}


void do_write_file_command(struct st_command *command, my_bool append)
{
  static DYNAMIC_STRING ds_content;
  static DYNAMIC_STRING ds_filename;
  static DYNAMIC_STRING ds_delimiter;
  const struct command_arg write_file_args[] = {
    { "filename", ARG_STRING, TRUE, &ds_filename, "File to write to" },
    { "delimiter", ARG_STRING, FALSE, &ds_delimiter, "Delimiter to read until" }
  };
  DBUG_ENTER("do_write_file");

  check_command_args(command,
                     command->first_argument,
                     write_file_args,
                     sizeof(write_file_args)/sizeof(struct command_arg),
                     ' ');

  /* If no delimiter was provided, use EOF */
  if (ds_delimiter.length == 0)
    dynstr_set(&ds_delimiter, "EOF");

  init_dynamic_string(&ds_content, "", 1024, 1024);
  read_until_delimiter(&ds_content, &ds_delimiter);
  DBUG_PRINT("info", ("Writing to file: %s", ds_filename.str));
  str_to_file2(ds_filename.str, ds_content.str, ds_content.length, append);
  dynstr_free(&ds_content);
  dynstr_free(&ds_filename);
  dynstr_free(&ds_delimiter);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  do_write_file
  command	called command

  DESCRIPTION
  write_file <file_name> [<delimiter>];
  <what to write line 1>
  <...>
  < what to write line n>
  EOF

  --write_file <file_name>;
  <what to write line 1>
  <...>
  < what to write line n>
  EOF

  Write everything between the "write_file" command and 'delimiter'
  to "file_name"

  NOTE! Overwrites existing file

  Default <delimiter> is EOF

*/

void do_write_file(struct st_command *command)
{
  do_write_file_command(command, FALSE);
}


/*
  SYNOPSIS
  do_append_file
  command	called command

  DESCRIPTION
  append_file <file_name> [<delimiter>];
  <what to write line 1>
  <...>
  < what to write line n>
  EOF

  --append_file <file_name>;
  <what to write line 1>
  <...>
  < what to write line n>
  EOF

  Append everything between the "append_file" command
  and 'delimiter' to "file_name"

  Default <delimiter> is EOF

*/

void do_append_file(struct st_command *command)
{
  do_write_file_command(command, TRUE);
}


/*
  SYNOPSIS
  do_cat_file
  command	called command

  DESCRIPTION
  cat_file <file_name>;

  Print the given file to result log

*/

void do_cat_file(struct st_command *command)
{
  int fd;
  uint len;
  char buff[512];
  static DYNAMIC_STRING ds_filename;
  const struct command_arg cat_file_args[] = {
    { "filename", ARG_STRING, TRUE, &ds_filename, "File to read from" }
  };
  DBUG_ENTER("do_cat_file");

  check_command_args(command,
                     command->first_argument,
                     cat_file_args,
                     sizeof(cat_file_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("info", ("Reading from, file: %s", ds_filename.str));

  if ((fd= my_open(ds_filename.str, O_RDONLY, MYF(0))) < 0)
    die("Failed to open file %s", ds_filename.str);
  while((len= my_read(fd, (byte*)&buff,
                      sizeof(buff), MYF(0))) > 0)
  {
    char *p= buff, *start= buff;
    while (p < buff+len)
    {
      /* Convert cr/lf to lf */
      if (*p == '\r' && *(p+1) && *(p+1)== '\n')
      {
        /* Add fake newline instead of cr and output the line */
        *p= '\n';
        p++; /* Step past the "fake" newline */
        dynstr_append_mem(&ds_res, start, p-start);
        p++; /* Step past the "fake" newline */
        start= p;
      }
      else
        p++;
    }
    /* Output any chars that migh be left */
    dynstr_append_mem(&ds_res, start, p-start);
  }
  my_close(fd, MYF(0));
  dynstr_free(&ds_filename);
  DBUG_VOID_RETURN;
}



/*
  SYNOPSIS
  do_diff_files
  command	called command

  DESCRIPTION
  diff_files <file1> <file2>;

  Fails if the two files differ.

*/

void do_diff_files(struct st_command *command)
{
  int error= 0;
  int fd, fd2;
  uint len, len2;
  char buff[512], buff2[512];
  static DYNAMIC_STRING ds_filename;
  static DYNAMIC_STRING ds_filename2;
  const struct command_arg diff_file_args[] = {
    { "file1", ARG_STRING, TRUE, &ds_filename, "First file to diff" },
    { "file2", ARG_STRING, TRUE, &ds_filename2, "Second file to diff" }
  };
  DBUG_ENTER("do_diff_files");

  check_command_args(command,
                     command->first_argument,
                     diff_file_args,
                     sizeof(diff_file_args)/sizeof(struct command_arg),
                     ' ');

  if ((fd= my_open(ds_filename.str, O_RDONLY, MYF(0))) < 0)
    die("Failed to open first file %s", ds_filename.str);
  if ((fd2= my_open(ds_filename2.str, O_RDONLY, MYF(0))) < 0)
  {
    my_close(fd, MYF(0));
    die("Failed to open second file %s", ds_filename2.str);
  }
  while((len= my_read(fd, (byte*)&buff,
                      sizeof(buff), MYF(0))) > 0)
  {
    if ((len2= my_read(fd2, (byte*)&buff2,
                       sizeof(buff2), MYF(0))) != len)
    {
      /* File 2 was smaller */
      error= 1;
      break;
    }
    if ((memcmp(buff, buff2, len)))
    {
      /* Content of this part differed */
      error= 1;
      break;
    }
  }
  if (my_read(fd2, (byte*)&buff2,
              sizeof(buff2), MYF(0)) > 0)
  {
    /* File 1 was smaller */
    error= 1;
  }

  my_close(fd, MYF(0));
  my_close(fd2, MYF(0));
  dynstr_free(&ds_filename);
  dynstr_free(&ds_filename2);
  handle_command_error(command, error);
  DBUG_VOID_RETURN;
}

/*
  SYNOPSIS
  do_perl
  command	command handle

  DESCRIPTION
  perl [<delimiter>];
  <perlscript line 1>
  <...>
  <perlscript line n>
  EOF

  Execute everything after "perl" until <delimiter> as perl.
  Useful for doing more advanced things
  but still being able to execute it on all platforms.

  Default <delimiter> is EOF
*/

void do_perl(struct st_command *command)
{
  int error;
  char buf[FN_REFLEN];
  FILE *res_file;
  static DYNAMIC_STRING ds_script;
  static DYNAMIC_STRING ds_delimiter;
  const struct command_arg perl_args[] = {
    { "delimiter", ARG_STRING, FALSE, &ds_delimiter, "Delimiter to read until" }
  };
  DBUG_ENTER("do_perl");

  check_command_args(command,
                     command->first_argument,
                     perl_args,
                     sizeof(perl_args)/sizeof(struct command_arg),
                     ' ');

  /* If no delimiter was provided, use EOF */
  if (ds_delimiter.length == 0)
    dynstr_set(&ds_delimiter, "EOF");

  init_dynamic_string(&ds_script, "", 1024, 1024);
  read_until_delimiter(&ds_script, &ds_delimiter);

  DBUG_PRINT("info", ("Executing perl: %s", ds_script.str));

  /* Format a name for a tmp .pl file that is unique for this process */
  my_snprintf(buf, sizeof(buf), "%s/tmp/tmp_%d.pl",
              getenv("MYSQLTEST_VARDIR"), getpid());
  str_to_file(buf, ds_script.str, ds_script.length);

  /* Format the perl <filename> command */
  my_snprintf(buf, sizeof(buf), "perl %s/tmp/tmp_%d.pl",
              getenv("MYSQLTEST_VARDIR"), getpid());

  if (!(res_file= popen(buf, "r")) && command->abort_on_error)
    die("popen(\"%s\", \"r\") failed", buf);

  while (fgets(buf, sizeof(buf), res_file))
  {
    if (disable_result_log)
    {
      buf[strlen(buf)-1]=0;
      DBUG_PRINT("exec_result",("%s", buf));
    }
    else
    {
      replace_dynstr_append(&ds_res, buf);
    }
  }
  error= pclose(res_file);
  handle_command_error(command, WEXITSTATUS(error));
  dynstr_free(&ds_script);
  dynstr_free(&ds_delimiter);
  DBUG_VOID_RETURN;
}


/*
  Print the content between echo and <delimiter> to result file.
  Evaluate all variables in the string before printing, allow
  for variable names to be escaped using \

  SYNOPSIS
  do_echo()
  command  called command

  DESCRIPTION
  echo text
  Print the text after echo until end of command to result file

  echo $<var_name>
  Print the content of the variable <var_name> to result file

  echo Some text $<var_name>
  Print "Some text" plus the content of the variable <var_name> to
  result file

  echo Some text \$<var_name>
  Print "Some text" plus $<var_name> to result file
*/

int do_echo(struct st_command *command)
{
  DYNAMIC_STRING ds_echo;
  DBUG_ENTER("do_echo");

  init_dynamic_string(&ds_echo, "", command->query_len, 256);
  do_eval(&ds_echo, command->first_argument, command->end, FALSE);
  dynstr_append_mem(&ds_res, ds_echo.str, ds_echo.length);
  dynstr_append_mem(&ds_res, "\n", 1);
  dynstr_free(&ds_echo);
  command->last_argument= command->end;
  DBUG_RETURN(0);
}


void do_wait_for_slave_to_stop(struct st_command *c __attribute__((unused)))
{
  static int SLAVE_POLL_INTERVAL= 300000;
  MYSQL* mysql = &cur_con->mysql;
  for (;;)
  {
    MYSQL_RES *res;
    MYSQL_ROW row;
    int done;
    LINT_INIT(res);

    if (mysql_query(mysql,"show status like 'Slave_running'") ||
	!(res=mysql_store_result(mysql)))
      die("Query failed while probing slave for stop: %s",
	  mysql_error(mysql));
    if (!(row=mysql_fetch_row(res)) || !row[1])
    {
      mysql_free_result(res);
      die("Strange result from query while probing slave for stop");
    }
    done = !strcmp(row[1],"OFF");
    mysql_free_result(res);
    if (done)
      break;
    my_sleep(SLAVE_POLL_INTERVAL);
  }
  return;
}


void do_sync_with_master2(long offset)
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL *mysql= &cur_con->mysql;
  char query_buf[FN_REFLEN+128];
  int tries= 0;
  int rpl_parse;

  if (!master_pos.file[0])
    die("Calling 'sync_with_master' without calling 'save_master_pos'");
  rpl_parse= mysql_rpl_parse_enabled(mysql);
  mysql_disable_rpl_parse(mysql);

  sprintf(query_buf, "select master_pos_wait('%s', %ld)", master_pos.file,
	  master_pos.pos + offset);

wait_for_position:

  if (mysql_query(mysql, query_buf))
    die("failed in '%s': %d: %s", query_buf, mysql_errno(mysql),
        mysql_error(mysql));

  if (!(res= mysql_store_result(mysql)))
    die("mysql_store_result() returned NULL for '%s'", query_buf);
  if (!(row= mysql_fetch_row(res)))
  {
    mysql_free_result(res);
    die("empty result in %s", query_buf);
  }
  if (!row[0])
  {
    /*
      It may be that the slave SQL thread has not started yet, though START
      SLAVE has been issued ?
    */
    mysql_free_result(res);
    if (tries++ == 30)
      die("could not sync with master ('%s' returned NULL)", query_buf);
    sleep(1); /* So at most we will wait 30 seconds and make 31 tries */
    goto wait_for_position;
  }
  mysql_free_result(res);
  if (rpl_parse)
    mysql_enable_rpl_parse(mysql);

  return;
}


void do_sync_with_master(struct st_command *command)
{
  long offset= 0;
  char *p= command->first_argument;
  const char *offset_start= p;
  if (*offset_start)
  {
    for (; my_isdigit(charset_info, *p); p++)
      offset = offset * 10 + *p - '0';

    if(*p && !my_isspace(charset_info, *p))
      die("Invalid integer argument \"%s\"", offset_start);
    command->last_argument= p;
  }
  do_sync_with_master2(offset);
  return;
}


/*
  when ndb binlog is on, this call will wait until last updated epoch
  (locally in the mysqld) has been received into the binlog
*/
int do_save_master_pos()
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL *mysql = &cur_con->mysql;
  const char *query;
  int rpl_parse;
  DBUG_ENTER("do_save_master_pos");

  rpl_parse = mysql_rpl_parse_enabled(mysql);
  mysql_disable_rpl_parse(mysql);

#ifdef HAVE_NDB_BINLOG
  /*
    Wait for ndb binlog to be up-to-date with all changes
    done on the local mysql server
  */
  {
    ulong have_ndbcluster;
    if (mysql_query(mysql, query= "show variables like 'have_ndbcluster'"))
      die("'%s' failed: %d %s", query,
          mysql_errno(mysql), mysql_error(mysql));
    if (!(res= mysql_store_result(mysql)))
      die("mysql_store_result() returned NULL for '%s'", query);
    if (!(row= mysql_fetch_row(res)))
      die("Query '%s' returned empty result", query);

    have_ndbcluster= strcmp("YES", row[1]) == 0;
    mysql_free_result(res);

    if (have_ndbcluster)
    {
      ulonglong start_epoch= 0, handled_epoch= 0,
	latest_epoch=0, latest_trans_epoch=0,
	latest_handled_binlog_epoch= 0, latest_received_binlog_epoch= 0,
	latest_applied_binlog_epoch= 0;
      int count= 0;
      int do_continue= 1;
      while (do_continue)
      {
        const char binlog[]= "binlog";
	const char latest_epoch_str[]=
          "latest_epoch=";
        const char latest_trans_epoch_str[]=
          "latest_trans_epoch=";
	const char latest_received_binlog_epoch_str[]=
	  "latest_received_binlog_epoch";
        const char latest_handled_binlog_epoch_str[]=
          "latest_handled_binlog_epoch=";
        const char latest_applied_binlog_epoch_str[]=
          "latest_applied_binlog_epoch=";
        if (count)
          sleep(1);
        if (mysql_query(mysql, query= "show engine ndb status"))
          die("failed in '%s': %d %s", query,
              mysql_errno(mysql), mysql_error(mysql));
        if (!(res= mysql_store_result(mysql)))
          die("mysql_store_result() returned NULL for '%s'", query);
        while ((row= mysql_fetch_row(res)))
        {
          if (strcmp(row[1], binlog) == 0)
          {
            const char *status= row[2];

	    /* latest_epoch */
	    while (*status && strncmp(status, latest_epoch_str,
				      sizeof(latest_epoch_str)-1))
	      status++;
	    if (*status)
            {
	      status+= sizeof(latest_epoch_str)-1;
	      latest_epoch= strtoull(status, (char**) 0, 10);
	    }
	    else
	      die("result does not contain '%s' in '%s'",
		  latest_epoch_str, query);
	    /* latest_trans_epoch */
	    while (*status && strncmp(status, latest_trans_epoch_str,
				      sizeof(latest_trans_epoch_str)-1))
	      status++;
	    if (*status)
	    {
	      status+= sizeof(latest_trans_epoch_str)-1;
	      latest_trans_epoch= strtoull(status, (char**) 0, 10);
	    }
	    else
	      die("result does not contain '%s' in '%s'",
		  latest_trans_epoch_str, query);
	    /* latest_received_binlog_epoch */
	    while (*status &&
		   strncmp(status, latest_received_binlog_epoch_str,
			   sizeof(latest_received_binlog_epoch_str)-1))
	      status++;
	    if (*status)
	    {
	      status+= sizeof(latest_received_binlog_epoch_str)-1;
	      latest_received_binlog_epoch= strtoull(status, (char**) 0, 10);
	    }
	    else
	      die("result does not contain '%s' in '%s'",
		  latest_received_binlog_epoch_str, query);
	    /* latest_handled_binlog */
	    while (*status &&
		   strncmp(status, latest_handled_binlog_epoch_str,
			   sizeof(latest_handled_binlog_epoch_str)-1))
	      status++;
	    if (*status)
	    {
	      status+= sizeof(latest_handled_binlog_epoch_str)-1;
	      latest_handled_binlog_epoch= strtoull(status, (char**) 0, 10);
	    }
	    else
	      die("result does not contain '%s' in '%s'",
		  latest_handled_binlog_epoch_str, query);
	    /* latest_applied_binlog_epoch */
	    while (*status &&
		   strncmp(status, latest_applied_binlog_epoch_str,
			   sizeof(latest_applied_binlog_epoch_str)-1))
	      status++;
	    if (*status)
	    {
	      status+= sizeof(latest_applied_binlog_epoch_str)-1;
	      latest_applied_binlog_epoch= strtoull(status, (char**) 0, 10);
	    }
	    else
	      die("result does not contain '%s' in '%s'",
		  latest_applied_binlog_epoch_str, query);
	    if (count == 0)
	      start_epoch= latest_trans_epoch;
	    break;
	  }
	}
	if (!row)
	  die("result does not contain '%s' in '%s'",
	      binlog, query);
	if (latest_handled_binlog_epoch > handled_epoch)
	  count= 0;
	handled_epoch= latest_handled_binlog_epoch;
	count++;
	if (latest_handled_binlog_epoch >= start_epoch)
          do_continue= 0;
        else if (count > 30)
	{
	  break;
        }
        mysql_free_result(res);
      }
    }
  }
#endif
  if (mysql_query(mysql, query= "show master status"))
    die("failed in 'show master status': %d %s",
	mysql_errno(mysql), mysql_error(mysql));

  if (!(res = mysql_store_result(mysql)))
    die("mysql_store_result() retuned NULL for '%s'", query);
  if (!(row = mysql_fetch_row(res)))
    die("empty result in show master status");
  strnmov(master_pos.file, row[0], sizeof(master_pos.file)-1);
  master_pos.pos = strtoul(row[1], (char**) 0, 10);
  mysql_free_result(res);

  if (rpl_parse)
    mysql_enable_rpl_parse(mysql);

  DBUG_RETURN(0);
}


/*
  Assign the variable <var_name> with <var_val>

  SYNOPSIS
  do_let()
  query	called command

  DESCRIPTION
  let $<var_name>=<var_val><delimiter>

  <var_name>  - is the string string found between the $ and =
  <var_val>   - is the content between the = and <delimiter>, it may span
  multiple line and contain any characters except <delimiter>
  <delimiter> - is a string containing of one or more chars, default is ;

  RETURN VALUES
  Program will die if error detected
*/

void do_let(struct st_command *command)
{
  char *p= command->first_argument;
  char *var_name, *var_name_end;
  DYNAMIC_STRING let_rhs_expr;
  DBUG_ENTER("do_let");

  init_dynamic_string(&let_rhs_expr, "", 512, 2048);

  /* Find <var_name> */
  if (!*p)
    die("Missing arguments to let");
  var_name= p;
  while (*p && (*p != '=') && !my_isspace(charset_info,*p))
    p++;
  var_name_end= p;
  if (var_name == var_name_end ||
      (var_name+1 == var_name_end && *var_name == '$'))
    die("Missing variable name in let");
  while (my_isspace(charset_info,*p))
    p++;
  if (*p++ != '=')
    die("Missing assignment operator in let");

  /* Find start of <var_val> */
  while (*p && my_isspace(charset_info,*p))
    p++;

  do_eval(&let_rhs_expr, p, command->end, FALSE);

  command->last_argument= command->end;
  /* Assign var_val to var_name */
  var_set(var_name, var_name_end, let_rhs_expr.str,
          (let_rhs_expr.str + let_rhs_expr.length));
  dynstr_free(&let_rhs_expr);
  DBUG_VOID_RETURN;
}


int do_rpl_probe(struct st_command *command __attribute__((unused)))
{
  DBUG_ENTER("do_rpl_probe");
  if (mysql_rpl_probe(&cur_con->mysql))
    die("Failed in mysql_rpl_probe(): '%s'", mysql_error(&cur_con->mysql));
  DBUG_RETURN(0);
}


int do_enable_rpl_parse(struct st_command *command __attribute__((unused)))
{
  mysql_enable_rpl_parse(&cur_con->mysql);
  return 0;
}


int do_disable_rpl_parse(struct st_command *command __attribute__((unused)))
{
  mysql_disable_rpl_parse(&cur_con->mysql);
  return 0;
}


/*
  Sleep the number of specified seconds

  SYNOPSIS
  do_sleep()
  q	       called command
  real_sleep   use the value from opt_sleep as number of seconds to sleep
               if real_sleep is false

  DESCRIPTION
  sleep <seconds>
  real_sleep <seconds>

  The difference between the sleep and real_sleep commands is that sleep
  uses the delay from the --sleep command-line option if there is one.
  (If the --sleep option is not given, the sleep command uses the delay
  specified by its argument.) The real_sleep command always uses the
  delay specified by its argument.  The logic is that sometimes delays are
  cpu-dependent, and --sleep can be used to set this delay.  real_sleep is
  used for cpu-independent delays.
*/

int do_sleep(struct st_command *command, my_bool real_sleep)
{
  int error= 0;
  char *p= command->first_argument;
  char *sleep_start, *sleep_end= command->end;
  double sleep_val;

  while (my_isspace(charset_info, *p))
    p++;
  if (!*p)
    die("Missing argument to %.*s", command->first_word_len, command->query);
  sleep_start= p;
  /* Check that arg starts with a digit, not handled by my_strtod */
  if (!my_isdigit(charset_info, *sleep_start))
    die("Invalid argument to %.*s \"%s\"", command->first_word_len,
        command->query,command->first_argument);
  sleep_val= my_strtod(sleep_start, &sleep_end, &error);
  if (error)
    die("Invalid argument to %.*s \"%s\"", command->first_word_len,
        command->query, command->first_argument);

  /* Fixed sleep time selected by --sleep option */
  if (opt_sleep >= 0 && !real_sleep)
    sleep_val= opt_sleep;

  DBUG_PRINT("info", ("sleep_val: %f", sleep_val));
  if (sleep_val)
    my_sleep((ulong) (sleep_val * 1000000L));
  command->last_argument= sleep_end;
  return 0;
}


void do_get_file_name(struct st_command *command,
                      char* dest, uint dest_max_len)
{
  char *p= command->first_argument, *name;
  if (!*p)
    die("Missing file name argument");
  name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  if (*p)
    *p++= 0;
  command->last_argument= p;
  strmake(dest, name, dest_max_len);
}


void do_set_charset(struct st_command *command)
{
  char *charset_name= command->first_argument;
  char *p;

  if (!charset_name || !*charset_name)
    die("Missing charset name in 'character_set'");
  /* Remove end space */
  p= charset_name;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  if(*p)
    *p++= 0;
  command->last_argument= p;
  charset_info= get_charset_by_csname(charset_name,MY_CS_PRIMARY,MYF(MY_WME));
  if (!charset_info)
    abort_not_supported_test("Test requires charset '%s'", charset_name);
}


#if MYSQL_VERSION_ID >= 50000
/* List of error names to error codes, available from 5.0 */
typedef struct
{
  const char *name;
  uint        code;
} st_error;

static st_error global_error_names[] =
{
#include <mysqld_ername.h>
  { 0, 0 }
};

uint get_errcode_from_name(char *error_name, char *error_end)
{
  /* SQL error as string */
  st_error *e= global_error_names;

  DBUG_ENTER("get_errcode_from_name");
  DBUG_PRINT("enter", ("error_name: %s", error_name));

  /* Loop through the array of known error names */
  for (; e->name; e++)
  {
    /*
      If we get a match, we need to check the length of the name we
      matched against in case it was longer than what we are checking
      (as in ER_WRONG_VALUE vs. ER_WRONG_VALUE_COUNT).
    */
    if (!strncmp(error_name, e->name, (int) (error_end - error_name)) &&
        (uint) strlen(e->name) == (uint) (error_end - error_name))
    {
      DBUG_RETURN(e->code);
    }
  }
  if (!e->name)
    die("Unknown SQL error name '%s'", error_name);
  DBUG_RETURN(0);
}
#else
uint get_errcode_from_name(char *error_name __attribute__((unused)),
                           char *error_end __attribute__((unused)))
{
  abort_not_in_this_version();
  return 0; /* Never reached */
}
#endif



void do_get_errcodes(struct st_command *command)
{
  struct st_match_err *to= saved_expected_errors.err;
  char *p= command->first_argument;
  uint count= 0;

  DBUG_ENTER("do_get_errcodes");

  if (!*p)
    die("Missing argument(s) to 'error'");

  do
  {
    char *end;

    /* Skip leading spaces */
    while (*p && *p == ' ')
      p++;

    /* Find end */
    end= p;
    while (*end && *end != ',' && *end != ' ')
      end++;

    if (*p == 'S')
    {
      char *to_ptr= to->code.sqlstate;

      /*
        SQLSTATE string
        - Must be SQLSTATE_LENGTH long
        - May contain only digits[0-9] and _uppercase_ letters
      */
      p++; /* Step past the S */
      if ((end - p) != SQLSTATE_LENGTH)
        die("The sqlstate must be exactly %d chars long", SQLSTATE_LENGTH);

      /* Check sqlstate string validity */
      while (*p && p < end)
      {
        if (my_isdigit(charset_info, *p) || my_isupper(charset_info, *p))
          *to_ptr++= *p++;
        else
          die("The sqlstate may only consist of digits[0-9] " \
              "and _uppercase_ letters");
      }

      *to_ptr= 0;
      to->type= ERR_SQLSTATE;
      DBUG_PRINT("info", ("ERR_SQLSTATE: %s", to->code.sqlstate));
    }
    else if (*p == 's')
    {
      die("The sqlstate definition must start with an uppercase S");
    }
    else if (*p == 'E')
    {
      /* Error name string */

      DBUG_PRINT("info", ("Error name: %s", p));
      to->code.errnum= get_errcode_from_name(p, end);
      to->type= ERR_ERRNO;
      DBUG_PRINT("info", ("ERR_ERRNO: %d", to->code.errnum));
    }
    else if (*p == 'e')
    {
      die("The error name definition must start with an uppercase E");
    }
    else
    {
      long val;
      char *start= p;
      /* Check that the string passed to str2int only contain digits */
      while (*p && p != end)
      {
        if (!my_isdigit(charset_info, *p))
          die("Invalid argument to error: '%s' - "\
              "the errno may only consist of digits[0-9]",
              command->first_argument);
        p++;
      }

      /* Convert the sting to int */
      if (!str2int(start, 10, (long) INT_MIN, (long) INT_MAX, &val))
	die("Invalid argument to error: '%s'", command->first_argument);

      to->code.errnum= (uint) val;
      to->type= ERR_ERRNO;
      DBUG_PRINT("info", ("ERR_ERRNO: %d", to->code.errnum));
    }
    to++;
    count++;

    if (count >= (sizeof(saved_expected_errors.err) /
                  sizeof(struct st_match_err)))
      die("Too many errorcodes specified");

    /* Set pointer to the end of the last error code */
    p= end;

    /* Find next ',' */
    while (*p && *p != ',')
      p++;

    if (*p)
      p++; /* Step past ',' */

  } while (*p);

  command->last_argument= p;
  to->type= ERR_EMPTY;                        /* End of data */

  DBUG_PRINT("info", ("Expected errors: %d", count));
  saved_expected_errors.count= count;
  DBUG_VOID_RETURN;
}


/*
  Get a string;  Return ptr to end of string
  Strings may be surrounded by " or '

  If string is a '$variable', return the value of the variable.
*/

char *get_string(char **to_ptr, char **from_ptr,
                 struct st_command *command)
{
  char c, sep;
  char *to= *to_ptr, *from= *from_ptr, *start=to;
  DBUG_ENTER("get_string");

  /* Find separator */
  if (*from == '"' || *from == '\'')
    sep= *from++;
  else
    sep=' ';				/* Separated with space */

  for ( ; (c=*from) ; from++)
  {
    if (c == '\\' && from[1])
    {					/* Escaped character */
      /* We can't translate \0 -> ASCII 0 as replace can't handle ASCII 0 */
      switch (*++from) {
      case 'n':
	*to++= '\n';
	break;
      case 't':
	*to++= '\t';
	break;
      case 'r':
	*to++ = '\r';
	break;
      case 'b':
	*to++ = '\b';
	break;
      case 'Z':				/* ^Z must be escaped on Win32 */
	*to++='\032';
	break;
      default:
	*to++ = *from;
	break;
      }
    }
    else if (c == sep)
    {
      if (c == ' ' || c != *++from)
	break;				/* Found end of string */
      *to++=c;				/* Copy duplicated separator */
    }
    else
      *to++=c;
  }
  if (*from != ' ' && *from)
    die("Wrong string argument in %s", command->query);

  while (my_isspace(charset_info,*from))	/* Point to next string */
    from++;

  *to =0;				/* End of string marker */
  *to_ptr= to+1;			/* Store pointer to end */
  *from_ptr= from;

  /* Check if this was a variable */
  if (*start == '$')
  {
    const char *end= to;
    VAR *var=var_get(start, &end, 0, 1);
    if (var && to == (char*) end+1)
    {
      DBUG_PRINT("info",("var: '%s' -> '%s'", start, var->str_val));
      DBUG_RETURN(var->str_val);	/* return found variable value */
    }
  }
  DBUG_RETURN(start);
}


void set_reconnect(MYSQL* mysql, int val)
{
  my_bool reconnect= val;
  DBUG_ENTER("set_reconnect");
  DBUG_PRINT("info", ("val: %d", val));
#if MYSQL_VERSION_ID < 50000
  mysql->reconnect= reconnect;
#else
  mysql_options(mysql, MYSQL_OPT_RECONNECT, (char *)&reconnect);
#endif
  DBUG_VOID_RETURN;
}


struct st_connection * find_connection_by_name(const char *name)
{
  struct st_connection *con;
  for (con= connections; con < next_con; con++)
  {
    if (!strcmp(con->name, name))
    {
      return con;
    }
  }
  return 0; /* Connection not found */
}


int select_connection_name(const char *name)
{
  DBUG_ENTER("select_connection_name");
  DBUG_PRINT("enter",("name: '%s'", name));

  if (!(cur_con= find_connection_by_name(name)))
    die("connection '%s' not found in connection pool", name);
  DBUG_RETURN(0);
}


int select_connection(struct st_command *command)
{
  char *name;
  char *p= command->first_argument;
  DBUG_ENTER("select_connection");

  if (!*p)
    die("Missing connection name in connect");
  name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  if (*p)
    *p++= 0;
  command->last_argument= p;
  DBUG_RETURN(select_connection_name(name));
}


void do_close_connection(struct st_command *command)
{
  char *p= command->first_argument, *name;
  struct st_connection *con;

  DBUG_ENTER("close_connection");
  DBUG_PRINT("enter",("name: '%s'",p));

  if (!*p)
    die("Missing connection name in disconnect");
  name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;

  if (*p)
    *p++= 0;
  command->last_argument= p;

  /* Loop through connection pool for connection to close */
  for (con= connections; con < next_con; con++)
  {
    DBUG_PRINT("info", ("con->name: %s", con->name));
    if (!strcmp(con->name, name))
    {
      DBUG_PRINT("info", ("Closing connection %s", con->name));
#ifndef EMBEDDED_LIBRARY
      if (command->type == Q_DIRTY_CLOSE)
      {
	if (con->mysql.net.vio)
	{
	  vio_delete(con->mysql.net.vio);
	  con->mysql.net.vio = 0;
	}
      }
#endif
      if (next_con->stmt)
        mysql_stmt_close(next_con->stmt);
      next_con->stmt= 0;

      mysql_close(&con->mysql);
      if (con->util_mysql)
	mysql_close(con->util_mysql);
      con->util_mysql= 0;
      my_free(con->name, MYF(0));

      /*
        When the connection is closed set name to "closed_connection"
        to make it possible to reuse the connection name.
        The connection slot will not be reused
      */
      if (!(con->name = my_strdup("closed_connection", MYF(MY_WME))))
        die("Out of memory");

      DBUG_VOID_RETURN;
    }
  }
  die("connection '%s' not found in connection pool", name);
}


/*
  Connect to a server doing several retries if needed.

  SYNOPSIS
  safe_connect()
  con               - connection structure to be used
  host, user, pass, - connection parameters
  db, port, sock

  NOTE

  Sometimes in a test the client starts before
  the server - to solve the problem, we try again
  after some sleep if connection fails the first
  time

  This function will try to connect to the given server
  "opt_max_connect_retries" times and sleep "connection_retry_sleep"
  seconds between attempts before finally giving up.
  This helps in situation when the client starts
  before the server (which happens sometimes).
  It will only ignore connection errors during these retries.

*/

void safe_connect(MYSQL* mysql, const char *name, const char *host,
                  const char *user, const char *pass, const char *db,
                  int port, const char *sock)
{
  int failed_attempts= 0;
  static ulong connection_retry_sleep= 100000; /* Microseconds */

  DBUG_ENTER("safe_connect");
  while(!mysql_real_connect(mysql, host,user, pass, db, port, sock,
                            CLIENT_MULTI_STATEMENTS | CLIENT_REMEMBER_OPTIONS))
  {
    /*
      Connect failed

      Only allow retry if this was an error indicating the server
      could not be contacted. Error code differs depending
      on protocol/connection type
    */

    if ((mysql_errno(mysql) == CR_CONN_HOST_ERROR ||
         mysql_errno(mysql) == CR_CONNECTION_ERROR) &&
        failed_attempts < opt_max_connect_retries)
    {
      verbose_msg("Connect attempt %d/%d failed: %d: %s", failed_attempts,
                  opt_max_connect_retries, mysql_errno(mysql),
                  mysql_error(mysql));
      my_sleep(connection_retry_sleep);
    }
    else
    {
      if (failed_attempts > 0)
        die("Could not open connection '%s' after %d attempts: %d %s", name,
            failed_attempts, mysql_errno(mysql), mysql_error(mysql));
      else
        die("Could not open connection '%s': %d %s", name,
            mysql_errno(mysql), mysql_error(mysql));
    }
    failed_attempts++;
  }
  DBUG_VOID_RETURN;
}


/*
  Connect to a server and handle connection errors in case they occur.

  SYNOPSIS
  connect_n_handle_errors()
  q                 - context of connect "query" (command)
  con               - connection structure to be used
  host, user, pass, - connection parameters
  db, port, sock

  DESCRIPTION
  This function will try to establish a connection to server and handle
  possible errors in the same manner as if "connect" was usual SQL-statement
  (If error is expected it will ignore it once it occurs and log the
  "statement" to the query log).
  Unlike safe_connect() it won't do several attempts.

  RETURN VALUES
  1 - Connected
  0 - Not connected

*/

int connect_n_handle_errors(struct st_command *command,
                            MYSQL* con, const char* host,
                            const char* user, const char* pass,
                            const char* db, int port, const char* sock)
{
  DYNAMIC_STRING *ds;

  ds= &ds_res;

  /* Only log if an error is expected */
  if (!command->abort_on_error &&
      !disable_query_log)
  {
    /*
      Log the connect to result log
    */
    dynstr_append_mem(ds, "connect(", 8);
    replace_dynstr_append(ds, host);
    dynstr_append_mem(ds, ",", 1);
    replace_dynstr_append(ds, user);
    dynstr_append_mem(ds, ",", 1);
    replace_dynstr_append(ds, pass);
    dynstr_append_mem(ds, ",", 1);
    if (db)
      replace_dynstr_append(ds, db);
    dynstr_append_mem(ds, ",", 1);
    replace_dynstr_append_uint(ds, port);
    dynstr_append_mem(ds, ",", 1);
    if (sock)
      replace_dynstr_append(ds, sock);
    dynstr_append_mem(ds, ")", 1);
    dynstr_append_mem(ds, delimiter, delimiter_length);
    dynstr_append_mem(ds, "\n", 1);
  }
  if (!mysql_real_connect(con, host, user, pass, db, port, sock ? sock: 0,
                          CLIENT_MULTI_STATEMENTS))
  {
    handle_error(command, mysql_errno(con), mysql_error(con),
		 mysql_sqlstate(con), ds);
    return 0; /* Not connected */
  }

  handle_no_error(command);
  return 1; /* Connected */
}


/*
  Open a new connection to MySQL Server with the parameters
  specified. Make the new connection the current connection.

  SYNOPSIS
  do_connect()
  q	       called command

  DESCRIPTION
  connect(<name>,<host>,<user>,[<pass>,[<db>,[<port>,<sock>[<opts>]]]]);
  connect <name>,<host>,<user>,[<pass>,[<db>,[<port>,<sock>[<opts>]]]];

  <name> - name of the new connection
  <host> - hostname of server
  <user> - user to connect as
  <pass> - password used when connecting
  <db>   - initial db when connected
  <port> - server port
  <sock> - server socket
  <opts> - options to use for the connection
   * SSL - use SSL if available
   * COMPRESS - use compression if available

*/

void do_connect(struct st_command *command)
{
  int con_port= opt_port;
  char *con_options;
  bool con_ssl= 0, con_compress= 0;
  char *ptr;

  static DYNAMIC_STRING ds_connection_name;
  static DYNAMIC_STRING ds_host;
  static DYNAMIC_STRING ds_user;
  static DYNAMIC_STRING ds_password;
  static DYNAMIC_STRING ds_database;
  static DYNAMIC_STRING ds_port;
  static DYNAMIC_STRING ds_sock;
  static DYNAMIC_STRING ds_options;
  const struct command_arg connect_args[] = {
    { "connection name", ARG_STRING, TRUE, &ds_connection_name, "Name of the connection" },
    { "host", ARG_STRING, TRUE, &ds_host, "Host to connect to" },
    { "user", ARG_STRING, FALSE, &ds_user, "User to connect as" },
    { "passsword", ARG_STRING, FALSE, &ds_password, "Password used when connecting" },
    { "database", ARG_STRING, FALSE, &ds_database, "Database to select after connect" },
    { "port", ARG_STRING, FALSE, &ds_port, "Port to connect to" },
    { "socket", ARG_STRING, FALSE, &ds_sock, "Socket to connect with" },
    { "options", ARG_STRING, FALSE, &ds_options, "Options to use while connecting" }
  };

  DBUG_ENTER("do_connect");
  DBUG_PRINT("enter",("connect: %s", command->first_argument));

  /* Remove parenteses around connect arguments */
  if ((ptr= strstr(command->first_argument, "(")))
  {
    /* Replace it with a space */
    *ptr= ' ';
    if ((ptr= strstr(command->first_argument, ")")))
    {
      /* Replace it with \0 */
      *ptr= 0;
    }
    else
      die("connect - argument list started with '(' must be ended with ')'");
  }

  check_command_args(command, command->first_argument, connect_args,
                     sizeof(connect_args)/sizeof(struct command_arg),
                     ',');

  /* Port */
  if (ds_port.length)
  {
    con_port= atoi(ds_port.str);
    if (con_port == 0)
      die("Illegal argument for port: '%s'", ds_port.str);
  }

  /* Sock */
  if (ds_sock.length)
  {
    /*
      If the socket is specified just as a name without path
      append tmpdir in front
    */
    if (*ds_sock.str != FN_LIBCHAR)
    {
      char buff[FN_REFLEN];
      fn_format(buff, ds_sock.str, TMPDIR, "", 0);
      dynstr_set(&ds_sock, buff);
    }
  }
  else
  {
    /* No socket specified, use default */
    dynstr_set(&ds_sock, unix_sock);
  }
  DBUG_PRINT("info", ("socket: %s", ds_sock.str));


  /* Options */
  con_options= ds_options.str;
  while (*con_options)
  {
    char* end;
    /* Step past any spaces in beginning of option*/
    while (*con_options && my_isspace(charset_info, *con_options))
     con_options++;
    /* Find end of this option */
    end= con_options;
    while (*end && !my_isspace(charset_info, *end))
      end++;
    if (!strncmp(con_options, "SSL", 3))
      con_ssl= 1;
    else if (!strncmp(con_options, "COMPRESS", 8))
      con_compress= 1;
    else
      die("Illegal option to connect: %.*s", 
          (int) (end - con_options), con_options);
    /* Process next option */
    con_options= end;
  }

  if (next_con == connections_end)
    die("Connection limit exhausted, you can have max %d connections",
        (int) (sizeof(connections)/sizeof(struct st_connection)));

  if (find_connection_by_name(ds_connection_name.str))
    die("Connection %s already exists", ds_connection_name.str);

  if (!mysql_init(&next_con->mysql))
    die("Failed on mysql_init()");
  if (opt_compress || con_compress)
    mysql_options(&next_con->mysql, MYSQL_OPT_COMPRESS, NullS);
  mysql_options(&next_con->mysql, MYSQL_OPT_LOCAL_INFILE, 0);
  mysql_options(&next_con->mysql, MYSQL_SET_CHARSET_NAME,
                charset_info->csname);
  if (opt_charsets_dir)
    mysql_options(&cur_con->mysql, MYSQL_SET_CHARSET_DIR,
                  opt_charsets_dir);

#ifdef HAVE_OPENSSL
  if (opt_use_ssl || con_ssl)
  {
    mysql_ssl_set(&next_con->mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
#if MYSQL_VERSION_ID >= 50000
    /* Turn on ssl_verify_server_cert only if host is "localhost" */
    opt_ssl_verify_server_cert= !strcmp(ds_host.str, "localhost");
    mysql_options(&next_con->mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                  &opt_ssl_verify_server_cert);
#endif
  }
#endif

  /* Use default db name */
  if (ds_database.length == 0)
    dynstr_set(&ds_database, opt_db);

  /* Special database to allow one to connect without a database name */
  if (ds_database.length && !strcmp(ds_database.str,"*NO-ONE*"))
    dynstr_set(&ds_database, "");

  if (connect_n_handle_errors(command, &next_con->mysql,
                              ds_host.str,ds_user.str,
                              ds_password.str, ds_database.str,
                              con_port, ds_sock.str))
  {
    DBUG_PRINT("info", ("Inserting connection %s in connection pool",
                        ds_connection_name.str));
    if (!(next_con->name= my_strdup(ds_connection_name.str, MYF(MY_WME))))
      die("Out of memory");
    cur_con= next_con++;
  }

  dynstr_free(&ds_connection_name);
  dynstr_free(&ds_host);
  dynstr_free(&ds_user);
  dynstr_free(&ds_password);
  dynstr_free(&ds_database);
  dynstr_free(&ds_port);
  dynstr_free(&ds_sock);
  dynstr_free(&ds_options);
  DBUG_VOID_RETURN;
}


int do_done(struct st_command *command)
{
  /* Check if empty block stack */
  if (cur_block == block_stack)
  {
    if (*command->query != '}')
      die("Stray 'end' command - end of block before beginning");
    die("Stray '}' - end of block before beginning");
  }

  /* Test if inner block has been executed */
  if (cur_block->ok && cur_block->cmd == cmd_while)
  {
    /* Pop block from stack, re-execute outer block */
    cur_block--;
    parser.current_line = cur_block->line;
  }
  else
  {
    /* Pop block from stack, goto next line */
    cur_block--;
    parser.current_line++;
  }
  return 0;
}


/*
  Process start of a "if" or "while" statement

  SYNOPSIS
  do_block()
  cmd        Type of block
  q	       called command

  DESCRIPTION
  if ([!]<expr>)
  {
  <block statements>
  }

  while ([!]<expr>)
  {
  <block statements>
  }

  Evaluates the <expr> and if it evaluates to
  greater than zero executes the following code block.
  A '!' can be used before the <expr> to indicate it should
  be executed if it evaluates to zero.

*/

void do_block(enum block_cmd cmd, struct st_command* command)
{
  char *p= command->first_argument;
  const char *expr_start, *expr_end;
  VAR v;
  const char *cmd_name= (cmd == cmd_while ? "while" : "if");
  my_bool not_expr= FALSE;
  DBUG_ENTER("do_block");
  DBUG_PRINT("enter", ("%s", cmd_name));

  /* Check stack overflow */
  if (cur_block == block_stack_end)
    die("Nesting too deeply");

  /* Set way to find outer block again, increase line counter */
  cur_block->line= parser.current_line++;

  /* If this block is ignored */
  if (!cur_block->ok)
  {
    /* Inner block should be ignored too */
    cur_block++;
    cur_block->cmd= cmd;
    cur_block->ok= FALSE;
    DBUG_VOID_RETURN;
  }

  /* Parse and evaluate test expression */
  expr_start= strchr(p, '(');
  if (!expr_start++)
    die("missing '(' in %s", cmd_name);

  /* Check for !<expr> */
  if (*expr_start == '!')
  {
    not_expr= TRUE;
    expr_start++; /* Step past the '!' */
  }
  /* Find ending ')' */
  expr_end= strrchr(expr_start, ')');
  if (!expr_end)
    die("missing ')' in %s", cmd_name);
  p= (char*)expr_end+1;

  while (*p && my_isspace(charset_info, *p))
    p++;
  if (*p && *p != '{')
    die("Missing '{' after %s. Found \"%s\"", cmd_name, p);

  var_init(&v,0,0,0,0);
  eval_expr(&v, expr_start, &expr_end);

  /* Define inner block */
  cur_block++;
  cur_block->cmd= cmd;
  cur_block->ok= (v.int_val ? TRUE : FALSE);

  if (not_expr)
    cur_block->ok = !cur_block->ok;

  DBUG_PRINT("info", ("OK: %d", cur_block->ok));

  var_free(&v);
  DBUG_VOID_RETURN;
}


void do_delimiter(struct st_command* command)
{
  char* p= command->first_argument;
  DBUG_ENTER("do_delimiter");
  DBUG_PRINT("enter", ("first_argument: %s", command->first_argument));

  while (*p && my_isspace(charset_info, *p))
    p++;

  if (!(*p))
    die("Can't set empty delimiter");

  strmake(delimiter, p, sizeof(delimiter) - 1);
  delimiter_length= strlen(delimiter);

  DBUG_PRINT("exit", ("delimiter: %s", delimiter));
  command->last_argument= p + delimiter_length;
  DBUG_VOID_RETURN;
}


my_bool match_delimiter(int c, const char *delim, uint length)
{
  uint i;
  char tmp[MAX_DELIMITER_LENGTH];

  if (c != *delim)
    return 0;

  for (i= 1; i < length &&
	 (c= my_getc(cur_file->file)) == *(delim + i);
       i++)
    tmp[i]= c;

  if (i == length)
    return 1;					/* Found delimiter */

  /* didn't find delimiter, push back things that we read */
  my_ungetc(c);
  while (i > 1)
    my_ungetc(tmp[--i]);
  return 0;
}


my_bool end_of_query(int c)
{
  return match_delimiter(c, delimiter, delimiter_length);
}


/*
  Read one "line" from the file

  SYNOPSIS
  read_line
  buf     buffer for the read line
  size    size of the buffer i.e max size to read

  DESCRIPTION
  This function actually reads several lines and adds them to the
  buffer buf. It continues to read until it finds what it believes
  is a complete query.

  Normally that means it will read lines until it reaches the
  "delimiter" that marks end of query. Default delimiter is ';'
  The function should be smart enough not to detect delimiter's
  found inside strings surrounded with '"' and '\'' escaped strings.

  If the first line in a query starts with '#' or '-' this line is treated
  as a comment. A comment is always terminated when end of line '\n' is
  reached.

*/

int read_line(char *buf, int size)
{
  char c, last_quote;
  char *p= buf, *buf_end= buf + size - 1;
  int skip_char= 0;
  enum {R_NORMAL, R_Q, R_SLASH_IN_Q,
        R_COMMENT, R_LINE_START} state= R_LINE_START;
  DBUG_ENTER("read_line");
  LINT_INIT(last_quote);

  start_lineno= cur_file->lineno;
  DBUG_PRINT("info", ("Starting to read at lineno: %d", start_lineno));
  for (; p < buf_end ;)
  {
    skip_char= 0;
    c= my_getc(cur_file->file);
    if (feof(cur_file->file))
    {
  found_eof:
      if (cur_file->file != stdin)
      {
	my_fclose(cur_file->file, MYF(0));
        cur_file->file= 0;
      }
      my_free((gptr)cur_file->file_name, MYF(MY_ALLOW_ZERO_PTR));
      cur_file->file_name= 0;
      if (cur_file == file_stack)
      {
        /* We're back at the first file, check if
           all { have matching }
        */
        if (cur_block != block_stack)
          die("Missing end of block");

        *p= 0;
        DBUG_PRINT("info", ("end of file at line %d", cur_file->lineno));
        DBUG_RETURN(1);
      }
      cur_file--;
      start_lineno= cur_file->lineno;
      continue;
    }

    if (c == '\n')
    {
      /* Line counting is independent of state */
      cur_file->lineno++;

      /* Convert cr/lf to lf */
      if (p != buf && *(p-1) == '\r')
        p--;
    }

    switch(state) {
    case R_NORMAL:
      if (end_of_query(c))
      {
	*p= 0;
        DBUG_PRINT("exit", ("Found delimiter '%s' at line %d",
                            delimiter, cur_file->lineno));
	DBUG_RETURN(0);
      }
      else if ((c == '{' &&
                (!my_strnncoll_simple(charset_info, (const uchar*) "while", 5,
                                      (uchar*) buf, min(5, p - buf), 0) ||
                 !my_strnncoll_simple(charset_info, (const uchar*) "if", 2,
                                      (uchar*) buf, min(2, p - buf), 0))))
      {
        /* Only if and while commands can be terminated by { */
        *p++= c;
	*p= 0;
        DBUG_PRINT("exit", ("Found '{' indicating start of block at line %d",
                            cur_file->lineno));
	DBUG_RETURN(0);
      }
      else if (c == '\'' || c == '"' || c == '`')
      {
        last_quote= c;
	state= R_Q;
      }
      break;

    case R_COMMENT:
      if (c == '\n')
      {
        /* Comments are terminated by newline */
	*p= 0;
        DBUG_PRINT("exit", ("Found newline in comment at line: %d",
                            cur_file->lineno));
	DBUG_RETURN(0);
      }
      break;

    case R_LINE_START:
      if (c == '#' || c == '-')
      {
        /* A # or - in the first position of the line - this is a comment */
	state = R_COMMENT;
      }
      else if (my_isspace(charset_info, c))
      {
        /* Skip all space at begining of line */
	if (c == '\n')
        {
          /* Query hasn't started yet */
	  start_lineno= cur_file->lineno;
          DBUG_PRINT("info", ("Query hasn't started yet, start_lineno: %d",
                              start_lineno));
        }
	skip_char= 1;
      }
      else if (end_of_query(c))
      {
	*p= 0;
        DBUG_PRINT("exit", ("Found delimiter '%s' at line: %d",
                            delimiter, cur_file->lineno));
	DBUG_RETURN(0);
      }
      else if (c == '}')
      {
        /* A "}" need to be by itself in the begining of a line to terminate */
        *p++= c;
	*p= 0;
        DBUG_PRINT("exit", ("Found '}' in begining of a line at line: %d",
                            cur_file->lineno));
	DBUG_RETURN(0);
      }
      else if (c == '\'' || c == '"' || c == '`')
      {
        last_quote= c;
	state= R_Q;
      }
      else
	state= R_NORMAL;
      break;

    case R_Q:
      if (c == last_quote)
	state= R_NORMAL;
      else if (c == '\\')
	state= R_SLASH_IN_Q;
      break;

    case R_SLASH_IN_Q:
      state= R_Q;
      break;

    }

    if (!skip_char)
    {
      /* Could be a multibyte character */
      /* This code is based on the code in "sql_load.cc" */
#ifdef USE_MB
      int charlen = my_mbcharlen(charset_info, c);
      /* We give up if multibyte character is started but not */
      /* completed before we pass buf_end */
      if ((charlen > 1) && (p + charlen) <= buf_end)
      {
	int i;
	char* mb_start = p;

	*p++ = c;

	for (i= 1; i < charlen; i++)
	{
	  if (feof(cur_file->file))
	    goto found_eof;
	  c= my_getc(cur_file->file);
	  *p++ = c;
	}
	if (! my_ismbchar(charset_info, mb_start, p))
	{
	  /* It was not a multiline char, push back the characters */
	  /* We leave first 'c', i.e. pretend it was a normal char */
	  while (p > mb_start)
	    my_ungetc(*--p);
	}
      }
      else
#endif
	*p++= c;
    }
  }
  die("The input buffer is too small for this query.x\n" \
      "check your query or increase MAX_QUERY and recompile");
  DBUG_RETURN(0);
}


/*
  Convert the read query to result format version 1

  That is: After newline, all spaces need to be skipped
  unless the previous char was a quote

  This is due to an old bug that has now been fixed, but the
  version 1 output format is preserved by using this function

*/

void convert_to_format_v1(char* query)
{
  int last_c_was_quote= 0;
  char *p= query, *to= query;
  char *end= strend(query);
  char last_c;

  while (p <= end)
  {
    if (*p == '\n' && !last_c_was_quote)
    {
      *to++ = *p++; /* Save the newline */

      /* Skip any spaces on next line */
      while (*p && my_isspace(charset_info, *p))
        p++;

      last_c_was_quote= 0;
    }
    else if (*p == '\'' || *p == '"' || *p == '`')
    {
      last_c= *p;
      *to++ = *p++;

      /* Copy anything until the next quote of same type */
      while (*p && *p != last_c)
        *to++ = *p++;

      *to++ = *p++;

      last_c_was_quote= 1;
    }
    else
    {
      *to++ = *p++;
      last_c_was_quote= 0;
    }
  }
}


/*
  Check a command that is about to be sent (or should have been
  sent if parsing was enabled) to mysql server for
  suspicious things and generate warnings.
*/

void scan_command_for_warnings(struct st_command *command)
{
  const char *ptr= command->query;
  DBUG_ENTER("scan_command_for_warnings");
  DBUG_PRINT("enter", ("query: %s", command->query));

  while(*ptr)
  {
    /*
      Look for query's that lines that start with a -- comment
      and has a mysqltest command
    */
    if (ptr[0] == '\n' &&
        ptr[1] && ptr[1] == '-' &&
        ptr[2] && ptr[2] == '-' &&
        ptr[3])
    {
      uint type;
      char save;
      char *end, *start= (char*)ptr+3;
      /* Skip leading spaces */
      while (*start && my_isspace(charset_info, *start))
        start++;
      end= start;
      /* Find end of command(next space) */
      while (*end && !my_isspace(charset_info, *end))
        end++;
      save= *end;
      *end= 0;
      DBUG_PRINT("info", ("Checking '%s'", start));
      type= find_type(start, &command_typelib, 1+2);
      if (type)
        warning_msg("Embedded mysqltest command '--%s' detected in "
                    "query '%s' was this intentional? ",
                    start, command->query);
      *end= save;
    }

    ptr++;
  }
  DBUG_VOID_RETURN;
}

/*
  Check for unexpected "junk" after the end of query
  This is normally caused by missing delimiters or when
  switching between different delimiters
*/

void check_eol_junk_line(const char *line)
{
  const char *p= line;
  DBUG_ENTER("check_eol_junk_line");
  DBUG_PRINT("enter", ("line: %s", line));

  /* Check for extra delimiter */
  if (*p && !strncmp(p, delimiter, delimiter_length))
    die("Extra delimiter \"%s\" found", delimiter);

  /* Allow trailing # comment */
  if (*p && *p != '#')
  {
    if (*p == '\n')
      die("Missing delimiter");
    die("End of line junk detected: \"%s\"", p);
  }
  DBUG_VOID_RETURN;
}

void check_eol_junk(const char *eol)
{
  const char *p= eol;
  DBUG_ENTER("check_eol_junk");
  DBUG_PRINT("enter", ("eol: %s", eol));

  /* Skip past all spacing chars and comments */
  while (*p && (my_isspace(charset_info, *p) || *p == '#' || *p == '\n'))
  {
    /* Skip past comments started with # and ended with newline */
    if (*p && *p == '#')
    {
      p++;
      while (*p && *p != '\n')
        p++;
    }

    /* Check this line */
    if (*p && *p == '\n')
      check_eol_junk_line(p);

    if (*p)
      p++;
  }

  check_eol_junk_line(p);

  DBUG_VOID_RETURN;
}



/*
  Create a command from a set of lines

  SYNOPSIS
    read_command()
    command_ptr pointer where to return the new query

  DESCRIPTION
    Converts lines returned by read_line into a command, this involves
    parsing the first word in the read line to find the command type.

  A -- comment may contain a valid query as the first word after the
  comment start. Thus it's always checked to see if that is the case.
  The advantage with this approach is to be able to execute commands
  terminated by new line '\n' regardless how many "delimiter" it contain.
*/

#define MAX_QUERY (256*1024) /* 256K -- a test in sp-big is >128K */
static char read_command_buf[MAX_QUERY];

int read_command(struct st_command** command_ptr)
{
  char *p= read_command_buf;
  struct st_command* command;
  DBUG_ENTER("read_command");

  if (parser.current_line < parser.read_lines)
  {
    get_dynamic(&q_lines, (gptr) command_ptr, parser.current_line) ;
    DBUG_RETURN(0);
  }
  if (!(*command_ptr= command=
        (struct st_command*) my_malloc(sizeof(*command), MYF(MY_WME))) ||
      insert_dynamic(&q_lines, (gptr) &command))
    die(NullS);

  command->require_file[0]= 0;
  command->first_word_len= 0;
  command->query_len= 0;

  command->type= Q_UNKNOWN;
  command->query_buf= command->query= 0;
  read_command_buf[0]= 0;
  if (read_line(read_command_buf, sizeof(read_command_buf)))
  {
    check_eol_junk(read_command_buf);
    DBUG_RETURN(1);
  }

  convert_to_format_v1(read_command_buf);

  DBUG_PRINT("info", ("query: %s", read_command_buf));
  if (*p == '#')
  {
    command->type= Q_COMMENT;
  }
  else if (p[0] == '-' && p[1] == '-')
  {
    command->type= Q_COMMENT_WITH_COMMAND;
    p+= 2; /* Skip past -- */
  }

  /* Skip leading spaces */
  while (*p && my_isspace(charset_info, *p))
    p++;

  if (!(command->query_buf= command->query= my_strdup(p, MYF(MY_WME))))
    die("Out of memory");

  /* Calculate first word and first argument */
  for (p= command->query; *p && !my_isspace(charset_info, *p) ; p++) ;
  command->first_word_len= (uint) (p - command->query);

  /* Skip spaces between command and first argument */
  while (*p && my_isspace(charset_info, *p))
    p++;
  command->first_argument= p;

  command->end= strend(command->query);
  command->query_len= (command->end - command->query);
  parser.read_lines++;
  DBUG_RETURN(0);
}


static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "Basedir for tests.", (gptr*) &opt_basedir,
   (gptr*) &opt_basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory where character sets are.", (gptr*) &opt_charsets_dir,
   (gptr*) &opt_charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use the compressed server/client protocol.",
   (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"cursor-protocol", OPT_CURSOR_PROTOCOL, "Use cursors for prepared statements.",
   (gptr*) &cursor_protocol, (gptr*) &cursor_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"database", 'D', "Database to use.", (gptr*) &opt_db, (gptr*) &opt_db, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit",
   0,0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"host", 'h', "Connect to host.", (gptr*) &opt_host, (gptr*) &opt_host, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"include", 'i', "Include SQL before each test case.", (gptr*) &opt_include,
   (gptr*) &opt_include, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"logdir", OPT_LOG_DIR, "Directory for log files", (gptr*) &opt_logdir,
   (gptr*) &opt_logdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"mark-progress", OPT_MARK_PROGRESS,
   "Write linenumber and elapsed time to <testname>.progress ",
   (gptr*) &opt_mark_progress, (gptr*) &opt_mark_progress, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"max-connect-retries", OPT_MAX_CONNECT_RETRIES,
   "Max number of connection attempts when connecting to server",
   (gptr*) &opt_max_connect_retries, (gptr*) &opt_max_connect_retries, 0,
   GET_INT, REQUIRED_ARG, 500, 1, 10000, 0, 0, 0},
  {"password", 'p', "Password to use when connecting to server.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.", (gptr*) &opt_port,
   (gptr*) &opt_port, 0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ps-protocol", OPT_PS_PROTOCOL, "Use prepared statements protocol for communication",
   (gptr*) &ps_protocol, (gptr*) &ps_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"quiet", 's', "Suppress all normal output.", (gptr*) &silent,
   (gptr*) &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"record", 'r', "Record output of test_file into result file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"result-file", 'R', "Read/Store result from/in this file.",
   (gptr*) &result_file_name, (gptr*) &result_file_name, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-arg", 'A', "Send option value to embedded server as a parameter.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-file", 'F', "Read embedded server arguments from file.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Suppress all normal output. Synonym for --quiet.",
   (gptr*) &silent, (gptr*) &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-safemalloc", OPT_SKIP_SAFEMALLOC,
   "Don't use the memory allocation checking.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"sleep", 'T', "Sleep always this many seconds on sleep commands.",
   (gptr*) &opt_sleep, (gptr*) &opt_sleep, 0, GET_INT, REQUIRED_ARG, -1, 0, 0,
   0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
   (gptr*) &unix_sock, (gptr*) &unix_sock, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"sp-protocol", OPT_SP_PROTOCOL, "Use stored procedures for select",
   (gptr*) &sp_protocol, (gptr*) &sp_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#include "sslopt-longopts.h"
  {"test-file", 'x', "Read test from/in this file (default stdin).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"timer-file", 'm', "File where the timing in micro seconds is stored.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't', "Temporary directory where sockets are put.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login.", (gptr*) &opt_user, (gptr*) &opt_user, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Write more.", (gptr*) &verbose, (gptr*) &verbose, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"view-protocol", OPT_VIEW_PROTOCOL, "Use views for select",
   (gptr*) &view_protocol, (gptr*) &view_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


#include <help_start.h>

void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,MTEST_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

void usage()
{
  print_version();
  printf("MySQL AB, by Sasha, Matt, Monty & Jani\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Runs a test against the mysql server and compares output with a results file.\n\n");
  printf("Usage: %s [OPTIONS] [database] < test_file\n", my_progname);
  my_print_help(my_long_options);
  printf("  --no-defaults       Don't read default options from any options file.\n");
  my_print_variables(my_long_options);
}

#include <help_end.h>


/*
  Read arguments for embedded server and put them into
  embedded_server_args[]
*/

void read_embedded_server_arguments(const char *name)
{
  char argument[1024],buff[FN_REFLEN], *str=0;
  FILE *file;

  if (!test_if_hard_path(name))
  {
    strxmov(buff, opt_basedir, name, NullS);
    name=buff;
  }
  fn_format(buff, name, "", "", MY_UNPACK_FILENAME);

  if (!embedded_server_arg_count)
  {
    embedded_server_arg_count=1;
    embedded_server_args[0]= (char*) "";		/* Progname */
  }
  if (!(file=my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(MY_WME))))
    die("Failed to open file %s", buff);

  while (embedded_server_arg_count < MAX_EMBEDDED_SERVER_ARGS &&
	 (str=fgets(argument,sizeof(argument), file)))
  {
    *(strend(str)-1)=0;				/* Remove end newline */
    if (!(embedded_server_args[embedded_server_arg_count]=
	  (char*) my_strdup(str,MYF(MY_WME))))
    {
      my_fclose(file,MYF(0));
      die("Out of memory");

    }
    embedded_server_arg_count++;
  }
  my_fclose(file,MYF(0));
  if (str)
    die("Too many arguments in option file: %s",name);

  return;
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument)
{
  switch(optid) {
  case '#':
#ifndef DBUG_OFF
    DBUG_PUSH(argument ? argument : "d:t:S:i:O,/tmp/mysqltest.trace");
#endif
    break;
  case 'r':
    record = 1;
    break;
  case 'x':
  {
    char buff[FN_REFLEN];
    if (!test_if_hard_path(argument))
    {
      strxmov(buff, opt_basedir, argument, NullS);
      argument= buff;
    }
    fn_format(buff, argument, "", "", MY_UNPACK_FILENAME);
    DBUG_ASSERT(cur_file == file_stack && cur_file->file == 0);
    if (!(cur_file->file=
          my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(0))))
      die("Could not open %s: errno = %d", buff, errno);
    cur_file->file_name= my_strdup(buff, MYF(MY_FAE));
    cur_file->lineno= 1;
    break;
  }
  case 'm':
  {
    static char buff[FN_REFLEN];
    if (!test_if_hard_path(argument))
    {
      strxmov(buff, opt_basedir, argument, NullS);
      argument= buff;
    }
    fn_format(buff, argument, "", "", MY_UNPACK_FILENAME);
    timer_file= buff;
    unlink(timer_file);	     /* Ignore error, may not exist */
    break;
  }
  case 'p':
    if (argument)
    {
      my_free(opt_pass, MYF(MY_ALLOW_ZERO_PTR));
      opt_pass= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      tty_password= 0;
    }
    else
      tty_password= 1;
    break;
#include <sslopt-case.h>
  case 't':
    strnmov(TMPDIR, argument, sizeof(TMPDIR));
    break;
  case 'A':
    if (!embedded_server_arg_count)
    {
      embedded_server_arg_count=1;
      embedded_server_args[0]= (char*) "";
    }
    if (embedded_server_arg_count == MAX_EMBEDDED_SERVER_ARGS-1 ||
        !(embedded_server_args[embedded_server_arg_count++]=
          my_strdup(argument, MYF(MY_FAE))))
    {
      die("Can't use server argument");
    }
    break;
  case 'F':
    read_embedded_server_arguments(argument);
    break;
  case OPT_SKIP_SAFEMALLOC:
#ifdef SAFEMALLOC
    sf_malloc_quick=1;
#endif
    break;
  case 'V':
    print_version();
    exit(0);
  case '?':
    usage();
    exit(0);
  }
  return 0;
}


int parse_args(int argc, char **argv)
{
  load_defaults("my",load_default_groups,&argc,&argv);
  default_argv= argv;

  if ((handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(1);

  if (argc > 1)
  {
    usage();
    exit(1);
  }
  if (argc == 1)
    opt_db= *argv;
  if (tty_password)
    opt_pass= get_tty_password(NullS);          /* purify tested */

  return 0;
}

/*
  Write the content of str into file

  SYNOPSIS
  str_to_file2
  fname - name of file to truncate/create and write to
  str - content to write to file
  size - size of content witten to file
  append - append to file instead of overwriting old file
*/

void str_to_file2(const char *fname, char *str, int size, my_bool append)
{
  int fd;
  char buff[FN_REFLEN];
  int flags= O_WRONLY | O_CREAT;
  if (!test_if_hard_path(fname))
  {
    strxmov(buff, opt_basedir, fname, NullS);
    fname= buff;
  }
  fn_format(buff, fname, "", "", MY_UNPACK_FILENAME);

  if (!append)
    flags|= O_TRUNC;
  if ((fd= my_open(buff, flags,
                   MYF(MY_WME | MY_FFNF))) < 0)
    die("Could not open %s: errno = %d", buff, errno);
  if (append && my_seek(fd, 0, SEEK_END, MYF(0)) == MY_FILEPOS_ERROR)
    die("Could not find end of file %s: errno = %d", buff, errno);
  if (my_write(fd, (byte*)str, size, MYF(MY_WME|MY_FNABP)))
    die("write failed");
  my_close(fd, MYF(0));
}

/*
  Write the content of str into file

  SYNOPSIS
  str_to_file
  fname - name of file to truncate/create and write to
  str - content to write to file
  size - size of content witten to file
*/

void str_to_file(const char *fname, char *str, int size)
{
  str_to_file2(fname, str, size, FALSE);
}


void dump_result_to_reject_file(char *buf, int size)
{
  char reject_file[FN_REFLEN];
  str_to_file(fn_format(reject_file, result_file_name, "", ".reject",
                        MY_REPLACE_EXT),
              buf, size);
}

void dump_result_to_log_file(char *buf, int size)
{
  char log_file[FN_REFLEN];
  str_to_file(fn_format(log_file, result_file_name, opt_logdir, ".log",
                        *opt_logdir ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              buf, size);
}

void dump_progress(void)
{
  char progress_file[FN_REFLEN];
  str_to_file(fn_format(progress_file, result_file_name,
                        opt_logdir, ".progress",
                        *opt_logdir ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              ds_progress.str, ds_progress.length);
}

void dump_warning_messages(void)
{
  char warn_file[FN_REFLEN];

  str_to_file(fn_format(warn_file, result_file_name, opt_logdir, ".warnings",
                        *opt_logdir ? MY_REPLACE_DIR | MY_REPLACE_EXT :
                        MY_REPLACE_EXT),
              ds_warning_messages.str, ds_warning_messages.length);
}

void check_regerr(my_regex_t* r, int err)
{
  char err_buf[1024];

  if (err)
  {
    my_regerror(err,r,err_buf,sizeof(err_buf));
    die("Regex error: %s\n", err_buf);
  }
}


#ifdef __WIN__

DYNAMIC_ARRAY patterns;

/*
  init_win_path_patterns

  DESCRIPTION
  Setup string patterns that will be used to detect filenames that
  needs to be converted from Win to Unix format

*/

void init_win_path_patterns()
{
  /* List of string patterns to match in order to find paths */
  const char* paths[] = { "$MYSQL_TEST_DIR",
                          "$MYSQL_TMP_DIR",
                          "$MYSQLTEST_VARDIR",
                          "./test/" };
  int num_paths= sizeof(paths)/sizeof(char*);
  int i;
  char* p;

  DBUG_ENTER("init_win_path_patterns");

  my_init_dynamic_array(&patterns, sizeof(const char*), 16, 16);

  /* Loop through all paths in the array */
  for (i= 0; i < num_paths; i++)
  {
    VAR* v;
    if (*(paths[i]) == '$')
    {
      v= var_get(paths[i], 0, 0, 0);
      p= my_strdup(v->str_val, MYF(MY_FAE));
    }
    else
      p= my_strdup(paths[i], MYF(MY_FAE));

    /* Don't insert zero length strings in patterns array */
    if (strlen(p) == 0)
    {
      my_free(p, MYF(0));
      continue;
    }

    if (insert_dynamic(&patterns, (gptr) &p))
      die(NullS);

    DBUG_PRINT("info", ("p: %s", p));
    while (*p)
    {
      if (*p == '/')
        *p='\\';
      p++;
    }
  }
  DBUG_VOID_RETURN;
}

void free_win_path_patterns()
{
  uint i= 0;
  for (i=0 ; i < patterns.elements ; i++)
  {
    const char** pattern= dynamic_element(&patterns, i, const char**);
    my_free((gptr) *pattern, MYF(0));
  }
  delete_dynamic(&patterns);
}

/*
  fix_win_paths

  DESCRIPTION
  Search the string 'val' for the patterns that are known to be
  strings that contain filenames. Convert all \ to / in the
  filenames that are found.

  Ex:
  val = 'Error "c:\mysql\mysql-test\var\test\t1.frm" didn't exist'
  => $MYSQL_TEST_DIR is found by strstr
  => all \ from c:\mysql\m... until next space is converted into /
*/

void fix_win_paths(const char *val, int len)
{
  uint i;
  char *p;

  DBUG_ENTER("fix_win_paths");
  for (i= 0; i < patterns.elements; i++)
  {
    const char** pattern= dynamic_element(&patterns, i, const char**);
    DBUG_PRINT("info", ("pattern: %s", *pattern));

    /* Search for the path in string */
    while ((p= strstr(val, *pattern)))
    {
      DBUG_PRINT("info", ("Found %s in val p: %s", *pattern, p));

      while (*p && !my_isspace(charset_info, *p))
      {
        if (*p == '\\')
          *p= '/';
        p++;
      }
      DBUG_PRINT("info", ("Converted \\ to /, p: %s", p));
    }
  }
  DBUG_PRINT("exit", (" val: %s, len: %d", val, len));
  DBUG_VOID_RETURN;
}
#endif



/*
  Append the result for one field to the dynamic string ds
*/

void append_field(DYNAMIC_STRING *ds, uint col_idx, MYSQL_FIELD* field,
                  const char* val, ulonglong len, bool is_null)
{
  if (col_idx < max_replace_column && replace_column[col_idx])
  {
    val= replace_column[col_idx];
    len= strlen(val);
  }
  else if (is_null)
  {
    val= "NULL";
    len= 4;
  }
#ifdef __WIN__
  else if ((field->type == MYSQL_TYPE_DOUBLE ||
            field->type == MYSQL_TYPE_FLOAT ) &&
           field->decimals >= 31)
  {
    /* Convert 1.2e+018 to 1.2e+18 and 1.2e-018 to 1.2e-18 */
    char *start= strchr(val, 'e');
    if (start && strlen(start) >= 5 &&
        (start[1] == '-' || start[1] == '+') && start[2] == '0')
    {
      start+=2; /* Now points at first '0' */
      /* Move all chars after the first '0' one step left */
      memmove(start, start + 1, strlen(start));
      len--;
    }
  }
#endif

  if (!display_result_vertically)
  {
    if (col_idx)
      dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_mem(ds, val, (int)len);
  }
  else
  {
    dynstr_append(ds, field->name);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_mem(ds, val, (int)len);
    dynstr_append_mem(ds, "\n", 1);
  }
}


/*
  Append all results to the dynamic string separated with '\t'
  Values may be converted with 'replace_column'
*/

void append_result(DYNAMIC_STRING *ds, MYSQL_RES *res)
{
  MYSQL_ROW row;
  uint num_fields= mysql_num_fields(res);
  MYSQL_FIELD *fields= mysql_fetch_fields(res);
  ulong *lengths;

  while ((row = mysql_fetch_row(res)))
  {
    uint i;
    lengths = mysql_fetch_lengths(res);
    for (i = 0; i < num_fields; i++)
      append_field(ds, i, &fields[i],
                   (const char*)row[i], lengths[i], !row[i]);
    if (!display_result_vertically)
      dynstr_append_mem(ds, "\n", 1);
  }
}


/*
  Append all results from ps execution to the dynamic string separated
  with '\t'. Values may be converted with 'replace_column'
*/

void append_stmt_result(DYNAMIC_STRING *ds, MYSQL_STMT *stmt,
                        MYSQL_FIELD *fields, uint num_fields)
{
  MYSQL_BIND *my_bind;
  my_bool *is_null;
  ulong *length;
  uint i;

  /* Allocate array with bind structs, lengths and NULL flags */
  my_bind= (MYSQL_BIND*) my_malloc(num_fields * sizeof(MYSQL_BIND),
				MYF(MY_WME | MY_FAE | MY_ZEROFILL));
  length= (ulong*) my_malloc(num_fields * sizeof(ulong),
			     MYF(MY_WME | MY_FAE));
  is_null= (my_bool*) my_malloc(num_fields * sizeof(my_bool),
				MYF(MY_WME | MY_FAE));

  /* Allocate data for the result of each field */
  for (i= 0; i < num_fields; i++)
  {
    uint max_length= fields[i].max_length + 1;
    my_bind[i].buffer_type= MYSQL_TYPE_STRING;
    my_bind[i].buffer= (char *)my_malloc(max_length, MYF(MY_WME | MY_FAE));
    my_bind[i].buffer_length= max_length;
    my_bind[i].is_null= &is_null[i];
    my_bind[i].length= &length[i];

    DBUG_PRINT("bind", ("col[%d]: buffer_type: %d, buffer_length: %lu",
			i, my_bind[i].buffer_type, my_bind[i].buffer_length));
  }

  if (mysql_stmt_bind_result(stmt, my_bind))
    die("mysql_stmt_bind_result failed: %d: %s",
	mysql_stmt_errno(stmt), mysql_stmt_error(stmt));

  while (mysql_stmt_fetch(stmt) == 0)
  {
    for (i= 0; i < num_fields; i++)
      append_field(ds, i, &fields[i], (const char *) my_bind[i].buffer,
                   *my_bind[i].length, *my_bind[i].is_null);
    if (!display_result_vertically)
      dynstr_append_mem(ds, "\n", 1);
  }

  if (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA)
    die("fetch didn't end with MYSQL_NO_DATA from statement: %d %s",
	mysql_stmt_errno(stmt), mysql_stmt_error(stmt));

  for (i= 0; i < num_fields; i++)
  {
    /* Free data for output */
    my_free((gptr)my_bind[i].buffer, MYF(MY_WME | MY_FAE));
  }
  /* Free array with bind structs, lengths and NULL flags */
  my_free((gptr)my_bind    , MYF(MY_WME | MY_FAE));
  my_free((gptr)length  , MYF(MY_WME | MY_FAE));
  my_free((gptr)is_null , MYF(MY_WME | MY_FAE));
}


/*
  Append metadata for fields to output
*/

void append_metadata(DYNAMIC_STRING *ds,
                     MYSQL_FIELD *field,
                     uint num_fields)
{
  MYSQL_FIELD *field_end;
  dynstr_append(ds,"Catalog\tDatabase\tTable\tTable_alias\tColumn\t"
                "Column_alias\tType\tLength\tMax length\tIs_null\t"
                "Flags\tDecimals\tCharsetnr\n");

  for (field_end= field+num_fields ;
       field < field_end ;
       field++)
  {
    dynstr_append_mem(ds, field->catalog,
                      field->catalog_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->db, field->db_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->org_table,
                      field->org_table_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->table,
                      field->table_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->org_name,
                      field->org_name_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, field->name, field->name_length);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->type);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->length);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->max_length);
    dynstr_append_mem(ds, "\t", 1);
    dynstr_append_mem(ds, (char*) (IS_NOT_NULL(field->flags) ?
                                   "N" : "Y"), 1);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->flags);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->decimals);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_uint(ds, field->charsetnr);
    dynstr_append_mem(ds, "\n", 1);
  }
}


/*
  Append affected row count and other info to output
*/

void append_info(DYNAMIC_STRING *ds, ulonglong affected_rows,
                 const char *info)
{
  char buf[40], buff2[21];
  sprintf(buf,"affected rows: %s\n", llstr(affected_rows, buff2));
  dynstr_append(ds, buf);
  if (info)
  {
    dynstr_append(ds, "info: ");
    dynstr_append(ds, info);
    dynstr_append_mem(ds, "\n", 1);
  }
}


/*
  Display the table headings with the names tab separated
*/

void append_table_headings(DYNAMIC_STRING *ds,
                           MYSQL_FIELD *field,
                           uint num_fields)
{
  uint col_idx;
  for (col_idx= 0; col_idx < num_fields; col_idx++)
  {
    if (col_idx)
      dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append(ds, field[col_idx].name);
  }
  dynstr_append_mem(ds, "\n", 1);
}

/*
  Fetch warnings from server and append to ds

  RETURN VALUE
  Number of warnings appended to ds
*/

int append_warnings(DYNAMIC_STRING *ds, MYSQL* mysql)
{
  uint count;
  MYSQL_RES *warn_res;
  DBUG_ENTER("append_warnings");

  if (!(count= mysql_warning_count(mysql)))
    DBUG_RETURN(0);

  /*
    If one day we will support execution of multi-statements
    through PS API we should not issue SHOW WARNINGS until
    we have not read all results...
  */
  DBUG_ASSERT(!mysql_more_results(mysql));

  if (mysql_real_query(mysql, "SHOW WARNINGS", 13))
    die("Error running query \"SHOW WARNINGS\": %s", mysql_error(mysql));

  if (!(warn_res= mysql_store_result(mysql)))
    die("Warning count is %u but didn't get any warnings",
	count);

  append_result(ds, warn_res);
  mysql_free_result(warn_res);

  DBUG_PRINT("warnings", ("%s", ds->str));

  DBUG_RETURN(count);
}


/*
  Run query using MySQL C API

  SYNOPSIS
    run_query_normal()
    mysql	mysql handle
    command	current command pointer
    flags	flags indicating if we should SEND and/or REAP
    query	query string to execute
    query_len	length query string to execute
    ds		output buffer where to store result form query
*/

void run_query_normal(struct st_connection *cn, struct st_command *command,
                      int flags, char *query, int query_len,
                      DYNAMIC_STRING *ds, DYNAMIC_STRING *ds_warnings)
{
  MYSQL_RES *res= 0;
  MYSQL *mysql= &cn->mysql;
  int err= 0, counter= 0;
  DBUG_ENTER("run_query_normal");
  DBUG_PRINT("enter",("flags: %d", flags));
  DBUG_PRINT("enter", ("query: '%-.60s'", query));

  if (flags & QUERY_SEND_FLAG)
  {
    /*
      Send the query
    */
    if (do_send_query(cn, query, query_len, flags))
    {
      handle_error(command, mysql_errno(mysql), mysql_error(mysql),
		   mysql_sqlstate(mysql), ds);
      goto end;
    }
  }
#ifdef EMBEDDED_LIBRARY
  /*
   Here we handle 'reap' command, so we need to check if the
   query's thread was finished and probably wait
  */
  else if (flags & QUERY_REAP_FLAG)
  {
    pthread_mutex_lock(&cn->mutex);
    while (!cn->query_done)
      pthread_cond_wait(&cn->cond, &cn->mutex);
    pthread_mutex_unlock(&cn->mutex);
  }
#endif /*EMBEDDED_LIBRARY*/
  if (!(flags & QUERY_REAP_FLAG))
    DBUG_VOID_RETURN;

  do
  {
    /*
      When  on first result set, call mysql_read_query_result to retrieve
      answer to the query sent earlier
    */
    if ((counter==0) && mysql_read_query_result(mysql))
    {
      handle_error(command, mysql_errno(mysql), mysql_error(mysql),
		   mysql_sqlstate(mysql), ds);
      goto end;

    }

    /*
      Store the result of the query if it will return any fields
    */
    if (mysql_field_count(mysql) && ((res= mysql_store_result(mysql)) == 0))
    {
      handle_error(command, mysql_errno(mysql), mysql_error(mysql),
		   mysql_sqlstate(mysql), ds);
      goto end;
    }

    if (!disable_result_log)
    {
      ulonglong affected_rows;    /* Ok to be undef if 'disable_info' is set */
      LINT_INIT(affected_rows);

      if (res)
      {
	MYSQL_FIELD *fields= mysql_fetch_fields(res);
	uint num_fields= mysql_num_fields(res);

	if (display_metadata)
          append_metadata(ds, fields, num_fields);

	if (!display_result_vertically)
	  append_table_headings(ds, fields, num_fields);

	append_result(ds, res);
      }

      /*
        Need to call mysql_affected_rows() before the "new"
        query to find the warnings
      */
      if (!disable_info)
        affected_rows= mysql_affected_rows(mysql);

      /*
        Add all warnings to the result. We can't do this if we are in
        the middle of processing results from multi-statement, because
        this will break protocol.
      */
      if (!disable_warnings && !mysql_more_results(mysql))
      {
	if (append_warnings(ds_warnings, mysql) || ds_warnings->length)
	{
	  dynstr_append_mem(ds, "Warnings:\n", 10);
	  dynstr_append_mem(ds, ds_warnings->str, ds_warnings->length);
	}
      }

      if (!disable_info)
	append_info(ds, affected_rows, mysql_info(mysql));
    }

    if (res)
    {
      mysql_free_result(res);
      res= 0;
    }
    counter++;
  } while (!(err= mysql_next_result(mysql)));
  if (err > 0)
  {
    /* We got an error from mysql_next_result, maybe expected */
    handle_error(command, mysql_errno(mysql), mysql_error(mysql),
		 mysql_sqlstate(mysql), ds);
    goto end;
  }
  DBUG_ASSERT(err == -1); /* Successful and there are no more results */

  /* If we come here the query is both executed and read successfully */
  handle_no_error(command);

end:

  /*
    We save the return code (mysql_errno(mysql)) from the last call sent
    to the server into the mysqltest builtin variable $mysql_errno. This
    variable then can be used from the test case itself.
  */
  var_set_errno(mysql_errno(mysql));
  DBUG_VOID_RETURN;
}


/*
  Handle errors which occurred during execution

  SYNOPSIS
  handle_error()
  q     - query context
  err_errno - error number
  err_error - error message
  err_sqlstate - sql state
  ds    - dynamic string which is used for output buffer

  NOTE
  If there is an unexpected error this function will abort mysqltest
  immediately.

  RETURN VALUE
  error - function will not return
*/

void handle_error(struct st_command *command,
                  unsigned int err_errno, const char *err_error,
                  const char *err_sqlstate, DYNAMIC_STRING *ds)
{
  uint i;

  DBUG_ENTER("handle_error");

  if (command->require_file[0])
  {
    /*
      The query after a "--require" failed. This is fine as long the server
      returned a valid reponse. Don't allow 2013 or 2006 to trigger an
      abort_not_supported_test
    */
    if (err_errno == CR_SERVER_LOST ||
        err_errno == CR_SERVER_GONE_ERROR)
      die("require query '%s' failed: %d: %s", command->query,
          err_errno, err_error);

    /* Abort the run of this test, pass the failed query as reason */
    abort_not_supported_test("Query '%s' failed, required functionality " \
                             "not supported", command->query);
  }

  if (command->abort_on_error)
    die("query '%s' failed: %d: %s", command->query, err_errno, err_error);

  DBUG_PRINT("info", ("expected_errors.count: %d",
                      command->expected_errors.count));
  for (i= 0 ; (uint) i < command->expected_errors.count ; i++)
  {
    if (((command->expected_errors.err[i].type == ERR_ERRNO) &&
         (command->expected_errors.err[i].code.errnum == err_errno)) ||
        ((command->expected_errors.err[i].type == ERR_SQLSTATE) &&
         (strncmp(command->expected_errors.err[i].code.sqlstate,
                  err_sqlstate, SQLSTATE_LENGTH) == 0)))
    {
      if (!disable_result_log)
      {
        if (command->expected_errors.count == 1)
        {
          /* Only log error if there is one possible error */
          dynstr_append_mem(ds, "ERROR ", 6);
          replace_dynstr_append(ds, err_sqlstate);
          dynstr_append_mem(ds, ": ", 2);
          replace_dynstr_append(ds, err_error);
          dynstr_append_mem(ds,"\n",1);
        }
        /* Don't log error if we may not get an error */
        else if (command->expected_errors.err[0].type == ERR_SQLSTATE ||
                 (command->expected_errors.err[0].type == ERR_ERRNO &&
                  command->expected_errors.err[0].code.errnum != 0))
          dynstr_append(ds,"Got one of the listed errors\n");
      }
      /* OK */
      DBUG_VOID_RETURN;
    }
  }

  DBUG_PRINT("info",("i: %d  expected_errors: %d", i,
                     command->expected_errors.count));

  if (!disable_result_log)
  {
    dynstr_append_mem(ds, "ERROR ",6);
    replace_dynstr_append(ds, err_sqlstate);
    dynstr_append_mem(ds, ": ", 2);
    replace_dynstr_append(ds, err_error);
    dynstr_append_mem(ds, "\n", 1);
  }

  if (i)
  {
    if (command->expected_errors.err[0].type == ERR_ERRNO)
      die("query '%s' failed with wrong errno %d: '%s', instead of %d...",
          command->query, err_errno, err_error,
          command->expected_errors.err[0].code.errnum);
    else
      die("query '%s' failed with wrong sqlstate %s: '%s', instead of %s...",
          command->query, err_sqlstate, err_error,
	  command->expected_errors.err[0].code.sqlstate);
  }

  DBUG_VOID_RETURN;
}


/*
  Handle absence of errors after execution

  SYNOPSIS
  handle_no_error()
  q - context of query

  RETURN VALUE
  error - function will not return
*/

void handle_no_error(struct st_command *command)
{
  DBUG_ENTER("handle_no_error");

  if (command->expected_errors.err[0].type == ERR_ERRNO &&
      command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    die("query '%s' succeeded - should have failed with errno %d...",
        command->query, command->expected_errors.err[0].code.errnum);
  }
  else if (command->expected_errors.err[0].type == ERR_SQLSTATE &&
           strcmp(command->expected_errors.err[0].code.sqlstate,"00000") != 0)
  {
    /* SQLSTATE we wanted was != "00000", i.e. not an expected success */
    die("query '%s' succeeded - should have failed with sqlstate %s...",
        command->query, command->expected_errors.err[0].code.sqlstate);
  }

  DBUG_VOID_RETURN;
}


/*
  Run query using prepared statement C API

  SYNPOSIS
  run_query_stmt
  mysql - mysql handle
  command - currrent command pointer
  query - query string to execute
  query_len - length query string to execute
  ds - output buffer where to store result form query

  RETURN VALUE
  error - function will not return
*/

void run_query_stmt(MYSQL *mysql, struct st_command *command,
                    char *query, int query_len, DYNAMIC_STRING *ds,
                    DYNAMIC_STRING *ds_warnings)
{
  MYSQL_RES *res= NULL;     /* Note that here 'res' is meta data result set */
  MYSQL_STMT *stmt;
  DYNAMIC_STRING ds_prepare_warnings;
  DYNAMIC_STRING ds_execute_warnings;
  DBUG_ENTER("run_query_stmt");
  DBUG_PRINT("query", ("'%-.60s'", query));

  /*
    Init a new stmt if it's not already one created for this connection
  */
  if(!(stmt= cur_con->stmt))
  {
    if (!(stmt= mysql_stmt_init(mysql)))
      die("unable to init stmt structure");
    cur_con->stmt= stmt;
  }

  /* Init dynamic strings for warnings */
  if (!disable_warnings)
  {
    init_dynamic_string(&ds_prepare_warnings, NULL, 0, 256);
    init_dynamic_string(&ds_execute_warnings, NULL, 0, 256);
  }

  /*
    Prepare the query
  */
  if (mysql_stmt_prepare(stmt, query, query_len))
  {
    handle_error(command,  mysql_stmt_errno(stmt),
                 mysql_stmt_error(stmt), mysql_stmt_sqlstate(stmt), ds);
    goto end;
  }

  /*
    Get the warnings from mysql_stmt_prepare and keep them in a
    separate string
  */
  if (!disable_warnings)
    append_warnings(&ds_prepare_warnings, mysql);

  /*
    No need to call mysql_stmt_bind_param() because we have no
    parameter markers.
  */

#if MYSQL_VERSION_ID >= 50000
  if (cursor_protocol_enabled)
  {
    /*
      Use cursor when retrieving result
    */
    ulong type= CURSOR_TYPE_READ_ONLY;
    if (mysql_stmt_attr_set(stmt, STMT_ATTR_CURSOR_TYPE, (void*) &type))
      die("mysql_stmt_attr_set(STMT_ATTR_CURSOR_TYPE) failed': %d %s",
          mysql_stmt_errno(stmt), mysql_stmt_error(stmt));
  }
#endif

  /*
    Execute the query
  */
  if (mysql_stmt_execute(stmt))
  {
    handle_error(command, mysql_stmt_errno(stmt),
                 mysql_stmt_error(stmt), mysql_stmt_sqlstate(stmt), ds);
    goto end;
  }

  /*
    When running in cursor_protocol get the warnings from execute here
    and keep them in a separate string for later.
  */
  if (cursor_protocol_enabled && !disable_warnings)
    append_warnings(&ds_execute_warnings, mysql);

  /*
    We instruct that we want to update the "max_length" field in
    mysql_stmt_store_result(), this is our only way to know how much
    buffer to allocate for result data
  */
  {
    my_bool one= 1;
    if (mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (void*) &one))
      die("mysql_stmt_attr_set(STMT_ATTR_UPDATE_MAX_LENGTH) failed': %d %s",
          mysql_stmt_errno(stmt), mysql_stmt_error(stmt));
  }

  /*
    If we got here the statement succeeded and was expected to do so,
    get data. Note that this can still give errors found during execution!
    Store the result of the query if if will return any fields
  */
  if (mysql_stmt_field_count(stmt) && mysql_stmt_store_result(stmt))
  {
    handle_error(command, mysql_stmt_errno(stmt),
                 mysql_stmt_error(stmt), mysql_stmt_sqlstate(stmt), ds);
    goto end;
  }

  /* If we got here the statement was both executed and read successfully */
  handle_no_error(command);
  if (!disable_result_log)
  {
    /*
      Not all statements creates a result set. If there is one we can
      now create another normal result set that contains the meta
      data. This set can be handled almost like any other non prepared
      statement result set.
    */
    if ((res= mysql_stmt_result_metadata(stmt)) != NULL)
    {
      /* Take the column count from meta info */
      MYSQL_FIELD *fields= mysql_fetch_fields(res);
      uint num_fields= mysql_num_fields(res);

      if (display_metadata)
        append_metadata(ds, fields, num_fields);

      if (!display_result_vertically)
        append_table_headings(ds, fields, num_fields);

      append_stmt_result(ds, stmt, fields, num_fields);

      mysql_free_result(res);     /* Free normal result set with meta data */

      /* Clear prepare warnings */
      dynstr_set(&ds_prepare_warnings, NULL);
    }
    else
    {
      /*
	This is a query without resultset
      */
    }

    if (!disable_warnings)
    {
      /* Get the warnings from execute */

      /* Append warnings to ds - if there are any */
      if (append_warnings(&ds_execute_warnings, mysql) ||
          ds_execute_warnings.length ||
          ds_prepare_warnings.length ||
          ds_warnings->length)
      {
        dynstr_append_mem(ds, "Warnings:\n", 10);
	if (ds_warnings->length)
	  dynstr_append_mem(ds, ds_warnings->str,
			    ds_warnings->length);
	if (ds_prepare_warnings.length)
	  dynstr_append_mem(ds, ds_prepare_warnings.str,
			    ds_prepare_warnings.length);
	if (ds_execute_warnings.length)
	  dynstr_append_mem(ds, ds_execute_warnings.str,
			    ds_execute_warnings.length);
      }
    }

    if (!disable_info)
      append_info(ds, mysql_affected_rows(mysql), mysql_info(mysql));

  }

end:
  if (!disable_warnings)
  {
    dynstr_free(&ds_prepare_warnings);
    dynstr_free(&ds_execute_warnings);
  }


  /* Close the statement if - no reconnect, need new prepare */
  if (mysql->reconnect)
  {
    mysql_stmt_close(stmt);
    cur_con->stmt= NULL;
  }

  /*
    We save the return code (mysql_stmt_errno(stmt)) from the last call sent
    to the server into the mysqltest builtin variable $mysql_errno. This
    variable then can be used from the test case itself.
  */

  var_set_errno(mysql_stmt_errno(stmt));

  DBUG_VOID_RETURN;
}



/*
  Create a util connection if one does not already exists
  and use that to run the query
  This is done to avoid implict commit when creating/dropping objects such
  as view, sp etc.
*/

int util_query(MYSQL* org_mysql, const char* query){

  MYSQL* mysql;
  DBUG_ENTER("util_query");

  if(!(mysql= cur_con->util_mysql))
  {
    DBUG_PRINT("info", ("Creating util_mysql"));
    if (!(mysql= mysql_init(mysql)))
      die("Failed in mysql_init()");

    safe_connect(mysql, "util", org_mysql->host, org_mysql->user,
                 org_mysql->passwd, org_mysql->db, org_mysql->port,
                 org_mysql->unix_socket);

    cur_con->util_mysql= mysql;
  }

  return mysql_query(mysql, query);
}



/*
  Run query

  SYNPOSIS
    run_query()
     mysql	mysql handle
     command	currrent command pointer

  flags control the phased/stages of query execution to be performed
  if QUERY_SEND_FLAG bit is on, the query will be sent. If QUERY_REAP_FLAG
  is on the result will be read - for regular query, both bits must be on
*/

void run_query(struct st_connection *cn, struct st_command *command, int flags)
{
  MYSQL *mysql= &cn->mysql;
  DYNAMIC_STRING *ds;
  DYNAMIC_STRING *save_ds= NULL;
  DYNAMIC_STRING ds_result;
  DYNAMIC_STRING ds_sorted;
  DYNAMIC_STRING ds_warnings;
  DYNAMIC_STRING eval_query;
  char *query;
  int query_len;
  my_bool view_created= 0, sp_created= 0;
  my_bool complete_query= ((flags & QUERY_SEND_FLAG) &&
                           (flags & QUERY_REAP_FLAG));
  DBUG_ENTER("run_query");

  init_dynamic_string(&ds_warnings, NULL, 0, 256);

  /* Scan for warning before sendign to server */
  scan_command_for_warnings(command);

  /*
    Evaluate query if this is an eval command
  */
  if (command->type == Q_EVAL)
  {
    init_dynamic_string(&eval_query, "", command->query_len+256, 1024);
    do_eval(&eval_query, command->query, command->end, FALSE);
    query = eval_query.str;
    query_len = eval_query.length;
  }
  else
  {
    query = command->query;
    query_len = strlen(query);
  }

  /*
    When command->require_file is set the output of _this_ query
    should be compared with an already existing file
    Create a temporary dynamic string to contain the output from
    this query.
  */
  if (command->require_file[0])
  {
    init_dynamic_string(&ds_result, "", 1024, 1024);
    ds= &ds_result;
  }
  else
    ds= &ds_res;

  /*
    Log the query into the output buffer
  */
  if (!disable_query_log && (flags & QUERY_SEND_FLAG))
  {
    replace_dynstr_append_mem(ds, query, query_len);
    dynstr_append_mem(ds, delimiter, delimiter_length);
    dynstr_append_mem(ds, "\n", 1);
  }

  if (view_protocol_enabled &&
      complete_query &&
      match_re(&view_re, query))
  {
    /*
      Create the query as a view.
      Use replace since view can exist from a failed mysqltest run
    */
    DYNAMIC_STRING query_str;
    init_dynamic_string(&query_str,
			"CREATE OR REPLACE VIEW mysqltest_tmp_v AS ",
			query_len+64, 256);
    dynstr_append_mem(&query_str, query, query_len);
    if (util_query(mysql, query_str.str))
    {
      /*
	Failed to create the view, this is not fatal
	just run the query the normal way
      */
      DBUG_PRINT("view_create_error",
		 ("Failed to create view '%s': %d: %s", query_str.str,
		  mysql_errno(mysql), mysql_error(mysql)));

      /* Log error to create view */
      verbose_msg("Failed to create view '%s' %d: %s", query_str.str,
		  mysql_errno(mysql), mysql_error(mysql));
    }
    else
    {
      /*
	Yes, it was possible to create this query as a view
      */
      view_created= 1;
      query= (char*)"SELECT * FROM mysqltest_tmp_v";
      query_len = strlen(query);

      /*
        Collect warnings from create of the view that should otherwise
        have been produced when the SELECT was executed
      */
      append_warnings(&ds_warnings, cur_con->util_mysql);
    }

    dynstr_free(&query_str);

  }

  if (sp_protocol_enabled &&
      complete_query &&
      match_re(&sp_re, query))
  {
    /*
      Create the query as a stored procedure
      Drop first since sp can exist from a failed mysqltest run
    */
    DYNAMIC_STRING query_str;
    init_dynamic_string(&query_str,
			"DROP PROCEDURE IF EXISTS mysqltest_tmp_sp;",
			query_len+64, 256);
    util_query(mysql, query_str.str);
    dynstr_set(&query_str, "CREATE PROCEDURE mysqltest_tmp_sp()\n");
    dynstr_append_mem(&query_str, query, query_len);
    if (util_query(mysql, query_str.str))
    {
      /*
	Failed to create the stored procedure for this query,
	this is not fatal just run the query the normal way
      */
      DBUG_PRINT("sp_create_error",
		 ("Failed to create sp '%s': %d: %s", query_str.str,
		  mysql_errno(mysql), mysql_error(mysql)));

      /* Log error to create sp */
      verbose_msg("Failed to create sp '%s' %d: %s", query_str.str,
		  mysql_errno(mysql), mysql_error(mysql));

    }
    else
    {
      sp_created= 1;

      query= (char*)"CALL mysqltest_tmp_sp()";
      query_len = strlen(query);
    }
    dynstr_free(&query_str);
  }

  if (display_result_sorted)
  {
    /*
       Collect the query output in a separate string
       that can be sorted before it's added to the
       global result string
    */
    init_dynamic_string(&ds_sorted, "", 1024, 1024);
    save_ds= ds; /* Remember original ds */
    ds= &ds_sorted;
  }

  /*
    Find out how to run this query

    Always run with normal C API if it's not a complete
    SEND + REAP

    If it is a '?' in the query it may be a SQL level prepared
    statement already and we can't do it twice
  */
  if (ps_protocol_enabled &&
      complete_query &&
      match_re(&ps_re, query))
    run_query_stmt(mysql, command, query, query_len, ds, &ds_warnings);
  else
    run_query_normal(cn, command, flags, query, query_len,
		     ds, &ds_warnings);

  if (display_result_sorted)
  {
    /* Sort the result set and append it to result */
    dynstr_append_sorted(save_ds, &ds_sorted);
    ds= save_ds;
    dynstr_free(&ds_sorted);
  }

  if (sp_created)
  {
    if (util_query(mysql, "DROP PROCEDURE mysqltest_tmp_sp "))
      die("Failed to drop sp: %d: %s", mysql_errno(mysql), mysql_error(mysql));
  }

  if (view_created)
  {
    if (util_query(mysql, "DROP VIEW mysqltest_tmp_v "))
      die("Failed to drop view: %d: %s",
	  mysql_errno(mysql), mysql_error(mysql));
  }

  if (command->require_file[0])
  {
    /* A result file was specified for _this_ query
       and the output should be checked against an already
       existing file which has been specified using --require or --result
    */
    check_require(ds, command->require_file);
  }

  dynstr_free(&ds_warnings);
  if (ds == &ds_result)
    dynstr_free(&ds_result);
  if (command->type == Q_EVAL)
    dynstr_free(&eval_query);
  DBUG_VOID_RETURN;
}

/****************************************************************************/
/*
  Functions to detect different SQL statements
*/

char *re_eprint(int err)
{
  static char epbuf[100];
  size_t len= my_regerror(REG_ITOA|err, (my_regex_t *)NULL,
			  epbuf, sizeof(epbuf));
  assert(len <= sizeof(epbuf));
  return(epbuf);
}

void init_re_comp(my_regex_t *re, const char* str)
{
  int err= my_regcomp(re, str, (REG_EXTENDED | REG_ICASE | REG_NOSUB),
                      &my_charset_latin1);
  if (err)
  {
    char erbuf[100];
    int len= my_regerror(err, re, erbuf, sizeof(erbuf));
    die("error %s, %d/%d `%s'\n",
	re_eprint(err), len, (int)sizeof(erbuf), erbuf);
  }
}

void init_re(void)
{
  /*
    Filter for queries that can be run using the
    MySQL Prepared Statements C API
  */
  const char *ps_re_str =
    "^("
    "[[:space:]]*REPLACE[[:space:]]|"
    "[[:space:]]*INSERT[[:space:]]|"
    "[[:space:]]*UPDATE[[:space:]]|"
    "[[:space:]]*DELETE[[:space:]]|"
    "[[:space:]]*SELECT[[:space:]]|"
    "[[:space:]]*CREATE[[:space:]]+TABLE[[:space:]]|"
    "[[:space:]]*DO[[:space:]]|"
    "[[:space:]]*SET[[:space:]]+OPTION[[:space:]]|"
    "[[:space:]]*DELETE[[:space:]]+MULTI[[:space:]]|"
    "[[:space:]]*UPDATE[[:space:]]+MULTI[[:space:]]|"
    "[[:space:]]*INSERT[[:space:]]+SELECT[[:space:]])";

  /*
    Filter for queries that can be run using the
    Stored procedures
  */
  const char *sp_re_str =ps_re_str;

  /*
    Filter for queries that can be run as views
  */
  const char *view_re_str =
    "^("
    "[[:space:]]*SELECT[[:space:]])";

  init_re_comp(&ps_re, ps_re_str);
  init_re_comp(&sp_re, sp_re_str);
  init_re_comp(&view_re, view_re_str);
}


int match_re(my_regex_t *re, char *str)
{
  int err= my_regexec(re, str, (size_t)0, NULL, 0);

  if (err == 0)
    return 1;
  else if (err == REG_NOMATCH)
    return 0;

  {
    char erbuf[100];
    int len= my_regerror(err, re, erbuf, sizeof(erbuf));
    die("error %s, %d/%d `%s'\n",
	re_eprint(err), len, (int)sizeof(erbuf), erbuf);
  }
  return 0;
}

void free_re(void)
{
  my_regfree(&ps_re);
  my_regfree(&sp_re);
  my_regfree(&view_re);
  my_regex_end();
}

/****************************************************************************/

void get_command_type(struct st_command* command)
{
  char save;
  uint type;
  DBUG_ENTER("get_command_type");

  if (*command->query == '}')
  {
    command->type = Q_END_BLOCK;
    DBUG_VOID_RETURN;
  }

  save= command->query[command->first_word_len];
  command->query[command->first_word_len]= 0;
  type= find_type(command->query, &command_typelib, 1+2);
  command->query[command->first_word_len]= save;
  if (type > 0)
  {
    command->type=(enum enum_commands) type;		/* Found command */

    /*
      Look for case where "query" was explicitly specified to
      force command being sent to server
    */
    if (type == Q_QUERY)
    {
      /* Skip the "query" part */
      command->query= command->first_argument;
    }
  }
  else
  {
    /* No mysqltest command matched */

    if (command->type != Q_COMMENT_WITH_COMMAND)
    {
      /* A query that will sent to mysqld */
      command->type= Q_QUERY;
    }
    else
    {
      /* -- comment that didn't contain a mysqltest command */
      command->type= Q_COMMENT;
      warning_msg("Suspicious command '--%s' detected, was this intentional? "\
                  "Use # instead of -- to avoid this warning",
                  command->query);

      if (command->first_word_len &&
          strcmp(command->query + command->first_word_len - 1, delimiter) == 0)
      {
        /*
          Detect comment with command using extra delimiter
          Ex --disable_query_log;
          ^ Extra delimiter causing the command
          to be skipped
        */
        save= command->query[command->first_word_len-1];
        command->query[command->first_word_len-1]= 0;
        if (find_type(command->query, &command_typelib, 1+2) > 0)
          die("Extra delimiter \";\" found");
        command->query[command->first_word_len-1]= save;

      }
    }
  }

  /* Set expected error on command */
  memcpy(&command->expected_errors, &saved_expected_errors,
         sizeof(saved_expected_errors));
  DBUG_PRINT("info", ("There are %d expected errors",
                      command->expected_errors.count));
  command->abort_on_error= (command->expected_errors.count == 0 &&
                            abort_on_error);

  DBUG_VOID_RETURN;
}



/*
  Record how many milliseconds it took to execute the test file
  up until the current line and save it in the dynamic string ds_progress.

  The ds_progress will be dumped to <test_name>.progress when
  test run completes

*/

void mark_progress(struct st_command* command __attribute__((unused)),
                   int line)
{
  char buf[32], *end;
  ulonglong timer= timer_now();
  if (!progress_start)
    progress_start= timer;
  timer-= progress_start;

  /* Milliseconds since start */
  end= longlong2str(timer, buf, 10);
  dynstr_append_mem(&ds_progress, buf, (int)(end-buf));
  dynstr_append_mem(&ds_progress, "\t", 1);

  /* Parser line number */
  end= int10_to_str(line, buf, 10);
  dynstr_append_mem(&ds_progress, buf, (int)(end-buf));
  dynstr_append_mem(&ds_progress, "\t", 1);

  /* Filename */
  dynstr_append(&ds_progress, cur_file->file_name);
  dynstr_append_mem(&ds_progress, ":", 1);

  /* Line in file */
  end= int10_to_str(cur_file->lineno, buf, 10);
  dynstr_append_mem(&ds_progress, buf, (int)(end-buf));


  dynstr_append_mem(&ds_progress, "\n", 1);

}


int main(int argc, char **argv)
{
  struct st_command *command;
  my_bool q_send_flag= 0, abort_flag= 0;
  uint command_executed= 0, last_command_executed= 0;
  char save_file[FN_REFLEN];
  MY_STAT res_info;
  MY_INIT(argv[0]);

  save_file[0]= 0;
  TMPDIR[0]= 0;

  /* Init expected errors */
  memset(&saved_expected_errors, 0, sizeof(saved_expected_errors));

  /* Init connections */
  memset(connections, 0, sizeof(connections));
  connections_end= connections +
    (sizeof(connections)/sizeof(struct st_connection)) - 1;
  next_con= connections + 1;
  cur_con= connections;

  /* Init file stack */
  memset(file_stack, 0, sizeof(file_stack));
  file_stack_end=
    file_stack + (sizeof(file_stack)/sizeof(struct st_test_file)) - 1;
  cur_file= file_stack;

  /* Init block stack */
  memset(block_stack, 0, sizeof(block_stack));
  block_stack_end=
    block_stack + (sizeof(block_stack)/sizeof(struct st_block)) - 1;
  cur_block= block_stack;
  cur_block->ok= TRUE; /* Outer block should always be executed */
  cur_block->cmd= cmd_none;

  my_init_dynamic_array(&q_lines, sizeof(struct st_command*), 1024, 1024);

  if (hash_init(&var_hash, charset_info,
                1024, 0, 0, get_var_key, var_free, MYF(0)))
    die("Variable hash initialization failed");

  var_set_string("$MYSQL_SERVER_VERSION", MYSQL_SERVER_VERSION);

  memset(&master_pos, 0, sizeof(master_pos));

  parser.current_line= parser.read_lines= 0;
  memset(&var_reg, 0, sizeof(var_reg));

  init_builtin_echo();
#ifdef __WIN__
#ifndef USE_CYGWIN
  is_windows= 1;
#endif
  init_tmp_sh_file();
  init_win_path_patterns();
#endif

  init_dynamic_string(&ds_res, "", 65536, 65536);
  init_dynamic_string(&ds_progress, "", 0, 2048);
  init_dynamic_string(&ds_warning_messages, "", 0, 2048);
  parse_args(argc, argv);

  var_set_int("$PS_PROTOCOL", ps_protocol);
  var_set_int("$SP_PROTOCOL", sp_protocol);
  var_set_int("$VIEW_PROTOCOL", view_protocol);
  var_set_int("$CURSOR_PROTOCOL", cursor_protocol);

  DBUG_PRINT("info",("result_file: '%s'",
                     result_file_name ? result_file_name : ""));
  if (mysql_server_init(embedded_server_arg_count,
			embedded_server_args,
			(char**) embedded_server_groups))
    die("Can't initialize MySQL server");
  server_initialized= 1;
  if (cur_file == file_stack && cur_file->file == 0)
  {
    cur_file->file= stdin;
    cur_file->file_name= my_strdup("<stdin>", MYF(MY_WME));
    cur_file->lineno= 1;
  }
  init_re();
  ps_protocol_enabled= ps_protocol;
  sp_protocol_enabled= sp_protocol;
  view_protocol_enabled= view_protocol;
  cursor_protocol_enabled= cursor_protocol;
  /* Cursor protcol implies ps protocol */
  if (cursor_protocol_enabled)
    ps_protocol_enabled= 1;

  if (!( mysql_init(&cur_con->mysql)))
    die("Failed in mysql_init()");
  if (opt_compress)
    mysql_options(&cur_con->mysql,MYSQL_OPT_COMPRESS,NullS);
  mysql_options(&cur_con->mysql, MYSQL_OPT_LOCAL_INFILE, 0);
  mysql_options(&cur_con->mysql, MYSQL_SET_CHARSET_NAME,
                charset_info->csname);
  if (opt_charsets_dir)
    mysql_options(&cur_con->mysql, MYSQL_SET_CHARSET_DIR,
                  opt_charsets_dir);

#ifdef HAVE_OPENSSL

  if (opt_use_ssl)
  {
    mysql_ssl_set(&cur_con->mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
#if MYSQL_VERSION_ID >= 50000
    /* Turn on ssl_verify_server_cert only if host is "localhost" */
    opt_ssl_verify_server_cert= opt_host && !strcmp(opt_host, "localhost");
    mysql_options(&cur_con->mysql, MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
                  &opt_ssl_verify_server_cert);
#endif
  }
#endif

  if (!(cur_con->name = my_strdup("default", MYF(MY_WME))))
    die("Out of memory");

  safe_connect(&cur_con->mysql, cur_con->name, opt_host, opt_user, opt_pass,
               opt_db, opt_port, unix_sock);

  /* Use all time until exit if no explicit 'start_timer' */
  timer_start= timer_now();

  /*
    Initialize $mysql_errno with -1, so we can
    - distinguish it from valid values ( >= 0 ) and
    - detect if there was never a command sent to the server
  */
  var_set_errno(-1);

  if (opt_include)
  {
    open_file(opt_include);
  }

  while (!read_command(&command) && !abort_flag)
  {
    int current_line_inc = 1, processed = 0;
    if (command->type == Q_UNKNOWN || command->type == Q_COMMENT_WITH_COMMAND)
      get_command_type(command);

    if (parsing_disabled &&
        command->type != Q_ENABLE_PARSING &&
        command->type != Q_DISABLE_PARSING)
    {
      command->type= Q_COMMENT;
      scan_command_for_warnings(command);
    }

    if (cur_block->ok)
    {
      command->last_argument= command->first_argument;
      processed = 1;
      switch (command->type) {
      case Q_CONNECT:
        do_connect(command);
        break;
      case Q_CONNECTION: select_connection(command); break;
      case Q_DISCONNECT:
      case Q_DIRTY_CLOSE:
	do_close_connection(command); break;
      case Q_RPL_PROBE: do_rpl_probe(command); break;
      case Q_ENABLE_RPL_PARSE:	 do_enable_rpl_parse(command); break;
      case Q_DISABLE_RPL_PARSE:  do_disable_rpl_parse(command); break;
      case Q_ENABLE_QUERY_LOG:   disable_query_log=0; break;
      case Q_DISABLE_QUERY_LOG:  disable_query_log=1; break;
      case Q_ENABLE_ABORT_ON_ERROR:  abort_on_error=1; break;
      case Q_DISABLE_ABORT_ON_ERROR: abort_on_error=0; break;
      case Q_ENABLE_RESULT_LOG:  disable_result_log=0; break;
      case Q_DISABLE_RESULT_LOG: disable_result_log=1; break;
      case Q_ENABLE_WARNINGS:    disable_warnings=0; break;
      case Q_DISABLE_WARNINGS:   disable_warnings=1; break;
      case Q_ENABLE_INFO:        disable_info=0; break;
      case Q_DISABLE_INFO:       disable_info=1; break;
      case Q_ENABLE_METADATA:    display_metadata=1; break;
      case Q_DISABLE_METADATA:   display_metadata=0; break;
      case Q_SOURCE: do_source(command); break;
      case Q_SLEEP: do_sleep(command, 0); break;
      case Q_REAL_SLEEP: do_sleep(command, 1); break;
      case Q_WAIT_FOR_SLAVE_TO_STOP: do_wait_for_slave_to_stop(command); break;
      case Q_INC: do_modify_var(command, DO_INC); break;
      case Q_DEC: do_modify_var(command, DO_DEC); break;
      case Q_ECHO: do_echo(command); command_executed++; break;
      case Q_SYSTEM: do_system(command); break;
      case Q_REMOVE_FILE: do_remove_file(command); break;
      case Q_FILE_EXIST: do_file_exist(command); break;
      case Q_WRITE_FILE: do_write_file(command); break;
      case Q_APPEND_FILE: do_append_file(command); break;
      case Q_DIFF_FILES: do_diff_files(command); break;
      case Q_CAT_FILE: do_cat_file(command); break;
      case Q_COPY_FILE: do_copy_file(command); break;
      case Q_CHMOD_FILE: do_chmod_file(command); break;
      case Q_PERL: do_perl(command); break;
      case Q_DELIMITER:
        do_delimiter(command);
	break;
      case Q_DISPLAY_VERTICAL_RESULTS:
        display_result_vertically= TRUE;
        break;
      case Q_DISPLAY_HORIZONTAL_RESULTS:
	display_result_vertically= FALSE;
        break;
      case Q_SORTED_RESULT:
        /*
          Turn on sorting of result set, will be reset after next
          command
        */
	display_result_sorted= TRUE;
        break;
      case Q_LET: do_let(command); break;
      case Q_EVAL_RESULT:
        eval_result = 1; break;
      case Q_EVAL:
      case Q_QUERY_VERTICAL:
      case Q_QUERY_HORIZONTAL:
	if (command->query == command->query_buf)
        {
          /* Skip the first part of command, i.e query_xxx */
	  command->query= command->first_argument;
          command->first_word_len= 0;
        }
	/* fall through */
      case Q_QUERY:
      case Q_REAP:
      {
	my_bool old_display_result_vertically= display_result_vertically;
        /* Default is full query, both reap and send  */
        int flags= QUERY_REAP_FLAG | QUERY_SEND_FLAG;

        if (q_send_flag)
        {
          /* Last command was an empty 'send' */
          flags= QUERY_SEND_FLAG;
          q_send_flag= 0;
        }
        else if (command->type == Q_REAP)
        {
          flags= QUERY_REAP_FLAG;
        }

        /* Check for special property for this query */
        display_result_vertically|= (command->type == Q_QUERY_VERTICAL);

	if (save_file[0])
	{
	  strmake(command->require_file, save_file, sizeof(save_file));
	  save_file[0]= 0;
	}
	run_query(cur_con, command, flags);
	command_executed++;
        command->last_argument= command->end;

        /* Restore settings */
	display_result_vertically= old_display_result_vertically;

	break;
      }
      case Q_SEND:
        if (!*command->first_argument)
        {
          /*
            This is a send without arguments, it indicates that _next_ query
            should be send only
          */
          q_send_flag= 1;
          break;
        }

        /* Remove "send" if this is first iteration */
	if (command->query == command->query_buf)
	  command->query= command->first_argument;

	/*
	  run_query() can execute a query partially, depending on the flags.
	  QUERY_SEND_FLAG flag without QUERY_REAP_FLAG tells it to just send
          the query and read the result some time later when reap instruction
	  is given on this connection.
        */
	run_query(cur_con, command, QUERY_SEND_FLAG);
	command_executed++;
        command->last_argument= command->end;
	break;
      case Q_REQUIRE:
	do_get_file_name(command, save_file, sizeof(save_file));
	break;
      case Q_ERROR:
        do_get_errcodes(command);
	break;
      case Q_REPLACE:
	do_get_replace(command);
	break;
      case Q_REPLACE_REGEX:
        do_get_replace_regex(command);
        break;
      case Q_REPLACE_COLUMN:
	do_get_replace_column(command);
	break;
      case Q_SAVE_MASTER_POS: do_save_master_pos(); break;
      case Q_SYNC_WITH_MASTER: do_sync_with_master(command); break;
      case Q_SYNC_SLAVE_WITH_MASTER:
      {
	do_save_master_pos();
	if (*command->first_argument)
	  select_connection(command);
	else
	  select_connection_name("slave");
	do_sync_with_master2(0);
	break;
      }
      case Q_COMMENT:				/* Ignore row */
        command->last_argument= command->end;
	break;
      case Q_PING:
	(void) mysql_ping(&cur_con->mysql);
	break;
      case Q_EXEC:
	do_exec(command);
	command_executed++;
	break;
      case Q_START_TIMER:
	/* Overwrite possible earlier start of timer */
	timer_start= timer_now();
	break;
      case Q_END_TIMER:
	/* End timer before ending mysqltest */
	timer_output();
	break;
      case Q_CHARACTER_SET:
	do_set_charset(command);
	break;
      case Q_DISABLE_PS_PROTOCOL:
        ps_protocol_enabled= 0;
        /* Close any open statements */
        close_statements();
        break;
      case Q_ENABLE_PS_PROTOCOL:
        ps_protocol_enabled= ps_protocol;
        break;
      case Q_DISABLE_RECONNECT:
        set_reconnect(&cur_con->mysql, 0);
        break;
      case Q_ENABLE_RECONNECT:
        set_reconnect(&cur_con->mysql, 1);
        /* Close any open statements - no reconnect, need new prepare */
        close_statements();
        break;
      case Q_DISABLE_PARSING:
        if (parsing_disabled == 0)
          parsing_disabled= 1;
        else
          die("Parsing is already disabled");
        break;
      case Q_ENABLE_PARSING:
        /*
          Ensure we don't get parsing_disabled < 0 as this would accidentally
          disable code we don't want to have disabled
        */
        if (parsing_disabled == 1)
          parsing_disabled= 0;
        else
          die("Parsing is already enabled");
        break;
      case Q_DIE:
        /* Abort test with error code and error message */
        die("%s", command->first_argument);
        break;
      case Q_EXIT:
        /* Stop processing any more commands */
        abort_flag= 1;
        break;
      case Q_SKIP:
        abort_not_supported_test("%s", command->first_argument);
        break;

      case Q_RESULT:
        die("result, deprecated command");
        break;

      default:
        processed= 0;
        break;
      }
    }

    if (!processed)
    {
      current_line_inc= 0;
      switch (command->type) {
      case Q_WHILE: do_block(cmd_while, command); break;
      case Q_IF: do_block(cmd_if, command); break;
      case Q_END_BLOCK: do_done(command); break;
      default: current_line_inc = 1; break;
      }
    }
    else
      check_eol_junk(command->last_argument);

    if (command->type != Q_ERROR &&
        command->type != Q_COMMENT)
    {
      /*
        As soon as any non "error" command or comment has been executed,
        the array with expected errors should be cleared
      */
      memset(&saved_expected_errors, 0, sizeof(saved_expected_errors));
    }

    if (command_executed != last_command_executed)
    {
      /*
        As soon as any command has been executed,
        the replace structures should be cleared
      */
      free_all_replace();

      /* Also reset "sorted_result" */
      display_result_sorted= FALSE;
    }
    last_command_executed= command_executed;

    parser.current_line += current_line_inc;
    if ( opt_mark_progress )
      mark_progress(command, parser.current_line);
  }

  start_lineno= 0;

  if (parsing_disabled)
    die("Test ended with parsing disabled");

  /*
    The whole test has been executed _sucessfully_.
    Time to compare result or save it to record file.
    The entire output from test is now kept in ds_res.
  */
  if (ds_res.length)
  {
    if (result_file_name)
    {
      /* A result file has been specified */

      if (record)
      {
	/* Recording - dump the output from test to result file */
	str_to_file(result_file_name, ds_res.str, ds_res.length);
      }
      else
      {
	/* Check that the output from test is equal to result file
	   - detect missing result file
	   - detect zero size result file
        */
	check_result(&ds_res);
      }
    }
    else
    {
      /* No result_file_name specified to compare with, print to stdout */
      printf("%s", ds_res.str);
    }
  }
  else
  {
    die("The test didn't produce any output");
  }

  if (!command_executed &&
      result_file_name && my_stat(result_file_name, &res_info, 0))
  {
    /*
      my_stat() successful on result file. Check if we have not run a
      single query, but we do have a result file that contains data.
      Note that we don't care, if my_stat() fails. For example, for a
      non-existing or non-readable file, we assume it's fine to have
      no query output from the test file, e.g. regarded as no error.
    */
    die("No queries executed but result file found!");
  }

  if ( opt_mark_progress && result_file_name )
    dump_progress();

  /* Dump warning messages */
  if (result_file_name && ds_warning_messages.length)
    dump_warning_messages();

  timer_output();
  /* Yes, if we got this far the test has suceeded! Sakila smiles */
  cleanup_and_exit(0);
  return 0; /* Keep compiler happy too */
}


/*
  A primitive timer that give results in milliseconds if the
  --timer-file=<filename> is given. The timer result is written
  to that file when the result is available. To not confuse
  mysql-test-run with an old obsolete result, we remove the file
  before executing any commands. The time we measure is

  - If no explicit 'start_timer' or 'end_timer' is given in the
  test case, the timer measure how long we execute in mysqltest.

  - If only 'start_timer' is given we measure how long we execute
  from that point until we terminate mysqltest.

  - If only 'end_timer' is given we measure how long we execute
  from that we enter mysqltest to the 'end_timer' is command is
  executed.

  - If both 'start_timer' and 'end_timer' are given we measure
  the time between executing the two commands.
*/

void timer_output(void)
{
  if (timer_file)
  {
    char buf[32], *end;
    ulonglong timer= timer_now() - timer_start;
    end= longlong2str(timer, buf, 10);
    str_to_file(timer_file,buf, (int) (end-buf));
    /* Timer has been written to the file, don't use it anymore */
    timer_file= 0;
  }
}


ulonglong timer_now(void)
{
  return my_getsystime() / 10000;
}


/*
  Get arguments for replace_columns. The syntax is:
  replace-column column_number to_string [column_number to_string ...]
  Where each argument may be quoted with ' or "
  A argument may also be a variable, in which case the value of the
  variable is replaced.
*/

void do_get_replace_column(struct st_command *command)
{
  char *from= command->first_argument;
  char *buff, *start;
  DBUG_ENTER("get_replace_columns");

  free_replace_column();
  if (!*from)
    die("Missing argument in %s", command->query);

  /* Allocate a buffer for results */
  start= buff= my_malloc(strlen(from)+1,MYF(MY_WME | MY_FAE));
  while (*from)
  {
    char *to;
    uint column_number;

    to= get_string(&buff, &from, command);
    if (!(column_number= atoi(to)) || column_number > MAX_COLUMNS)
      die("Wrong column number to replace_column in '%s'", command->query);
    if (!*from)
      die("Wrong number of arguments to replace_column in '%s'", command->query);
    to= get_string(&buff, &from, command);
    my_free(replace_column[column_number-1], MY_ALLOW_ZERO_PTR);
    replace_column[column_number-1]= my_strdup(to, MYF(MY_WME | MY_FAE));
    set_if_bigger(max_replace_column, column_number);
  }
  my_free(start, MYF(0));
  command->last_argument= command->end;
}


void free_replace_column()
{
  uint i;
  for (i=0 ; i < max_replace_column ; i++)
  {
    if (replace_column[i])
    {
      my_free(replace_column[i], 0);
      replace_column[i]= 0;
    }
  }
  max_replace_column= 0;
}


/****************************************************************************/
/*
  Replace functions
*/

/* Definitions for replace result */

typedef struct st_pointer_array {		/* when using array-strings */
  TYPELIB typelib;				/* Pointer to strings */
  byte	*str;					/* Strings is here */
  int7	*flag;					/* Flag about each var. */
  uint	array_allocs,max_count,length,max_length;
} POINTER_ARRAY;

struct st_replace;
struct st_replace *init_replace(my_string *from, my_string *to, uint count,
				my_string word_end_chars);
int insert_pointer_name(reg1 POINTER_ARRAY *pa,my_string name);
void replace_strings_append(struct st_replace *rep, DYNAMIC_STRING* ds,
                            const char *from, int len);
void free_pointer_array(POINTER_ARRAY *pa);

struct st_replace *glob_replace;

/*
  Get arguments for replace. The syntax is:
  replace from to [from to ...]
  Where each argument may be quoted with ' or "
  A argument may also be a variable, in which case the value of the
  variable is replaced.
*/

void do_get_replace(struct st_command *command)
{
  uint i;
  char *from= command->first_argument;
  char *buff, *start;
  char word_end_chars[256], *pos;
  POINTER_ARRAY to_array, from_array;
  DBUG_ENTER("get_replace");

  free_replace();

  bzero((char*) &to_array,sizeof(to_array));
  bzero((char*) &from_array,sizeof(from_array));
  if (!*from)
    die("Missing argument in %s", command->query);
  start= buff= my_malloc(strlen(from)+1,MYF(MY_WME | MY_FAE));
  while (*from)
  {
    char *to= buff;
    to= get_string(&buff, &from, command);
    if (!*from)
      die("Wrong number of arguments to replace_result in '%s'",
          command->query);
    insert_pointer_name(&from_array,to);
    to= get_string(&buff, &from, command);
    insert_pointer_name(&to_array,to);
  }
  for (i= 1,pos= word_end_chars ; i < 256 ; i++)
    if (my_isspace(charset_info,i))
      *pos++= i;
  *pos=0;					/* End pointer */
  if (!(glob_replace= init_replace((char**) from_array.typelib.type_names,
				  (char**) to_array.typelib.type_names,
				  (uint) from_array.typelib.count,
				  word_end_chars)))
    die("Can't initialize replace from '%s'", command->query);
  free_pointer_array(&from_array);
  free_pointer_array(&to_array);
  my_free(start, MYF(0));
  command->last_argument= command->end;
  DBUG_VOID_RETURN;
}


void free_replace()
{
  DBUG_ENTER("free_replace");
  if (glob_replace)
  {
    my_free((char*) glob_replace,MYF(0));
    glob_replace=0;
  }
  DBUG_VOID_RETURN;
}


typedef struct st_replace {
  bool	 found;
  struct st_replace *next[256];
} REPLACE;

typedef struct st_replace_found {
  bool found;
  char *replace_string;
  uint to_offset;
  int from_offset;
} REPLACE_STRING;


void replace_strings_append(REPLACE *rep, DYNAMIC_STRING* ds,
                            const char *str,
                            int len __attribute__((unused)))
{
  reg1 REPLACE *rep_pos;
  reg2 REPLACE_STRING *rep_str;
  const char *start, *from;
  DBUG_ENTER("replace_strings_append");

  start= from= str;
  rep_pos=rep+1;
  for (;;)
  {
    /* Loop through states */
    DBUG_PRINT("info", ("Looping through states"));
    while (!rep_pos->found)
      rep_pos= rep_pos->next[(uchar) *from++];

    /* Does this state contain a string to be replaced */
    if (!(rep_str = ((REPLACE_STRING*) rep_pos))->replace_string)
    {
      /* No match found */
      dynstr_append_mem(ds, start, from - start - 1);
      DBUG_PRINT("exit", ("Found no more string to replace, appended: %s", start));
      DBUG_VOID_RETURN;
    }

    /* Found a string that needs to be replaced */
    DBUG_PRINT("info", ("found: %d, to_offset: %d, from_offset: %d, string: %s",
                        rep_str->found, rep_str->to_offset,
                        rep_str->from_offset, rep_str->replace_string));

    /* Append part of original string before replace string */
    dynstr_append_mem(ds, start, (from - rep_str->to_offset) - start);

    /* Append replace string */
    dynstr_append_mem(ds, rep_str->replace_string,
                      strlen(rep_str->replace_string));

    if (!*(from-=rep_str->from_offset) && rep_pos->found != 2)
    {
      /* End of from string */
      DBUG_PRINT("exit", ("Found end of from string"));
      DBUG_VOID_RETURN;
    }
    DBUG_ASSERT(from <= str+len);
    start= from;
    rep_pos=rep;
  }
}


/*
  Regex replace  functions
*/


/* Stores regex substitutions */

struct st_regex
{
  char* pattern; /* Pattern to be replaced */
  char* replace; /* String or expression to replace the pattern with */
  int icase; /* true if the match is case insensitive */
};

struct st_replace_regex
{
  DYNAMIC_ARRAY regex_arr; /* stores a list of st_regex subsitutions */

  /*
    Temporary storage areas for substitutions. To reduce unnessary copying
    and memory freeing/allocation, we pre-allocate two buffers, and alternate
    their use, one for input/one for output, the roles changing on the next
    st_regex substition. At the end of substitutions  buf points to the
    one containing the final result.
  */
  char* buf;
  char* even_buf;
  char* odd_buf;
  int even_buf_len;
  int odd_buf_len;
};

struct st_replace_regex *glob_replace_regex= 0;

int reg_replace(char** buf_p, int* buf_len_p, char *pattern, char *replace,
                char *string, int icase);



/*
  Finds the next (non-escaped) '/' in the expression.
  (If the character '/' is needed, it can be escaped using '\'.)
*/

#define PARSE_REGEX_ARG                         \
  while (p < expr_end)                          \
  {                                             \
    char c= *p;                                 \
    if (c == '/')                               \
    {                                           \
      if (last_c == '\\')                       \
      {                                         \
        buf_p[-1]= '/';                         \
      }                                         \
      else                                      \
      {                                         \
        *buf_p++ = 0;                           \
        break;                                  \
      }                                         \
    }                                           \
    else                                        \
      *buf_p++ = c;                             \
                                                \
    last_c= c;                                  \
    p++;                                        \
  }                                             \
                                                \
/*
  Initializes the regular substitution expression to be used in the
  result output of test.

  Returns: st_replace_regex struct with pairs of substitutions
*/

struct st_replace_regex* init_replace_regex(char* expr)
{
  struct st_replace_regex* res;
  char* buf,*expr_end;
  char* p;
  char* buf_p;
  uint expr_len= strlen(expr);
  char last_c = 0;
  struct st_regex reg;

  /* my_malloc() will die on fail with MY_FAE */
  res=(struct st_replace_regex*)my_malloc(
                                          sizeof(*res)+expr_len ,MYF(MY_FAE+MY_WME));
  my_init_dynamic_array(&res->regex_arr,sizeof(struct st_regex),128,128);

  buf= (char*)res + sizeof(*res);
  expr_end= expr + expr_len;
  p= expr;
  buf_p= buf;

  /* for each regexp substitution statement */
  while (p < expr_end)
  {
    bzero(&reg,sizeof(reg));
    /* find the start of the statement */
    while (p < expr_end)
    {
      if (*p == '/')
        break;
      p++;
    }

    if (p == expr_end || ++p == expr_end)
    {
      if (res->regex_arr.elements)
        break;
      else
        goto err;
    }
    /* we found the start */
    reg.pattern= buf_p;

    /* Find first argument -- pattern string to be removed */
    PARSE_REGEX_ARG

      if (p == expr_end || ++p == expr_end)
        goto err;

    /* buf_p now points to the replacement pattern terminated with \0 */
    reg.replace= buf_p;

    /* Find second argument -- replace string to replace pattern */
    PARSE_REGEX_ARG

      if (p == expr_end)
        goto err;

    /* skip the ending '/' in the statement */
    p++;

    /* Check if we should do matching case insensitive */
    if (p < expr_end && *p == 'i')
      reg.icase= 1;

    /* done parsing the statement, now place it in regex_arr */
    if (insert_dynamic(&res->regex_arr,(gptr) &reg))
      die("Out of memory");
  }
  res->odd_buf_len= res->even_buf_len= 8192;
  res->even_buf= (char*)my_malloc(res->even_buf_len,MYF(MY_WME+MY_FAE));
  res->odd_buf= (char*)my_malloc(res->odd_buf_len,MYF(MY_WME+MY_FAE));
  res->buf= res->even_buf;

  return res;

err:
  my_free((gptr)res,0);
  die("Error parsing replace_regex \"%s\"", expr);
  return 0;
}

/*
  Execute all substitutions on val.

  Returns: true if substituition was made, false otherwise
  Side-effect: Sets r->buf to be the buffer with all substitutions done.

  IN:
  struct st_replace_regex* r
  char* val
  Out:
  struct st_replace_regex* r
  r->buf points at the resulting buffer
  r->even_buf and r->odd_buf might have been reallocated
  r->even_buf_len and r->odd_buf_len might have been changed

  TODO:  at some point figure out if there is a way to do everything
  in one pass
*/

int multi_reg_replace(struct st_replace_regex* r,char* val)
{
  uint i;
  char* in_buf, *out_buf;
  int* buf_len_p;

  in_buf= val;
  out_buf= r->even_buf;
  buf_len_p= &r->even_buf_len;
  r->buf= 0;

  /* For each substitution, do the replace */
  for (i= 0; i < r->regex_arr.elements; i++)
  {
    struct st_regex re;
    char* save_out_buf= out_buf;

    get_dynamic(&r->regex_arr,(gptr)&re,i);

    if (!reg_replace(&out_buf, buf_len_p, re.pattern, re.replace,
                     in_buf, re.icase))
    {
      /* if the buffer has been reallocated, make adjustements */
      if (save_out_buf != out_buf)
      {
        if (save_out_buf == r->even_buf)
          r->even_buf= out_buf;
        else
          r->odd_buf= out_buf;
      }

      r->buf= out_buf;
      if (in_buf == val)
        in_buf= r->odd_buf;

      swap_variables(char*,in_buf,out_buf);

      buf_len_p= (out_buf == r->even_buf) ? &r->even_buf_len :
        &r->odd_buf_len;
    }
  }

  return (r->buf == 0);
}

/*
  Parse the regular expression to be used in all result files
  from now on.

  The syntax is --replace_regex /from/to/i /from/to/i ...
  i means case-insensitive match. If omitted, the match is
  case-sensitive

*/
void do_get_replace_regex(struct st_command *command)
{
  char *expr= command->first_argument;
  free_replace_regex();
  if (!(glob_replace_regex=init_replace_regex(expr)))
    die("Could not init replace_regex");
  command->last_argument= command->end;
}

void free_replace_regex()
{
  if (glob_replace_regex)
  {
    delete_dynamic(&glob_replace_regex->regex_arr);
    my_free(glob_replace_regex->even_buf,MYF(MY_ALLOW_ZERO_PTR));
    my_free(glob_replace_regex->odd_buf,MYF(MY_ALLOW_ZERO_PTR));
    my_free((char*) glob_replace_regex,MYF(0));
    glob_replace_regex=0;
  }
}



/*
  auxiluary macro used by reg_replace
  makes sure the result buffer has sufficient length
*/
#define SECURE_REG_BUF   if (buf_len < need_buf_len)                    \
  {                                                                     \
    int off= res_p - buf;                                               \
    buf= (char*)my_realloc(buf,need_buf_len,MYF(MY_WME+MY_FAE));        \
    res_p= buf + off;                                                   \
    buf_len= need_buf_len;                                              \
  }                                                                     \
                                                                        \
/*
  Performs a regex substitution

  IN:

  buf_p - result buffer pointer. Will change if reallocated
  buf_len_p - result buffer length. Will change if the buffer is reallocated
  pattern - regexp pattern to match
  replace - replacement expression
  string - the string to perform substituions in
  icase - flag, if set to 1 the match is case insensitive
*/
int reg_replace(char** buf_p, int* buf_len_p, char *pattern,
                char *replace, char *string, int icase)
{
  my_regex_t r;
  my_regmatch_t *subs;
  char *replace_end;
  char *buf= *buf_p;
  int len;
  int buf_len, need_buf_len;
  int cflags= REG_EXTENDED;
  int err_code;
  char *res_p,*str_p,*str_end;

  buf_len= *buf_len_p;
  len= strlen(string);
  str_end= string + len;

  /* start with a buffer of a reasonable size that hopefully will not
     need to be reallocated
  */
  need_buf_len= len * 2 + 1;
  res_p= buf;

  SECURE_REG_BUF

  if (icase)
    cflags|= REG_ICASE;

  if ((err_code= my_regcomp(&r,pattern,cflags,&my_charset_latin1)))
  {
    check_regerr(&r,err_code);
    return 1;
  }

  subs= (my_regmatch_t*)my_malloc(sizeof(my_regmatch_t) * (r.re_nsub+1),
                                  MYF(MY_WME+MY_FAE));

  *res_p= 0;
  str_p= string;
  replace_end= replace + strlen(replace);

  /* for each pattern match instance perform a replacement */
  while (!err_code)
  {
    /* find the match */
    err_code= my_regexec(&r,str_p, r.re_nsub+1, subs,
                         (str_p == string) ? REG_NOTBOL : 0);

    /* if regular expression error (eg. bad syntax, or out of memory) */
    if (err_code && err_code != REG_NOMATCH)
    {
      check_regerr(&r,err_code);
      my_regfree(&r);
      return 1;
    }

    /* if match found */
    if (!err_code)
    {
      char* expr_p= replace;
      int c;

      /*
        we need at least what we have so far in the buffer + the part
        before this match
      */
      need_buf_len= (res_p - buf) + (int) subs[0].rm_so;

      /* on this pass, calculate the memory for the result buffer */
      while (expr_p < replace_end)
      {
        int back_ref_num= -1;
        c= *expr_p;

        if (c == '\\' && expr_p + 1 < replace_end)
        {
          back_ref_num= (int) (expr_p[1] - '0');
        }

        /* found a valid back_ref (eg. \1)*/
        if (back_ref_num >= 0 && back_ref_num <= (int)r.re_nsub)
        {
          regoff_t start_off, end_off;
          if ((start_off=subs[back_ref_num].rm_so) > -1 &&
              (end_off=subs[back_ref_num].rm_eo) > -1)
          {
            need_buf_len += (int) (end_off - start_off);
          }
          expr_p += 2;
        }
        else
        {
          expr_p++;
          need_buf_len++;
        }
      }
      need_buf_len++;
      /*
        now that we know the size of the buffer,
        make sure it is big enough
      */
      SECURE_REG_BUF

        /* copy the pre-match part */
        if (subs[0].rm_so)
        {
          memcpy(res_p, str_p, (size_t) subs[0].rm_so);
          res_p+= subs[0].rm_so;
        }

      expr_p= replace;

      /* copy the match and expand back_refs */
      while (expr_p < replace_end)
      {
        int back_ref_num= -1;
        c= *expr_p;

        if (c == '\\' && expr_p + 1 < replace_end)
        {
          back_ref_num= expr_p[1] - '0';
        }

        if (back_ref_num >= 0 && back_ref_num <= (int)r.re_nsub)
        {
          regoff_t start_off, end_off;
          if ((start_off=subs[back_ref_num].rm_so) > -1 &&
              (end_off=subs[back_ref_num].rm_eo) > -1)
          {
            int block_len= (int) (end_off - start_off);
            memcpy(res_p,str_p + start_off, block_len);
            res_p += block_len;
          }
          expr_p += 2;
        }
        else
        {
          *res_p++ = *expr_p++;
        }
      }

      /* handle the post-match part */
      if (subs[0].rm_so == subs[0].rm_eo)
      {
        if (str_p + subs[0].rm_so >= str_end)
          break;
        str_p += subs[0].rm_eo ;
        *res_p++ = *str_p++;
      }
      else
      {
        str_p += subs[0].rm_eo;
      }
    }
    else /* no match this time, just copy the string as is */
    {
      int left_in_str= str_end-str_p;
      need_buf_len= (res_p-buf) + left_in_str;
      SECURE_REG_BUF
        memcpy(res_p,str_p,left_in_str);
      res_p += left_in_str;
      str_p= str_end;
    }
  }
  my_free((gptr)subs, MYF(0));
  my_regfree(&r);
  *res_p= 0;
  *buf_p= buf;
  *buf_len_p= buf_len;
  return 0;
}


#ifndef WORD_BIT
#define WORD_BIT (8*sizeof(uint))
#endif

#define SET_MALLOC_HUNC 64
#define LAST_CHAR_CODE 259

typedef struct st_rep_set {
  uint	*bits;				/* Pointer to used sets */
  short next[LAST_CHAR_CODE];		/* Pointer to next sets */
  uint	found_len;			/* Best match to date */
  int	found_offset;
  uint	table_offset;
  uint	size_of_bits;			/* For convinience */
} REP_SET;

typedef struct st_rep_sets {
  uint		count;			/* Number of sets */
  uint		extra;			/* Extra sets in buffer */
  uint		invisible;		/* Sets not chown */
  uint		size_of_bits;
  REP_SET	*set,*set_buffer;
  uint		*bit_buffer;
} REP_SETS;

typedef struct st_found_set {
  uint table_offset;
  int found_offset;
} FOUND_SET;

typedef struct st_follow {
  int chr;
  uint table_offset;
  uint len;
} FOLLOWS;


int init_sets(REP_SETS *sets,uint states);
REP_SET *make_new_set(REP_SETS *sets);
void make_sets_invisible(REP_SETS *sets);
void free_last_set(REP_SETS *sets);
void free_sets(REP_SETS *sets);
void internal_set_bit(REP_SET *set, uint bit);
void internal_clear_bit(REP_SET *set, uint bit);
void or_bits(REP_SET *to,REP_SET *from);
void copy_bits(REP_SET *to,REP_SET *from);
int cmp_bits(REP_SET *set1,REP_SET *set2);
int get_next_bit(REP_SET *set,uint lastpos);
int find_set(REP_SETS *sets,REP_SET *find);
int find_found(FOUND_SET *found_set,uint table_offset,
               int found_offset);
uint start_at_word(my_string pos);
uint end_of_word(my_string pos);

static uint found_sets=0;


uint replace_len(my_string str)
{
  uint len=0;
  while (*str)
  {
    if (str[0] == '\\' && str[1])
      str++;
    str++;
    len++;
  }
  return len;
}

/* Init a replace structure for further calls */

REPLACE *init_replace(my_string *from, my_string *to,uint count,
		      my_string word_end_chars)
{
  static const int SPACE_CHAR= 256;
  static const int START_OF_LINE= 257;
  static const int END_OF_LINE= 258;

  uint i,j,states,set_nr,len,result_len,max_length,found_end,bits_set,bit_nr;
  int used_sets,chr,default_state;
  char used_chars[LAST_CHAR_CODE],is_word_end[256];
  my_string pos,to_pos,*to_array;
  REP_SETS sets;
  REP_SET *set,*start_states,*word_states,*new_set;
  FOLLOWS *follow,*follow_ptr;
  REPLACE *replace;
  FOUND_SET *found_set;
  REPLACE_STRING *rep_str;
  DBUG_ENTER("init_replace");

  /* Count number of states */
  for (i=result_len=max_length=0 , states=2 ; i < count ; i++)
  {
    len=replace_len(from[i]);
    if (!len)
    {
      errno=EINVAL;
      my_message(0,"No to-string for last from-string",MYF(ME_BELL));
      DBUG_RETURN(0);
    }
    states+=len+1;
    result_len+=(uint) strlen(to[i])+1;
    if (len > max_length)
      max_length=len;
  }
  bzero((char*) is_word_end,sizeof(is_word_end));
  for (i=0 ; word_end_chars[i] ; i++)
    is_word_end[(uchar) word_end_chars[i]]=1;

  if (init_sets(&sets,states))
    DBUG_RETURN(0);
  found_sets=0;
  if (!(found_set= (FOUND_SET*) my_malloc(sizeof(FOUND_SET)*max_length*count,
					  MYF(MY_WME))))
  {
    free_sets(&sets);
    DBUG_RETURN(0);
  }
  VOID(make_new_set(&sets));			/* Set starting set */
  make_sets_invisible(&sets);			/* Hide previus sets */
  used_sets=-1;
  word_states=make_new_set(&sets);		/* Start of new word */
  start_states=make_new_set(&sets);		/* This is first state */
  if (!(follow=(FOLLOWS*) my_malloc((states+2)*sizeof(FOLLOWS),MYF(MY_WME))))
  {
    free_sets(&sets);
    my_free((gptr) found_set,MYF(0));
    DBUG_RETURN(0);
  }

  /* Init follow_ptr[] */
  for (i=0, states=1, follow_ptr=follow+1 ; i < count ; i++)
  {
    if (from[i][0] == '\\' && from[i][1] == '^')
    {
      internal_set_bit(start_states,states+1);
      if (!from[i][2])
      {
	start_states->table_offset=i;
	start_states->found_offset=1;
      }
    }
    else if (from[i][0] == '\\' && from[i][1] == '$')
    {
      internal_set_bit(start_states,states);
      internal_set_bit(word_states,states);
      if (!from[i][2] && start_states->table_offset == (uint) ~0)
      {
	start_states->table_offset=i;
	start_states->found_offset=0;
      }
    }
    else
    {
      internal_set_bit(word_states,states);
      if (from[i][0] == '\\' && (from[i][1] == 'b' && from[i][2]))
	internal_set_bit(start_states,states+1);
      else
	internal_set_bit(start_states,states);
    }
    for (pos=from[i], len=0; *pos ; pos++)
    {
      if (*pos == '\\' && *(pos+1))
      {
	pos++;
	switch (*pos) {
	case 'b':
	  follow_ptr->chr = SPACE_CHAR;
	  break;
	case '^':
	  follow_ptr->chr = START_OF_LINE;
	  break;
	case '$':
	  follow_ptr->chr = END_OF_LINE;
	  break;
	case 'r':
	  follow_ptr->chr = '\r';
	  break;
	case 't':
	  follow_ptr->chr = '\t';
	  break;
	case 'v':
	  follow_ptr->chr = '\v';
	  break;
	default:
	  follow_ptr->chr = (uchar) *pos;
	  break;
	}
      }
      else
	follow_ptr->chr= (uchar) *pos;
      follow_ptr->table_offset=i;
      follow_ptr->len= ++len;
      follow_ptr++;
    }
    follow_ptr->chr=0;
    follow_ptr->table_offset=i;
    follow_ptr->len=len;
    follow_ptr++;
    states+=(uint) len+1;
  }


  for (set_nr=0,pos=0 ; set_nr < sets.count ; set_nr++)
  {
    set=sets.set+set_nr;
    default_state= 0;				/* Start from beginning */

    /* If end of found-string not found or start-set with current set */

    for (i= (uint) ~0; (i=get_next_bit(set,i)) ;)
    {
      if (!follow[i].chr)
      {
	if (! default_state)
	  default_state= find_found(found_set,set->table_offset,
				    set->found_offset+1);
      }
    }
    copy_bits(sets.set+used_sets,set);		/* Save set for changes */
    if (!default_state)
      or_bits(sets.set+used_sets,sets.set);	/* Can restart from start */

    /* Find all chars that follows current sets */
    bzero((char*) used_chars,sizeof(used_chars));
    for (i= (uint) ~0; (i=get_next_bit(sets.set+used_sets,i)) ;)
    {
      used_chars[follow[i].chr]=1;
      if ((follow[i].chr == SPACE_CHAR && !follow[i+1].chr &&
	   follow[i].len > 1) || follow[i].chr == END_OF_LINE)
	used_chars[0]=1;
    }

    /* Mark word_chars used if \b is in state */
    if (used_chars[SPACE_CHAR])
      for (pos= word_end_chars ; *pos ; pos++)
	used_chars[(int) (uchar) *pos] = 1;

    /* Handle other used characters */
    for (chr= 0 ; chr < 256 ; chr++)
    {
      if (! used_chars[chr])
	set->next[chr]= chr ? default_state : -1;
      else
      {
	new_set=make_new_set(&sets);
	set=sets.set+set_nr;			/* if realloc */
	new_set->table_offset=set->table_offset;
	new_set->found_len=set->found_len;
	new_set->found_offset=set->found_offset+1;
	found_end=0;

	for (i= (uint) ~0 ; (i=get_next_bit(sets.set+used_sets,i)) ; )
	{
	  if (!follow[i].chr || follow[i].chr == chr ||
	      (follow[i].chr == SPACE_CHAR &&
	       (is_word_end[chr] ||
		(!chr && follow[i].len > 1 && ! follow[i+1].chr))) ||
	      (follow[i].chr == END_OF_LINE && ! chr))
	  {
	    if ((! chr || (follow[i].chr && !follow[i+1].chr)) &&
		follow[i].len > found_end)
	      found_end=follow[i].len;
	    if (chr && follow[i].chr)
	      internal_set_bit(new_set,i+1);		/* To next set */
	    else
	      internal_set_bit(new_set,i);
	  }
	}
	if (found_end)
	{
	  new_set->found_len=0;			/* Set for testing if first */
	  bits_set=0;
	  for (i= (uint) ~0; (i=get_next_bit(new_set,i)) ;)
	  {
	    if ((follow[i].chr == SPACE_CHAR ||
		 follow[i].chr == END_OF_LINE) && ! chr)
	      bit_nr=i+1;
	    else
	      bit_nr=i;
	    if (follow[bit_nr-1].len < found_end ||
		(new_set->found_len &&
		 (chr == 0 || !follow[bit_nr].chr)))
	      internal_clear_bit(new_set,i);
	    else
	    {
	      if (chr == 0 || !follow[bit_nr].chr)
	      {					/* best match  */
		new_set->table_offset=follow[bit_nr].table_offset;
		if (chr || (follow[i].chr == SPACE_CHAR ||
			    follow[i].chr == END_OF_LINE))
		  new_set->found_offset=found_end;	/* New match */
		new_set->found_len=found_end;
	      }
	      bits_set++;
	    }
	  }
	  if (bits_set == 1)
	  {
	    set->next[chr] = find_found(found_set,
					new_set->table_offset,
					new_set->found_offset);
	    free_last_set(&sets);
	  }
	  else
	    set->next[chr] = find_set(&sets,new_set);
	}
	else
	  set->next[chr] = find_set(&sets,new_set);
      }
    }
  }

  /* Alloc replace structure for the replace-state-machine */

  if ((replace=(REPLACE*) my_malloc(sizeof(REPLACE)*(sets.count)+
				    sizeof(REPLACE_STRING)*(found_sets+1)+
				    sizeof(my_string)*count+result_len,
				    MYF(MY_WME | MY_ZEROFILL))))
  {
    rep_str=(REPLACE_STRING*) (replace+sets.count);
    to_array=(my_string*) (rep_str+found_sets+1);
    to_pos=(my_string) (to_array+count);
    for (i=0 ; i < count ; i++)
    {
      to_array[i]=to_pos;
      to_pos=strmov(to_pos,to[i])+1;
    }
    rep_str[0].found=1;
    rep_str[0].replace_string=0;
    for (i=1 ; i <= found_sets ; i++)
    {
      pos=from[found_set[i-1].table_offset];
      rep_str[i].found= !bcmp(pos,"\\^",3) ? 2 : 1;
      rep_str[i].replace_string=to_array[found_set[i-1].table_offset];
      rep_str[i].to_offset=found_set[i-1].found_offset-start_at_word(pos);
      rep_str[i].from_offset=found_set[i-1].found_offset-replace_len(pos)+
	end_of_word(pos);
    }
    for (i=0 ; i < sets.count ; i++)
    {
      for (j=0 ; j < 256 ; j++)
	if (sets.set[i].next[j] >= 0)
	  replace[i].next[j]=replace+sets.set[i].next[j];
	else
	  replace[i].next[j]=(REPLACE*) (rep_str+(-sets.set[i].next[j]-1));
    }
  }
  my_free((gptr) follow,MYF(0));
  free_sets(&sets);
  my_free((gptr) found_set,MYF(0));
  DBUG_PRINT("exit",("Replace table has %d states",sets.count));
  DBUG_RETURN(replace);
}


int init_sets(REP_SETS *sets,uint states)
{
  bzero((char*) sets,sizeof(*sets));
  sets->size_of_bits=((states+7)/8);
  if (!(sets->set_buffer=(REP_SET*) my_malloc(sizeof(REP_SET)*SET_MALLOC_HUNC,
					      MYF(MY_WME))))
    return 1;
  if (!(sets->bit_buffer=(uint*) my_malloc(sizeof(uint)*sets->size_of_bits*
					   SET_MALLOC_HUNC,MYF(MY_WME))))
  {
    my_free((gptr) sets->set,MYF(0));
    return 1;
  }
  return 0;
}

/* Make help sets invisible for nicer codeing */

void make_sets_invisible(REP_SETS *sets)
{
  sets->invisible=sets->count;
  sets->set+=sets->count;
  sets->count=0;
}

REP_SET *make_new_set(REP_SETS *sets)
{
  uint i,count,*bit_buffer;
  REP_SET *set;
  if (sets->extra)
  {
    sets->extra--;
    set=sets->set+ sets->count++;
    bzero((char*) set->bits,sizeof(uint)*sets->size_of_bits);
    bzero((char*) &set->next[0],sizeof(set->next[0])*LAST_CHAR_CODE);
    set->found_offset=0;
    set->found_len=0;
    set->table_offset= (uint) ~0;
    set->size_of_bits=sets->size_of_bits;
    return set;
  }
  count=sets->count+sets->invisible+SET_MALLOC_HUNC;
  if (!(set=(REP_SET*) my_realloc((gptr) sets->set_buffer,
                                  sizeof(REP_SET)*count,
				  MYF(MY_WME))))
    return 0;
  sets->set_buffer=set;
  sets->set=set+sets->invisible;
  if (!(bit_buffer=(uint*) my_realloc((gptr) sets->bit_buffer,
				      (sizeof(uint)*sets->size_of_bits)*count,
				      MYF(MY_WME))))
    return 0;
  sets->bit_buffer=bit_buffer;
  for (i=0 ; i < count ; i++)
  {
    sets->set_buffer[i].bits=bit_buffer;
    bit_buffer+=sets->size_of_bits;
  }
  sets->extra=SET_MALLOC_HUNC;
  return make_new_set(sets);
}

void free_last_set(REP_SETS *sets)
{
  sets->count--;
  sets->extra++;
  return;
}

void free_sets(REP_SETS *sets)
{
  my_free((gptr)sets->set_buffer,MYF(0));
  my_free((gptr)sets->bit_buffer,MYF(0));
  return;
}

void internal_set_bit(REP_SET *set, uint bit)
{
  set->bits[bit / WORD_BIT] |= 1 << (bit % WORD_BIT);
  return;
}

void internal_clear_bit(REP_SET *set, uint bit)
{
  set->bits[bit / WORD_BIT] &= ~ (1 << (bit % WORD_BIT));
  return;
}


void or_bits(REP_SET *to,REP_SET *from)
{
  reg1 uint i;
  for (i=0 ; i < to->size_of_bits ; i++)
    to->bits[i]|=from->bits[i];
  return;
}

void copy_bits(REP_SET *to,REP_SET *from)
{
  memcpy((byte*) to->bits,(byte*) from->bits,
	 (size_t) (sizeof(uint) * to->size_of_bits));
}

int cmp_bits(REP_SET *set1,REP_SET *set2)
{
  return bcmp((byte*) set1->bits,(byte*) set2->bits,
	      sizeof(uint) * set1->size_of_bits);
}


/* Get next set bit from set. */

int get_next_bit(REP_SET *set,uint lastpos)
{
  uint pos,*start,*end,bits;

  start=set->bits+ ((lastpos+1) / WORD_BIT);
  end=set->bits + set->size_of_bits;
  bits=start[0] & ~((1 << ((lastpos+1) % WORD_BIT)) -1);

  while (! bits && ++start < end)
    bits=start[0];
  if (!bits)
    return 0;
  pos=(uint) (start-set->bits)*WORD_BIT;
  while (! (bits & 1))
  {
    bits>>=1;
    pos++;
  }
  return pos;
}

/* find if there is a same set in sets. If there is, use it and
   free given set, else put in given set in sets and return its
   position */

int find_set(REP_SETS *sets,REP_SET *find)
{
  uint i;
  for (i=0 ; i < sets->count-1 ; i++)
  {
    if (!cmp_bits(sets->set+i,find))
    {
      free_last_set(sets);
      return i;
    }
  }
  return i;				/* return new postion */
}

/* find if there is a found_set with same table_offset & found_offset
   If there is return offset to it, else add new offset and return pos.
   Pos returned is -offset-2 in found_set_structure because it is
   saved in set->next and set->next[] >= 0 points to next set and
   set->next[] == -1 is reserved for end without replaces.
*/

int find_found(FOUND_SET *found_set,uint table_offset, int found_offset)
{
  int i;
  for (i=0 ; (uint) i < found_sets ; i++)
    if (found_set[i].table_offset == table_offset &&
	found_set[i].found_offset == found_offset)
      return -i-2;
  found_set[i].table_offset=table_offset;
  found_set[i].found_offset=found_offset;
  found_sets++;
  return -i-2;				/* return new postion */
}

/* Return 1 if regexp starts with \b or ends with \b*/

uint start_at_word(my_string pos)
{
  return (((!bcmp(pos,"\\b",2) && pos[2]) || !bcmp(pos,"\\^",2)) ? 1 : 0);
}

uint end_of_word(my_string pos)
{
  my_string end=strend(pos);
  return ((end > pos+2 && !bcmp(end-2,"\\b",2)) ||
	  (end >= pos+2 && !bcmp(end-2,"\\$",2))) ?
    1 : 0;
}

/****************************************************************************
 * Handle replacement of strings
 ****************************************************************************/

#define PC_MALLOC		256	/* Bytes for pointers */
#define PS_MALLOC		512	/* Bytes for data */

int insert_pointer_name(reg1 POINTER_ARRAY *pa,my_string name)
{
  uint i,length,old_count;
  byte *new_pos;
  const char **new_array;
  DBUG_ENTER("insert_pointer_name");

  if (! pa->typelib.count)
  {
    if (!(pa->typelib.type_names=(const char **)
	  my_malloc(((PC_MALLOC-MALLOC_OVERHEAD)/
		     (sizeof(my_string)+sizeof(*pa->flag))*
		     (sizeof(my_string)+sizeof(*pa->flag))),MYF(MY_WME))))
      DBUG_RETURN(-1);
    if (!(pa->str= (byte*) my_malloc((uint) (PS_MALLOC-MALLOC_OVERHEAD),
				     MYF(MY_WME))))
    {
      my_free((gptr) pa->typelib.type_names,MYF(0));
      DBUG_RETURN (-1);
    }
    pa->max_count=(PC_MALLOC-MALLOC_OVERHEAD)/(sizeof(byte*)+
					       sizeof(*pa->flag));
    pa->flag= (int7*) (pa->typelib.type_names+pa->max_count);
    pa->length=0;
    pa->max_length=PS_MALLOC-MALLOC_OVERHEAD;
    pa->array_allocs=1;
  }
  length=(uint) strlen(name)+1;
  if (pa->length+length >= pa->max_length)
  {
    if (!(new_pos= (byte*) my_realloc((gptr) pa->str,
				      (uint) (pa->max_length+PS_MALLOC),
				      MYF(MY_WME))))
      DBUG_RETURN(1);
    if (new_pos != pa->str)
    {
      my_ptrdiff_t diff=PTR_BYTE_DIFF(new_pos,pa->str);
      for (i=0 ; i < pa->typelib.count ; i++)
	pa->typelib.type_names[i]= ADD_TO_PTR(pa->typelib.type_names[i],diff,
					      char*);
      pa->str=new_pos;
    }
    pa->max_length+=PS_MALLOC;
  }
  if (pa->typelib.count >= pa->max_count-1)
  {
    int len;
    pa->array_allocs++;
    len=(PC_MALLOC*pa->array_allocs - MALLOC_OVERHEAD);
    if (!(new_array=(const char **) my_realloc((gptr) pa->typelib.type_names,
					       (uint) len/
                                               (sizeof(byte*)+sizeof(*pa->flag))*
                                               (sizeof(byte*)+sizeof(*pa->flag)),
                                               MYF(MY_WME))))
      DBUG_RETURN(1);
    pa->typelib.type_names=new_array;
    old_count=pa->max_count;
    pa->max_count=len/(sizeof(byte*) + sizeof(*pa->flag));
    pa->flag= (int7*) (pa->typelib.type_names+pa->max_count);
    memcpy((byte*) pa->flag,(my_string) (pa->typelib.type_names+old_count),
	   old_count*sizeof(*pa->flag));
  }
  pa->flag[pa->typelib.count]=0;			/* Reset flag */
  pa->typelib.type_names[pa->typelib.count++]= pa->str+pa->length;
  pa->typelib.type_names[pa->typelib.count]= NullS;	/* Put end-mark */
  VOID(strmov(pa->str+pa->length,name));
  pa->length+=length;
  DBUG_RETURN(0);
} /* insert_pointer_name */


/* free pointer array */

void free_pointer_array(POINTER_ARRAY *pa)
{
  if (pa->typelib.count)
  {
    pa->typelib.count=0;
    my_free((gptr) pa->typelib.type_names,MYF(0));
    pa->typelib.type_names=0;
    my_free((gptr) pa->str,MYF(0));
  }
} /* free_pointer_array */


/* Functions that uses replace and replace_regex */

/* Append the string to ds, with optional replace */
void replace_dynstr_append_mem(DYNAMIC_STRING *ds,
                               const char *val, int len)
{
#ifdef __WIN__
  fix_win_paths(val, len);
#endif

  if (glob_replace_regex)
  {
    /* Regex replace */
    if (!multi_reg_replace(glob_replace_regex, (char*)val))
    {
      val= glob_replace_regex->buf;
      len= strlen(val);
    }
  }

  if (glob_replace)
  {
    /* Normal replace */
    replace_strings_append(glob_replace, ds, val, len);
  }
  else
    dynstr_append_mem(ds, val, len);
}


/* Append zero-terminated string to ds, with optional replace */
void replace_dynstr_append(DYNAMIC_STRING *ds, const char *val)
{
  replace_dynstr_append_mem(ds, val, strlen(val));
}

/* Append uint to ds, with optional replace */
void replace_dynstr_append_uint(DYNAMIC_STRING *ds, uint val)
{
  char buff[22]; /* This should be enough for any int */
  char *end= longlong10_to_str(val, buff, 10);
  replace_dynstr_append_mem(ds, buff, end - buff);
}



/*
  Build a list of pointer to each line in ds_input, sort
  the list and use the sorted list to append the strings
  sorted to the output ds

  SYNOPSIS
  dynstr_append_sorted
  ds - string where the sorted output will be appended
  ds_input - string to be sorted

*/

static int comp_lines(const char **a, const char **b)
{
  return (strcmp(*a,*b));
}

void dynstr_append_sorted(DYNAMIC_STRING* ds, DYNAMIC_STRING *ds_input)
{
  unsigned i;
  char *start= ds_input->str;
  DYNAMIC_ARRAY lines;
  DBUG_ENTER("dynstr_append_sorted");

  if (!*start)
    DBUG_VOID_RETURN;  /* No input */

  my_init_dynamic_array(&lines, sizeof(const char*), 32, 32);

  /* First line is result header, skip past it */
  while (*start && *start != '\n')
    start++;
  start++; /* Skip past \n */
  dynstr_append_mem(ds, ds_input->str, start - ds_input->str);

  /* Insert line(s) in array */
  while (*start)
  {
    char* line_end= (char*)start;

    /* Find end of line */
    while (*line_end && *line_end != '\n')
      line_end++;
    *line_end= 0;

    /* Insert pointer to the line in array */
    if (insert_dynamic(&lines, (gptr) &start))
      die("Out of memory inserting lines to sort");

    start= line_end+1;
  }

  /* Sort array */
  qsort(lines.buffer, lines.elements,
        sizeof(char**), (qsort_cmp)comp_lines);

  /* Create new result */
  for (i= 0; i < lines.elements ; i++)
  {
    const char **line= dynamic_element(&lines, i, const char**);
    dynstr_append(ds, *line);
    dynstr_append(ds, "\n");
  }

  delete_dynamic(&lines);
  DBUG_VOID_RETURN;
}
