/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  mysqltest

  Tool used for executing a .test file

  See the "MySQL Test framework manual" for more information
  http://dev.mysql.com/doc/mysqltest/en/index.html

  Please keep the test framework tools identical in all versions!
*/

#define MTEST_VERSION "3.3"

#include "client_priv.h"
#include "my_default.h"
#include <mysql_version.h>
#include <mysqld_error.h>
#include <sql_common.h>
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include <stdarg.h>
#include <violite.h>
#include "my_regex.h" /* Our own version of regex */
#include "my_thread_local.h"
#ifndef _WIN32
#include <sys/wait.h>
#endif
#ifdef _WIN32
#include <direct.h>
#endif
#include <signal.h>
#include <my_stacktrace.h>

#include <welcome_copyright_notice.h> // ORACLE_WELCOME_COPYRIGHT_NOTICE

#include <string>
#include <algorithm>
#include <functional>
#include "prealloced_array.h"
#include "template_utils.h"

using std::min;
using std::max;

#ifdef _WIN32
#include <crtdbg.h>
#define SIGNAL_FMT "exception 0x%x"
#else
#define SIGNAL_FMT "signal %d"
#endif

#ifdef _WIN32
#define setenv(a,b,c) _putenv_s(a,b)
#define popen _popen
#define pclose _pclose
#endif

#define MAX_VAR_NAME_LENGTH    256
#define MAX_COLUMNS            256
#define MAX_EMBEDDED_SERVER_ARGS 64
#define MAX_DELIMITER_LENGTH 16
#define DEFAULT_MAX_CONN       128

/* Flags controlling send and reap */
#define QUERY_SEND_FLAG  1
#define QUERY_REAP_FLAG  2

#define APPEND_TYPE(type)                                                      \
{                                                                              \
  dynstr_append(ds, "-- ");                                                    \
  switch (type)                                                                \
  {                                                                            \
  case SESSION_TRACK_SYSTEM_VARIABLES:                                         \
    dynstr_append(ds, "Tracker : SESSION_TRACK_SYSTEM_VARIABLES\n");           \
    break;                                                                     \
  case SESSION_TRACK_SCHEMA:                                                   \
    dynstr_append(ds, "Tracker : SESSION_TRACK_SCHEMA\n");                     \
    break;                                                                     \
  case SESSION_TRACK_STATE_CHANGE:                                             \
    dynstr_append(ds, "Tracker : SESSION_TRACK_STATE_CHANGE\n");               \
    break;                                                                     \
  case SESSION_TRACK_GTIDS:                                                    \
    dynstr_append(ds, "Tracker : SESSION_TRACK_GTIDS\n");                      \
    break;                                                                     \
  case SESSION_TRACK_TRANSACTION_CHARACTERISTICS:                              \
    dynstr_append(ds, "Tracker : SESSION_TRACK_TRANSACTION_CHARACTERISTICS\n");\
    break;                                                                     \
  case SESSION_TRACK_TRANSACTION_STATE:                                        \
    dynstr_append(ds, "Tracker : SESSION_TRACK_TRANSACTION_STATE\n");          \
    break;                                                                     \
  default:                                                                     \
    dynstr_append(ds, "\n");                                                   \
  }                                                                            \
}

C_MODE_START
static void signal_handler(int sig);
static my_bool get_one_option(int optid, const struct my_option *,
                              char *argument);
C_MODE_END

enum {
  OPT_PS_PROTOCOL=OPT_MAX_CLIENT_OPTION, OPT_SP_PROTOCOL,
  OPT_CURSOR_PROTOCOL, OPT_VIEW_PROTOCOL, OPT_MAX_CONNECT_RETRIES,
  OPT_MAX_CONNECTIONS, OPT_MARK_PROGRESS, OPT_LOG_DIR,
  OPT_TAIL_LINES, OPT_RESULT_FORMAT_VERSION, OPT_TRACE_PROTOCOL,
  OPT_EXPLAIN_PROTOCOL, OPT_JSON_EXPLAIN_PROTOCOL
};

static int record= 0, opt_sleep= -1;
static char *opt_db= 0, *opt_pass= 0;
const char *opt_user= 0, *opt_host= 0, *unix_sock= 0, *opt_basedir= "./";
static char *shared_memory_base_name=0;
const char *opt_logdir= "";
const char *opt_include= 0, *opt_charsets_dir;
static int opt_port= 0;
static int opt_max_connect_retries;
static int opt_result_format_version;
static int opt_max_connections= DEFAULT_MAX_CONN;
static my_bool opt_compress= 0, silent= 0, verbose= 0;
static my_bool debug_info_flag= 0, debug_check_flag= 0;
static my_bool tty_password= 0;
static my_bool opt_mark_progress= 0;
static my_bool ps_protocol= 0, ps_protocol_enabled= 0;
static my_bool sp_protocol= 0, sp_protocol_enabled= 0;
static my_bool view_protocol= 0, view_protocol_enabled= 0;
static my_bool opt_trace_protocol= 0, opt_trace_protocol_enabled= 0;
static my_bool explain_protocol= 0, explain_protocol_enabled= 0;
static my_bool json_explain_protocol= 0, json_explain_protocol_enabled= 0;
static my_bool cursor_protocol= 0, cursor_protocol_enabled= 0;
static my_bool parsing_disabled= 0;
static my_bool display_result_vertically= FALSE, display_result_lower= FALSE,
  display_metadata= FALSE, display_result_sorted= FALSE,
  display_session_track_info= FALSE;
static my_bool disable_query_log= 0, disable_result_log= 0;
static my_bool disable_connect_log= 1;
static my_bool disable_warnings= 0;
static my_bool disable_info= 1;
static my_bool abort_on_error= 1;
static my_bool server_initialized= 0;
static my_bool is_windows= 0;
static char **default_argv;
static const char *load_default_groups[]= { "mysqltest", "client", 0 };
static char line_buffer[MAX_DELIMITER_LENGTH], *line_buffer_pos= line_buffer;
#if !defined(HAVE_YASSL)
static const char *opt_server_public_key= 0;
#endif
static my_bool can_handle_expired_passwords= TRUE;

/* Info on properties that can be set with --enable_X and --disable_X */

struct property {
  my_bool *var;			/* Actual variable */
  my_bool set;			/* Has been set for ONE command */
  my_bool old;			/* If set, thus is the old value */
  my_bool reverse;		/* Varible is true if disabled */
  const char *env_name;		/* Env. variable name */
};

static struct property prop_list[] = {
  { &abort_on_error, 0, 1, 0, "$ENABLED_ABORT_ON_ERROR" },
  { &disable_connect_log, 0, 1, 1, "$ENABLED_CONNECT_LOG" },
  { &disable_info, 0, 1, 1, "$ENABLED_INFO" },
  { &display_session_track_info, 0, 1, 1, "$ENABLED_STATE_CHANGE_INFO" },
  { &display_metadata, 0, 0, 0, "$ENABLED_METADATA" },
  { &ps_protocol_enabled, 0, 0, 0, "$ENABLED_PS_PROTOCOL" },
  { &disable_query_log, 0, 0, 1, "$ENABLED_QUERY_LOG" },
  { &disable_result_log, 0, 0, 1, "$ENABLED_RESULT_LOG" },
  { &disable_warnings, 0, 0, 1, "$ENABLED_WARNINGS" }
};

static my_bool once_property= FALSE;

enum enum_prop {
  P_ABORT= 0,
  P_CONNECT,
  P_INFO,
  P_SESSION_TRACK,
  P_META,
  P_PS,
  P_QUERY,
  P_RESULT,
  P_WARN,
  P_MAX
};

static uint start_lineno= 0; /* Start line of current command */
static uint my_end_arg= 0;

/* Number of lines of the result to include in failure report */
static uint opt_tail_lines= 0;

static uint opt_connect_timeout= 0;

static char delimiter[MAX_DELIMITER_LENGTH]= ";";
static size_t delimiter_length= 1;

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
  char            delim[MAX_DELIMITER_LENGTH];  /* Delimiter before block */
};

static struct st_block block_stack[32];
static struct st_block *cur_block, *block_stack_end;

/* Open file stack */
struct st_test_file
{
  FILE* file;
  char *file_name;
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


static ulong connection_retry_sleep= 100000; /* Microseconds */

static char *opt_plugin_dir= 0;

/* Precompiled re's */
static my_regex_t ps_re;     /* the query can be run using PS protocol */
static my_regex_t sp_re;     /* the query can be run as a SP */
static my_regex_t view_re;   /* the query can be run as a view*/
/* the query can be traced with optimizer trace*/
static my_regex_t opt_trace_re;
static my_regex_t explain_re;/* the query can be converted to EXPLAIN */

static void init_re(void);
static int match_re(my_regex_t *, char *);
static void free_re(void);

#ifndef EMBEDDED_LIBRARY
static uint opt_protocol= 0;
#endif

struct st_command;
typedef Prealloced_array<st_command*, 1024> Q_lines;
Q_lines *q_lines;

#include "sslopt-vars.h"

struct Parser
{
  int read_lines,current_line;
} parser;

struct MasterPos
{
  char file[FN_REFLEN];
  ulong pos;
} master_pos;

/* if set, all results are concated and compared against this file */
const char *result_file_name= 0;

typedef struct
{
  char *name;
  size_t name_len;
  char *str_val;
  size_t str_val_len;
  int int_val;
  size_t alloced_len;
  bool int_dirty; /* do not update string if int is updated until first read */
  bool is_int;
  bool alloced;
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
  size_t name_len;
  MYSQL_STMT* stmt;
  /* Set after send to disallow other queries before reap */
  my_bool pending;

#ifdef EMBEDDED_LIBRARY
  my_thread_handle tid;
  const char *cur_query;
  size_t cur_query_len;
  int command, result;
  native_mutex_t query_mutex;
  native_cond_t query_cond;
  native_mutex_t result_mutex;
  native_cond_t result_cond;
  int query_done;
  my_bool has_thread;
#endif /*EMBEDDED_LIBRARY*/
};

struct st_connection *connections= NULL;
struct st_connection* cur_con= NULL, *next_con, *connections_end;

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
  Q_EVAL_RESULT,
  Q_ENABLE_QUERY_LOG, Q_DISABLE_QUERY_LOG,
  Q_ENABLE_RESULT_LOG, Q_DISABLE_RESULT_LOG,
  Q_ENABLE_CONNECT_LOG, Q_DISABLE_CONNECT_LOG,
  Q_WAIT_FOR_SLAVE_TO_STOP,
  Q_ENABLE_WARNINGS, Q_DISABLE_WARNINGS,
  Q_ENABLE_INFO, Q_DISABLE_INFO,
  Q_ENABLE_SESSION_TRACK_INFO, Q_DISABLE_SESSION_TRACK_INFO,
  Q_ENABLE_METADATA, Q_DISABLE_METADATA,
  Q_EXEC, Q_EXECW, Q_DELIMITER,
  Q_DISABLE_ABORT_ON_ERROR, Q_ENABLE_ABORT_ON_ERROR,
  Q_DISPLAY_VERTICAL_RESULTS, Q_DISPLAY_HORIZONTAL_RESULTS,
  Q_QUERY_VERTICAL, Q_QUERY_HORIZONTAL, Q_SORTED_RESULT,
  Q_LOWERCASE,
  Q_START_TIMER, Q_END_TIMER,
  Q_CHARACTER_SET, Q_DISABLE_PS_PROTOCOL, Q_ENABLE_PS_PROTOCOL,
  Q_DISABLE_RECONNECT, Q_ENABLE_RECONNECT,
  Q_IF,
  Q_DISABLE_PARSING, Q_ENABLE_PARSING,
  Q_REPLACE_REGEX, Q_REMOVE_FILE, Q_FILE_EXIST,
  Q_WRITE_FILE, Q_COPY_FILE, Q_PERL, Q_DIE, Q_EXIT, Q_SKIP,
  Q_CHMOD_FILE, Q_APPEND_FILE, Q_CAT_FILE, Q_DIFF_FILES,
  Q_SEND_QUIT, Q_CHANGE_USER, Q_MKDIR, Q_RMDIR,
  Q_LIST_FILES, Q_LIST_FILES_WRITE_FILE, Q_LIST_FILES_APPEND_FILE,
  Q_SEND_SHUTDOWN, Q_SHUTDOWN_SERVER,
  Q_RESULT_FORMAT_VERSION,
  Q_MOVE_FILE, Q_REMOVE_FILES_WILDCARD, Q_SEND_EVAL,
  Q_OUTPUT,                            /* redirect output to a file */
  Q_RESET_CONNECTION,
  Q_UNKNOWN,			       /* Unknown command.   */
  Q_COMMENT,			       /* Comments, ignored. */
  Q_COMMENT_WITH_COMMAND,
  Q_EMPTY_LINE
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
  "eval_result",
  /* Enable/disable that the _query_ is logged to result file */
  "enable_query_log",
  "disable_query_log",
  /* Enable/disable that the _result_ from a query is logged to result file */
  "enable_result_log",
  "disable_result_log",
  "enable_connect_log",
  "disable_connect_log",
  "wait_for_slave_to_stop",
  "enable_warnings",
  "disable_warnings",
  "enable_info",
  "disable_info",
  "enable_session_track_info",
  "disable_session_track_info",
  "enable_metadata",
  "disable_metadata",
  "exec",
  "execw",
  "delimiter",
  "disable_abort_on_error",
  "enable_abort_on_error",
  "vertical_results",
  "horizontal_results",
  "query_vertical",
  "query_horizontal",
  "sorted_result",
  "lowercase_result",
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
  "send_quit",
  "change_user",
  "mkdir",
  "rmdir",
  "list_files",
  "list_files_write_file",
  "list_files_append_file",
  "send_shutdown",
  "shutdown_server",
  "result_format",
  "move_file",
  "remove_files_wildcard",
  "send_eval",
  "output",
  "reset_connection",

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
  struct st_match_err err[20];
  uint count;
};
static struct st_expected_errors saved_expected_errors;

struct st_command
{
  char *query, *query_buf,*first_argument,*last_argument,*end;
  DYNAMIC_STRING content;
  size_t first_word_len, query_len;
  my_bool abort_on_error, used_replace;
  struct st_expected_errors expected_errors;
  char require_file[FN_REFLEN];
  char output_file[FN_REFLEN];
  enum enum_commands type;
};

TYPELIB command_typelib= {array_elements(command_names),"",
			  command_names, 0};

DYNAMIC_STRING ds_res;
DYNAMIC_STRING ds_result;
/* Points to ds_warning in run_query, so it can be freed */
DYNAMIC_STRING *ds_warn= 0;
struct st_command *curr_command= 0;

char builtin_echo[FN_REFLEN];

/* Stores regex substitutions */

struct st_regex
{
  char* pattern; /* Pattern to be replaced */
  char* replace; /* String or expression to replace the pattern with */
  int icase; /* true if the match is case insensitive */
};

struct st_replace_regex
{
  st_replace_regex()
    : regex_arr(PSI_NOT_INSTRUMENTED),
      buf(NULL),
      even_buf(NULL),
      odd_buf(NULL),
      even_buf_len(0),
      odd_buf_len(0)
  {}
  /* stores a list of st_regex subsitutions */
  Prealloced_array<st_regex, 128> regex_arr;

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

struct st_replace;
struct st_replace *glob_replace= 0;
void replace_strings_append(struct st_replace *rep, DYNAMIC_STRING* ds,
                            const char *from, size_t len);

static void cleanup_and_exit(int exit_code) MY_ATTRIBUTE((noreturn));

void die(const char *fmt, ...)
  MY_ATTRIBUTE((format(printf, 1, 2))) MY_ATTRIBUTE((noreturn));
void abort_not_supported_test(const char *fmt, ...)
  MY_ATTRIBUTE((format(printf, 1, 2))) MY_ATTRIBUTE((noreturn));
void verbose_msg(const char *fmt, ...)
  MY_ATTRIBUTE((format(printf, 1, 2)));
void log_msg(const char *fmt, ...)
  MY_ATTRIBUTE((format(printf, 1, 2)));

VAR* var_from_env(const char *, const char *);
VAR* var_init(VAR* v, const char *name, size_t name_len, const char *val,
              size_t val_len);
VAR* var_get(const char *var_name, const char** var_name_end,
             my_bool raw, my_bool ignore_not_existing);
void eval_expr(VAR* v, const char *p, const char** p_end,
               bool open_end=false, bool do_eval=true);
my_bool match_delimiter(int c, const char *delim, size_t length);

void do_eval(DYNAMIC_STRING *query_eval, const char *query,
             const char *query_end, my_bool pass_through_escape_chars);
void str_to_file(const char *fname, char *str, size_t size);
void str_to_file2(const char *fname, char *str, size_t size, my_bool append);

void fix_win_paths(const char *val, size_t len);
const char *get_errname_from_code (uint error_code);
int multi_reg_replace(struct st_replace_regex* r,char* val);

#ifdef _WIN32
void free_win_path_patterns();
#endif


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

/* Used by sleep */
void check_eol_junk_line(const char *eol);

static void var_set(const char *var_name, const char *var_name_end,
                    const char *var_val, const char *var_val_end);

void free_all_replace(){
  free_replace();
  free_replace_regex();
  free_replace_column();
}


class LogFile {
  FILE* m_file;
  char m_file_name[FN_REFLEN];
  size_t m_bytes_written;
public:
  LogFile() : m_file(NULL), m_bytes_written(0) {
    memset(m_file_name, 0, sizeof(m_file_name));
  }

  ~LogFile() {
    close();
  }

  const char* file_name() const { return m_file_name; }
  size_t bytes_written() const { return m_bytes_written; }

  void open(const char* dir, const char* name, const char* ext)
  {
    DBUG_ENTER("LogFile::open");
    DBUG_PRINT("enter", ("dir: '%s', name: '%s'",
                         dir, name));
    if (!name)
    {
      m_file= stdout;
      DBUG_VOID_RETURN;
    }

    fn_format(m_file_name, name, dir, ext,
              *dir ? MY_REPLACE_DIR | MY_REPLACE_EXT :
              MY_REPLACE_EXT);

    DBUG_PRINT("info", ("file_name: %s", m_file_name));

    if ((m_file= fopen(m_file_name, "wb+")) == NULL)
      die("Failed to open log file %s, errno: %d", m_file_name, errno);

    DBUG_VOID_RETURN;
  }

  void close()
  {
    if (m_file) {
      if (m_file != stdout)
        fclose(m_file);
      else
        fflush(m_file);
    }
    m_file= NULL;
  }

  void flush()
  {
    if (m_file)
    {
      if (fflush(m_file))
        die("Failed to flush '%s', errno: %d", m_file_name, errno);
    }
  }

  void write(DYNAMIC_STRING* ds)
  {
    DBUG_ENTER("LogFile::write");
    DBUG_ASSERT(m_file);

    if (ds->length == 0)
      DBUG_VOID_RETURN;
    DBUG_ASSERT(ds->str);

    if (fwrite(ds->str, 1, ds->length, m_file) != ds->length)
      die("Failed to write %lu bytes to '%s', errno: %d",
          (unsigned long)ds->length, m_file_name, errno);
    m_bytes_written+= ds->length;
    DBUG_VOID_RETURN;
  }

  void show_tail(uint lines) {
    DBUG_ENTER("LogFile::show_tail");

    if (!m_file || m_file == stdout)
      DBUG_VOID_RETURN;

    if (lines == 0)
      DBUG_VOID_RETURN;
    lines++;

    int show_offset= 0;
    char buf[256];
    size_t bytes;
    bool found_bof= false;

    /* Search backward in file until "lines" newline has been found */
    while (lines && !found_bof)
    {
      show_offset-= sizeof(buf);
      while(fseek(m_file, show_offset, SEEK_END) != 0 && show_offset < 0)
      {
        found_bof= true;
        // Seeking before start of file
        show_offset++;
      }

      if ((bytes= fread(buf, 1, sizeof(buf), m_file)) <= 0)
      {
	// ferror=0 will happen here if no queries executed yet
	if (ferror(m_file))
	  fprintf(stderr,
	          "Failed to read from '%s', errno: %d, feof:%d, ferror:%d\n",
	          m_file_name, errno, feof(m_file), ferror(m_file));
        DBUG_VOID_RETURN;
      }

      DBUG_PRINT("info", ("Read %lu bytes from file, buf: %s",
                          (unsigned long)bytes, buf));

      char* show_from= buf + bytes;
      while(show_from > buf && lines > 0 )
      {
        show_from--;
        if (*show_from == '\n')
          lines--;
      }
      if (show_from != buf)
      {
        // The last new line was found in this buf, adjust offset
        show_offset+= static_cast<int>(show_from - buf) + 1;
        DBUG_PRINT("info", ("adjusted offset to %d", show_offset));
      }
      DBUG_PRINT("info", ("show_offset: %d", show_offset));
    }

    fprintf(stderr, "\nThe result from queries just before the failure was:\n");

    DBUG_PRINT("info", ("show_offset: %d", show_offset));
    if (!lines)
    {
      fprintf(stderr, "< snip >\n");

      if (fseek(m_file, show_offset, SEEK_END) != 0)
      {
        fprintf(stderr, "Failed to seek to position %d in '%s', errno: %d",
                show_offset, m_file_name, errno);
        DBUG_VOID_RETURN;
      }

    }
    else {
      DBUG_PRINT("info", ("Showing the whole file"));
      if (fseek(m_file, 0L, SEEK_SET) != 0)
      {
        fprintf(stderr, "Failed to seek to pos 0 in '%s', errno: %d",
                m_file_name, errno);
        DBUG_VOID_RETURN;
      }
    }

    while ((bytes= fread(buf, 1, sizeof(buf), m_file)) > 0) {
      if (fwrite(buf, 1, bytes, stderr) != bytes) {
        perror("fwrite");
      }
    }
    fflush(stderr);

    DBUG_VOID_RETURN;
  }
};

LogFile log_file;
LogFile progress_file;

void replace_dynstr_append_mem(DYNAMIC_STRING *ds, const char *val,
                               size_t len);
void replace_dynstr_append(DYNAMIC_STRING *ds, const char *val);
void replace_dynstr_append_uint(DYNAMIC_STRING *ds, uint val);
void dynstr_append_sorted(DYNAMIC_STRING* ds, DYNAMIC_STRING* ds_input);

static int match_expected_error(struct st_command *command,
                                unsigned int err_errno,
                                const char *err_sqlstate);
void handle_error(struct st_command*,
                  unsigned int err_errno, const char *err_error,
                  const char *err_sqlstate, DYNAMIC_STRING *ds);
void handle_no_error(struct st_command*);
void revert_properties();

#ifdef EMBEDDED_LIBRARY

#define EMB_SEND_QUERY 1
#define EMB_READ_QUERY_RESULT 2
#define EMB_END_CONNECTION 3

/* attributes of the query thread */
my_thread_attr_t cn_thd_attrib;


/*
  This procedure represents the connection and actually
  runs queries when in the EMBEDDED-SERVER mode.
  The run_query_normal() just sends request for running
  mysql_send_query and mysql_read_query_result() here.
*/

extern "C" void *connection_thread(void *arg)
{
  struct st_connection *cn= (struct st_connection*)arg;

  mysql_thread_init();
  while (cn->command != EMB_END_CONNECTION)
  {
    if (!cn->command)
    {
      native_mutex_lock(&cn->query_mutex);
      while (!cn->command)
        native_cond_wait(&cn->query_cond, &cn->query_mutex);
      native_mutex_unlock(&cn->query_mutex);
    }
    switch (cn->command)
    {
      case EMB_END_CONNECTION:
        goto end_thread;
      case EMB_SEND_QUERY:
        cn->result= mysql_send_query(&cn->mysql, cn->cur_query,
                                     static_cast<ulong>(cn->cur_query_len));
        break;
      case EMB_READ_QUERY_RESULT:
        cn->result= mysql_read_query_result(&cn->mysql);
        break;
      default:
        DBUG_ASSERT(0);
    }
    cn->command= 0;
    native_mutex_lock(&cn->result_mutex);
    cn->query_done= 1;
    native_cond_signal(&cn->result_cond);
    native_mutex_unlock(&cn->result_mutex);
  }

end_thread:
  cn->query_done= 1;
  mysql_thread_end();
  my_thread_exit(0);
  return 0;
}

static void wait_query_thread_done(struct st_connection *con)
{
  DBUG_ASSERT(con->has_thread);
  if (!con->query_done)
  {
    native_mutex_lock(&con->result_mutex);
    while (!con->query_done)
      native_cond_wait(&con->result_cond, &con->result_mutex);
    native_mutex_unlock(&con->result_mutex);
  }
}


static void signal_connection_thd(struct st_connection *cn, int command)
{
  DBUG_ASSERT(cn->has_thread);
  cn->query_done= 0;
  cn->command= command;
  native_mutex_lock(&cn->query_mutex);
  native_cond_signal(&cn->query_cond);
  native_mutex_unlock(&cn->query_mutex);
}


/*
  Sometimes we try to execute queries when the connection is closed.
  It's done to make sure it was closed completely.
  So that if our connection is closed (cn->has_thread == 0), we just return
  the mysql_send_query() result which is an error in this case.
*/

static int do_send_query(struct st_connection *cn, const char *q, size_t q_len)
{
  if (!cn->has_thread)
    return mysql_send_query(&cn->mysql, q, static_cast<ulong>(q_len));
  cn->cur_query= q;
  cn->cur_query_len= q_len;
  signal_connection_thd(cn, EMB_SEND_QUERY);
  return 0;
}

static int do_read_query_result(struct st_connection *cn)
{
  DBUG_ASSERT(cn->has_thread);
  wait_query_thread_done(cn);
  signal_connection_thd(cn, EMB_READ_QUERY_RESULT);
  wait_query_thread_done(cn);

  return cn->result;
}


static void emb_close_connection(struct st_connection *cn)
{
  if (!cn->has_thread)
    return;
  wait_query_thread_done(cn);
  signal_connection_thd(cn, EMB_END_CONNECTION);
  my_thread_join(&cn->tid, NULL);
  cn->has_thread= FALSE;
  native_mutex_destroy(&cn->query_mutex);
  native_cond_destroy(&cn->query_cond);
  native_mutex_destroy(&cn->result_mutex);
  native_cond_destroy(&cn->result_cond);
}


static void init_connection_thd(struct st_connection *cn)
{
  cn->query_done= 1;
  cn->command= 0;
  if (native_mutex_init(&cn->query_mutex, NULL) ||
      native_cond_init(&cn->query_cond) ||
      native_mutex_init(&cn->result_mutex, NULL) ||
      native_cond_init(&cn->result_cond) ||
      my_thread_create(&cn->tid, &cn_thd_attrib, connection_thread, (void*)cn))
    die("Error in the thread library");
  cn->has_thread=TRUE;
}

#else /*EMBEDDED_LIBRARY*/

#define do_send_query(cn,q,q_len) mysql_send_query(&cn->mysql, q, static_cast<ulong>(q_len))
#define do_read_query_result(cn) mysql_read_query_result(&cn->mysql)

#endif /*EMBEDDED_LIBRARY*/

void do_eval(DYNAMIC_STRING *query_eval, const char *query,
             const char *query_end, my_bool pass_through_escape_chars)
{
  const char *p;
  char c, next_c;
  int escaped = 0;
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
#ifdef _WIN32
    fix_win_paths(query_eval->str, query_eval->length);
#endif
  DBUG_VOID_RETURN;
}


/*
  Run query and dump the result to stderr in vertical format

  NOTE! This function should be safe to call when an error
  has occured and thus any further errors will be ignored(although logged)

  SYNOPSIS
  show_query
  mysql - connection to use
  query - query to run

*/

static void show_query(MYSQL* mysql, const char* query)
{
  MYSQL_RES* res;
  DBUG_ENTER("show_query");

  if (!mysql)
    DBUG_VOID_RETURN;

  if (mysql_query(mysql, query))
  {
    log_msg("Error running query '%s': %d %s",
            query, mysql_errno(mysql), mysql_error(mysql));
    DBUG_VOID_RETURN;
  }

  if ((res= mysql_store_result(mysql)) == NULL)
  {
    /* No result set returned */
    DBUG_VOID_RETURN;
  }

  {
    MYSQL_ROW row;
    unsigned int i;
    unsigned int row_num= 0;
    unsigned int num_fields= mysql_num_fields(res);
    MYSQL_FIELD *fields= mysql_fetch_fields(res);

    fprintf(stderr, "=== %s ===\n", query);
    while ((row= mysql_fetch_row(res)))
    {
      unsigned long *lengths= mysql_fetch_lengths(res);
      row_num++;

      fprintf(stderr, "---- %d. ----\n", row_num);
      for(i= 0; i < num_fields; i++)
      {
      /* looks ugly , but put here to convince parfait */
        assert(lengths);
        fprintf(stderr, "%s\t%.*s\n",
                fields[i].name,
                (int)lengths[i], row[i] ? row[i] : "NULL");
      }
    }
    for (i= 0; i < strlen(query)+8; i++)
      fprintf(stderr, "=");
    fprintf(stderr, "\n\n");
  }
  mysql_free_result(res);

  DBUG_VOID_RETURN;
}


/*
  Show any warnings just before the error. Since the last error
  is added to the warning stack, only print @@warning_count-1 warnings.

  NOTE! This function should be safe to call when an error
  has occured and this any further errors will be ignored(although logged)

  SYNOPSIS
  show_warnings_before_error
  mysql - connection to use

*/

static void show_warnings_before_error(MYSQL* mysql)
{
  MYSQL_RES* res;
  const char* query= "SHOW WARNINGS";
  DBUG_ENTER("show_warnings_before_error");

  if (!mysql)
    DBUG_VOID_RETURN;

  if (mysql_query(mysql, query))
  {
    log_msg("Error running query '%s': %d %s",
            query, mysql_errno(mysql), mysql_error(mysql));
    DBUG_VOID_RETURN;
  }

  if ((res= mysql_store_result(mysql)) == NULL)
  {
    /* No result set returned */
    DBUG_VOID_RETURN;
  }

  if (mysql_num_rows(res) <= 1)
  {
    /* Don't display the last row, it's "last error" */
  }
  else
  {
    MYSQL_ROW row;
    unsigned int row_num= 0;
    unsigned int num_fields= mysql_num_fields(res);

    fprintf(stderr, "\nWarnings from just before the error:\n");
    while ((row= mysql_fetch_row(res)))
    {
      unsigned int i;
      unsigned long *lengths= mysql_fetch_lengths(res);

      if (++row_num >= mysql_num_rows(res))
      {
        /* Don't display the last row, it's "last error" */
        break;
      }

      for(i= 0; i < num_fields; i++)
      {
      /* looks ugly , but put here to convince parfait */
        assert(lengths);
        fprintf(stderr, "%.*s ", (int)lengths[i],
                row[i] ? row[i] : "NULL");
      }
      fprintf(stderr, "\n");
    }
  }
  mysql_free_result(res);

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
    char delimiter;

    switch (arg->type) {
      /* A string */
    case ARG_STRING:
      /* Skip leading spaces */
      while (*ptr && *ptr == ' ')
        ptr++;
      start= ptr;
      delimiter = delimiter_arg;
      /* If start of arg is ' ` or " search to matching quote end instead */
      if (*ptr && strchr ("'`\"", *ptr))
      {
	delimiter= *ptr;
	start= ++ptr;
      }
      /* Find end of arg, terminated by "delimiter" */
      while (*ptr && *ptr != delimiter)
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
      /* Find real end of arg, terminated by "delimiter_arg" */
      /* This will do nothing if arg was not closed by quotes */
      while (*ptr && *ptr != delimiter_arg)
        ptr++;      

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
          static_cast<int>(command->first_word_len), command->query);

  }
  /* Check for too many arguments passed */
  ptr= command->last_argument;
  while(ptr <= command->end && *ptr != '#')
  {
    if (*ptr && *ptr != ' ')
      die("Extra argument '%s' passed to '%.*s'",
          ptr, static_cast<int>(command->first_word_len), command->query);
    ptr++;
  }
  DBUG_VOID_RETURN;
}

void handle_command_error(struct st_command *command, uint error)
{
  DBUG_ENTER("handle_command_error");
  DBUG_PRINT("enter", ("error: %d", error));
  if (error != 0)
  {
    int i;

    if (command->abort_on_error)
      die("command \"%.*s\" failed with error %d. my_errno=%d",
          static_cast<int>(command->first_word_len),
          command->query, error, my_errno());

    i= match_expected_error(command, error, NULL);

    if (i >= 0)
    {
      DBUG_PRINT("info", ("command \"%.*s\" failed with expected error: %d",
                          static_cast<int>(command->first_word_len),
                          command->query, error));
      goto end;
    }
    if (command->expected_errors.count > 0)
      die("command \"%.*s\" failed with wrong error: %d. my_errno=%d",
          static_cast<int>(command->first_word_len),
          command->query, error, my_errno());
  }
  else if (command->expected_errors.err[0].type == ERR_ERRNO &&
           command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    die("command \"%.*s\" succeeded - should have failed with errno %d...",
        static_cast<int>(command->first_word_len), command->query,
        command->expected_errors.err[0].code.errnum);
  }
end:
  // Save error code
  {
    const char var_name[]= "__error";
    char buf[10];
    size_t err_len= my_snprintf(buf, 10, "%u", error);
    buf[err_len > 9 ? 9 : err_len]= '0';
    var_set(var_name, var_name + 7, buf, buf + err_len);
  }

  revert_properties();
  DBUG_VOID_RETURN;
}


void close_connections()
{
  DBUG_ENTER("close_connections");
  for (--next_con; next_con >= connections; --next_con)
  {
#ifdef EMBEDDED_LIBRARY
    emb_close_connection(next_con);
#endif
    if (next_con->stmt)
      mysql_stmt_close(next_con->stmt);
    next_con->stmt= 0;
    mysql_close(&next_con->mysql);
    if (next_con->util_mysql)
      mysql_close(next_con->util_mysql);
    my_free(next_con->name);
  }
  my_free(connections);
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
      fclose(cur_file->file);
    }
    my_free(cur_file->file_name);
    cur_file->file_name= 0;
  }
  DBUG_VOID_RETURN;
}


void free_used_memory()
{
  uint i;
  // Do not use DBUG_ENTER("free_used_memory"); here, see below.

  if (connections)
    close_connections();
  close_files();
  my_hash_free(&var_hash);

  struct st_command **q;
  for (q= q_lines->begin(); q != q_lines->end(); ++q)
  {
    my_free((*q)->query_buf);
    if ((*q)->content.str)
      dynstr_free(&(*q)->content);
    my_free((*q));
  }
  for (i= 0; i < 10; i++)
  {
    if (var_reg[i].alloced_len)
      my_free(var_reg[i].str_val);
  }
  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count]);
  delete q_lines;
  dynstr_free(&ds_res);
  dynstr_free(&ds_result);
  if (ds_warn)
    dynstr_free(ds_warn);
  free_all_replace();
  my_free(opt_pass);
  free_defaults(default_argv);
  free_re();
#ifdef _WIN32
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
  my_end(my_end_arg);

  if (!silent) {
    switch (exit_code) {
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

  /* exit() appears to be not 100% reliable on Windows under some conditions */
#ifdef _WIN32
  fflush(stdout);
  fflush(stderr);
  _exit(exit_code);
#else
  exit(exit_code);
#endif
}

void print_file_stack()
{
  fprintf(stderr, "file %s at line %d:\n",
          cur_file->file_name, cur_file->lineno);
  for (struct st_test_file* err_file= cur_file - 1;
       err_file >= file_stack;
       err_file--)
  {
    fprintf(stderr, "included from %s at line %d:\n",
            err_file->file_name, err_file->lineno);
  }
}

void die(const char *fmt, ...)
{
  static int dying= 0;
  va_list args;
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
  {
    fprintf(stderr, "In included ");
    print_file_stack();
  }
  
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

  log_file.show_tail(opt_tail_lines);

  /*
    Help debugging by displaying any warnings that might have
    been produced prior to the error
  */
  if (cur_con && !cur_con->pending)
    show_warnings_before_error(&cur_con->mysql);

  cleanup_and_exit(1);
}


void abort_not_supported_test(const char *fmt, ...)
{
  va_list args;
  DBUG_ENTER("abort_not_supported_test");

  /* Print include filestack */
  fprintf(stderr, "The test '%s' is not supported by this installation\n",
          file_stack->file_name);
  fprintf(stderr, "Detected in ");
  print_file_stack();

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


void log_msg(const char *fmt, ...)
{
  va_list args;
  char buff[1024];
  size_t len;
  DBUG_ENTER("log_msg");

  va_start(args, fmt);
  len= my_vsnprintf(buff, sizeof(buff)-1, fmt, args);
  va_end(args);

  dynstr_append_mem(&ds_res, buff, len);
  dynstr_append(&ds_res, "\n");

  DBUG_VOID_RETURN;
}


/*
  Read a file and append it to ds

  SYNOPSIS
  cat_file
  ds - pointer to dynamic string where to add the files content
  filename - name of the file to read

*/

int cat_file(DYNAMIC_STRING* ds, const char* filename)
{
  int fd;
  size_t len;
  char buff[512];
  bool dangling_cr= 0;

  if ((fd= my_open(filename, O_RDONLY, MYF(0))) < 0)
    return 1;
  while((len= my_read(fd, (uchar*)&buff,
                      sizeof(buff), MYF(0))) > 0)
  {
    char *p= buff, *start= buff;
    if (dangling_cr)
    {
      if (*p != '\n')
       dynstr_append_mem(ds, "\r", 1);
      dangling_cr= 0;
    }
    while (p < buff+len)
    {
      /* Convert cr/lf to lf */
      if (*p == '\r' && *(p+1) && *(p+1)== '\n')
      {
        /* Add fake newline instead of cr and output the line */
        *p= '\n';
        p++; /* Step past the "fake" newline */
        dynstr_append_mem(ds, start, p-start);
        p++; /* Step past the "fake" newline */
        start= p;
      }
      else
        p++;
    }
    if (*(p-1) == '\r' && len == 512)
      dangling_cr=1;
    /* Output any chars that migh be left */
    if (dangling_cr)
      dynstr_append_mem(ds, start, p-start-1);
    else
      dynstr_append_mem(ds, start, p-start);
  }
  my_close(fd, MYF(0));
  return 0;
}


/*
  Run the specified command with popen

  SYNOPSIS
  run_command
  cmd - command to execute(should be properly quoted
  ds_res- pointer to dynamic string where to store the result

*/

static int run_command(char* cmd,
                       DYNAMIC_STRING *ds_res)
{
  char buf[512]= {0};
  FILE *res_file;
  int error;

  if (!(res_file= popen(cmd, "r")))
    die("popen(\"%s\", \"r\") failed", cmd);

  while (fgets(buf, sizeof(buf), res_file))
  {
    DBUG_PRINT("info", ("buf: %s", buf));
    if(ds_res)
    {
      /* Save the output of this command in the supplied string */
      dynstr_append(ds_res, buf);
    }
    else
    {
      /* Print it directly on screen */
      fprintf(stdout, "%s", buf);
    }
  }

  error= pclose(res_file);
  return WEXITSTATUS(error);
}


/*
  Run the specified tool with variable number of arguments

  SYNOPSIS
  run_tool
  tool_path - the name of the tool to run
  ds_res - pointer to dynamic string where to store the result
  ... - variable number of arguments that will be properly
        quoted and appended after the tool's name

*/

static int run_tool(const char *tool_path, DYNAMIC_STRING *ds_res, ...)
{
  int ret;
  const char* arg;
  va_list args;
  DYNAMIC_STRING ds_cmdline;

  DBUG_ENTER("run_tool");
  DBUG_PRINT("enter", ("tool_path: %s", tool_path));

  if (init_dynamic_string(&ds_cmdline, IF_WIN("\"", ""), FN_REFLEN, FN_REFLEN))
    die("Out of memory");

  dynstr_append_os_quoted(&ds_cmdline, tool_path, NullS);
  dynstr_append(&ds_cmdline, " ");

  va_start(args, ds_res);

  while ((arg= va_arg(args, char *)))
  {
    /* Options should be os quoted */
    if (strncmp(arg, "--", 2) == 0)
      dynstr_append_os_quoted(&ds_cmdline, arg, NullS);
    else
      dynstr_append(&ds_cmdline, arg);
    dynstr_append(&ds_cmdline, " ");
  }

  va_end(args);

#ifdef _WIN32
  dynstr_append(&ds_cmdline, "\"");
#endif

  DBUG_PRINT("info", ("Running: %s", ds_cmdline.str));
  ret= run_command(ds_cmdline.str, ds_res);
  DBUG_PRINT("exit", ("ret: %d", ret));
  dynstr_free(&ds_cmdline);
  DBUG_RETURN(ret);
}


/*
  Test if diff is present.  This is needed on Windows systems
  as the OS returns 1 whether diff is successful or if it is
  not present.

  We run diff -v and look for output in stdout.
  We don't redirect stderr to stdout to make for a simplified check
  Windows will output '"diff"' is not recognized... to stderr if it is
  not present.
*/

#ifdef _WIN32

static int diff_check(const char *diff_name)
{
  FILE *res_file;
  char buf[128];
  int have_diff= 0;

  my_snprintf(buf, sizeof(buf), "%s -v", diff_name);

  if (!(res_file= popen(buf, "r")))
    die("popen(\"%s\", \"r\") failed", buf);

  /* if diff is not present, nothing will be in stdout to increment have_diff */
  if (fgets(buf, sizeof(buf), res_file))
    have_diff= 1;

  pclose(res_file);

  return have_diff;
}

#endif


/*
  Show the diff of two files using the systems builtin diff
  command. If no such diff command exist, just dump the content
  of the two files and inform about how to get "diff"

  SYNOPSIS
  show_diff
  ds - pointer to dynamic string where to add the diff(may be NULL)
  filename1 - name of first file
  filename2 - name of second file

*/

void show_diff(DYNAMIC_STRING* ds,
               const char* filename1, const char* filename2)
{
  DYNAMIC_STRING ds_tmp;
  const char *diff_name = 0;

  if (init_dynamic_string(&ds_tmp, "", 256, 256))
    die("Out of memory");

  /* determine if we have diff on Windows
     needs special processing due to return values
     on that OS
     This test is only done on Windows since it's only needed there
     in order to correctly detect non-availibility of 'diff', and
     the way it's implemented does not work with default 'diff' on Solaris.
  */
#ifdef _WIN32
  if (diff_check("diff"))
    diff_name = "diff";
  else if (diff_check("mtrdiff"))
    diff_name = "mtrdiff";
  else
    diff_name = 0;
#else
  diff_name = "diff";           /* Otherwise always assume it's called diff */
#endif

  if (diff_name)
  {
    /* First try with unified diff */
    if (run_tool(diff_name,
                 &ds_tmp, /* Get output from diff in ds_tmp */
                 "-u",
                 filename1,
                 filename2,
                 "2>&1",
                 NULL) > 1) /* Most "diff" tools return >1 if error */
    {
      dynstr_set(&ds_tmp, "");

      /* Fallback to context diff with "diff -c" */
      if (run_tool(diff_name,
                   &ds_tmp, /* Get output from diff in ds_tmp */
                   "-c",
                   filename1,
                   filename2,
                   "2>&1",
                   NULL) > 1) /* Most "diff" tools return >1 if error */
      {
	dynstr_set(&ds_tmp, "");

	/* Fallback to simple diff with "diff" */
	if (run_tool(diff_name,
		     &ds_tmp, /* Get output from diff in ds_tmp */
		     filename1,
		     filename2,
		     "2>&1",
		     NULL) > 1) /* Most "diff" tools return >1 if error */
	    {
		diff_name= 0;
	    }
      }
    }
  }  

  if (! diff_name)
  {
    /*
      Fallback to dump both files to result file and inform
      about installing "diff"
    */
	dynstr_append(&ds_tmp, "\n");
    dynstr_append(&ds_tmp,
"\n"
"The two files differ but it was not possible to execute 'diff' in\n"
"order to show only the difference. Instead the whole content of the\n"
"two files was shown for you to diff manually.\n\n"
"To get a better report you should install 'diff' on your system, which you\n"
"for example can get from http://www.gnu.org/software/diffutils/diffutils.html\n"
#ifdef _WIN32
"or http://gnuwin32.sourceforge.net/packages/diffutils.htm\n"
#endif
"\n");

    dynstr_append(&ds_tmp, " --- ");
    dynstr_append(&ds_tmp, filename1);
    dynstr_append(&ds_tmp, " >>>\n");
    cat_file(&ds_tmp, filename1);
    dynstr_append(&ds_tmp, "<<<\n --- ");
    dynstr_append(&ds_tmp, filename1);
    dynstr_append(&ds_tmp, " >>>\n");
    cat_file(&ds_tmp, filename2);
    dynstr_append(&ds_tmp, "<<<<\n");
  }

  if (ds)
  {
    /* Add the diff to output */
    dynstr_append_mem(ds, ds_tmp.str, ds_tmp.length);
  }
  else
  {
    /* Print diff directly to stdout */
    fprintf(stderr, "%s\n", ds_tmp.str);
  }
 
  dynstr_free(&ds_tmp);

}


enum compare_files_result_enum {
   RESULT_OK= 0,
   RESULT_CONTENT_MISMATCH= 1,
   RESULT_LENGTH_MISMATCH= 2
};

/*
  Compare two files, given a fd to the first file and
  name of the second file

  SYNOPSIS
  compare_files2
  fd - Open file descriptor of the first file
  filename2 - Name of second file

  RETURN VALUES
  According to the values in "compare_files_result_enum"

*/

int compare_files2(File fd, const char* filename2)
{
  int error= RESULT_OK;
  File fd2;
  size_t len, len2;
  char buff[512], buff2[512];

  if ((fd2= my_open(filename2, O_RDONLY, MYF(0))) < 0)
  {
    my_close(fd, MYF(0));
    die("Failed to open second file: '%s'", filename2);
  }
  while((len= my_read(fd, (uchar*)&buff,
                      sizeof(buff), MYF(0))) > 0)
  {
    if ((len2= my_read(fd2, (uchar*)&buff2,
                       sizeof(buff2), MYF(0))) < len)
    {
      /* File 2 was smaller */
      error= RESULT_LENGTH_MISMATCH;
      break;
    }
    if (len2 > len)
    {
      /* File 1 was smaller */
      error= RESULT_LENGTH_MISMATCH;
      break;
    }
    if ((memcmp(buff, buff2, len)))
    {
      /* Content of this part differed */
      error= RESULT_CONTENT_MISMATCH;
      break;
    }
  }
  if (!error && my_read(fd2, (uchar*)&buff2,
                        sizeof(buff2), MYF(0)) > 0)
  {
    /* File 1 was smaller */
    error= RESULT_LENGTH_MISMATCH;
  }

  my_close(fd2, MYF(0));

  return error;
}


/*
  Compare two files, given their filenames

  SYNOPSIS
  compare_files
  filename1 - Name of first file
  filename2 - Name of second file

  RETURN VALUES
  See 'compare_files2'

*/

int compare_files(const char* filename1, const char* filename2)
{
  File fd;
  int error;

  if ((fd= my_open(filename1, O_RDONLY, MYF(0))) < 0)
    die("Failed to open first file: '%s'", filename1);

  error= compare_files2(fd, filename2);

  my_close(fd, MYF(0));

  return error;
}


/*
  Compare content of the string in ds to content of file fname

  SYNOPSIS
  dyn_string_cmp
  ds - Dynamic string containing the string o be compared
  fname - Name of file to compare with

  RETURN VALUES
  See 'compare_files2'
*/

int dyn_string_cmp(DYNAMIC_STRING* ds, const char *fname)
{
  int error;
  File fd;
  char temp_file_path[FN_REFLEN];

  DBUG_ENTER("dyn_string_cmp");
  DBUG_PRINT("enter", ("fname: %s", fname));

  if ((fd= create_temp_file(temp_file_path, TMPDIR,
                            "tmp", O_CREAT | O_SHARE | O_RDWR,
                            MYF(MY_WME))) < 0)
    die("Failed to create temporary file for ds");

  /* Write ds to temporary file and set file pos to beginning*/
  if (my_write(fd, (uchar *) ds->str, ds->length,
               MYF(MY_FNABP | MY_WME)) ||
      my_seek(fd, 0, SEEK_SET, MYF(0)) == MY_FILEPOS_ERROR)
  {
    my_close(fd, MYF(0));
    /* Remove the temporary file */
    my_delete(temp_file_path, MYF(0));
    die("Failed to write file '%s'", temp_file_path);
  }

  error= compare_files2(fd, fname);

  my_close(fd, MYF(0));
  /* Remove the temporary file */
  my_delete(temp_file_path, MYF(0));

  DBUG_RETURN(error);
}


/*
  Check the content of log against result file

  SYNOPSIS
  check_result

  RETURN VALUES
  error - the function will not return

*/

void check_result()
{
  const char* mess= "Result content mismatch\n";

  DBUG_ENTER("check_result");
  DBUG_ASSERT(result_file_name);
  DBUG_PRINT("enter", ("result_file_name: %s", result_file_name));

  switch (compare_files(log_file.file_name(), result_file_name)) {
  case RESULT_OK:
    break; /* ok */
  case RESULT_LENGTH_MISMATCH:
    mess= "Result length mismatch\n";
    /* Fallthrough */
  case RESULT_CONTENT_MISMATCH:
  {
    /*
      Result mismatched, dump results to .reject file
      and then show the diff
    */
    char reject_file[FN_REFLEN];
    size_t reject_length;
    dirname_part(reject_file, result_file_name, &reject_length);

    /* Put reject file in opt_logdir */
    fn_format(reject_file, result_file_name, opt_logdir,
                ".reject", MY_REPLACE_DIR | MY_REPLACE_EXT);

    if (my_copy(log_file.file_name(), reject_file, MYF(0)) != 0)
      die("Failed to copy '%s' to '%s', errno: %d",
          log_file.file_name(), reject_file, errno);

    show_diff(NULL, result_file_name, reject_file);
    die("%s", mess);
    break;
  }
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
  check_require
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


/*
   Remove surrounding chars from string

   Return 1 if first character is found but not last
*/
static int strip_surrounding(char* str, char c1, char c2)
{
  char* ptr= str;

  /* Check if the first non space character is c1 */
  while(*ptr && my_isspace(charset_info, *ptr))
    ptr++;
  if (*ptr == c1)
  {
    /* Replace it with a space */
    *ptr= ' ';

    /* Last non space charecter should be c2 */
    ptr= strend(str)-1;
    while(*ptr && my_isspace(charset_info, *ptr))
      ptr--;
    if (*ptr == c2)
    {
      /* Replace it with \0 */
      *ptr= 0;
    }
    else
    {
      /* Mismatch detected */
      return 1;
    }
  }
  return 0;
}


static void strip_parentheses(struct st_command *command)
{
  if (strip_surrounding(command->first_argument, '(', ')'))
      die("%.*s - argument list started with '%c' must be ended with '%c'",
          static_cast<int>(command->first_word_len), command->query, '(', ')');
}


C_MODE_START

static uchar *get_var_key(const uchar* var, size_t *len,
                          my_bool MY_ATTRIBUTE((unused)) t)
{
  char* key;
  key = ((VAR*)var)->name;
  *len = ((VAR*)var)->name_len;
  return (uchar*)key;
}


static void var_free(void *v)
{
  VAR *var= (VAR*) v;
  my_free(var->str_val);
  if (var->alloced)
    my_free(var);
}

C_MODE_END

void var_check_int(VAR *v)
{
  char *endptr;
  char *str= v->str_val;
  
  /* Initially assume not a number */
  v->int_val= 0;
  v->is_int= false;
  v->int_dirty= false;
  if (!str) return;
  
  v->int_val = (int) strtol(str, &endptr, 10);
  /* It is an int if strtol consumed something up to end/space/tab */
  if (endptr > str && (!*endptr || *endptr == ' ' || *endptr == '\t'))
    v->is_int= true;
}


VAR *var_init(VAR *v, const char *name, size_t name_len, const char *val,
              size_t val_len)
{
  size_t val_alloc_len;
  VAR *tmp_var;
  if (!name_len && name)
    name_len = strlen(name);
  if (!val_len && val)
    val_len = strlen(val) ;
  if (!val)
    val_len= 0;
  val_alloc_len = val_len + 16; /* room to grow */
  if (!(tmp_var=v) && !(tmp_var = (VAR*)my_malloc(PSI_NOT_INSTRUMENTED,
                                                  sizeof(*tmp_var)
                                                  + name_len+2, MYF(MY_WME))))
    die("Out of memory");

  if (name != NULL)
  {
    tmp_var->name= reinterpret_cast<char*>(tmp_var) + sizeof(*tmp_var);
    memcpy(tmp_var->name, name, name_len);
    tmp_var->name[name_len]= 0;
  }
  else
    tmp_var->name= NULL;

  tmp_var->alloced = (v == 0);

  if (!(tmp_var->str_val = (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                                            val_alloc_len+1, MYF(MY_WME))))
    die("Out of memory");

  if (val)
    memcpy(tmp_var->str_val, val, val_len);
  tmp_var->str_val[val_len]= 0;

  var_check_int(tmp_var);
  tmp_var->name_len = name_len;
  tmp_var->str_val_len = val_len;
  tmp_var->alloced_len = val_alloc_len;
  return tmp_var;
}


VAR* var_from_env(const char *name, const char *def_val)
{
  const char *tmp;
  VAR *v;
  if (!(tmp = getenv(name)))
    tmp = def_val;

  v = var_init(0, name, strlen(name), tmp, strlen(tmp));
  my_hash_insert(&var_hash, (uchar*)v);
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

    if (!(v = (VAR*) my_hash_search(&var_hash, (const uchar*) save_var_name,
                                    length)))
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
    v->int_dirty= false;
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
  if ((v = (VAR*)my_hash_search(&var_hash, (const uchar *) name, len)))
    return v;
  v = var_init(0, name, len, "", 0);
  my_hash_insert(&var_hash, (uchar*)v);
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
    if (v->int_dirty)
    {
      sprintf(v->str_val, "%d", v->int_val);
      v->int_dirty=false;
      v->str_val_len= strlen(v->str_val);
    }
    /* setenv() expects \0-terminated strings */
    DBUG_ASSERT(v->name[v->name_len] == 0);
    setenv(v->name, v->str_val, 1);
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
  var_set_string("$mysql_errname", get_errname_from_code(sql_errno));
}

/* Functions to handle --disable and --enable properties */

void set_once_property(enum_prop prop, my_bool val)
{
  property &pr= prop_list[prop];
  pr.set= 1;
  pr.old= *pr.var;
  *pr.var= val;
  var_set_int(pr.env_name, (val != pr.reverse));
  once_property= TRUE;
}

void set_property(st_command *command, enum_prop prop, my_bool val)
{
  char* p= command->first_argument;
  if (p && !strcmp (p, "ONCE")) 
  {
    command->last_argument= p + 4;
    set_once_property(prop, val);
    return;
  }
  property &pr= prop_list[prop];
  *pr.var= val;
  pr.set= 0;
  var_set_int(pr.env_name, (val != pr.reverse));
}

void revert_properties()
{
  if (! once_property)
    return;
  for (int i= 0; i < (int) P_MAX; i++) 
  {
    property &pr= prop_list[i];
    if (pr.set) 
    {
      *pr.var= pr.old;
      pr.set= 0;
      var_set_int(pr.env_name, (pr.old != pr.reverse));
    }
  }
  once_property=FALSE;
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
  MYSQL_RES *res= NULL;
  MYSQL_ROW row;
  MYSQL* mysql = &cur_con->mysql;
  DYNAMIC_STRING ds_query;
  DBUG_ENTER("var_query_set");

  /* Only white space or ) allowed past ending ` */
  while (end > query && *end != '`')
  {
    if (*end && (*end != ' ' && *end != '\t' && *end != '\n' && *end != ')'))
      die("Spurious text after `query` expression");
    --end;
  }

  if (query == end)
    die("Syntax error in query, missing '`'");
  ++query;

  /* Eval the query, thus replacing all environment variables */
  init_dynamic_string(&ds_query, 0, (end - query) + 32, 256);
  do_eval(&ds_query, query, end, FALSE);

  if (mysql_real_query(mysql, ds_query.str,
                       static_cast<ulong>(ds_query.length)))
  {
    handle_error (curr_command, mysql_errno(mysql), mysql_error(mysql),
                  mysql_sqlstate(mysql), &ds_res);
    /* If error was acceptable, return empty string */
    dynstr_free(&ds_query);
    eval_expr(var, "", 0);
    DBUG_VOID_RETURN;
  }
  
  if (!(res= mysql_store_result(mysql)))
    die("Query '%s' didn't return a result set", ds_query.str);
  dynstr_free(&ds_query);

  if ((row= mysql_fetch_row(res)) && row[0])
  {
    /*
      Concatenate all fields in the first row with tab in between
      and assign that string to the $variable
    */
    DYNAMIC_STRING result;
    uint i;
    ulong *lengths;

    init_dynamic_string(&result, "", 512, 512);
    lengths= mysql_fetch_lengths(res);
    for (i= 0; i < mysql_num_fields(res); i++)
    {
      if (row[i])
      {
        /* Add column to tab separated string */
	char *val= row[i];
	size_t len= lengths[i];
	
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
	  replace_strings_append(glob_replace, &result, val, len);
	else
	  dynstr_append_mem(&result, val, len);
      }
      dynstr_append_mem(&result, "\t", 1);
    }
    end= result.str + result.length-1;
    /* Evaluation should not recurse via backtick */
    eval_expr(var, result.str, (const char**) &end, false, false);
    dynstr_free(&result);
  }
  else
    eval_expr(var, "", 0);

  mysql_free_result(res);
  DBUG_VOID_RETURN;
}


static void
set_result_format_version(ulong new_version)
{
  switch (new_version){
  case 1:
    /* The first format */
    break;
  case 2:
    /* New format that also writes comments and empty lines
       from test file to result */
    break;
  default:
    die("Version format %lu has not yet been implemented", new_version);
    break;
  }
  opt_result_format_version= new_version;
}


/*
  Set the result format version to use when generating
  the .result file
*/

static void
do_result_format_version(struct st_command *command)
{
  long version;
  static DYNAMIC_STRING ds_version;
  const struct command_arg result_format_args[] = {
    {"version", ARG_STRING, TRUE, &ds_version, "Version to use"}
  };

  DBUG_ENTER("do_result_format_version");

  check_command_args(command, command->first_argument,
                     result_format_args,
                     sizeof(result_format_args)/sizeof(struct command_arg),
                     ',');

  /* Convert version  number to int */
  if (!str2int(ds_version.str, 10, (long) 0, (long) INT_MAX, &version))
    die("Invalid version number: '%s'", ds_version.str);

  set_result_format_version(version);

  dynstr_append(&ds_res, "result_format: ");
  dynstr_append_mem(&ds_res, ds_version.str, ds_version.length);
  dynstr_append(&ds_res, "\n");
  dynstr_free(&ds_version);
  DBUG_VOID_RETURN;
}

/* List of error names to error codes */
typedef struct
{
  const char *name;
  uint        code;
  const char *text;
} st_error;

static st_error global_error_names[] =
{
  { "<No error>", (uint)-1, "" },
#include <mysqld_ername.h>
  { 0, 0, 0 }
};

uint get_errcode_from_name(char *, char *);

/*

  This function is useful when one needs to convert between error numbers and  error strings

  SYNOPSIS
  var_set_convert_error(struct st_command *command,VAR *var)

  DESCRIPTION
  let $var=convert_error(ER_UNKNOWN_ERROR); 
  let $var=convert_error(1234); 
  
  The variable var will be populated with error number if the argument is string.
  The variable var will be populated with error string if the argument is number.

*/
void var_set_convert_error(struct st_command *command,VAR *var)
{
  char *last;
  char *first=command->query;
  const char *err_name;
    
  DBUG_ENTER("var_set_convert_error");

  DBUG_PRINT("info", ("query: %s", command->query));

  /* the command->query contains the statement convert_error(1234) */ 
  first=strchr(command->query,'(') + 1;
  last=strchr(command->query,')');


  if( last == first )  /* denoting an empty string */
  {
    eval_expr(var,"0",0);
    DBUG_VOID_RETURN;
  }
  

  /* if the string is an error string , it starts with 'E' as is the norm*/
  if ( *first == 'E')    
  {
    char str[100];
    uint num;
    num=get_errcode_from_name(first, last);
    sprintf(str,"%i",num);
    eval_expr(var,str,0);
  }
  else  if (my_isdigit(charset_info, *first ))/* if the error is a number */
  {
    long int err;

    err=strtol(first,&last,0);
    err_name = get_errname_from_code(err);
    eval_expr(var,err_name,0);
  }
  else
  {
    die("Invalid error in input");
  }

  DBUG_VOID_RETURN;
}

/*
  Set variable from the result of a field in a query

  This function is useful when checking for a certain value
  in the output from a query that can't be restricted to only
  return some values. A very good example of that is most SHOW
  commands.

  SYNOPSIS
  var_set_query_get_value()

  DESCRIPTION
  let $variable= query_get_value(<query to run>,<column name>,<row no>);

  <query to run> -    The query that should be sent to the server
  <column name> -     Name of the column that holds the field be compared
                      against the expected value
  <row no> -          Number of the row that holds the field to be
                      compared against the expected value

*/

void var_set_query_get_value(struct st_command *command, VAR *var)
{
  long row_no;
  int col_no= -1;
  MYSQL_RES* res= NULL;
  MYSQL* mysql= &cur_con->mysql;

  static DYNAMIC_STRING ds_query;
  static DYNAMIC_STRING ds_col;
  static DYNAMIC_STRING ds_row;
  const struct command_arg query_get_value_args[] = {
    {"query", ARG_STRING, TRUE, &ds_query, "Query to run"},
    {"column name", ARG_STRING, TRUE, &ds_col, "Name of column"},
    {"row number", ARG_STRING, TRUE, &ds_row, "Number for row"}
  };

  DBUG_ENTER("var_set_query_get_value");

  strip_parentheses(command);
  DBUG_PRINT("info", ("query: %s", command->query));
  check_command_args(command, command->first_argument, query_get_value_args,
                     sizeof(query_get_value_args)/sizeof(struct command_arg),
                     ',');

  DBUG_PRINT("info", ("query: %s", ds_query.str));
  DBUG_PRINT("info", ("col: %s", ds_col.str));

  /* Convert row number to int */
  if (!str2int(ds_row.str, 10, (long) 0, (long) INT_MAX, &row_no))
    die("Invalid row number: '%s'", ds_row.str);
  DBUG_PRINT("info", ("row: %s, row_no: %ld", ds_row.str, row_no));
  dynstr_free(&ds_row);

  /* Remove any surrounding "'s from the query - if there is any */
  if (strip_surrounding(ds_query.str, '"', '"'))
    die("Mismatched \"'s around query '%s'", ds_query.str);

  /* Run the query */
  if (mysql_real_query(mysql, ds_query.str,
                       static_cast<ulong>(ds_query.length)))
  {
    handle_error (curr_command, mysql_errno(mysql), mysql_error(mysql),
                  mysql_sqlstate(mysql), &ds_res);
    /* If error was acceptable, return empty string */
    dynstr_free(&ds_query);
    dynstr_free(&ds_col);
    eval_expr(var, "", 0);
    DBUG_VOID_RETURN;
  }

  if (!(res= mysql_store_result(mysql)))
    die("Query '%s' didn't return a result set", ds_query.str);

  {
    /* Find column number from the given column name */
    uint i;
    uint num_fields= mysql_num_fields(res);
    MYSQL_FIELD *fields= mysql_fetch_fields(res);

    for (i= 0; i < num_fields; i++)
    {
      if (strcmp(fields[i].name, ds_col.str) == 0 &&
          strlen(fields[i].name) == ds_col.length)
      {
        col_no= i;
        break;
      }
    }
    if (col_no == -1)
    {
      mysql_free_result(res);
      die("Could not find column '%s' in the result of '%s'",
          ds_col.str, ds_query.str);
    }
    DBUG_PRINT("info", ("Found column %d with name '%s'",
                        i, fields[i].name));
  }
  dynstr_free(&ds_col);

  {
    /* Get the value */
    MYSQL_ROW row;
    long rows= 0;
    const char* value= "No such row";

    while ((row= mysql_fetch_row(res)))
    {
      if (++rows == row_no)
      {

        DBUG_PRINT("info", ("At row %ld, column %d is '%s'",
                            row_no, col_no, row[col_no]));
        /* Found the row to get */
        if (row[col_no])
          value= row[col_no];
        else
          value= "NULL";

        break;
      }
    }
    eval_expr(var, value, 0, false, false);
  }
  dynstr_free(&ds_query);
  mysql_free_result(res);

  DBUG_VOID_RETURN;
}


void var_copy(VAR *dest, VAR *src)
{
  dest->int_val= src->int_val;
  dest->is_int= src->is_int;
  dest->int_dirty= src->int_dirty;

  /* Alloc/realloc data for str_val in dest */
  if (dest->alloced_len < src->alloced_len &&
      !(dest->str_val= dest->str_val
        ? (char*)my_realloc(PSI_NOT_INSTRUMENTED,
                            dest->str_val, src->alloced_len, MYF(MY_WME))
        : (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                           src->alloced_len, MYF(MY_WME))))
    die("Out of memory");
  else
    dest->alloced_len= src->alloced_len;

  /* Copy str_val data to dest */
  dest->str_val_len= src->str_val_len;
  if (src->str_val_len)
    memcpy(dest->str_val, src->str_val, src->str_val_len);
}


void eval_expr(VAR *v, const char *p, const char **p_end,
               bool open_end, bool do_eval)
{

  DBUG_ENTER("eval_expr");
  DBUG_PRINT("enter", ("p: '%s'", p));

  /* Skip to treat as pure string if no evaluation */
  if (! do_eval)
    goto NO_EVAL;
  
  if (*p == '$')
  {
    VAR *vp;
    const char* expected_end= *p_end; // Remember var end
    if ((vp= var_get(p, p_end, 0, 0)))
      var_copy(v, vp);

    /* Apparently it is not safe to assume null-terminated string */
    v->str_val[v->str_val_len]= 0;

    /* Make sure there was just a $variable and nothing else */
    const char* end= *p_end + 1;
    if (end < expected_end && !open_end)
      die("Found junk '%.*s' after $variable in expression",
          (int)(expected_end - end - 1), end);

    DBUG_VOID_RETURN;
  }

  if (*p == '`')
  {
    var_query_set(v, p, p_end);
    DBUG_VOID_RETURN;
  }

  {
    /* Check if this is a "let $var= query_get_value()" */
    const char* get_value_str= "query_get_value";
    const size_t len= strlen(get_value_str);
    if (strncmp(p, get_value_str, len)==0)
    {
      struct st_command command;
      memset(&command, 0, sizeof(command));
      command.query= (char*)p;
      command.first_word_len= len;
      command.first_argument= command.query + len;
      command.end= (char*)*p_end;
      var_set_query_get_value(&command, v);
      DBUG_VOID_RETURN;
    }
    /* Check if this is a "let $var= convert_error()" */
    const char* get_value_str1= "convert_error";
    const size_t len1= strlen(get_value_str1);
    if (strncmp(p, get_value_str1, len1)==0)
    {
      struct st_command command;
      memset(&command, 0, sizeof(command));
      command.query= (char*)p;
      command.first_word_len= len;
      command.first_argument= command.query + len;
      command.end= (char*)*p_end;
      var_set_convert_error(&command, v);
      DBUG_VOID_RETURN;
    }
  }

 NO_EVAL:
  {
    size_t new_val_len = (p_end && *p_end) ?
      static_cast<size_t>(*p_end - p) : strlen(p);
    if (new_val_len + 1 >= v->alloced_len)
    {
      static size_t MIN_VAR_ALLOC= 32;
      v->alloced_len = (new_val_len < MIN_VAR_ALLOC - 1) ?
        MIN_VAR_ALLOC : new_val_len + 1;
      if (!(v->str_val =
            v->str_val ?
            (char*)my_realloc(PSI_NOT_INSTRUMENTED,
                              v->str_val, v->alloced_len+1, MYF(MY_WME)) :
            (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                             v->alloced_len+1, MYF(MY_WME))))
        die("Out of memory");
    }
    v->str_val_len = new_val_len;
    memcpy(v->str_val, p, new_val_len);
    v->str_val[new_val_len] = 0;
    var_check_int(v);
  }
  DBUG_VOID_RETURN;
}


int open_file(const char *name)
{
  char buff[FN_REFLEN];
  size_t length;
  DBUG_ENTER("open_file");
  DBUG_PRINT("enter", ("name: %s", name));

  my_bool file_exists= false;
  /* Extract path from current file and try it as base first */
  if (dirname_part(buff, cur_file->file_name, &length))
  {
    strxmov(buff, buff, name, NullS);
    if (access(buff, F_OK) == 0)
    {
      DBUG_PRINT("info", ("The file exists"));
      name= buff;
      file_exists= true;
    }
  }

  if (!test_if_hard_path(name) && !file_exists)
  {
    strxmov(buff, opt_basedir, name, NullS);
    name=buff;
  }
  fn_format(buff, name, "", "", MY_UNPACK_FILENAME);

  if (cur_file == file_stack_end)
    die("Source directives are nesting too deep");
  cur_file++;
  if (!(cur_file->file = fopen(buff, "rb")))
  {
    cur_file--;
    die("Could not open '%s' for reading, errno: %d", buff, errno);
  }
  cur_file->file_name= my_strdup(PSI_NOT_INSTRUMENTED,
                                 buff, MYF(MY_FAE));
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
  DBUG_VOID_RETURN;
}


FILE* my_popen(DYNAMIC_STRING *ds_cmd, const char *mode,
               struct st_command *command)
{
#if _WIN32
  /*
    --execw is for tests executing commands containing non-ASCII characters.

    To correctly start such a program on Windows, we need to use the "wide"
    version of popen, with prior translation of the command line from
    the file character set to wide string. We use the current value
    of --character_set as a file character set, so before using --execw
    make sure to set --character_set properly.

    If we use the non-wide version of popen, Windows internally
    converts command line from the current ANSI code page to wide string.
    In case when character set of the command line does not match the
    current ANSI code page, non-ASCII characters get garbled in most cases.

    On Linux, the command line passed to popen() is considered
    as a binary string, no any internal to-wide and from-wide
    character set conversion happens, so we don't need to do anything.
    On Linux --execw is just a synonym to --exec.

    For simplicity, assume that  command line is limited to 4KB
    (like in cmd.exe) and that mode at most 10 characters.
  */
  if (command->type == Q_EXECW)
  {
    wchar_t wcmd[4096];
    wchar_t wmode[10];
    const char *cmd= ds_cmd->str;
    uint dummy_errors;
    size_t len;
    len= my_convert((char *) wcmd, sizeof(wcmd) - sizeof(wcmd[0]),
                    &my_charset_utf16le_bin,
                    ds_cmd->str, strlen(ds_cmd->str), charset_info,
                    &dummy_errors);
    wcmd[len / sizeof(wchar_t)]= 0;
    len= my_convert((char *) wmode, sizeof(wmode) - sizeof(wmode[0]),
                    &my_charset_utf16le_bin,
                    mode, strlen(mode), charset_info, &dummy_errors);
    wmode[len / sizeof(wchar_t)]= 0;
    return _wpopen(wcmd, wmode);
  }
#endif /* _WIN32 */

  return popen(ds_cmd->str, mode);
}


static void init_builtin_echo(void)
{
#ifdef _WIN32
  size_t echo_length;

  /* Look for "echo.exe" in same dir as mysqltest was started from */
  dirname_part(builtin_echo, my_progname, &echo_length);
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
                   const char *search_str, size_t search_len,
                   const char *replace_str, size_t replace_len)
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

#ifdef _WIN32
  /* Replace /dev/null with NUL */
  while(replace(&ds_cmd, "/dev/null", 9, "NUL", 3) == 0)
    ;
  /* Replace "closed stdout" with non existing output fd */
  while(replace(&ds_cmd, ">&-", 3, ">&4", 3) == 0)
    ;
#endif

  /* exec command is interpreted externally and will not take newlines */
  while(replace(&ds_cmd, "\n", 1, " ", 1) == 0)
    ;
  
  DBUG_PRINT("info", ("Executing '%s' as '%s'",
                      command->first_argument, ds_cmd.str));

  if (!(res_file= my_popen(&ds_cmd, "r", command)) && command->abort_on_error)
  {
    dynstr_free(&ds_cmd);
    die("popen(\"%s\", \"r\") failed", command->first_argument);
  }

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
#ifdef _WIN32
    uint status= WEXITSTATUS(error);
#else
    uint status= 0;
    // Do the same as many shells here: show SIGKILL as 137
    if (WIFEXITED(error))
      status= WEXITSTATUS(error);
    else if (WIFSIGNALED(error))
      status= 0x80 + WTERMSIG(error);
#endif

    int i;

    if (command->abort_on_error)
    {
      log_msg("exec of '%s' failed, error: %d, status: %d, errno: %d",
              ds_cmd.str, error, status, errno);
      dynstr_free(&ds_cmd);
      die("command \"%s\" failed\n\nOutput from before failure:\n%s\n",
          command->first_argument, ds_res.str);
    }

    DBUG_PRINT("info",
               ("error: %d, status: %d", error, status));

    i= match_expected_error(command, status, NULL);

    if (i >= 0)
      DBUG_PRINT("info", ("command \"%s\" failed with expected error: %d",
                          command->first_argument, status));
    else
    {
      dynstr_free(&ds_cmd);
      if (command->expected_errors.count > 0)
        die("command \"%s\" failed with wrong error: %d",
            command->first_argument, status);
    }
  }
  else if (command->expected_errors.err[0].type == ERR_ERRNO &&
           command->expected_errors.err[0].code.errnum != 0)
  {
    /* Error code we wanted was != 0, i.e. not an expected success */
    log_msg("exec of '%s failed, error: %d, errno: %d",
            ds_cmd.str, error, errno);
    dynstr_free(&ds_cmd);
    die("command \"%s\" succeeded - should have failed with errno %d...",
        command->first_argument, command->expected_errors.err[0].code.errnum);
  }
  // Save error code
  {
    const char var_name[]= "__error";
    char buf[10];
    size_t err_len= my_snprintf(buf, 10, "%u", error);
    buf[err_len > 9 ? 9 : err_len]= '0';
    var_set(var_name, var_name + 7, buf, buf + err_len);
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
  op    operation to perform on the var

  DESCRIPTION
  dec $var_name
  inc $var_name

*/

int do_modify_var(struct st_command *command,
                  enum enum_operator op)
{
  const char *p= command->first_argument;
  VAR* v;
  if (!*p)
    die("Missing argument to %.*s",
        static_cast<int>(command->first_word_len), command->query);
  if (*p != '$')
    die("The argument to %.*s must be a variable (start with $)",
        static_cast<int>(command->first_word_len), command->query);
  v= var_get(p, &p, 1, 0);
  if (! v->is_int)
    die("Cannot perform inc/dec on a non-numeric value");
  switch (op) {
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
  v->int_dirty= true;
  command->last_argument= (char*)++p;
  return 0;
}


/*
  SYNOPSIS
  set_wild_chars
  set  true to set * etc. as wild char, false to reset

  DESCRIPTION
  Auxiliary function to set "our" wild chars before calling wild_compare
  This is needed because the default values are changed to SQL syntax
  in mysqltest_embedded.
*/

void set_wild_chars (my_bool set)
{
  static char old_many= 0, old_one, old_prefix;

  if (set) 
  {
    if (wild_many == '*') return; // No need
    old_many= wild_many;
    old_one= wild_one;
    old_prefix= wild_prefix;
    wild_many= '*';
    wild_one= '?';
    wild_prefix= 0;
  }
  else 
  {
    if (! old_many) return;	// Was not set
    wild_many= old_many;
    wild_one= old_one;
    wild_prefix= old_prefix;
  }
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
  /*
    Some anti-virus programs hold access to files for a short time
    even after the application/server quit. During testing, sleep
    5 seconds and then retry once more to avoid spurious test failures.
    Also on slow/loaded machines the file system may need to catch up.
  */
  if (error)
  {
    my_sleep(5 * 1000 * 1000);
    error= my_delete(ds_filename.str, MYF(0)) != 0;
  }
  handle_command_error(command, error);
  dynstr_free(&ds_filename);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  do_remove_files_wildcard
  command	called command

  DESCRIPTION
  remove_files_wildcard <directory> [<file_name_pattern>]
  Remove the files in <directory> optionally matching <file_name_pattern>
*/

void do_remove_files_wildcard(struct st_command *command)
{
  int error= 0;
  uint i;
  MY_DIR *dir_info;
  FILEINFO *file;
  char dir_separator[2];
  static DYNAMIC_STRING ds_directory;
  static DYNAMIC_STRING ds_wild;
  static DYNAMIC_STRING ds_file_to_remove;
  char dirname[FN_REFLEN];
  
  const struct command_arg rm_args[] = {
    { "directory", ARG_STRING, TRUE, &ds_directory,
      "Directory containing files to delete" },
    { "filename", ARG_STRING, FALSE, &ds_wild, "File pattern to delete" }
  };
  DBUG_ENTER("do_remove_files_wildcard");

  check_command_args(command, command->first_argument,
                     rm_args, sizeof(rm_args)/sizeof(struct command_arg),
                     ' ');
  fn_format(dirname, ds_directory.str, "", "", MY_UNPACK_FILENAME);

  DBUG_PRINT("info", ("listing directory: %s", dirname));
  /* Note that my_dir sorts the list if not given any flags */
  if (!(dir_info= my_dir(dirname, MYF(MY_DONT_SORT | MY_WANT_STAT))))
  {
    error= 1;
    goto end;
  }
  init_dynamic_string(&ds_file_to_remove, dirname, 1024, 1024);
  dir_separator[0]= FN_LIBCHAR;
  dir_separator[1]= 0;
  dynstr_append(&ds_file_to_remove, dir_separator);
  
  /* Set default wild chars for wild_compare, is changed in embedded mode */
  set_wild_chars(1);
  
  size_t length;
  /* Storing the length of the path to the file, so it can be reused */
  length= ds_file_to_remove.length;
  for (i= 0; i < (uint) dir_info->number_off_files; i++)
  {
    ds_file_to_remove.length= length;
    file= dir_info->dir_entry + i;
    /* Remove only regular files, i.e. no directories etc. */
    /* if (!MY_S_ISREG(file->mystat->st_mode)) */
    /* MY_S_ISREG does not work here on Windows, just skip directories */
    if (MY_S_ISDIR(file->mystat->st_mode))
      continue;
    if (ds_wild.length &&
        wild_compare(file->name, ds_wild.str, 0))
      continue;
    /* Not required as the var ds_file_to_remove.length already has the
       length in canonnicalized form */
    /* ds_file_to_remove.length= ds_directory.length + 1;
    ds_file_to_remove.str[ds_directory.length + 1]= 0; */
    dynstr_append(&ds_file_to_remove, file->name);
    DBUG_PRINT("info", ("removing file: %s", ds_file_to_remove.str));
    error= my_delete(ds_file_to_remove.str, MYF(0)) != 0;
    if (error)
      break;
  }
  set_wild_chars(0);
  my_dirend(dir_info);

end:
  handle_command_error(command, error);
  dynstr_free(&ds_directory);
  dynstr_free(&ds_wild);
  dynstr_free(&ds_file_to_remove);
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
  /* MY_HOLD_ORIGINAL_MODES prevents attempts to chown the file */
  error= (my_copy(ds_from_file.str, ds_to_file.str,
                  MYF(MY_DONT_OVERWRITE_FILE | MY_HOLD_ORIGINAL_MODES)) != 0);
  /*
    Some anti-virus programs hold access to files for a short time
    even after the application/server quit. During testing, sleep
    5 seconds and then retry once more to avoid spurious test failures.
    Also on slow/loaded machines the file system may need to catch up.
  */
  if (error)
  {
    my_sleep(5 * 1000 * 1000);
    error= (my_copy(ds_from_file.str, ds_to_file.str,
                    MYF(MY_DONT_OVERWRITE_FILE | MY_HOLD_ORIGINAL_MODES)) != 0);
  }
  handle_command_error(command, error);
  dynstr_free(&ds_from_file);
  dynstr_free(&ds_to_file);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  move_file_by_copy_delete
  from  path of source
  to    path of destination

  DESCRIPTION
  Move <from_file> to <to_file>
  Auxiliary function for copying <from_file> to <to_file> followed by
  deleting <to_file>.
*/

static int move_file_by_copy_delete(const char *from, const char *to)
{
  int error_copy,error_delete;
  error_copy= (my_copy(from,to,
                  MYF(MY_HOLD_ORIGINAL_MODES)) != 0);
  /*
    Some anti-virus programs hold access to files for a short time
    even after the application/server quit. During testing, sleep
    5 seconds and then retry once more to avoid spurious test failures.
    Also on slow/loaded machines the file system may need to catch up.
  */
  if (error_copy)
  {
    my_sleep(5 * 1000 * 1000);
    error_copy= (my_copy(from,to,
                  MYF(MY_HOLD_ORIGINAL_MODES)) != 0);
  }
  if (error_copy)
  {
    return error_copy;
  }

  error_delete= my_delete(from, MYF(0)) != 0;
  /*
    Some anti-virus programs hold access to files for a short time
    even after the application/server quit. During testing, sleep
    5 seconds and then retry once more to avoid spurious test failures.
    Also on slow/loaded machines the file system may need to catch up.
  */
  if (error_delete)
  {
    my_sleep(5 * 1000 * 1000);
    error_delete= my_delete(from, MYF(0)) != 0;
  }
  /*
    If deleting the source file fails, rollback by deleting the
    redundant copy at the destinatiion.
  */
  if (error_delete)
  {
    my_delete(to, MYF(0));
  }
  return error_delete;
}

/*
  SYNOPSIS
  do_move_file
  command	command handle

  DESCRIPTION
  move_file <from_file> <to_file>
  Move <from_file> to <to_file>
*/

void do_move_file(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_from_file;
  static DYNAMIC_STRING ds_to_file;
  const struct command_arg move_file_args[] = {
    { "from_file", ARG_STRING, TRUE, &ds_from_file, "Filename to move from" },
    { "to_file", ARG_STRING, TRUE, &ds_to_file, "Filename to move to" }
  };
  DBUG_ENTER("do_move_file");

  check_command_args(command, command->first_argument,
                     move_file_args,
                     sizeof(move_file_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("info", ("Move %s to %s", ds_from_file.str, ds_to_file.str));
  error= (my_rename(ds_from_file.str, ds_to_file.str,
                    MYF(0)) != 0);
  /*
    Use my_copy() followed by my_delete() for moving a file instead of
    my_rename() when my_errno is EXDEV. This is because my_rename() fails
    with the error "Invalid cross-device link" while moving a file between
    locations having different filesystems in some operating systems.
  */
  if (error && (my_errno() == EXDEV))
  {
    error= move_file_by_copy_delete(ds_from_file.str, ds_to_file.str);
  }
  else if (error)
  {
    /*
      Some anti-virus programs hold access to files for a short time
      even after the application/server quit. During testing, sleep
      5 seconds and then retry once more to avoid spurious test failures.
      Also on slow/loaded machines the file system may need to catch up.
    */
    my_sleep(5 * 1000 * 1000);
    error= (my_rename(ds_from_file.str, ds_to_file.str,
                      MYF(0)) != 0);
    if (error && (my_errno() == EXDEV))
    {
      error= move_file_by_copy_delete(ds_from_file.str, ds_to_file.str);
    }
  }
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
  chmod <octal> <file_name>
  Change file permission of <file_name>

*/

void do_chmod_file(struct st_command *command)
{
  long mode= 0;
  int err_code;
  static DYNAMIC_STRING ds_mode;
  static DYNAMIC_STRING ds_file;
  const struct command_arg chmod_file_args[] = {
    { "mode", ARG_STRING, TRUE, &ds_mode, "Mode of file(octal) ex. 0660"}, 
    { "filename", ARG_STRING, TRUE, &ds_file, "Filename of file to modify" }
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
  err_code= chmod(ds_file.str, mode);
  if (err_code < 0)
    err_code= 1;
  handle_command_error(command, err_code);
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
  SYNOPSIS
  do_mkdir
  command	called command

  DESCRIPTION
  mkdir <dir_name>
  Create the directory <dir_name>
*/

void do_mkdir(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_dirname;
  const struct command_arg mkdir_args[] = {
    {"dirname", ARG_STRING, TRUE, &ds_dirname, "Directory to create"}
  };
  DBUG_ENTER("do_mkdir");

  check_command_args(command, command->first_argument,
                     mkdir_args, sizeof(mkdir_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("info", ("creating directory: %s", ds_dirname.str));
  error= my_mkdir(ds_dirname.str, 0777, MYF(0)) != 0;
  handle_command_error(command, error);
  dynstr_free(&ds_dirname);
  DBUG_VOID_RETURN;
}

/*
  SYNOPSIS
  do_rmdir
  command	called command

  DESCRIPTION
  rmdir <dir_name>
  Remove the empty directory <dir_name>
*/

void do_rmdir(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_dirname;
  const struct command_arg rmdir_args[] = {
    {"dirname", ARG_STRING, TRUE, &ds_dirname, "Directory to remove"}
  };
  DBUG_ENTER("do_rmdir");

  check_command_args(command, command->first_argument,
                     rmdir_args, sizeof(rmdir_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("info", ("removing directory: %s", ds_dirname.str));
  error= rmdir(ds_dirname.str) != 0;
  handle_command_error(command, error);
  dynstr_free(&ds_dirname);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  get_list_files
  ds          output
  ds_dirname  dir to list
  ds_wild     wild-card file pattern (can be empty)

  DESCRIPTION
  list all entries in directory (matching ds_wild if given)
*/

static int get_list_files(DYNAMIC_STRING *ds, const DYNAMIC_STRING *ds_dirname,
                          const DYNAMIC_STRING *ds_wild)
{
  uint i;
  MY_DIR *dir_info;
  FILEINFO *file;
  DBUG_ENTER("get_list_files");

  DBUG_PRINT("info", ("listing directory: %s", ds_dirname->str));
  /* Note that my_dir sorts the list if not given any flags */
  if (!(dir_info= my_dir(ds_dirname->str, MYF(0))))
    DBUG_RETURN(1);
  set_wild_chars(1);
  for (i= 0; i < (uint) dir_info->number_off_files; i++)
  {
    file= dir_info->dir_entry + i;
    if (file->name[0] == '.' &&
        (file->name[1] == '\0' ||
         (file->name[1] == '.' && file->name[2] == '\0')))
      continue;                               /* . or .. */
    if (ds_wild && ds_wild->length &&
        wild_compare(file->name, ds_wild->str, 0))
      continue;
    replace_dynstr_append(ds, file->name);
    dynstr_append(ds, "\n");
  }
  set_wild_chars(0);
  my_dirend(dir_info);
  DBUG_RETURN(0);
}


/*
  SYNOPSIS
  do_list_files
  command	called command

  DESCRIPTION
  list_files <dir_name> [<file_name>]
  List files and directories in directory <dir_name> (like `ls`)
  [Matching <file_name>, where wild-cards are allowed]
*/

static void do_list_files(struct st_command *command)
{
  int error;
  static DYNAMIC_STRING ds_dirname;
  static DYNAMIC_STRING ds_wild;
  const struct command_arg list_files_args[] = {
    {"dirname", ARG_STRING, TRUE, &ds_dirname, "Directory to list"},
    {"file", ARG_STRING, FALSE, &ds_wild, "Filename (incl. wildcard)"}
  };
  DBUG_ENTER("do_list_files");
  command->used_replace= 1;
  
  check_command_args(command, command->first_argument,
                     list_files_args,
                     sizeof(list_files_args)/sizeof(struct command_arg), ' ');

  error= get_list_files(&ds_res, &ds_dirname, &ds_wild);
  handle_command_error(command, error);
  dynstr_free(&ds_dirname);
  dynstr_free(&ds_wild);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  do_list_files_write_file_command
  command       called command
  append        append file, or create new

  DESCRIPTION
  list_files_{write|append}_file <filename> <dir_name> [<match_file>]
  List files and directories in directory <dir_name> (like `ls`)
  [Matching <match_file>, where wild-cards are allowed]

  Note: File will be truncated if exists and append is not true.
*/

static void do_list_files_write_file_command(struct st_command *command,
                                             my_bool append)
{
  int error;
  static DYNAMIC_STRING ds_content;
  static DYNAMIC_STRING ds_filename;
  static DYNAMIC_STRING ds_dirname;
  static DYNAMIC_STRING ds_wild;
  const struct command_arg list_files_args[] = {
    {"filename", ARG_STRING, TRUE, &ds_filename, "Filename for write"},
    {"dirname", ARG_STRING, TRUE, &ds_dirname, "Directory to list"},
    {"file", ARG_STRING, FALSE, &ds_wild, "Filename (incl. wildcard)"}
  };
  DBUG_ENTER("do_list_files_write_file");
  command->used_replace= 1;

  check_command_args(command, command->first_argument,
                     list_files_args,
                     sizeof(list_files_args)/sizeof(struct command_arg), ' ');

  init_dynamic_string(&ds_content, "", 1024, 1024);
  error= get_list_files(&ds_content, &ds_dirname, &ds_wild);
  handle_command_error(command, error);
  str_to_file2(ds_filename.str, ds_content.str, ds_content.length, append);
  dynstr_free(&ds_content);
  dynstr_free(&ds_filename);
  dynstr_free(&ds_dirname);
  dynstr_free(&ds_wild);
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
  DBUG_PRINT("enter", ("delimiter: %s, length: %u",
                       ds_delimiter->str, (uint) ds_delimiter->length));

  if (ds_delimiter->length > MAX_DELIMITER_LENGTH)
    die("Max delimiter length(%d) exceeded", MAX_DELIMITER_LENGTH);

  /* Read from file until delimiter is found */
  while (1)
  {
    c= my_getc(cur_file->file);

    if (c == '\n')
    {
      cur_file->lineno++;

      /* Skip newline from the same line as the command */
      if (start_lineno == (cur_file->lineno - 1))
        continue;
    }
    else if (start_lineno == cur_file->lineno)
    {
      /*
        No characters except \n are allowed on
        the same line as the command
      */
      die("Trailing characters found after command");
    }

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

  if (!append && access(ds_filename.str, F_OK) == 0)
  {
    /* The file should not be overwritten */
    die("File already exist: '%s'", ds_filename.str);
  }

  ds_content= command->content;
  /* If it hasn't been done already by a loop iteration, fill it in */
  if (! ds_content.str)
  {
    /* If no delimiter was provided, use EOF */
    if (ds_delimiter.length == 0)
      dynstr_set(&ds_delimiter, "EOF");

    init_dynamic_string(&ds_content, "", 1024, 1024);
    read_until_delimiter(&ds_content, &ds_delimiter);
    command->content= ds_content;
  }
  /* This function could be called even if "false", so check before printing */
  if (cur_block->ok)
  {
    DBUG_PRINT("info", ("Writing to file: %s", ds_filename.str));
    str_to_file2(ds_filename.str, ds_content.str, ds_content.length, append);
  }
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

  NOTE! Will fail if <file_name> exists

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
  int error;
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

  error= cat_file(&ds_res, ds_filename.str);
  handle_command_error(command, error);
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

  if (access(ds_filename.str, F_OK) != 0)
    die("command \"diff_files\" failed, file '%s' does not exist",
        ds_filename.str);

  if (access(ds_filename2.str, F_OK) != 0)
    die("command \"diff_files\" failed, file '%s' does not exist",
        ds_filename2.str);

  if ((error= compare_files(ds_filename.str, ds_filename2.str)) &&
      match_expected_error(command, error, NULL) < 0)
  {
    /* Compare of the two files failed, append them to output
       so the failure can be analyzed, but only if it was not
       expected to fail.
    */
    show_diff(&ds_res, ds_filename.str, ds_filename2.str);
    log_file.write(&ds_res);
    log_file.flush();
    dynstr_set(&ds_res, 0);
  }

  dynstr_free(&ds_filename);
  dynstr_free(&ds_filename2);
  handle_command_error(command, error);
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


/*
  SYNOPSIS
  do_send_quit
  command	called command

  DESCRIPTION
  Sends a simple quit command to the server for the named connection.

*/

void do_send_quit(struct st_command *command)
{
  char *p= command->first_argument, *name;
  struct st_connection *con;

  DBUG_ENTER("do_send_quit");
  DBUG_PRINT("enter",("name: '%s'",p));

  if (!*p)
    die("Missing connection name in send_quit");
  name= p;
  while (*p && !my_isspace(charset_info,*p))
    p++;

  if (*p)
    *p++= 0;
  command->last_argument= p;

  if (!(con= find_connection_by_name(name)))
    die("connection '%s' not found in connection pool", name);

  simple_command(&con->mysql,COM_QUIT,0,0,1);

  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
  do_change_user
  command       called command

  DESCRIPTION
  change_user [<user>], [<passwd>], [<db>]
  <user> - user to change to
  <passwd> - user password
  <db> - default database

  Changes the user and causes the database specified by db to become
  the default (current) database for the the current connection.

*/

void do_change_user(struct st_command *command)
{
  MYSQL *mysql = &cur_con->mysql;
  static DYNAMIC_STRING ds_user, ds_passwd, ds_db;
  const struct command_arg change_user_args[] = {
    { "user", ARG_STRING, FALSE, &ds_user, "User to connect as" },
    { "password", ARG_STRING, FALSE, &ds_passwd, "Password used when connecting" },
    { "database", ARG_STRING, FALSE, &ds_db, "Database to select after connect" },
  };

  DBUG_ENTER("do_change_user");

  check_command_args(command, command->first_argument,
                     change_user_args,
                     sizeof(change_user_args)/sizeof(struct command_arg),
                     ',');

  if (cur_con->stmt)
  {
    mysql_stmt_close(cur_con->stmt);
    cur_con->stmt= NULL;
  }

  if (!ds_user.length)
  {
    dynstr_set(&ds_user, mysql->user);

    if (!ds_passwd.length)
      dynstr_set(&ds_passwd, mysql->passwd);

    if (!ds_db.length)
      dynstr_set(&ds_db, mysql->db);
  }

  DBUG_PRINT("info",("connection: '%s' user: '%s' password: '%s' database: '%s'",
                      cur_con->name, ds_user.str, ds_passwd.str, ds_db.str));

  if (mysql_change_user(mysql, ds_user.str, ds_passwd.str, ds_db.str))
  {
    handle_error(curr_command, mysql_errno(mysql), mysql_error(mysql),
                 mysql_sqlstate(mysql), &ds_res);
    mysql->reconnect= 1;
    mysql_reconnect(&cur_con->mysql);
  }

  dynstr_free(&ds_user);
  dynstr_free(&ds_passwd);
  dynstr_free(&ds_db);

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
  File fd;
  FILE *res_file;
  char buf[FN_REFLEN];
  char temp_file_path[FN_REFLEN];
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

  ds_script= command->content;
  /* If it hasn't been done already by a loop iteration, fill it in */
  if (! ds_script.str)
  {
    /* If no delimiter was provided, use EOF */
    if (ds_delimiter.length == 0)
      dynstr_set(&ds_delimiter, "EOF");

    init_dynamic_string(&ds_script, "", 1024, 1024);
    read_until_delimiter(&ds_script, &ds_delimiter);
    command->content= ds_script;
  }

  /* This function could be called even if "false", so check before doing */
  if (cur_block->ok)
  {
    DBUG_PRINT("info", ("Executing perl: %s", ds_script.str));

    /* Create temporary file name */
    if ((fd= create_temp_file(temp_file_path, getenv("MYSQLTEST_VARDIR"),
                              "tmp", O_CREAT | O_SHARE | O_RDWR,
                              MYF(MY_WME))) < 0)
      die("Failed to create temporary file for perl command");
    my_close(fd, MYF(0));

    str_to_file(temp_file_path, ds_script.str, ds_script.length);

    /* Format the "perl <filename>" command */
    my_snprintf(buf, sizeof(buf), "perl %s", temp_file_path);

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

    /* Remove the temporary file, but keep it if perl failed */
    if (!error)
      my_delete(temp_file_path, MYF(0));

    /* Check for error code that indicates perl could not be started */
    int exstat= WEXITSTATUS(error);
#ifdef _WIN32
    if (exstat == 1)
      /* Text must begin 'perl not found' as mtr looks for it */
      abort_not_supported_test("perl not found in path or did not start");
#else
    if (exstat == 127)
      abort_not_supported_test("perl not found in path");
#endif
    else
      handle_command_error(command, exstat);
  }
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


void do_wait_for_slave_to_stop(struct st_command *c MY_ATTRIBUTE((unused)))
{
  static int SLAVE_POLL_INTERVAL= 300000;
  MYSQL* mysql = &cur_con->mysql;
  for (;;)
  {
    MYSQL_RES *res= NULL;
    MYSQL_ROW row;
    int done;

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


void do_sync_with_master2(struct st_command *command, long offset)
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL *mysql= &cur_con->mysql;
  char query_buf[FN_REFLEN+128];
  int timeout= 300; /* seconds */

  if (!master_pos.file[0])
    die("Calling 'sync_with_master' without calling 'save_master_pos'");

  sprintf(query_buf, "select master_pos_wait('%s', %ld, %d)",
          master_pos.file, master_pos.pos + offset, timeout);

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

  int result= -99;
  const char* result_str= row[0];
  if (result_str)
    result= atoi(result_str);

  mysql_free_result(res);

  if (!result_str || result < 0)
  {
    /* master_pos_wait returned NULL or < 0 */
    show_query(mysql, "SHOW MASTER STATUS");
    show_query(mysql, "SHOW SLAVE STATUS");
    show_query(mysql, "SHOW PROCESSLIST");
    fprintf(stderr, "analyze: sync_with_master\n");

    if (!result_str)
    {
      /*
        master_pos_wait returned NULL. This indicates that
        slave SQL thread is not started, the slave's master
        information is not initialized, the arguments are
        incorrect, or an error has occured
      */
      die("%.*s failed: '%s' returned NULL "\
          "indicating slave SQL thread failure",
          static_cast<int>(command->first_word_len), command->query, query_buf);

    }

    if (result == -1)
      die("%.*s failed: '%s' returned -1 "\
          "indicating timeout after %d seconds",
          static_cast<int>(command->first_word_len),
          command->query, query_buf, timeout);
    else
      die("%.*s failed: '%s' returned unknown result :%d",
          static_cast<int>(command->first_word_len),
          command->query, query_buf, result);
  }

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
  do_sync_with_master2(command, offset);
  return;
}


/*
  Wait for ndb binlog injector to be up-to-date with all changes
  done on the local mysql server
*/

static void
ndb_wait_for_binlog_injector(void)
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL *mysql = &cur_con->mysql;
  const char *query;
  ulong have_ndbcluster;
  if (mysql_query(mysql, query=
                  "select count(*) from information_schema.engines"
                  "  where engine = 'ndbcluster' and"
                  "        support in ('YES', 'DEFAULT')"))
    die("'%s' failed: %d %s", query,
        mysql_errno(mysql), mysql_error(mysql));
  if (!(res= mysql_store_result(mysql)))
    die("mysql_store_result() returned NULL for '%s'", query);
  if (!(row= mysql_fetch_row(res)))
    die("Query '%s' returned empty result", query);

  have_ndbcluster= strcmp(row[0], "1") == 0;
  mysql_free_result(res);

  if (!have_ndbcluster)
  {
    return;
  }

  ulonglong start_epoch= 0,
    handled_epoch= 0,
    latest_trans_epoch=0,
    latest_handled_binlog_epoch= 0,
    start_handled_binlog_epoch= 0;
  const int WaitSeconds = 150;

  int count= 0;
  int do_continue= 1;
  while (do_continue)
  {
    const char binlog[]= "binlog";
    const char latest_trans_epoch_str[]=
      "latest_trans_epoch=";
    const char latest_handled_binlog_epoch_str[]=
      "latest_handled_binlog_epoch=";
    if (count)
      my_sleep(100*1000); /* 100ms */

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

        /* latest_trans_epoch */
        while (*status && strncmp(status, latest_trans_epoch_str,
                                  sizeof(latest_trans_epoch_str)-1))
          status++;
        if (*status)
        {
          status+= sizeof(latest_trans_epoch_str)-1;
          latest_trans_epoch= my_strtoull(status, (char**) 0, 10);
        }
        else
          die("result does not contain '%s' in '%s'",
              latest_trans_epoch_str, query);

        /* latest_handled_binlog */
        while (*status &&
               strncmp(status, latest_handled_binlog_epoch_str,
                       sizeof(latest_handled_binlog_epoch_str)-1))
          status++;
        if (*status)
        {
          status+= sizeof(latest_handled_binlog_epoch_str)-1;
          latest_handled_binlog_epoch= my_strtoull(status, (char**) 0, 10);
        }
        else
          die("result does not contain '%s' in '%s'",
              latest_handled_binlog_epoch_str, query);

        if (count == 0)
        {
          start_epoch= latest_trans_epoch;
          start_handled_binlog_epoch= latest_handled_binlog_epoch;
        }
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
    else if (count > (WaitSeconds * 10))
    {
      die("do_save_master_pos() timed out after %u s waiting for "
          "last committed epoch to be applied by the "
          "Ndb binlog injector.  "
          "Ndb epoch %llu/%llu to be handled.  "
          "Last handled epoch : %llu/%llu.  "
          "First handled epoch : %llu/%llu.",
          WaitSeconds,
          start_epoch >> 32,
          start_epoch & 0xffffffff,
          latest_handled_binlog_epoch >> 32,
          latest_handled_binlog_epoch & 0xffffffff,
          start_handled_binlog_epoch >> 32,
          start_handled_binlog_epoch & 0xffffffff);
    }

    mysql_free_result(res);
  }
}


int do_save_master_pos()
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL *mysql = &cur_con->mysql;
  const char *query;
  DBUG_ENTER("do_save_master_pos");
  /*
    when ndb binlog is on, this call will wait until last updated epoch
    (locally in the mysqld) has been received into the binlog
  */
  ndb_wait_for_binlog_injector();

  if (mysql_query(mysql, query= "show master status"))
    die("failed in 'show master status': %d %s",
	mysql_errno(mysql), mysql_error(mysql));

  if (!(res = mysql_store_result(mysql)))
    die("mysql_store_result() retuned NULL for '%s'", query);
  if (!(row = mysql_fetch_row(res)))
    die("empty result in show master status");
  my_stpnmov(master_pos.file, row[0], sizeof(master_pos.file)-1);
  master_pos.pos = strtoul(row[1], (char**) 0, 10);
  mysql_free_result(res);
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
  revert_properties();
  DBUG_VOID_RETURN;
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
  char *sleep_start, *sleep_end;
  double sleep_val;
  char *p;
  static DYNAMIC_STRING ds_sleep;
  const struct command_arg sleep_args[] = {
    { "sleep_delay", ARG_STRING, TRUE, &ds_sleep, "Number of seconds to sleep." }
  };
  check_command_args(command, command->first_argument, sleep_args,
                     sizeof(sleep_args)/sizeof(struct command_arg),
                     ' ');

  p= ds_sleep.str;
  sleep_end= ds_sleep.str + ds_sleep.length;
  while (my_isspace(charset_info, *p))
    p++;
  if (!*p)
    die("Missing argument to %.*s",
        static_cast<int>(command->first_word_len), command->query);
  sleep_start= p;
  /* Check that arg starts with a digit, not handled by my_strtod */
  if (!my_isdigit(charset_info, *sleep_start))
    die("Invalid argument to %.*s \"%s\"",
        static_cast<int>(command->first_word_len), command->query, sleep_start);
  sleep_val= my_strtod(sleep_start, &sleep_end, &error);
  check_eol_junk_line(sleep_end);
  if (error)
    die("Invalid argument to %.*s \"%s\"",
        static_cast<int>(command->first_word_len),
        command->query, command->first_argument);
  dynstr_free(&ds_sleep);

  /* Fixed sleep time selected by --sleep option */
  if (opt_sleep >= 0 && !real_sleep)
    sleep_val= opt_sleep;

  DBUG_PRINT("info", ("sleep_val: %f", sleep_val));
  if (sleep_val)
    my_sleep((ulong) (sleep_val * 1000000L));
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
  strmake(dest, name, dest_max_len - 1);
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


/*
  Run query and return one field in the result set from the
  first row and <column>
*/

int query_get_string(MYSQL* mysql, const char* query,
                     int column, std::string* ds)
{
  MYSQL_RES *res= NULL;
  MYSQL_ROW row;

  if (mysql_query(mysql, query))
    die("'%s' failed: %d %s", query,
        mysql_errno(mysql), mysql_error(mysql));
  if ((res= mysql_store_result(mysql)) == NULL)
    die("Failed to store result: %d %s",
        mysql_errno(mysql), mysql_error(mysql));

  if ((row= mysql_fetch_row(res)) == NULL)
  {
    mysql_free_result(res);
    ds= 0;
    return 1;
  }
  ds->assign(row[column] ? row[column] : "NULL");
  mysql_free_result(res);
  return 0;
}


/**
  Check if process is active.

  @param pid  Process id.

  @return true if process is active, false otherwise.
*/
static bool is_process_active(int pid)
{
#ifdef _WIN32
  DWORD exit_code;
  HANDLE proc;
  proc= OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
  if (proc == NULL)
    return false;  /* Process could not be found. */

  if (!GetExitCodeProcess(proc, &exit_code))
    exit_code= 0;

  CloseHandle(proc);
  if (exit_code != STILL_ACTIVE)
    return false;  /* Error or process has terminated. */

  return true;
#else
  return (kill(pid, 0) == 0);
#endif
}

/**
  kill process.

  @param pid  Process id.

  @return true if process is terminated, false otherwise.
*/
static bool kill_process(int pid)
{
  bool killed= true;
#ifdef _WIN32
  HANDLE proc;
  proc= OpenProcess(PROCESS_TERMINATE, FALSE, pid);
  if (proc == NULL)
    return true;  /* Process could not be found. */

  if (!TerminateProcess(proc, 201))
    killed= false;

  CloseHandle(proc);
#else
  killed= (kill(pid, SIGKILL) == 0);
#endif
  return killed;
}


/**
  Abort process.

  @param pid  Process id.
  @param path Path to create minidump file in.
*/
static void abort_process(int pid, const char *path)
{
#ifdef _WIN32
  HANDLE proc;
  proc= OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  verbose_msg("Aborting pid %d (handle: %p)\n", pid, proc);
  if (proc != NULL)
  {
    char name[FN_REFLEN], *end;
    BOOL is_debugged;

    if (path)
    {
      /* Append mysqld.<pid>.dmp to the path. */
      strncpy(name, path, sizeof(name) - 1);
      name[sizeof(name) - 1]= '\0';
      end= name + strlen(name);
      /* Make sure "/mysqld.nnnnnnnnnn.dmp" fits */
      if ((end - name) < (sizeof(name) - 23))
      {
        if (!is_directory_separator(end[-1]))
        {
          end[0]= FN_LIBCHAR2;   // datadir path normally uses '/'.
          end++;
        }
        my_snprintf(end, sizeof(name) + name - end - 1, "mysqld.%d.dmp", pid);

        verbose_msg("Creating minidump.\n");
        my_create_minidump(name, proc, pid);
      }
      else
        die("Path too long for creating minidump!\n");
    }
    /* If running in a debugger, send a break, otherwise terminate. */
    if (CheckRemoteDebuggerPresent(proc, &is_debugged) && is_debugged)
    {
      if (!DebugBreakProcess(proc))
      {
        DWORD err= GetLastError();
        verbose_msg("DebugBreakProcess failed: %d\n", err);
      }
      else
        verbose_msg("DebugBreakProcess succeeded!\n");
      CloseHandle(proc);
    }
    else
    {
      CloseHandle(proc);
      (void) kill_process(pid);
    }
  }
  else
  {
    DWORD err= GetLastError();
    verbose_msg("OpenProcess failed: %d\n", err);
  }
#else
  kill(pid, SIGABRT);
#endif
}


/**
  Shutdown or kill the server.
  If timeout is set to 0 the server is killed/terminated
  immediately. Otherwise the shutdown command is first sent
  and then it waits for the server to terminate within
  <timeout> seconds. If it has not terminated before <timeout>
  seconds the command will fail.

  @note Currently only works with local server

  @param commmand  Optionally including a timeout else the
  default of 60 seconds is used.
*/

void do_shutdown_server(struct st_command *command)
{
  long timeout=60;
  int pid, error= 0;
  std::string ds_file_name;
  MYSQL* mysql = &cur_con->mysql;
  static DYNAMIC_STRING ds_timeout;
  const struct command_arg shutdown_args[] = {
    {"timeout", ARG_STRING, FALSE, &ds_timeout, "Timeout before killing server"}
  };
  DBUG_ENTER("do_shutdown_server");

  check_command_args(command, command->first_argument, shutdown_args,
                     sizeof(shutdown_args)/sizeof(struct command_arg),
                     ' ');

  if (ds_timeout.length)
  {
    char* endptr;
    timeout= strtol(ds_timeout.str, &endptr, 10);
    if (*endptr != '\0')
      die("Illegal argument for timeout: '%s'", ds_timeout.str);
  }
  dynstr_free(&ds_timeout);

  /* Get the servers pid_file name and use it to read pid */
  if (query_get_string(mysql, "SHOW VARIABLES LIKE 'pid_file'", 1,
                       &ds_file_name))
    die("Failed to get pid_file from server");

  /* Read the pid from the file */
  {
    int fd;
    char buff[32];

    if ((fd= my_open(ds_file_name.c_str(), O_RDONLY, MYF(0))) < 0)
      die("Failed to open file '%s'", ds_file_name.c_str());

    if (my_read(fd, (uchar*)&buff,
                sizeof(buff), MYF(0)) <= 0){
      my_close(fd, MYF(0));
      die("pid file was empty");
    }
    my_close(fd, MYF(0));

    pid= atoi(buff);
    if (pid == 0)
      die("Pidfile didn't contain a valid number");
  }
  DBUG_PRINT("info", ("Got pid %d", pid));

  if (timeout)
  {
    /* Check if we should generate a minidump on timeout. */
    if (query_get_string(mysql, "SHOW VARIABLES LIKE 'core_file'", 1,
                         &ds_file_name) || strcmp("ON", ds_file_name.c_str()))
    {
    }
    else
    {
      /* Get the data dir and use it as path for a minidump if needed. */
      if (query_get_string(mysql, "SHOW VARIABLES LIKE 'datadir'", 1,
                           &ds_file_name))
        die("Failed to get datadir from server");
    }

    /* Tell server to shutdown if timeout > 0. */
    if (timeout > 0 && mysql_query(mysql, "shutdown"))
    {
      error= 1;   /* Failed to issue shutdown command. */
      goto end;
    }

    /* Check that server dies */
    do
    {
      if (!is_process_active(pid))
      {
        DBUG_PRINT("info", ("Process %d does not exist anymore", pid));
        goto end;
      }
      if (timeout > 0)
      {
        DBUG_PRINT("info", ("Sleeping, timeout: %ld", timeout));
        my_sleep(1000000L);
      }
    } while(timeout-- > 0);
    error= 2;
    /*
      Abort to make it easier to find the hang/problem.
    */
    abort_process(pid, ds_file_name.c_str());
  }
  else /* timeout == 0 */
  {
    /* Kill the server */
    DBUG_PRINT("info", ("Killing server, pid: %d", pid));
    /*
      kill_process can fail (bad privileges, non existing process on *nix etc),
      so also check if the process is active before setting error.
    */
    if (!kill_process(pid) && is_process_active(pid))
      error= 3;
  }

end:
  if (error)
    handle_command_error(command, error);
  DBUG_VOID_RETURN;
}


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

const char *get_errname_from_code (uint error_code)
{
   st_error *e= global_error_names;

   DBUG_ENTER("get_errname_from_code");
   DBUG_PRINT("enter", ("error_code: %d", error_code));

   if (! error_code)
   {
     DBUG_RETURN("");
   }
   for (; e->name; e++)
   {
     if (e->code == error_code)
     {
       DBUG_RETURN(e->name);
     }
   }
   /* Apparently, errors without known names may occur */
   DBUG_RETURN("<Unknown>");
} 

void do_get_errcodes(struct st_command *command)
{
  struct st_match_err *to= saved_expected_errors.err;
  char *p= command->first_argument;
  uint count= 0;
  char *next;

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

    next=end;

    /* code to handle variables passed to mysqltest */
     if( *p == '$')
     {
        const char* fin= NULL;
        VAR *var = var_get(p,&fin,0,0);
        p=var->str_val;
        end=p+var->str_val_len;
     }

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
    p= next;

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
  mysql_options(mysql, MYSQL_OPT_RECONNECT, (char *)&reconnect);
  DBUG_VOID_RETURN;
}


/**
  Change the current connection to the given st_connection, and update
  $mysql_get_server_version and $CURRENT_CONNECTION accordingly.
*/
void set_current_connection(struct st_connection *con)
{
  cur_con= con;
  /* Update $mysql_get_server_version to that of current connection */
  var_set_int("$mysql_get_server_version",
              mysql_get_server_version(&con->mysql));
  /* Update $CURRENT_CONNECTION to the name of the current connection */
  var_set_string("$CURRENT_CONNECTION", con->name);
}


void select_connection_name(const char *name)
{
  DBUG_ENTER("select_connection_name");
  DBUG_PRINT("enter",("name: '%s'", name));
  st_connection *con= find_connection_by_name(name);

  if (!con)
    die("connection '%s' not found in connection pool", name);

  set_current_connection(con);

  /* Connection logging if enabled */
  if (!disable_connect_log && !disable_query_log)
  {
    DYNAMIC_STRING *ds= &ds_res;

    dynstr_append_mem(ds, "connection ", 11);
    replace_dynstr_append(ds, name);
    dynstr_append_mem(ds, ";\n", 2);
  }

  DBUG_VOID_RETURN;
}


void select_connection(struct st_command *command)
{
  DBUG_ENTER("select_connection");
  static DYNAMIC_STRING ds_connection;
  const struct command_arg connection_args[] = {
    { "connection_name", ARG_STRING, TRUE, &ds_connection, "Name of the connection that we switch to." }
  };
  check_command_args(command, command->first_argument, connection_args,
                     sizeof(connection_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("info", ("changing connection: %s", ds_connection.str));
  select_connection_name(ds_connection.str);
  dynstr_free(&ds_connection);
  DBUG_VOID_RETURN;
}


void do_close_connection(struct st_command *command)
{
  DBUG_ENTER("close_connection");

  struct st_connection *con;
  static DYNAMIC_STRING ds_connection;
  const struct command_arg close_connection_args[] = {
    { "connection_name", ARG_STRING, TRUE, &ds_connection,
      "Name of the connection to close." }
  };
  check_command_args(command, command->first_argument,
                     close_connection_args,
                     sizeof(close_connection_args)/sizeof(struct command_arg),
                     ' ');

  DBUG_PRINT("enter",("connection name: '%s'", ds_connection.str));

  if (!(con= find_connection_by_name(ds_connection.str)))
    die("connection '%s' not found in connection pool", ds_connection.str);

  DBUG_PRINT("info", ("Closing connection %s", con->name));
#ifndef EMBEDDED_LIBRARY
  if (command->type == Q_DIRTY_CLOSE)
  {
    if (con->mysql.net.vio)
    {
      vio_delete(con->mysql.net.vio);
      con->mysql.net.vio = 0;
      end_server(&con->mysql);
    }
  }
#else
  /*
    As query could be still executed in a separate theread
    we need to check if the query's thread was finished and probably wait
    (embedded-server specific)
  */
  emb_close_connection(con);
#endif /*EMBEDDED_LIBRARY*/
  if (con->stmt)
    mysql_stmt_close(con->stmt);
  con->stmt= 0;

  mysql_close(&con->mysql);

  if (con->util_mysql)
    mysql_close(con->util_mysql);
  con->util_mysql= 0;
  con->pending= FALSE;
  
  my_free(con->name);

  /*
    When the connection is closed set name to "-closed_connection-"
    to make it possible to reuse the connection name.
  */
  if (!(con->name = my_strdup(PSI_NOT_INSTRUMENTED,
                              "-closed_connection-", MYF(MY_WME))))
    die("Out of memory");

  if (con == cur_con)
  {
    /* Current connection was closed */
    var_set_int("$mysql_get_server_version", 0xFFFFFFFF);
    var_set_string("$CURRENT_CONNECTION", con->name);
  }

  /* Connection logging if enabled */
  if (!disable_connect_log && !disable_query_log)
  {
    DYNAMIC_STRING *ds= &ds_res;

    dynstr_append_mem(ds, "disconnect ", 11);
    replace_dynstr_append(ds, ds_connection.str);
    dynstr_append_mem(ds, ";\n", 2);
  }

  dynstr_free(&ds_connection);
  DBUG_VOID_RETURN;
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

  DBUG_ENTER("safe_connect");

  verbose_msg("Connecting to server %s:%d (socket %s) as '%s'"
              ", connection '%s', attempt %d ...", 
              host, port, sock, user, name, failed_attempts);

  mysql_options(mysql, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD,
                 "program_name", "mysqltest");
  mysql_options(mysql, MYSQL_OPT_CAN_HANDLE_EXPIRED_PASSWORDS,
                &can_handle_expired_passwords);
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
  verbose_msg("... Connected.");
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
  int failed_attempts= 0;

  ds= &ds_res;

  /* Only log if an error is expected */
  if (command->expected_errors.count > 0 &&
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
  /* Simlified logging if enabled */
  if (!disable_connect_log && !disable_query_log)
  {
    replace_dynstr_append(ds, command->query);
    dynstr_append_mem(ds, ";\n", 2);
  }
  
  mysql_options(con, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(con, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name", "mysqltest");
  mysql_options(con, MYSQL_OPT_CAN_HANDLE_EXPIRED_PASSWORDS,
                &can_handle_expired_passwords);
  while (!mysql_real_connect(con, host, user, pass, db, port, sock ? sock: 0,
                          CLIENT_MULTI_STATEMENTS))
  {
    /*
      If we have used up all our connections check whether this
      is expected (by --error). If so, handle the error right away.
      Otherwise, give it some extra time to rule out race-conditions.
      If extra-time doesn't help, we have an unexpected error and
      must abort -- just proceeding to handle_error() when second
      and third chances are used up will handle that for us.

      There are various user-limits of which only max_user_connections
      and max_connections_per_hour apply at connect time. For the
      the second to create a race in our logic, we'd need a limits
      test that runs without a FLUSH for longer than an hour, so we'll
      stay clear of trying to work out which exact user-limit was
      exceeded.
    */

    if (((mysql_errno(con) == ER_TOO_MANY_USER_CONNECTIONS) ||
         (mysql_errno(con) == ER_USER_LIMIT_REACHED)) &&
        (failed_attempts++ < opt_max_connect_retries))
    {
      int i;

      i= match_expected_error(command, mysql_errno(con), mysql_sqlstate(con));

      if (i >= 0)
        goto do_handle_error;                 /* expected error, handle */

      my_sleep(connection_retry_sleep);       /* unexpected error, wait */
      continue;                               /* and give it 1 more chance */
    }

do_handle_error:
    var_set_errno(mysql_errno(con));
    handle_error(command, mysql_errno(con), mysql_error(con),
		 mysql_sqlstate(con), ds);
    return 0; /* Not connected */
  }

  var_set_errno(0);
  handle_no_error(command);
  revert_properties();
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
   * SHM - use shared memory if available
   * PIPE - use named pipe if available

*/

void do_connect(struct st_command *command)
{
  int con_port= opt_port;
  char *con_options;
  my_bool con_ssl= 0, con_compress= 0;
  my_bool con_pipe= 0, con_shm= 0, con_cleartext_enable= 0;
  struct st_connection* con_slot;
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  uint save_opt_ssl_mode= opt_ssl_mode;
#endif

  static DYNAMIC_STRING ds_connection_name;
  static DYNAMIC_STRING ds_host;
  static DYNAMIC_STRING ds_user;
  static DYNAMIC_STRING ds_password;
  static DYNAMIC_STRING ds_database;
  static DYNAMIC_STRING ds_port;
  static DYNAMIC_STRING ds_sock;
  static DYNAMIC_STRING ds_options;
  static DYNAMIC_STRING ds_default_auth;
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  static DYNAMIC_STRING ds_shm;
#endif
  const struct command_arg connect_args[] = {
    { "connection name", ARG_STRING, TRUE, &ds_connection_name, "Name of the connection" },
    { "host", ARG_STRING, TRUE, &ds_host, "Host to connect to" },
    { "user", ARG_STRING, FALSE, &ds_user, "User to connect as" },
    { "passsword", ARG_STRING, FALSE, &ds_password, "Password used when connecting" },
    { "database", ARG_STRING, FALSE, &ds_database, "Database to select after connect" },
    { "port", ARG_STRING, FALSE, &ds_port, "Port to connect to" },
    { "socket", ARG_STRING, FALSE, &ds_sock, "Socket to connect with" },
    { "options", ARG_STRING, FALSE, &ds_options, "Options to use while connecting" },
    { "default_auth", ARG_STRING, FALSE, &ds_default_auth, "Default authentication to use" }
  };

  DBUG_ENTER("do_connect");
  DBUG_PRINT("enter",("connect: %s", command->first_argument));

  strip_parentheses(command);
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

#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  /* Shared memory */
  init_dynamic_string(&ds_shm, ds_sock.str, 0, 0);
#endif

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
    else if (!strncmp(con_options, "PIPE", 4))
      con_pipe= 1;
    else if (!strncmp(con_options, "SHM", 3))
      con_shm= 1;
    else if (!strncmp(con_options, "CLEARTEXT", 9))
      con_cleartext_enable= 1;
    else
      die("Illegal option to connect: %.*s", 
          (int) (end - con_options), con_options);
    /* Process next option */
    con_options= end;
  }

  if (find_connection_by_name(ds_connection_name.str))
    die("Connection %s already exists", ds_connection_name.str);
    
  if (next_con != connections_end)
    con_slot= next_con;
  else
  {
    if (!(con_slot= find_connection_by_name("-closed_connection-")))
      die("Connection limit exhausted, you can have max %d connections",
          opt_max_connections);
  }

#ifdef EMBEDDED_LIBRARY
  init_connection_thd(con_slot);
#endif /*EMBEDDED_LIBRARY*/

  if (!mysql_init(&con_slot->mysql))
    die("Failed on mysql_init()");

  if (opt_connect_timeout)
    mysql_options(&con_slot->mysql, MYSQL_OPT_CONNECT_TIMEOUT,
                  (void *) &opt_connect_timeout);

  if (opt_compress || con_compress)
    mysql_options(&con_slot->mysql, MYSQL_OPT_COMPRESS, NullS);
  mysql_options(&con_slot->mysql, MYSQL_OPT_LOCAL_INFILE, 0);
  mysql_options(&con_slot->mysql, MYSQL_SET_CHARSET_NAME,
                charset_info->csname);
  if (opt_charsets_dir)
    mysql_options(&con_slot->mysql, MYSQL_SET_CHARSET_DIR,
                  opt_charsets_dir);

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  /*
    If mysqltest --ssl-mode option is set to DISABLED
    and connect(.., SSL) command used, set proper opt_ssl_mode.

    So, SSL connection is used either:
    a) mysqltest --ssl-mode option is NOT set to DISABLED or
    b) connect(.., SSL) command used.
  */
  if (opt_ssl_mode == SSL_MODE_DISABLED && con_ssl)
  {
    opt_ssl_mode= (opt_ssl_ca || opt_ssl_capath) ?
      SSL_MODE_VERIFY_CA : SSL_MODE_REQUIRED;
  }
#else
  /* keep the compiler happy about con_ssl */
  con_ssl = con_ssl ? TRUE : FALSE;
#endif
  SSL_SET_OPTIONS(&con_slot->mysql);
#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  opt_ssl_mode= save_opt_ssl_mode;
#endif

  if (con_pipe)
  {
#if defined(_WIN32) && !defined(EMBEDDED_LIBRARY)
    opt_protocol= MYSQL_PROTOCOL_PIPE;
#endif
  }

#ifndef EMBEDDED_LIBRARY
  if (opt_protocol)
    mysql_options(&con_slot->mysql, MYSQL_OPT_PROTOCOL, (char*) &opt_protocol);
#endif

  if (con_shm)
  {
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
    uint protocol= MYSQL_PROTOCOL_MEMORY;
    if (!ds_shm.length)
      die("Missing shared memory base name");
    mysql_options(&con_slot->mysql, MYSQL_SHARED_MEMORY_BASE_NAME, ds_shm.str);
    mysql_options(&con_slot->mysql, MYSQL_OPT_PROTOCOL, &protocol);
#endif
  }
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  else if (shared_memory_base_name)
  {
    mysql_options(&con_slot->mysql, MYSQL_SHARED_MEMORY_BASE_NAME,
                  shared_memory_base_name);
  }
#endif

  /* Use default db name */
  if (ds_database.length == 0)
    dynstr_set(&ds_database, opt_db);

  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(&con_slot->mysql, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (ds_default_auth.length)
    mysql_options(&con_slot->mysql, MYSQL_DEFAULT_AUTH, ds_default_auth.str);

#if !defined(HAVE_YASSL)
  /* Set server public_key */
  if (opt_server_public_key && *opt_server_public_key)
    mysql_options(&con_slot->mysql, MYSQL_SERVER_PUBLIC_KEY,
                  opt_server_public_key);
#endif
  
  if (con_cleartext_enable)
    mysql_options(&con_slot->mysql, MYSQL_ENABLE_CLEARTEXT_PLUGIN,
                  (char*) &con_cleartext_enable);

  /* Special database to allow one to connect without a database name */
  if (ds_database.length && !strcmp(ds_database.str,"*NO-ONE*"))
    dynstr_set(&ds_database, "");

  if (connect_n_handle_errors(command, &con_slot->mysql,
                              ds_host.str,ds_user.str,
                              ds_password.str, ds_database.str,
                              con_port, ds_sock.str))
  {
    DBUG_PRINT("info", ("Inserting connection %s in connection pool",
                        ds_connection_name.str));
    my_free(con_slot->name);
    if (!(con_slot->name= my_strdup(PSI_NOT_INSTRUMENTED,
                                    ds_connection_name.str, MYF(MY_WME))))
      die("Out of memory");
    con_slot->name_len= strlen(con_slot->name);
    set_current_connection(con_slot);

    if (con_slot == next_con)
      next_con++; /* if we used the next_con slot, advance the pointer */
  }

  dynstr_free(&ds_connection_name);
  dynstr_free(&ds_host);
  dynstr_free(&ds_user);
  dynstr_free(&ds_password);
  dynstr_free(&ds_database);
  dynstr_free(&ds_port);
  dynstr_free(&ds_sock);
  dynstr_free(&ds_options);
  dynstr_free(&ds_default_auth);
#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  dynstr_free(&ds_shm);
#endif
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
    if (*cur_block->delim) 
    {
      /* Restore "old" delimiter after false if block */
      strcpy (delimiter, cur_block->delim);
      delimiter_length= strlen(delimiter);
    }
    /* Pop block from stack, goto next line */
    cur_block--;
    parser.current_line++;
  }
  return 0;
}

/* Operands available in if or while conditions */

enum block_op {
  EQ_OP,
  NE_OP,
  GT_OP,
  GE_OP,
  LT_OP,
  LE_OP,
  ILLEG_OP
};


enum block_op find_operand(const char *start)
{
 char first= *start;
 char next= *(start+1);
 
 if (first == '=' && next == '=')
   return EQ_OP;
 if (first == '!' && next == '=')
   return NE_OP;
 if (first == '>' && next == '=')
   return GE_OP;
 if (first == '>')
   return GT_OP;
 if (first == '<' && next == '=')
   return LE_OP;
 if (first == '<')
   return LT_OP;
 
 return ILLEG_OP;
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

  <expr> can also be a simple comparison condition:

  <variable> <op> <expr>

  The left hand side must be a variable, the right hand side can be a
  variable, number, string or `query`. Operands are ==, !=, <, <=, >, >=.
  == and != can be used for strings, all can be used for numerical values.
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
    cur_block->delim[0]= '\0';
    DBUG_VOID_RETURN;
  }

  /* Parse and evaluate test expression */
  expr_start= strchr(p, '(');
  if (!expr_start++)
    die("missing '(' in %s", cmd_name);

  while (my_isspace(charset_info, *expr_start))
    expr_start++;
  
  /* Check for !<expr> */
  if (*expr_start == '!')
  {
    not_expr= TRUE;
    expr_start++; /* Step past the '!', then any whitespace */
    while (*expr_start && my_isspace(charset_info, *expr_start))
      expr_start++;
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

  /* If expression starts with a variable, it may be a compare condition */

  if (*expr_start == '$')
  {
    const char *curr_ptr= expr_end;
    eval_expr(&v, expr_start, &curr_ptr, true);
    while (my_isspace(charset_info, *++curr_ptr))
    {}
    /* If there was nothing past the variable, skip condition part */
    if (curr_ptr == expr_end)
      goto NO_COMPARE;

    enum block_op operand= find_operand(curr_ptr);
    if (operand == ILLEG_OP)
      die("Found junk '%.*s' after $variable in condition",
          (int)(expr_end - curr_ptr), curr_ptr);

    /* We could silently allow this, but may be confusing */
    if (not_expr)
      die("Negation and comparison should not be combined, please rewrite");
    
    /* Skip the 1 or 2 chars of the operand, then white space */
    if (operand == LT_OP || operand == GT_OP)
    {
      curr_ptr++;
    }
    else
    {
      curr_ptr+= 2;
    }
    while (my_isspace(charset_info, *curr_ptr))
      curr_ptr++;
    if (curr_ptr == expr_end)
      die("Missing right operand in comparison");

    /* Strip off trailing white space */
    while (my_isspace(charset_info, expr_end[-1]))
      expr_end--;
    /* strip off ' or " around the string */
    if (*curr_ptr == '\'' || *curr_ptr == '"')
    {
      if (expr_end[-1] != *curr_ptr)
        die("Unterminated string value");
      curr_ptr++;
      expr_end--;
    }
    VAR v2;
    var_init(&v2,0,0,0,0);
    eval_expr(&v2, curr_ptr, &expr_end);

    if ((operand!=EQ_OP && operand!=NE_OP) && ! (v.is_int && v2.is_int))
      die ("Only == and != are supported for string values");

    /* Now we overwrite the first variable with 0 or 1 (for false or true) */

    switch (operand)
    {
    case EQ_OP:
      if (v.is_int)
        v.int_val= (v2.is_int && v2.int_val == v.int_val);
      else
        v.int_val= !strcmp (v.str_val, v2.str_val);
      break;
      
    case NE_OP:
      if (v.is_int)
        v.int_val= ! (v2.is_int && v2.int_val == v.int_val);
      else
        v.int_val= (strcmp (v.str_val, v2.str_val) != 0);
      break;

    case LT_OP:
      v.int_val= (v.int_val < v2.int_val);
      break;
    case LE_OP:
      v.int_val= (v.int_val <= v2.int_val);
      break;
    case GT_OP:
      v.int_val= (v.int_val > v2.int_val);
      break;
    case GE_OP:
      v.int_val= (v.int_val >= v2.int_val);
      break;
    case ILLEG_OP:
      die("Impossible operator, this cannot happen");
    }

    v.is_int= TRUE;
    var_free(&v2);
  } else
  {
    if (*expr_start != '`' && ! my_isdigit(charset_info, *expr_start))
      die("Expression in if/while must beging with $, ` or a number");
    eval_expr(&v, expr_start, &expr_end);
  }

 NO_COMPARE:
  /* Define inner block */
  cur_block++;
  cur_block->cmd= cmd;
  if (v.is_int)
  {
    cur_block->ok= (v.int_val != 0);
  } else
  /* Any non-empty string which does not begin with 0 is also TRUE */
  {
    p= v.str_val;
    /* First skip any leading white space or unary -+ */
    while (*p && ((my_isspace(charset_info, *p) || *p == '-' || *p == '+')))
      p++;

    cur_block->ok= (*p && *p != '0') ? TRUE : FALSE;
  }
  
  if (not_expr)
    cur_block->ok = !cur_block->ok;

  if (cur_block->ok) 
  {
    cur_block->delim[0]= '\0';
  } else
  {
    /* Remember "old" delimiter if entering a false if block */
    strcpy (cur_block->delim, delimiter);
  }
  
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

/*
  do_reset_connection

  DESCRIPTION
  Reset the current session.
*/
void do_reset_connection()
{
  MYSQL *mysql = &cur_con->mysql;

  DBUG_ENTER("do_reset_connection");
  if (mysql_reset_connection(mysql))
    die("reset connection failed: %s", mysql_error(mysql));
  if (cur_con->stmt)
  {
    mysql_stmt_close(cur_con->stmt);
    cur_con->stmt= NULL;
  }
  DBUG_VOID_RETURN;
}

my_bool match_delimiter(int c, const char *delim, size_t length)
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
  char c, last_quote= 0, last_char= 0;
  char *p= buf, *buf_end= buf + size - 1;
  int skip_char= 0;
  my_bool have_slash= FALSE;
  
  enum {R_NORMAL, R_Q, R_SLASH_IN_Q,
        R_COMMENT, R_LINE_START} state= R_LINE_START;
  DBUG_ENTER("read_line");

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
	fclose(cur_file->file);
        cur_file->file= 0;
      }
      my_free(cur_file->file_name);
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
                                      (uchar*) buf, min<my_ptrdiff_t>(5, p - buf), 0) ||
                 !my_strnncoll_simple(charset_info, (const uchar*) "if", 2,
                                      (uchar*) buf, min<my_ptrdiff_t>(2, p - buf), 0))))
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
        if (! have_slash) 
        {
	  last_quote= c;
	  state= R_Q;
	}
      }
      have_slash= (c == '\\');
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
	if (c == '\n')
        {
          if (last_char == '\n')
          {
            /* Two new lines in a row, return empty line */
            DBUG_PRINT("info", ("Found two new lines in a row"));
            *p++= c;
            *p= 0;
            DBUG_RETURN(0);
          }

          /* Query hasn't started yet */
	  start_lineno= cur_file->lineno;
          DBUG_PRINT("info", ("Query hasn't started yet, start_lineno: %d",
                              start_lineno));
        }

        /* Skip all space at begining of line */
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

    last_char= c;

    if (!skip_char)
    {
      /* Could be a multibyte character */
      /* This code is based on the code in "sql_load.cc" */
      uint charlen;
      if (my_mbmaxlenlen(charset_info) == 1)
        charlen= my_mbcharlen(charset_info, (unsigned char) c);
      else
      {
        if (!(charlen= my_mbcharlen(charset_info, (unsigned char) c)))
        {
          int c1= my_getc(cur_file->file);
          if (c1 == EOF)
          {
            *p++= c;
            goto found_eof;
          }

          charlen= my_mbcharlen_2(charset_info, (unsigned char) c,
                                  (unsigned char) c1);
          my_ungetc(c1);
        }
      }
      if(charlen == 0)
        DBUG_RETURN(1);
      /* We give up if multibyte character is started but not */
      /* completed before we pass buf_end */
      if ((charlen > 1) && (p + charlen) <= buf_end)
      {
	char* mb_start = p;

	*p++ = c;

	for (uint i= 1; i < charlen; i++)
	{
	  c= my_getc(cur_file->file);
	  if (feof(cur_file->file))
	    goto found_eof;
	  *p++ = c;
	}
	if (! my_ismbchar(charset_info, mb_start, p))
	{
	  /* It was not a multiline char, push back the characters */
	  /* We leave first 'c', i.e. pretend it was a normal char */
	  while (p-1 > mb_start)
	    my_ungetc(*--p);
	}
      }
      else
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


bool is_delimiter(const char* p)
{
  uint match= 0;
  char* delim= delimiter;
  while (*p && *p == *delim++)
  {
    match++;
    p++;
  }

  return (match == delimiter_length);
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

#define MAX_QUERY (256*1024*2) /* 256K -- a test in sp-big is >128K */
static char read_command_buf[MAX_QUERY];

int read_command(struct st_command** command_ptr)
{
  char *p= read_command_buf;
  struct st_command* command;
  DBUG_ENTER("read_command");

  if (parser.current_line < parser.read_lines)
  {
    *command_ptr= q_lines->at(parser.current_line);
    DBUG_RETURN(0);
  }
  if (!(*command_ptr= command=
        (struct st_command*) my_malloc(PSI_NOT_INSTRUMENTED,
                                       sizeof(*command),
                                       MYF(MY_WME|MY_ZEROFILL))) ||
      q_lines->push_back(command))
    die("Out of memory");
  command->type= Q_UNKNOWN;

  read_command_buf[0]= 0;
  if (read_line(read_command_buf, sizeof(read_command_buf)))
  {
    check_eol_junk(read_command_buf);
    DBUG_RETURN(1);
  }

  if (opt_result_format_version == 1)
    convert_to_format_v1(read_command_buf);

  DBUG_PRINT("info", ("query: '%s'", read_command_buf));
  if (*p == '#')
  {
    command->type= Q_COMMENT;
  }
  else if (p[0] == '-' && p[1] == '-')
  {
    command->type= Q_COMMENT_WITH_COMMAND;
    p+= 2; /* Skip past -- */
  }
  else if (*p == '\n')
  {
    command->type= Q_EMPTY_LINE;
  }

  /* Skip leading spaces */
  while (*p && my_isspace(charset_info, *p))
    p++;

  if (!(command->query_buf= command->query= my_strdup(PSI_NOT_INSTRUMENTED,
                                                      p, MYF(MY_WME))))
    die("Out of memory");

  /*
    Calculate first word length(the command), terminated
    by 'space' , '(' or 'delimiter' */
  p= command->query;
  while (*p && !my_isspace(charset_info, *p) && *p != '(' && !is_delimiter(p))
    p++;
  command->first_word_len= (uint) (p - command->query);
  DBUG_PRINT("info", ("first_word: %.*s",
                      static_cast<int>(command->first_word_len),
                      command->query));

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
  {"basedir", 'b', "Basedir for tests.", &opt_basedir,
   &opt_basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"character-sets-dir", OPT_CHARSETS_DIR,
   "Directory for character set files.", &opt_charsets_dir,
   &opt_charsets_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use the compressed server/client protocol.",
   &opt_compress, &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"cursor-protocol", OPT_CURSOR_PROTOCOL, "Use cursors for prepared statements.",
   &cursor_protocol, &cursor_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"database", 'D', "Database to use.", &opt_db, &opt_db, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
  {"debug", '#', "This is a non-debug version. Catch this and exit.",
   0,0, 0, GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-check", OPT_DEBUG_CHECK, "This is a non-debug version. Catch this and exit.",
   0, 0, 0,
   GET_DISABLED, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", OPT_DEBUG_INFO, "This is a non-debug version. Catch this and exit.", 0,
   0, 0, GET_DISABLED, NO_ARG, 0, 0, 0, 0, 0, 0},
#else
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-check", OPT_DEBUG_CHECK, "Check memory and open file usage at exit.",
   &debug_check_flag, &debug_check_flag, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"debug-info", OPT_DEBUG_INFO, "Print some debug info at exit.",
   &debug_info_flag, &debug_info_flag,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
  {"host", 'h', "Connect to host.", &opt_host, &opt_host, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"include", 'i', "Include SQL before each test case.", &opt_include,
   &opt_include, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"logdir", OPT_LOG_DIR, "Directory for log files", &opt_logdir,
   &opt_logdir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"mark-progress", OPT_MARK_PROGRESS,
   "Write line number and elapsed time to <testname>.progress.",
   &opt_mark_progress, &opt_mark_progress, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"max-connect-retries", OPT_MAX_CONNECT_RETRIES,
   "Maximum number of attempts to connect to server.",
   &opt_max_connect_retries, &opt_max_connect_retries, 0,
   GET_INT, REQUIRED_ARG, 500, 1, 10000, 0, 0, 0},
  {"max-connections", OPT_MAX_CONNECTIONS,
   "Max number of open connections to server",
   &opt_max_connections, &opt_max_connections, 0,
   GET_INT, REQUIRED_ARG, 128, 8, 5120, 0, 0, 0},
  {"password", 'p', "Password to use when connecting to server.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"protocol", OPT_MYSQL_PROTOCOL, "The protocol of connection (tcp,socket,pipe,memory).",
   0, 0, 0, GET_STR,  REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection or 0 for default to, in "
   "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
   "/etc/services, "
#endif
   "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
   &opt_port, &opt_port, 0, GET_INT, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"ps-protocol", OPT_PS_PROTOCOL, 
   "Use prepared-statement protocol for communication.",
   &ps_protocol, &ps_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"quiet", 's', "Suppress all normal output.", &silent,
   &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"record", 'r', "Record output of test_file into result file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"result-file", 'R', "Read/store result from/in this file.",
   &result_file_name, &result_file_name, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"result-format-version", OPT_RESULT_FORMAT_VERSION,
   "Version of the result file format to use",
   &opt_result_format_version,
   &opt_result_format_version, 0,
   GET_INT, REQUIRED_ARG, 1, 1, 2, 0, 0, 0},
  {"server-arg", 'A', "Send option value to embedded server as a parameter.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-file", 'F', "Read embedded server arguments from file.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
   "Base name of shared memory.", &shared_memory_base_name, 
   &shared_memory_base_name, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 
   0, 0, 0},
  {"silent", 's', "Suppress all normal output. Synonym for --quiet.",
   &silent, &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"sleep", 'T', "Always sleep this many seconds on sleep commands.",
   &opt_sleep, &opt_sleep, 0, GET_INT, REQUIRED_ARG, -1, -1, 0,
   0, 0, 0},
  {"socket", 'S', "The socket file to use for connection.",
   &unix_sock, &unix_sock, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"sp-protocol", OPT_SP_PROTOCOL, "Use stored procedures for select.",
   &sp_protocol, &sp_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#include "sslopt-longopts.h"
  {"tail-lines", OPT_TAIL_LINES,
   "Number of lines of the result to include in a failure report.",
   &opt_tail_lines, &opt_tail_lines, 0,
   GET_INT, REQUIRED_ARG, 0, 0, 10000, 0, 0, 0},
  {"test-file", 'x', "Read test from/in this file (default stdin).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"timer-file", 'm', "File where the timing in microseconds is stored.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't', "Temporary directory where sockets are put.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login.", &opt_user, &opt_user, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Write more.", &verbose, &verbose, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"view-protocol", OPT_VIEW_PROTOCOL, "Use views for select.",
   &view_protocol, &view_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"opt-trace-protocol", OPT_TRACE_PROTOCOL,
   "Trace DML statements with optimizer trace",
   &opt_trace_protocol, &opt_trace_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"explain-protocol", OPT_EXPLAIN_PROTOCOL,
   "Explain all SELECT/INSERT/REPLACE/UPDATE/DELETE statements",
   &explain_protocol, &explain_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"json-explain-protocol", OPT_JSON_EXPLAIN_PROTOCOL,
   "Explain all SELECT/INSERT/REPLACE/UPDATE/DELETE statements with FORMAT=JSON",
   &json_explain_protocol, &json_explain_protocol, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"connect_timeout", OPT_CONNECT_TIMEOUT,
   "Number of seconds before connection timeout.",
   &opt_connect_timeout, &opt_connect_timeout, 0, GET_UINT, REQUIRED_ARG,
   120, 0, 3600 * 12, 0, 0, 0},
  {"plugin_dir", OPT_PLUGIN_DIR, "Directory for client-side plugins.",
    &opt_plugin_dir, &opt_plugin_dir, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#if !defined(HAVE_YASSL) 
  {"server-public-key-path", OPT_SERVER_PUBLIC_KEY,
   "File path to the server public RSA key in PEM format.",
   &opt_server_public_key, &opt_server_public_key, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
#endif
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,MTEST_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

void usage()
{
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  printf("Runs a test against the mysql server and compares output with a results file.\n\n");
  printf("Usage: %s [OPTIONS] [database] < test_file\n", my_progname);
  my_print_help(my_long_options);
  printf("  --no-defaults       Don't read default options from any options file.\n");
  my_print_variables(my_long_options);
}


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
    die("Failed to open file '%s'", buff);

  while (embedded_server_arg_count < MAX_EMBEDDED_SERVER_ARGS &&
	 (str=fgets(argument,sizeof(argument), file)))
  {
    *(strend(str)-1)=0;				/* Remove end newline */
    if (!(embedded_server_args[embedded_server_arg_count]=
	  (char*) my_strdup(PSI_NOT_INSTRUMENTED,
                            str,MYF(MY_WME))))
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
get_one_option(int optid, const struct my_option *opt, char *argument)
{
  switch(optid) {
  case '#':
#ifndef DBUG_OFF
    DBUG_PUSH(argument ? argument : "d:t:S:i:O,/tmp/mysqltest.trace");
    debug_check_flag= 1;
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
          fopen(buff, "rb")))
      die("Could not open '%s' for reading, errno: %d", buff, errno);
    cur_file->file_name= my_strdup(PSI_NOT_INSTRUMENTED,
                                   buff, MYF(MY_FAE));
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
    if (argument == disabled_my_option)
      argument= (char*) "";			// Don't require password
    if (argument)
    {
      my_free(opt_pass);
      opt_pass= my_strdup(PSI_NOT_INSTRUMENTED,
                          argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
      tty_password= 0;
    }
    else
      tty_password= 1;
    break;
#include <sslopt-case.h>
  case 't':
    my_stpnmov(TMPDIR, argument, sizeof(TMPDIR));
    break;
  case 'A':
    if (!embedded_server_arg_count)
    {
      embedded_server_arg_count=1;
      embedded_server_args[0]= (char*) "";
    }
    if (embedded_server_arg_count == MAX_EMBEDDED_SERVER_ARGS-1 ||
        !(embedded_server_args[embedded_server_arg_count++]=
          my_strdup(PSI_NOT_INSTRUMENTED,
                    argument, MYF(MY_FAE))))
    {
      die("Can't use server argument");
    }
    break;
  case OPT_LOG_DIR:
    /* Check that the file exists */
    if (access(opt_logdir, F_OK) != 0)
      die("The specified log directory does not exist: '%s'", opt_logdir);
    break;
  case 'F':
    read_embedded_server_arguments(argument);
    break;
  case OPT_RESULT_FORMAT_VERSION:
    set_result_format_version(opt_result_format_version);
    break;
  case 'V':
    print_version();
    exit(0);
  case OPT_MYSQL_PROTOCOL:
#ifndef EMBEDDED_LIBRARY
    opt_protocol= find_type_or_exit(argument, &sql_protocol_typelib,
                                    opt->name);
#endif
    break;
  case '?':
    usage();
    exit(0);
  }
  return 0;
}


int parse_args(int argc, char **argv)
{
  if (load_defaults("my",load_default_groups,&argc,&argv))
    exit(1);

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
    opt_pass= get_tty_password(NullS);
  if (debug_info_flag)
    my_end_arg= MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag)
    my_end_arg= MY_CHECK_ERROR;


  if (!record)
  {
    /* Check that the result file exists */
    if (result_file_name && access(result_file_name, F_OK) != 0)
      die("The specified result file '%s' does not exist", result_file_name);
  }

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

void str_to_file2(const char *fname, char *str, size_t size, my_bool append)
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
    die("Could not open '%s' for writing, errno: %d", buff, errno);
  if (append && my_seek(fd, 0, SEEK_END, MYF(0)) == MY_FILEPOS_ERROR)
    die("Could not find end of file '%s', errno: %d", buff, errno);
  if (my_write(fd, (uchar*)str, size, MYF(MY_WME|MY_FNABP)))
    die("write failed, errno: %d", errno);
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

void str_to_file(const char *fname, char *str, size_t size)
{
  str_to_file2(fname, str, size, FALSE);
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


#ifdef _WIN32

typedef Prealloced_array<const char*, 16> Patterns;
Patterns *patterns;

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
                          "$MASTER_MYSOCK",
                          "$MYSQL_SHAREDIR",
                          "$MYSQL_CHARSETSDIR",
                          "$MYSQL_LIBDIR",
                          "./test/",
                          ".ibd",
                          "ibdata",
                          "ibtmp",
                          "undo"};
  int num_paths= sizeof(paths)/sizeof(char*);
  int i;
  char* p;

  DBUG_ENTER("init_win_path_patterns");

  patterns= new Patterns(PSI_NOT_INSTRUMENTED);

  /* Loop through all paths in the array */
  for (i= 0; i < num_paths; i++)
  {
    VAR* v;
    if (*(paths[i]) == '$')
    {
      v= var_get(paths[i], 0, 0, 0);
      p= my_strdup(PSI_NOT_INSTRUMENTED,
                   v->str_val, MYF(MY_FAE));
    }
    else
      p= my_strdup(PSI_NOT_INSTRUMENTED,
                   paths[i], MYF(MY_FAE));

    /* Don't insert zero length strings in patterns array */
    if (strlen(p) == 0)
    {
      my_free(p);
      continue;
    }

    if (patterns->push_back(p))
      die("Out of memory");

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
  const char **pat;
  for (pat= patterns->begin(); pat != patterns->end(); ++pat)
  {
    my_free(const_cast<char*>(*pat));
  }
  delete patterns;
  patterns= NULL;
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

void fix_win_paths(const char *val, size_t len)
{
  DBUG_ENTER("fix_win_paths");
  const char **pat;
  for (pat= patterns->begin(); pat != patterns->end(); ++pat)
  {
    char *p;
    DBUG_PRINT("info", ("pattern: %s", *pat));

    /* Find and fix each path in this string */
    p= const_cast<char*>(val);
    while (p= strstr(p, *pat))
    {
      DBUG_PRINT("info", ("Found %s in val p: %s", *pat, p));
      /* Found the pattern.  Back up to the start of this path */
      while (p > val && !my_isspace(charset_info, *(p - 1)))
      {
        p--;
      }

      while (*p && !my_isspace(charset_info, *p))
      {
        if (*p == '\\')
          *p= '/';
        p++;
      }
      DBUG_PRINT("info", ("Converted \\ to / in %s", val));
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
                  char* val, size_t len, my_bool is_null)
{
  char null[]= "NULL";

  if (col_idx < max_replace_column && replace_column[col_idx])
  {
    val= replace_column[col_idx];
    len= strlen(val);
  }
  else if (is_null)
  {
    val= null;
    len= 4;
  }
#ifdef _WIN32
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
      if (field->flags & ZEROFILL_FLAG)
      {
        /* Move all chars before the first '0' one step right */
        memmove(val + 1, val, start - val);
        *val= '0';
      }
      else
      {
        /* Move all chars after the first '0' one step left */
        memmove(start, start + 1, strlen(start));
        len--;
      }
    }
  }
#endif

  if (!display_result_vertically)
  {
    if (col_idx)
      dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_mem(ds, val, len);
  }
  else
  {
    dynstr_append(ds, field->name);
    dynstr_append_mem(ds, "\t", 1);
    replace_dynstr_append_mem(ds, val, len);
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
    {
      /* looks ugly , but put here to convince parfait */
      assert(lengths);
      append_field(ds, i, &fields[i],
                   row[i], lengths[i], !row[i]);
    }
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
  my_bind= (MYSQL_BIND*) my_malloc(PSI_NOT_INSTRUMENTED,
                                   num_fields * sizeof(MYSQL_BIND),
				MYF(MY_WME | MY_FAE | MY_ZEROFILL));
  length= (ulong*) my_malloc(PSI_NOT_INSTRUMENTED,
                             num_fields * sizeof(ulong),
			     MYF(MY_WME | MY_FAE));
  is_null= (my_bool*) my_malloc(PSI_NOT_INSTRUMENTED,
                                num_fields * sizeof(my_bool),
				MYF(MY_WME | MY_FAE));

  /* Allocate data for the result of each field */
  for (i= 0; i < num_fields; i++)
  {
    size_t max_length= fields[i].max_length + 1;
    my_bind[i].buffer_type= MYSQL_TYPE_STRING;
    my_bind[i].buffer= my_malloc(PSI_NOT_INSTRUMENTED,
                                 max_length, MYF(MY_WME | MY_FAE));
    my_bind[i].buffer_length= static_cast<ulong>(max_length);
    my_bind[i].is_null= &is_null[i];
    my_bind[i].length= &length[i];

    DBUG_PRINT("bind", ("col[%d]: buffer_type: %d, buffer_length: %lu",
			i, my_bind[i].buffer_type,
                        my_bind[i].buffer_length));
  }

  if (mysql_stmt_bind_result(stmt, my_bind))
    die("mysql_stmt_bind_result failed: %d: %s",
	mysql_stmt_errno(stmt), mysql_stmt_error(stmt));

  while (mysql_stmt_fetch(stmt) == 0)
  {
    for (i= 0; i < num_fields; i++)
      append_field(ds, i, &fields[i], (char*)my_bind[i].buffer,
                   *my_bind[i].length, *my_bind[i].is_null);
    if (!display_result_vertically)
      dynstr_append_mem(ds, "\n", 1);
  }

  int rc;
  if ((rc= mysql_stmt_fetch(stmt)) != MYSQL_NO_DATA)
    die("fetch didn't end with MYSQL_NO_DATA from statement: %d: %s; rc=%d",
        mysql_stmt_errno(stmt), mysql_stmt_error(stmt), rc);

  for (i= 0; i < num_fields; i++)
  {
    /* Free data for output */
    my_free(my_bind[i].buffer);
  }
  /* Free array with bind structs, lengths and NULL flags */
  my_free(my_bind);
  my_free(length);
  my_free(is_null);
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


/**
  @brief Append state change information (received through Ok packet) to the output.

  @param ds    [INOUT]      Dynamic string to hold the content to be printed.
  @param mysql [IN]         Connection handle.
*/

void append_session_track_info(DYNAMIC_STRING *ds, MYSQL *mysql)
{
  for (unsigned int type= SESSION_TRACK_BEGIN; type <= SESSION_TRACK_END; type++)
  {
    const char *data;
    size_t data_length;

    if (!mysql_session_track_get_first(mysql,
                                       (enum_session_state_type) type,
                                       &data, &data_length))
    {
      /*
	Append the type information. Please update the definition of APPEND_TYPE when
	any changes are made to enum_session_state_type.
      */
      APPEND_TYPE(type);
      dynstr_append(ds, "-- ");
      dynstr_append_mem(ds, data, data_length);
    }
    else
      continue;
    while (!mysql_session_track_get_next(mysql,
                                        (enum_session_state_type) type,
                                        &data, &data_length))
    {
      dynstr_append(ds, "\n-- ");
      dynstr_append_mem(ds, data, data_length);
    }
    dynstr_append(ds, "\n\n");
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
                      int flags, char *query, size_t query_len,
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
    if (do_send_query(cn, query, query_len))
    {
      handle_error(command, mysql_errno(mysql), mysql_error(mysql),
		   mysql_sqlstate(mysql), ds);
      goto end;
    }
  }
  if (!(flags & QUERY_REAP_FLAG))
  {
    cn->pending= TRUE;
    DBUG_VOID_RETURN;
  }
  
  do
  {
    /*
      When  on first result set, call mysql_read_query_result to retrieve
      answer to the query sent earlier
    */
    if ((counter==0) && do_read_query_result(cn))
    {
      /* we've failed to collect the result set */
      cn->pending= TRUE;
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
        query to find the warnings.
      */
      if (!disable_info)
	append_info(ds, mysql_affected_rows(mysql), mysql_info(mysql));

      if (display_session_track_info)
        append_session_track_info(ds, mysql);

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
  revert_properties();

end:

  cn->pending= FALSE;
  /*
    We save the return code (mysql_errno(mysql)) from the last call sent
    to the server into the mysqltest builtin variable $mysql_errno. This
    variable then can be used from the test case itself.
  */
  var_set_errno(mysql_errno(mysql));
  DBUG_VOID_RETURN;
}


/*
  Check whether given error is in list of expected errors

  SYNOPSIS
    match_expected_error()

  PARAMETERS
    command        the current command (and its expect-list)
    err_errno      error number of the error that actually occurred
    err_sqlstate   SQL-state that was thrown, or NULL for impossible
                   (file-ops, diff, etc.)

  RETURNS
    -1 for not in list, index in list of expected errors otherwise

  NOTE
    If caller needs to know whether the list was empty, they should
    check command->expected_errors.count.
*/

static int match_expected_error(struct st_command *command,
                                unsigned int err_errno,
                                const char *err_sqlstate)
{
  uint i;

  for (i= 0 ; (uint) i < command->expected_errors.count ; i++)
  {
    if ((command->expected_errors.err[i].type == ERR_ERRNO) &&
        (command->expected_errors.err[i].code.errnum == err_errno))
      return i;

    if (command->expected_errors.err[i].type == ERR_SQLSTATE)
    {
      /*
        NULL is quite likely, but not in conjunction with a SQL-state expect!
      */
      if (unlikely(err_sqlstate == NULL))
        die("expecting a SQL-state (%s) from query '%s' which cannot produce one...",
            command->expected_errors.err[i].code.sqlstate, command->query);

      if (strncmp(command->expected_errors.err[i].code.sqlstate,
                  err_sqlstate, SQLSTATE_LENGTH) == 0)
        return i;
    }
  }
  return -1;
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
*/

void handle_error(struct st_command *command,
                  unsigned int err_errno, const char *err_error,
                  const char *err_sqlstate, DYNAMIC_STRING *ds)
{
  int i;

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

  i= match_expected_error(command, err_errno, err_sqlstate);

  if (i >= 0)
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
    revert_properties();
    DBUG_VOID_RETURN;
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

  if (command->expected_errors.count > 0)
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

  revert_properties();
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
                    char *query, size_t query_len, DYNAMIC_STRING *ds,
                    DYNAMIC_STRING *ds_warnings)
{
  MYSQL_RES *res= NULL;     /* Note that here 'res' is meta data result set */
  MYSQL_STMT *stmt;
  DYNAMIC_STRING ds_prepare_warnings= DYNAMIC_STRING();
  DYNAMIC_STRING ds_execute_warnings= DYNAMIC_STRING();
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
  if (mysql_stmt_prepare(stmt, query, static_cast<ulong>(query_len)))
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

      /*
        Clear prepare warnings if there are execute warnings,
        since they are probably duplicated.
      */
      if (ds_execute_warnings.length || mysql->warning_count)
        dynstr_set(&ds_prepare_warnings, NULL);
    }
    else
    {
      /*
	This is a query without resultset
      */
    }

    /*
      Fetch info before fetching warnings, since it will be reset
      otherwise.
    */

    if (!disable_info)
      append_info(ds, mysql_stmt_affected_rows(stmt), mysql_info(mysql));

    if (display_session_track_info)
      append_session_track_info(ds, mysql);

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
  }

end:
  if (!disable_warnings)
  {
    dynstr_free(&ds_prepare_warnings);
    dynstr_free(&ds_execute_warnings);
  }
  revert_properties();

  /*
    We save the return code (mysql_stmt_errno(stmt)) from the last call sent
    to the server into the mysqltest builtin variable $mysql_errno. This
    variable then can be used from the test case itself.
  */

  var_set_errno(mysql_stmt_errno(stmt));

  /* Close the statement if - no reconnect, need new prepare */
  if (mysql->reconnect)
  {
    mysql_stmt_close(stmt);
    cur_con->stmt= NULL;
  }


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

    if (opt_connect_timeout)
      mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT,
                    (void *) &opt_connect_timeout);

    /* enable local infile, in non-binary builds often disabled by default */
    mysql_options(mysql, MYSQL_OPT_LOCAL_INFILE, 0);
    safe_connect(mysql, "util", org_mysql->host, org_mysql->user,
                 org_mysql->passwd, org_mysql->db, org_mysql->port,
                 org_mysql->unix_socket);

    cur_con->util_mysql= mysql;
  }

 DBUG_RETURN(mysql_query(mysql, query));
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
  DYNAMIC_STRING ds_sorted;
  DYNAMIC_STRING ds_warnings;
  DYNAMIC_STRING eval_query;
  char *query;
  size_t query_len;
  my_bool view_created= 0, sp_created= 0;
  my_bool complete_query= ((flags & QUERY_SEND_FLAG) &&
                           (flags & QUERY_REAP_FLAG));
  DBUG_ENTER("run_query");
  dynstr_set(&ds_result, "");

  if (cn->pending && (flags & QUERY_SEND_FLAG))
    die ("Cannot run query on connection between send and reap");

  if (!(flags & QUERY_SEND_FLAG) && !cn->pending)
    die ("Cannot reap on a connection without pending send");
  
  init_dynamic_string(&ds_warnings, NULL, 0, 256);
  ds_warn= &ds_warnings;
  
  /*
    Evaluate query if this is an eval command
  */
  if (command->type == Q_EVAL || command->type == Q_SEND_EVAL)
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
  if (command->require_file[0] || command->output_file[0])
  {
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

  dynstr_free(&ds_warnings);
  ds_warn= 0;
  if (command->type == Q_EVAL || command->type == Q_SEND_EVAL)
    dynstr_free(&eval_query);

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

  if (command->output_file[0])
  {
    /* An output file was specified for _this_ query */
    str_to_file2(command->output_file, ds_result.str, ds_result.length, false);
    command->output_file[0]= 0;
  }

  DBUG_VOID_RETURN;
}

/**
   Display the optimizer trace produced by the last executed statement.
 */
void display_opt_trace(struct st_connection *cn,
                       struct st_command *command,
                       int flags)
{
  if (!disable_query_log &&
      opt_trace_protocol_enabled &&
      !cn->pending &&
      !command->expected_errors.count &&
      match_re(&opt_trace_re, command->query))
  {
    st_command save_command= *command;
    DYNAMIC_STRING query_str;
    init_dynamic_string(&query_str,
                        "SELECT trace FROM information_schema.optimizer_trace"
                        " /* injected by --opt-trace-protocol */",
                        128, 128);

    command->query= query_str.str;
    command->query_len= query_str.length;
    command->end= strend(command->query);

    /* Sorted trace is not readable at all, don't bother to lower case */
    /* No need to keep old values, will be reset anyway */
    display_result_sorted= FALSE;
    display_result_lower= FALSE;
    run_query(cn, command, flags);

    dynstr_free(&query_str);
    *command= save_command;
  }
}


void run_explain(struct st_connection *cn, struct st_command *command,
                 int flags, bool json)
{
  if ((flags & QUERY_REAP_FLAG) &&
      !command->expected_errors.count &&
      match_re(&explain_re, command->query))
  {
    st_command save_command= *command;
    DYNAMIC_STRING query_str;
    DYNAMIC_STRING ds_warning_messages;

    init_dynamic_string(&ds_warning_messages, "", 0, 2048);
    init_dynamic_string(&query_str, json ? "EXPLAIN FORMAT=JSON "
                                         : "EXPLAIN ", 256, 256);
    dynstr_append_mem(&query_str, command->query,
                      command->end - command->query);
    
    command->query= query_str.str;
    command->query_len= query_str.length;
    command->end= strend(command->query);

    run_query(cn, command, flags);

    dynstr_free(&query_str);
    dynstr_free(&ds_warning_messages);

    *command= save_command;
  }
}


/****************************************************************************/
/*
  Functions to detect different SQL statements
*/

char *re_eprint(int err)
{
  static char epbuf[100];
  size_t len= my_regerror(MY_REG_ITOA | err, NULL, epbuf, sizeof(epbuf));
  assert(len <= sizeof(epbuf));
  return(epbuf);
}

void init_re_comp(my_regex_t *re, const char* str)
{
  int err= my_regcomp(re, str, (MY_REG_EXTENDED | MY_REG_ICASE | MY_REG_NOSUB),
                      &my_charset_latin1);
  if (err)
  {
    char erbuf[100];
    size_t len= my_regerror(err, re, erbuf, sizeof(erbuf));
    die("error %s, %d/%d `%s'\n",
	re_eprint(err), (int)len, (int)sizeof(erbuf), erbuf);
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
    "[[:space:]]*CREATE[[:space:]]+INDEX[[:space:]]|"
    "[[:space:]]*DROP[[:space:]]+INDEX[[:space:]]|"
    "[[:space:]]*RENAME[[:space:]]+TABLE[[:space:]]|"
    "[[:space:]]*CREATE[[:space:]]+TEMPORARY[[:space:]]+TABLE[[:space:]]|"
    "[[:space:]]*DROP[[:space:]]+TEMPORARY[[:space:]]+TABLE[[:space:]]|"
    "[[:space:]]*DROP[[:space:]]+VIEW[[:space:]]|"
    "[[:space:]]*REVOKE[[:space:]]+ALL[[:space:]]+PRIVILEGES[[:space:]]|"
    "[[:space:]]*DROP[[:space:]]+USER[[:space:]]|"
    "[[:space:]]*DO[[:space:]]|"
    "[[:space:]]*SET[[:space:]]+OPTION[[:space:]]|"
    "[[:space:]]*DELETE[[:space:]]+MULTI[[:space:]]|"
    "[[:space:]]*UPDATE[[:space:]]+MULTI[[:space:]]|"
    "[[:space:]]*INSERT[[:space:]]+SELECT[[:space:]])[^;]*$";

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

  const char *opt_trace_re_str =
    "^("
    "[[:space:]]*INSERT[[:space:]]|"
    "[[:space:]]*UPDATE[[:space:]]|"
    "[[:space:]]*DELETE[[:space:]]|"
    "[[:space:]]*EXPLAIN[[:space:]]|"
    "[[:space:]]*SELECT[[:space:]])";

  /* Filter for queries that can be converted to EXPLAIN */
  const char *explain_re_str =
    "^("
    "[[:space:]]*(SELECT|DELETE|UPDATE|INSERT|REPLACE)[[:space:]])";

  init_re_comp(&ps_re, ps_re_str);
  init_re_comp(&sp_re, sp_re_str);
  init_re_comp(&view_re, view_re_str);
  init_re_comp(&opt_trace_re, opt_trace_re_str);
  init_re_comp(&explain_re, explain_re_str);
}


int match_re(my_regex_t *re, char *str)
{
  while (my_isspace(charset_info, *str))
    str++;
  if (str[0] == '/' && str[1] == '*')
  {
    char *comm_end= strstr (str, "*/");
    if (! comm_end)
      die("Statement is unterminated comment");
    str= comm_end + 2;
  }
  
  int err= my_regexec(re, str, (size_t)0, NULL, 0);

  if (err == 0)
    return 1;
  else if (err == MY_REG_NOMATCH)
    return 0;

  {
    char erbuf[100];
    size_t len= my_regerror(err, re, erbuf, sizeof(erbuf));
    die("error %s, %d/%d `%s'\n",
	re_eprint(err), (int)len, (int)sizeof(erbuf), erbuf);
  }
  return 0;
}

void free_re(void)
{
  my_regfree(&ps_re);
  my_regfree(&sp_re);
  my_regfree(&view_re);
  my_regfree(&opt_trace_re);
  my_regfree(&explain_re);
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
  type= find_type(command->query, &command_typelib, FIND_TYPE_NO_PREFIX);
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
      /* -- "comment" that didn't contain a mysqltest command */
      die("Found line beginning with --  that didn't contain "\
          "a valid mysqltest command, check your syntax or "\
          "use # if you intended to write a comment");
    }
  }
  DBUG_VOID_RETURN;
}


void update_expected_errors(struct st_command* command)
{
  DBUG_ENTER("update_expected_errors");

  /* Set expected error on command */
  memcpy(&command->expected_errors, &saved_expected_errors,
         sizeof(saved_expected_errors));
  DBUG_PRINT("info", ("There are %d expected errors",
                      command->expected_errors.count));
  DBUG_VOID_RETURN;
}



/*
  Record how many milliseconds it took to execute the test file
  up until the current line and write it to .progress file

*/

void mark_progress(struct st_command* command MY_ATTRIBUTE((unused)),
                   int line)
{
  static ulonglong progress_start= 0; // < Beware
  DYNAMIC_STRING ds_progress;

  char buf[32], *end;
  ulonglong timer= timer_now();
  if (!progress_start)
    progress_start= timer;
  timer-= progress_start;

  if (init_dynamic_string(&ds_progress, "", 256, 256))
    die("Out of memory");

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

  progress_file.write(&ds_progress);

  dynstr_free(&ds_progress);

}

#ifdef HAVE_STACKTRACE

static void dump_backtrace(void)
{
  struct st_connection *conn= cur_con;

  fprintf(stderr, "read_command_buf (%p): ", read_command_buf);
  my_safe_puts_stderr(read_command_buf, sizeof(read_command_buf));

  if (conn)
  {
    fprintf(stderr, "conn->name (%p): ", conn->name);
    my_safe_puts_stderr(conn->name, conn->name_len);
#ifdef EMBEDDED_LIBRARY
    fprintf(stderr, "conn->cur_query (%p): ", conn->cur_query);
    my_safe_puts_stderr(conn->cur_query, conn->cur_query_len);
#endif
  }
  fputs("Attempting backtrace...\n", stderr);
  my_print_stacktrace(NULL, my_thread_stack_size);
}

#else

static void dump_backtrace(void)
{
  fputs("Backtrace not available.\n", stderr);
}

#endif

static void signal_handler(int sig)
{
  fprintf(stderr, "mysqltest got " SIGNAL_FMT "\n", sig);
  dump_backtrace();

  fprintf(stderr, "Writing a core file...\n");
  fflush(stderr);
  my_write_core(sig);
#ifndef _WIN32
  exit(1);			// Shouldn't get here but just in case
#endif
}

#ifdef _WIN32

LONG WINAPI exception_filter(EXCEPTION_POINTERS *exp)
{
  __try
  {
    my_set_exception_pointers(exp);
    signal_handler(exp->ExceptionRecord->ExceptionCode);
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    fputs("Got exception in exception handler!\n", stderr);
  }

  return EXCEPTION_CONTINUE_SEARCH;
}


static void init_signal_handling(void)
{
  UINT mode;

  /* Set output destination of messages to the standard error stream. */
  _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

  /* Do not not display the a error message box. */
  mode= SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX;
  SetErrorMode(mode);

  SetUnhandledExceptionFilter(exception_filter);
}

#else /* _WIN32 */

static void init_signal_handling(void)
{
  struct sigaction sa;
  DBUG_ENTER("init_signal_handling");

#ifdef HAVE_STACKTRACE
  my_init_stacktrace();
#endif

  sa.sa_flags = SA_RESETHAND | SA_NODEFER;
  sigemptyset(&sa.sa_mask);
  sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);

  sa.sa_handler= signal_handler;

  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
#ifdef SIGBUS
  sigaction(SIGBUS, &sa, NULL);
#endif
  sigaction(SIGILL, &sa, NULL);
  sigaction(SIGFPE, &sa, NULL);

  DBUG_VOID_RETURN;
}

#endif /* !_WIN32 */

int main(int argc, char **argv)
{
  struct st_command *command;
  my_bool q_send_flag= 0, abort_flag= 0;
  uint command_executed= 0, last_command_executed= 0;
  char save_file[FN_REFLEN];
  char output_file[FN_REFLEN];
  MY_INIT(argv[0]);

  save_file[0]= 0;
  output_file[0]= 0;
  TMPDIR[0]= 0;

  init_signal_handling();

  /* Init expected errors */
  memset(&saved_expected_errors, 0, sizeof(saved_expected_errors));

#ifdef EMBEDDED_LIBRARY
  /* set appropriate stack for the 'query' threads */
  (void) my_thread_attr_init(&cn_thd_attrib);
  my_thread_attr_setstacksize(&cn_thd_attrib, DEFAULT_THREAD_STACK);
#endif /*EMBEDDED_LIBRARY*/

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

  q_lines= new Q_lines(PSI_NOT_INSTRUMENTED);

  if (my_hash_init(&var_hash, charset_info,
                   1024, 0, 0, get_var_key, var_free, MYF(0),
                   PSI_NOT_INSTRUMENTED))
    die("Variable hash initialization failed");

  {
    char path_separator[]= { FN_LIBCHAR, 0 };
    var_set_string("SYSTEM_PATH_SEPARATOR", path_separator);
  }
  var_set_string("MYSQL_SERVER_VERSION", MYSQL_SERVER_VERSION);
  var_set_string("MYSQL_SYSTEM_TYPE", SYSTEM_TYPE);
  var_set_string("MYSQL_MACHINE_TYPE", MACHINE_TYPE);
  if (sizeof(void *) == 8) {
    var_set_string("MYSQL_SYSTEM_ARCHITECTURE", "64");
  } else {
    var_set_string("MYSQL_SYSTEM_ARCHITECTURE", "32");
  }

  memset(&master_pos, 0, sizeof(master_pos));

  parser.current_line= parser.read_lines= 0;
  memset(&var_reg, 0, sizeof(var_reg));

  init_builtin_echo();
#ifdef _WIN32
  is_windows= 1;
  init_win_path_patterns();
#endif

  init_dynamic_string(&ds_res, "", 2048, 2048);
  init_dynamic_string(&ds_result, "", 1024, 1024);

  parse_args(argc, argv);

  log_file.open(opt_logdir, result_file_name, ".log");
  verbose_msg("Logging to '%s'.", log_file.file_name());
  if (opt_mark_progress)
  {
    progress_file.open(opt_logdir, result_file_name, ".progress");
    verbose_msg("Tracing progress in '%s'.", progress_file.file_name());
  }

  /* Init connections, allocate 1 extra as buffer + 1 for default */
  connections= (struct st_connection*)
    my_malloc(PSI_NOT_INSTRUMENTED,
              (opt_max_connections+2) * sizeof(struct st_connection),
              MYF(MY_WME | MY_ZEROFILL));
  connections_end= connections + opt_max_connections +1;
  next_con= connections + 1;
  
  var_set_int("$PS_PROTOCOL", ps_protocol);
  var_set_int("$SP_PROTOCOL", sp_protocol);
  var_set_int("$VIEW_PROTOCOL", view_protocol);
  var_set_int("$OPT_TRACE_PROTOCOL", opt_trace_protocol);
  var_set_int("$EXPLAIN_PROTOCOL", explain_protocol);
  var_set_int("$JSON_EXPLAIN_PROTOCOL", json_explain_protocol);
  var_set_int("$CURSOR_PROTOCOL", cursor_protocol);

  var_set_int("$ENABLED_QUERY_LOG", 1);
  var_set_int("$ENABLED_ABORT_ON_ERROR", 1);
  var_set_int("$ENABLED_RESULT_LOG", 1);
  var_set_int("$ENABLED_CONNECT_LOG", 0);
  var_set_int("$ENABLED_WARNINGS", 1);
  var_set_int("$ENABLED_INFO", 0);
  var_set_int("$ENABLED_METADATA", 0);

  DBUG_PRINT("info",("result_file: '%s'",
                     result_file_name ? result_file_name : ""));
  verbose_msg("Results saved in '%s'.", 
              result_file_name ? result_file_name : "");
  if (mysql_server_init(embedded_server_arg_count,
			embedded_server_args,
			(char**) embedded_server_groups))
    die("Can't initialize MySQL server");
  server_initialized= 1;
  if (cur_file == file_stack && cur_file->file == 0)
  {
    cur_file->file= stdin;
    cur_file->file_name= my_strdup(PSI_NOT_INSTRUMENTED,
                                   "<stdin>", MYF(MY_WME));
    cur_file->lineno= 1;
  }
  var_set_string("MYSQLTEST_FILE", cur_file->file_name);
  init_re();

  /* Cursor protcol implies ps protocol */
  if (cursor_protocol)
    ps_protocol= 1;

  ps_protocol_enabled= ps_protocol;
  sp_protocol_enabled= sp_protocol;
  view_protocol_enabled= view_protocol;
  opt_trace_protocol_enabled= opt_trace_protocol;
  explain_protocol_enabled= explain_protocol;
  json_explain_protocol_enabled= json_explain_protocol;
  cursor_protocol_enabled= cursor_protocol;

  st_connection *con= connections;
#ifdef EMBEDDED_LIBRARY
  if (ps_protocol)
    die("--ps-protocol is not supported in embedded mode");
  init_connection_thd(con);
#endif /*EMBEDDED_LIBRARY*/
  if (!( mysql_init(&con->mysql)))
    die("Failed in mysql_init()");
  if (opt_connect_timeout)
    mysql_options(&con->mysql, MYSQL_OPT_CONNECT_TIMEOUT,
                  (void *) &opt_connect_timeout);
  if (opt_compress)
    mysql_options(&con->mysql,MYSQL_OPT_COMPRESS,NullS);
  mysql_options(&con->mysql, MYSQL_OPT_LOCAL_INFILE, 0);
  mysql_options(&con->mysql, MYSQL_SET_CHARSET_NAME,
                charset_info->csname);
  if (opt_charsets_dir)
    mysql_options(&con->mysql, MYSQL_SET_CHARSET_DIR,
                  opt_charsets_dir);

#ifndef EMBEDDED_LIBRARY
  if (opt_protocol)
    mysql_options(&con->mysql,MYSQL_OPT_PROTOCOL,(char*)&opt_protocol);
#endif


#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  /* Turn on VERIFY_IDENTITY mode only if host=="localhost". */
  if (opt_ssl_mode == SSL_MODE_VERIFY_IDENTITY)
  {
    if (!opt_host || strcmp(opt_host, "localhost"))
      opt_ssl_mode = SSL_MODE_VERIFY_CA;
  }
#endif
  SSL_SET_OPTIONS(&con->mysql);


#if defined (_WIN32) && !defined (EMBEDDED_LIBRARY)
  if (shared_memory_base_name)
    mysql_options(&con->mysql,MYSQL_SHARED_MEMORY_BASE_NAME,shared_memory_base_name);
#endif

  if (!(con->name = my_strdup(PSI_NOT_INSTRUMENTED,
                              "default", MYF(MY_WME))))
    die("Out of memory");

  safe_connect(&con->mysql, con->name, opt_host, opt_user, opt_pass,
               opt_db, opt_port, unix_sock);

  /* Use all time until exit if no explicit 'start_timer' */
  timer_start= timer_now();

  /*
    Initialize $mysql_errno with -1, so we can
    - distinguish it from valid values ( >= 0 ) and
    - detect if there was never a command sent to the server
  */
  var_set_errno(-1);

  set_current_connection(con);

  if (opt_include)
  {
    open_file(opt_include);
  }

  verbose_msg("Start processing test commands from '%s' ...", cur_file->file_name);
  while (!read_command(&command) && !abort_flag)
  {
    int current_line_inc = 1, processed = 0;
    if (command->type == Q_UNKNOWN || command->type == Q_COMMENT_WITH_COMMAND)
      get_command_type(command);

    if(saved_expected_errors.count > 0)
      update_expected_errors(command);

    if (parsing_disabled &&
        command->type != Q_ENABLE_PARSING &&
        command->type != Q_DISABLE_PARSING)
    {
      /* Parsing is disabled, silently convert this line to a comment */
      command->type= Q_COMMENT;
    }

    /* (Re-)set abort_on_error for this command */
    command->abort_on_error= (command->expected_errors.count == 0 &&
                              abort_on_error);
    
    /* delimiter needs to be executed so we can continue to parse */
    my_bool ok_to_do= cur_block->ok || command->type == Q_DELIMITER;
    /*
      Some commands need to be "done" the first time if they may get
      re-iterated over in a true context. This can only happen if there's 
      a while loop at some level above the current block.
    */
    if (!ok_to_do)
    {
      if (command->type == Q_SOURCE ||
          command->type == Q_ERROR ||
          command->type == Q_WRITE_FILE ||
          command->type == Q_APPEND_FILE ||
	  command->type == Q_PERL)
      {
	for (struct st_block *stb= cur_block-1; stb >= block_stack; stb--)
	{
	  if (stb->cmd == cmd_while)
	  {
	    ok_to_do= 1;
	    break;
	  }
	}
      }
    }

    if (ok_to_do)
    {
      command->last_argument= command->first_argument;
      processed = 1;
      /* Need to remember this for handle_error() */
      curr_command= command;
      switch (command->type) {
      case Q_CONNECT:
        do_connect(command);
        break;
      case Q_CONNECTION: select_connection(command); break;
      case Q_DISCONNECT:
      case Q_DIRTY_CLOSE:
	do_close_connection(command); break;
      case Q_ENABLE_QUERY_LOG:
        set_property(command, P_QUERY, 0);
        break;
      case Q_DISABLE_QUERY_LOG:
        set_property(command, P_QUERY, 1);
        break;
      case Q_ENABLE_ABORT_ON_ERROR:
        set_property(command, P_ABORT, 1);
        break;
      case Q_DISABLE_ABORT_ON_ERROR:
        set_property(command, P_ABORT, 0);
        break;
      case Q_ENABLE_RESULT_LOG:
        set_property(command, P_RESULT, 0);
        break;
      case Q_DISABLE_RESULT_LOG:
        set_property(command, P_RESULT, 1);
        break;
      case Q_ENABLE_CONNECT_LOG:
        set_property(command, P_CONNECT, 0);
        break;
      case Q_DISABLE_CONNECT_LOG:
        set_property(command, P_CONNECT, 1);
        break;
      case Q_ENABLE_WARNINGS:
        set_property(command, P_WARN, 0);
        break;
      case Q_DISABLE_WARNINGS:
        set_property(command, P_WARN, 1);
        break;
      case Q_ENABLE_INFO:
        set_property(command, P_INFO, 0);
        break;
      case Q_DISABLE_INFO:
        set_property(command, P_INFO, 1);
        break;
      case Q_ENABLE_SESSION_TRACK_INFO:
        set_property(command, P_SESSION_TRACK, 1);
        break;
      case Q_DISABLE_SESSION_TRACK_INFO:
        set_property(command, P_SESSION_TRACK, 0);
        break;
      case Q_ENABLE_METADATA:
        set_property(command, P_META, 1);
        break;
      case Q_DISABLE_METADATA:
        set_property(command, P_META, 0);
        break;
      case Q_SOURCE: do_source(command); break;
      case Q_SLEEP: do_sleep(command, 0); break;
      case Q_REAL_SLEEP: do_sleep(command, 1); break;
      case Q_WAIT_FOR_SLAVE_TO_STOP: do_wait_for_slave_to_stop(command); break;
      case Q_INC: do_modify_var(command, DO_INC); break;
      case Q_DEC: do_modify_var(command, DO_DEC); break;
      case Q_ECHO: do_echo(command); command_executed++; break;
      case Q_SYSTEM:
        die("'system' command  is deprecated, use exec or\n"\
            "  see the manual for portable commands to use");
	break;
      case Q_REMOVE_FILE: do_remove_file(command); break;
      case Q_REMOVE_FILES_WILDCARD: do_remove_files_wildcard(command); break;
      case Q_MKDIR: do_mkdir(command); break;
      case Q_RMDIR: do_rmdir(command); break;
      case Q_LIST_FILES: do_list_files(command); break;
      case Q_LIST_FILES_WRITE_FILE:
        do_list_files_write_file_command(command, FALSE);
        break;
      case Q_LIST_FILES_APPEND_FILE:
        do_list_files_write_file_command(command, TRUE);
        break;
      case Q_FILE_EXIST: do_file_exist(command); break;
      case Q_WRITE_FILE: do_write_file(command); break;
      case Q_APPEND_FILE: do_append_file(command); break;
      case Q_DIFF_FILES: do_diff_files(command); break;
      case Q_SEND_QUIT: do_send_quit(command); break;
      case Q_CHANGE_USER: do_change_user(command); break;
      case Q_CAT_FILE: do_cat_file(command); break;
      case Q_COPY_FILE: do_copy_file(command); break;
      case Q_MOVE_FILE: do_move_file(command); break;
      case Q_CHMOD_FILE: do_chmod_file(command); break;
      case Q_PERL: do_perl(command); break;
      case Q_RESULT_FORMAT_VERSION: do_result_format_version(command); break;
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
      case Q_LOWERCASE:
        /*
          Turn on lowercasing of result, will be reset after next
          command
        */
        display_result_lower= TRUE;
        break;
      case Q_LET: do_let(command); break;
      case Q_EVAL_RESULT:
        die("'eval_result' command  is deprecated");
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

        /*
          We run EXPLAIN _before_ the query. If query is UPDATE/DELETE is
          matters: a DELETE may delete rows, and then EXPLAIN DELETE will
          usually terminate quickly with "no matching rows". To make it more
          interesting, EXPLAIN is now first.
        */
        if (explain_protocol_enabled)
          run_explain(cur_con, command, flags, 0);
        if (json_explain_protocol_enabled)
          run_explain(cur_con, command, flags, 1);
	/* Check for 'require' */
	if (*save_file)
	{
	  strmake(command->require_file, save_file, sizeof(save_file) - 1);
	  *save_file= 0;
	}
	if (*output_file)
	{
	  strmake(command->output_file, output_file, sizeof(output_file) - 1);
	  *output_file= 0;
	}
	run_query(cur_con, command, flags);
	display_opt_trace(cur_con, command, flags);
	command_executed++;
        command->last_argument= command->end;

        /* Restore settings */
	display_result_vertically= old_display_result_vertically;

	break;
      }
      case Q_SEND:
      case Q_SEND_EVAL:
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
	do_sync_with_master2(command, 0);
	break;
      }
      case Q_COMMENT:
      {
        command->last_argument= command->end;

        /* Don't output comments in v1 */
        if (opt_result_format_version == 1)
          break;

        /* Don't output comments if query logging is off */
        if (disable_query_log)
          break;

        /* Write comment's with two starting #'s to result file */
        const char* p= command->query;
        if (p && *p == '#' && *(p+1) == '#')
        {
          dynstr_append_mem(&ds_res, command->query, command->query_len);
          dynstr_append(&ds_res, "\n");
        }
	break;
      }
      case Q_EMPTY_LINE:
        /* Don't output newline in v1 */
        if (opt_result_format_version == 1)
          break;

        /* Don't output newline if query logging is off */
        if (disable_query_log)
          break;

        dynstr_append(&ds_res, "\n");
        break;
      case Q_PING:
        handle_command_error(command, mysql_ping(&cur_con->mysql));
        break;
      case Q_RESET_CONNECTION:
        do_reset_connection();
        break;
      case Q_SEND_SHUTDOWN:
        handle_command_error(command,
                             mysql_query(&cur_con->mysql,
                                            "shutdown"));
        break;
      case Q_SHUTDOWN_SERVER:
        do_shutdown_server(command);
        break;
      case Q_EXEC:
      case Q_EXECW:
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
        set_property(command, P_PS, 0);
        /* Close any open statements */
        close_statements();
        break;
      case Q_ENABLE_PS_PROTOCOL:
        set_property(command, P_PS, ps_protocol);
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

      case Q_OUTPUT:
        {
          static DYNAMIC_STRING ds_to_file;
          const struct command_arg output_file_args[] = 
            {{ "to_file", ARG_STRING, TRUE, &ds_to_file, "Output filename" }};
          check_command_args(command, command->first_argument,
                             output_file_args, 1, ' ');
          strmake(output_file, ds_to_file.str, FN_REFLEN);
          dynstr_free(&ds_to_file);
          break;
        }

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
        command->type != Q_COMMENT &&
        command->type != Q_IF &&
        command->type != Q_END_BLOCK)
    {
      /*
        As soon as any non "error" command or comment has been executed,
        the array with expected errors should be cleared
      */
      memset(&saved_expected_errors, 0, sizeof(saved_expected_errors));
    }

    if (command_executed != last_command_executed || command->used_replace)
    {
      /*
        As soon as any command has been executed,
        the replace structures should be cleared
      */
      free_all_replace();

      /* Also reset "sorted_result" and "lowercase"*/
      display_result_sorted= FALSE;
      display_result_lower= FALSE;
    }
    last_command_executed= command_executed;

    parser.current_line += current_line_inc;
    if ( opt_mark_progress )
      mark_progress(command, parser.current_line);

    /* Write result from command to log file immediately */
    log_file.write(&ds_res);
    log_file.flush();
    dynstr_set(&ds_res, 0);
  }

  log_file.close();

  start_lineno= 0;
  verbose_msg("... Done processing test commands.");

  if (parsing_disabled)
    die("Test ended with parsing disabled");

  my_bool empty_result= FALSE;
  
  /*
    The whole test has been executed _sucessfully_.
    Time to compare result or save it to record file.
    The entire output from test is in the log file
  */
  if (log_file.bytes_written())
  {
    if (result_file_name)
    {
      /* A result file has been specified */

      if (record)
      {
	/* Recording */

        /* save a copy of the log to result file */
        if (my_copy(log_file.file_name(), result_file_name, MYF(0)) != 0)
          die("Failed to copy '%s' to '%s', errno: %d",
              log_file.file_name(), result_file_name, errno);

      }
      else
      {
	/* Check that the output from test is equal to result file */
	check_result();
      }
    }
  }
  else
  {
    /* Empty output is an error *unless* we also have an empty result file */
    if (! result_file_name || record ||
        compare_files (log_file.file_name(), result_file_name))
    {
      die("The test didn't produce any output");
    }
    else 
    {
      empty_result= TRUE;  /* Meaning empty was expected */
    }
  }

  if (!command_executed && result_file_name && !empty_result)
    die("No queries executed but non-empty result file found!");

  verbose_msg("Test has succeeded!");
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
  return my_micro_time() / 1000;
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
  start= buff= (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                                strlen(from)+1,MYF(MY_WME | MY_FAE));
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
    my_free(replace_column[column_number-1]);
    replace_column[column_number-1]= my_strdup(PSI_NOT_INSTRUMENTED,
                                               to, MYF(MY_WME | MY_FAE));
    set_if_bigger(max_replace_column, column_number);
  }
  my_free(start);
  command->last_argument= command->end;

  DBUG_VOID_RETURN;
}


void free_replace_column()
{
  uint i;
  for (i=0 ; i < max_replace_column ; i++)
  {
    if (replace_column[i])
    {
      my_free(replace_column[i]);
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
  uchar	*str;					/* Strings is here */
  uint8 *flag;					/* Flag about each var. */
  uint	array_allocs,max_count,length,max_length;
} POINTER_ARRAY;

struct st_replace *init_replace(char * *from, char * *to, uint count,
				char * word_end_chars);
int insert_pointer_name(POINTER_ARRAY *pa,char * name);
void free_pointer_array(POINTER_ARRAY *pa);

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

  memset(&to_array, 0, sizeof(to_array));
  memset(&from_array, 0, sizeof(from_array));
  if (!*from)
    die("Missing argument in %s", command->query);
  start= buff= (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                                strlen(from)+1,MYF(MY_WME | MY_FAE));
  while (*from)
  {
    char *to= buff;
    to= get_string(&buff, &from, command);
    if (!*from)
      die("Wrong number of arguments to replace_result in '%s'",
          command->query);
#ifdef _WIN32
    fix_win_paths(to, from - to);
#endif
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
  my_free(start);
  command->last_argument= command->end;
  DBUG_VOID_RETURN;
}


void free_replace()
{
  DBUG_ENTER("free_replace");
  my_free(glob_replace);
  glob_replace= NULL;
  DBUG_VOID_RETURN;
}


typedef struct st_replace {
  my_bool found;
  struct st_replace *next[256];
} REPLACE;

typedef struct st_replace_found {
  my_bool found;
  char *replace_string;
  uint to_offset;
  int from_offset;
} REPLACE_STRING;


void replace_strings_append(REPLACE *rep, DYNAMIC_STRING* ds,
                            const char *str,
                            size_t len MY_ATTRIBUTE((unused)))
{
  REPLACE *rep_pos;
  REPLACE_STRING *rep_str;
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
  size_t expr_len= strlen(expr);
  char last_c = 0;
  struct st_regex reg;

  /* my_malloc() will die on fail with MY_FAE */
  void *rawmem= my_malloc(PSI_NOT_INSTRUMENTED,
                          sizeof(*res)+expr_len ,MYF(MY_FAE+MY_WME));
  res= new (rawmem) st_replace_regex;

  buf= (char*)res + sizeof(*res);
  expr_end= expr + expr_len;
  p= expr;
  buf_p= buf;

  /* for each regexp substitution statement */
  while (p < expr_end)
  {
    memset(&reg, 0, sizeof(reg));
    /* find the start of the statement */
    while (p < expr_end)
    {
      if (*p == '/')
        break;
      p++;
    }

    if (p == expr_end || ++p == expr_end)
    {
      if (res->regex_arr.size())
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
    if (res->regex_arr.push_back(reg))
      die("Out of memory");
  }
  res->odd_buf_len= res->even_buf_len= 8192;
  res->even_buf= (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                                  res->even_buf_len,MYF(MY_WME+MY_FAE));
  res->odd_buf= (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                                 res->odd_buf_len,MYF(MY_WME+MY_FAE));
  res->buf= res->even_buf;

  return res;

err:
  my_free(res);
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
  size_t i;
  char* in_buf, *out_buf;
  int* buf_len_p;

  in_buf= val;
  out_buf= r->even_buf;
  buf_len_p= &r->even_buf_len;
  r->buf= 0;

  /* For each substitution, do the replace */
  for (i= 0; i < r->regex_arr.size(); i++)
  {
    struct st_regex re(r->regex_arr[i]);
    char* save_out_buf= out_buf;

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
  /* Allow variable for the *entire* list of replacements */
  if (*expr == '$') 
  {
    VAR *val= var_get(expr, NULL, 0, 1);
    expr= val ? val->str_val : NULL;
  }
  if (expr && *expr && !(glob_replace_regex=init_replace_regex(expr)))
    die("Could not init replace_regex");
  command->last_argument= command->end;
}

void free_replace_regex()
{
  if (glob_replace_regex)
  {
    my_free(glob_replace_regex->even_buf);
    my_free(glob_replace_regex->odd_buf);
    glob_replace_regex->~st_replace_regex();
    my_free(glob_replace_regex);
    glob_replace_regex= NULL;
  }
}



/*
  auxiluary macro used by reg_replace
  makes sure the result buffer has sufficient length
*/
#define SECURE_REG_BUF   if (buf_len < need_buf_len)                    \
  {                                                                     \
    my_ptrdiff_t off= res_p - buf;                                      \
    buf= (char*)my_realloc(PSI_NOT_INSTRUMENTED,                        \
                           buf,need_buf_len,MYF(MY_WME+MY_FAE));        \
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
  size_t len;
  size_t buf_len, need_buf_len;
  int cflags= MY_REG_EXTENDED;
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
    cflags|= MY_REG_ICASE;

  if ((err_code= my_regcomp(&r,pattern,cflags,&my_charset_latin1)))
  {
    check_regerr(&r,err_code);
    return 1;
  }

  subs= (my_regmatch_t*)my_malloc(PSI_NOT_INSTRUMENTED,
                                  sizeof(my_regmatch_t) * (r.re_nsub+1),
                                  MYF(MY_WME+MY_FAE));

  *res_p= 0;
  str_p= string;
  replace_end= replace + strlen(replace);

  /* for each pattern match instance perform a replacement */
  while (!err_code)
  {
    /* find the match */
    err_code= my_regexec(&r,str_p, r.re_nsub+1, subs,
                         (str_p != string) ? MY_REG_NOTBOL : 0);

    /* if regular expression error (eg. bad syntax, or out of memory) */
    if (err_code && err_code != MY_REG_NOMATCH)
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
          my_regoff_t start_off, end_off;
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
          my_regoff_t start_off, end_off;
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
      size_t left_in_str= str_end-str_p;
      need_buf_len= (res_p-buf) + left_in_str;
      SECURE_REG_BUF
        memcpy(res_p,str_p,left_in_str);
      res_p += left_in_str;
      str_p= str_end;
    }
  }
  my_free(subs);
  my_regfree(&r);
  *res_p= 0;
  *buf_p= buf;
  *buf_len_p= static_cast<int>(buf_len);
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
uint start_at_word(char * pos);
uint end_of_word(char * pos);

static uint found_sets=0;


uint replace_len(char * str)
{
  uint len=0;
  while (*str)
  {
    str++;
    len++;
  }
  return len;
}

/* Init a replace structure for further calls */

REPLACE *init_replace(char * *from, char * *to,uint count,
		      char * word_end_chars)
{
  static const int SPACE_CHAR= 256;
  static const int END_OF_LINE= 258;

  uint i,j,states,set_nr,len,result_len,max_length,found_end,bits_set,bit_nr;
  int used_sets,chr,default_state;
  char used_chars[LAST_CHAR_CODE],is_word_end[256];
  char * pos, *to_pos, **to_array;
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
      DBUG_RETURN(0);
    }
    states+=len+1;
    result_len+=(uint) strlen(to[i])+1;
    if (len > max_length)
      max_length=len;
  }
  memset(is_word_end, 0, sizeof(is_word_end));
  for (i=0 ; word_end_chars[i] ; i++)
    is_word_end[(uchar) word_end_chars[i]]=1;

  if (init_sets(&sets,states))
    DBUG_RETURN(0);
  found_sets=0;
  if (!(found_set= (FOUND_SET*) my_malloc(PSI_NOT_INSTRUMENTED,
                                          sizeof(FOUND_SET)*max_length*count,
					  MYF(MY_WME))))
  {
    free_sets(&sets);
    DBUG_RETURN(0);
  }
  (void) make_new_set(&sets);			/* Set starting set */
  make_sets_invisible(&sets);			/* Hide previus sets */
  used_sets=-1;
  word_states=make_new_set(&sets);		/* Start of new word */
  start_states=make_new_set(&sets);		/* This is first state */
  if (!(follow=(FOLLOWS*) my_malloc(PSI_NOT_INSTRUMENTED,
                                    (states+2)*sizeof(FOLLOWS),MYF(MY_WME))))
  {
    free_sets(&sets);
    my_free(found_set);
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
    memset(used_chars, 0, sizeof(used_chars));
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
        assert(new_set);
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

  if ((replace=(REPLACE*) my_malloc(PSI_NOT_INSTRUMENTED,
                                    sizeof(REPLACE)*(sets.count)+
				    sizeof(REPLACE_STRING)*(found_sets+1)+
				    sizeof(char *)*count+result_len,
				    MYF(MY_WME | MY_ZEROFILL))))
  {
    rep_str=(REPLACE_STRING*) (replace+sets.count);
    to_array= (char **) (rep_str+found_sets+1);
    to_pos=(char *) (to_array+count);
    for (i=0 ; i < count ; i++)
    {
      to_array[i]=to_pos;
      to_pos=my_stpcpy(to_pos,to[i])+1;
    }
    rep_str[0].found=1;
    rep_str[0].replace_string=0;
    for (i=1 ; i <= found_sets ; i++)
    {
      pos=from[found_set[i-1].table_offset];
      rep_str[i].found= !memcmp(pos, "\\^", 3) ? 2 : 1;
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
  my_free(follow);
  free_sets(&sets);
  my_free(found_set);
  DBUG_PRINT("exit",("Replace table has %d states",sets.count));
  DBUG_RETURN(replace);
}


int init_sets(REP_SETS *sets,uint states)
{
  memset(sets, 0, sizeof(*sets));
  sets->size_of_bits=((states+7)/8);
  if (!(sets->set_buffer=(REP_SET*) my_malloc(PSI_NOT_INSTRUMENTED,
                                              sizeof(REP_SET)*SET_MALLOC_HUNC,
					      MYF(MY_WME))))
    return 1;
  if (!(sets->bit_buffer=(uint*) my_malloc(PSI_NOT_INSTRUMENTED,
                                           sizeof(uint)*sets->size_of_bits*
					   SET_MALLOC_HUNC,MYF(MY_WME))))
  {
    my_free(sets->set);
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
    memset(set->bits, 0, sizeof(uint)*sets->size_of_bits);
    memset(&set->next[0], 0, sizeof(set->next[0])*LAST_CHAR_CODE);
    set->found_offset=0;
    set->found_len=0;
    set->table_offset= (uint) ~0;
    set->size_of_bits=sets->size_of_bits;
    return set;
  }
  count=sets->count+sets->invisible+SET_MALLOC_HUNC;
  if (!(set=(REP_SET*) my_realloc(PSI_NOT_INSTRUMENTED,
                                  (uchar*) sets->set_buffer,
                                  sizeof(REP_SET)*count,
				  MYF(MY_WME))))
    return 0;
  sets->set_buffer=set;
  sets->set=set+sets->invisible;
  if (!(bit_buffer=(uint*) my_realloc(PSI_NOT_INSTRUMENTED,
                                      (uchar*) sets->bit_buffer,
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
  my_free(sets->set_buffer);
  my_free(sets->bit_buffer);
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
  uint i;
  for (i=0 ; i < to->size_of_bits ; i++)
    to->bits[i]|=from->bits[i];
  return;
}

void copy_bits(REP_SET *to,REP_SET *from)
{
  memcpy((uchar*) to->bits,(uchar*) from->bits,
	 (size_t) (sizeof(uint) * to->size_of_bits));
}

int cmp_bits(REP_SET *set1,REP_SET *set2)
{
  return memcmp(set1->bits, set2->bits,
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
  return i;				/* return new position */
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
  return -i-2;				/* return new position */
}

/* Return 1 if regexp starts with \b or ends with \b*/

uint start_at_word(char * pos)
{
  return (((!memcmp(pos, "\\b",2) && pos[2]) ||
           !memcmp(pos, "\\^", 2)) ? 1 : 0);
}

uint end_of_word(char * pos)
{
  char * end=strend(pos);
  return ((end > pos+2 && !memcmp(end-2, "\\b", 2)) ||
	  (end >= pos+2 && !memcmp(end-2, "\\$",2))) ? 1 : 0;
}

/****************************************************************************
 * Handle replacement of strings
 ****************************************************************************/

#define PC_MALLOC		256	/* Bytes for pointers */
#define PS_MALLOC		512	/* Bytes for data */

int insert_pointer_name(POINTER_ARRAY *pa,char * name)
{
  uint i,length,old_count;
  uchar *new_pos;
  const char **new_array;
  DBUG_ENTER("insert_pointer_name");

  if (! pa->typelib.count)
  {
    if (!(pa->typelib.type_names=(const char **)
	  my_malloc(PSI_NOT_INSTRUMENTED,
                    ((PC_MALLOC-MALLOC_OVERHEAD)/
		     (sizeof(char *)+sizeof(*pa->flag))*
		     (sizeof(char *)+sizeof(*pa->flag))),MYF(MY_WME))))
      DBUG_RETURN(-1);
    if (!(pa->str= (uchar*) my_malloc(PSI_NOT_INSTRUMENTED,
                                      (uint) (PS_MALLOC-MALLOC_OVERHEAD),
				     MYF(MY_WME))))
    {
      my_free(pa->typelib.type_names);
      DBUG_RETURN (-1);
    }
    pa->max_count=(PC_MALLOC-MALLOC_OVERHEAD)/(sizeof(uchar*)+
					       sizeof(*pa->flag));
    pa->flag= (uint8*) (pa->typelib.type_names+pa->max_count);
    pa->length=0;
    pa->max_length=PS_MALLOC-MALLOC_OVERHEAD;
    pa->array_allocs=1;
  }
  length=(uint) strlen(name)+1;
  if (pa->length+length >= pa->max_length)
  {
    if (!(new_pos= (uchar*) my_realloc(PSI_NOT_INSTRUMENTED,
                                       (uchar*) pa->str,
                                      (uint) (pa->length+length+PS_MALLOC),
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
    pa->max_length= pa->length+length+PS_MALLOC;
  }
  if (pa->typelib.count >= pa->max_count-1)
  {
    int len;
    pa->array_allocs++;
    len=(PC_MALLOC*pa->array_allocs - MALLOC_OVERHEAD);
    if (!(new_array=(const char **) my_realloc(PSI_NOT_INSTRUMENTED,
                                               (uchar*) pa->typelib.type_names,
					       (uint) len/
                                               (sizeof(uchar*)+sizeof(*pa->flag))*
                                               (sizeof(uchar*)+sizeof(*pa->flag)),
                                               MYF(MY_WME))))
      DBUG_RETURN(1);
    pa->typelib.type_names=new_array;
    old_count=pa->max_count;
    pa->max_count=len/(sizeof(uchar*) + sizeof(*pa->flag));
    pa->flag= (uint8*) (pa->typelib.type_names+pa->max_count);
    memcpy((uchar*) pa->flag,(char *) (pa->typelib.type_names+old_count),
	   old_count*sizeof(*pa->flag));
  }
  pa->flag[pa->typelib.count]=0;			/* Reset flag */
  pa->typelib.type_names[pa->typelib.count++]= (char*) pa->str+pa->length;
  pa->typelib.type_names[pa->typelib.count]= NullS;	/* Put end-mark */
  (void) my_stpcpy((char*) pa->str+pa->length,name);
  pa->length+=length;
  DBUG_RETURN(0);
} /* insert_pointer_name */


/* free pointer array */

void free_pointer_array(POINTER_ARRAY *pa)
{
  if (pa->typelib.count)
  {
    pa->typelib.count=0;
    my_free(pa->typelib.type_names);
    pa->typelib.type_names=0;
    my_free(pa->str);
  }
} /* free_pointer_array */


/* Functions that uses replace and replace_regex */

/* Append the string to ds, with optional replace */
void replace_dynstr_append_mem(DYNAMIC_STRING *ds,
                               const char *val, size_t len)
{
  char lower[512];
#ifdef _WIN32
  fix_win_paths(val, len);
#endif

  if (display_result_lower) 
  {
    /* Convert to lower case, and do this first */
    char *c= lower;
    for (const char *v= val;  *v;  v++)
      *c++= my_tolower(charset_info, *v);
    *c= '\0';
    /* Copy from this buffer instead */
    val= lower;
  }
  
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

class Comp_lines :
  public std::binary_function<const char*, const char *, bool>
{
public:
  bool operator()(const char *a, const char *b)
  {
    return strcmp(a, b) < 0;
  }
};

void dynstr_append_sorted(DYNAMIC_STRING* ds, DYNAMIC_STRING *ds_input)
{
  char *start= ds_input->str;
  Prealloced_array<const char*, 32> lines(PSI_NOT_INSTRUMENTED);
  DBUG_ENTER("dynstr_append_sorted");

  if (!*start)
    DBUG_VOID_RETURN;  /* No input */

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
    if (lines.push_back(start))
      die("Out of memory inserting lines to sort");

    start= line_end+1;
  }

  /* Sort array */
  std::sort(lines.begin(), lines.end(), Comp_lines());

  /* Create new result */
  for (const char **line= lines.begin(); line != lines.end(); ++line)
  {
    dynstr_append(ds, *line);
    dynstr_append(ds, "\n");
  }

  DBUG_VOID_RETURN;
}
