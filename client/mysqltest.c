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

/* mysqltest test tool
 * See the manual for more information
 * TODO: document better how mysqltest works
 *
 * Written by:
 *   Sasha Pachev <sasha@mysql.com>
 *   Matt Wagner  <matt@mysql.com>
 *   Monty
 *   Jani
 **/

/**********************************************************************
  TODO:

- Do comparison line by line, instead of doing a full comparison of
  the text file.  This will save space as we don't need to keep many
  results in memory.  It will also make it possible to do simple
  'comparison' fixes like accepting the result even if a float differed
  in the last decimals.

- Don't buffer lines from the test that you don't expect to need
  again.

- Change 'read_line' to be faster by using the readline.cc code;
  We can do better than calling feof() for each character!

**********************************************************************/

#define MTEST_VERSION "2.1"

#include <my_global.h>
#include <mysql_embed.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <mysql_version.h>
#include <mysqld_error.h>
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include <my_getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <violite.h>

#define MAX_QUERY     131072
#define MAX_VAR_NAME	256
#define MAX_COLUMNS	256
#define PAD_SIZE	128
#define MAX_CONS	128
#define MAX_INCLUDE_DEPTH 16
#define LAZY_GUESS_BUF_SIZE 8192
#define INIT_Q_LINES	  1024
#define MIN_VAR_ALLOC	  32
#define BLOCK_STACK_DEPTH  32
#define MAX_EXPECTED_ERRORS 10
#define QUERY_SEND  1
#define QUERY_REAP  2
#ifndef MYSQL_MANAGER_PORT
#define MYSQL_MANAGER_PORT 23546
#endif
#define MAX_SERVER_ARGS 20

/*
  Sometimes in a test the client starts before
  the server - to solve the problem, we try again
  after some sleep if connection fails the first
  time
*/
#define CON_RETRY_SLEEP 2
#define MAX_CON_TRIES	5

#define SLAVE_POLL_INTERVAL 300000 /* 0.3 of a sec */
#define DEFAULT_DELIMITER ";"
#define MAX_DELIMITER 16

enum {OPT_MANAGER_USER=256,OPT_MANAGER_HOST,OPT_MANAGER_PASSWD,
      OPT_MANAGER_PORT,OPT_MANAGER_WAIT_TIMEOUT, OPT_SKIP_SAFEMALLOC,
      OPT_SSL_SSL, OPT_SSL_KEY, OPT_SSL_CERT, OPT_SSL_CA, OPT_SSL_CAPATH,
      OPT_SSL_CIPHER};

static int record = 0, opt_sleep=0;
static char *db = 0, *pass=0;
const char* user = 0, *host = 0, *unix_sock = 0, *opt_basedir="./";
static int port = 0;
static my_bool opt_big_test= 0, opt_compress= 0, silent= 0, verbose = 0,
	       tty_password= 0;
static uint start_lineno, *lineno;
const char* manager_user="root",*manager_host=0;
char *manager_pass=0;
int manager_port=MYSQL_MANAGER_PORT;
int manager_wait_timeout=3;
MYSQL_MANAGER* manager=0;

static char **default_argv;
static const char *load_default_groups[]= { "mysqltest","client",0 };
static char line_buffer[MAX_DELIMITER], *line_buffer_pos= line_buffer;

static FILE* file_stack[MAX_INCLUDE_DEPTH];
static FILE** cur_file;
static FILE** file_stack_end;
static uint lineno_stack[MAX_INCLUDE_DEPTH];
static char TMPDIR[FN_REFLEN];
static char delimiter[MAX_DELIMITER]= DEFAULT_DELIMITER;
static uint delimiter_length= 1;

static int *cur_block, *block_stack_end;
static int block_stack[BLOCK_STACK_DEPTH];

static int block_ok_stack[BLOCK_STACK_DEPTH];
static uint global_expected_errno[MAX_EXPECTED_ERRORS], global_expected_errors;
static CHARSET_INFO *charset_info= &my_charset_latin1; /* Default charset */
static char *charset_name = "latin1"; /* Default character set name */

static int embedded_server_arg_count=0;
static char *embedded_server_args[MAX_SERVER_ARGS];

static my_bool display_result_vertically= FALSE, display_metadata= FALSE;

static const char *embedded_server_groups[] = {
  "server",
  "embedded",
  "mysqltest_SERVER",
  NullS
};

DYNAMIC_ARRAY q_lines;

#include "sslopt-vars.h"

typedef struct
{
  char file[FN_REFLEN];
  ulong pos;
} MASTER_POS ;

struct connection
{
  MYSQL mysql;
  char *name;
};

typedef struct
{
  int read_lines,current_line;
} PARSER;

MYSQL_RES *last_result=0;

PARSER parser;
MASTER_POS master_pos;
int *block_ok; /* set to 0 if the current block should not be executed */
int false_block_depth = 0;
/* if set, all results are concated and compared against this file */
const char *result_file = 0;

typedef struct
{
  char *name;
  int name_len;
  char *str_val;
  int str_val_len;
  int int_val;
  int alloced_len;
  int int_dirty; /* do not update string if int is updated until first read */
  int alloced;
} VAR;

#ifdef __NETWARE__
/*
  Netware doesn't proved environment variable substitution that is done
  by the shell in unix environments. We do this in the following function:
*/

static char *subst_env_var(const char *cmd);
static FILE *my_popen(const char *cmd, const char *mode);
#define popen(A,B) my_popen((A),(B))
#endif /* __NETWARE__ */

VAR var_reg[10];
/*Perl/shell-like variable registers */
HASH var_hash;
my_bool disable_query_log=0, disable_result_log=0, disable_warnings=0;
my_bool disable_info= 1;			/* By default off */

struct connection cons[MAX_CONS];
struct connection* cur_con, *next_con, *cons_end;

  /* Add new commands before Q_UNKNOWN !*/

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
Q_SERVER_START, Q_SERVER_STOP,Q_REQUIRE_MANAGER,
Q_WAIT_FOR_SLAVE_TO_STOP,
Q_ENABLE_WARNINGS, Q_DISABLE_WARNINGS,
Q_ENABLE_INFO, Q_DISABLE_INFO,
Q_ENABLE_METADATA, Q_DISABLE_METADATA,
Q_EXEC, Q_DELIMITER,
Q_DISPLAY_VERTICAL_RESULTS, Q_DISPLAY_HORIZONTAL_RESULTS,
Q_QUERY_VERTICAL, Q_QUERY_HORIZONTAL,
Q_CHARACTER_SET,

Q_UNKNOWN,			       /* Unknown command.   */
Q_COMMENT,			       /* Comments, ignored. */
Q_COMMENT_WITH_COMMAND
};

/* this should really be called command */
struct st_query
{
  char *query, *query_buf,*first_argument,*end;
  int first_word_len;
  my_bool abort_on_error, require_file;
  uint expected_errno[MAX_EXPECTED_ERRORS];
  uint expected_errors;
  char record_file[FN_REFLEN];
  enum enum_commands type;
};

const char *command_names[]=
{
  "connection",
  "query",
  "connect",
  /* the difference between sleep and real_sleep is that sleep will use
     the delay from command line (--sleep) if there is one.
     real_sleep always uses delay from it's argument.
     the logic is that sometimes delays are cpu-dependent (and --sleep
     can be used to set this delay. real_sleep is used for cpu-independent
     delays
   */
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
  "enable_query_log",
  "disable_query_log",
  "enable_result_log",
  "disable_result_log",
  "server_start",
  "server_stop",
  "require_manager",
  "wait_for_slave_to_stop",
  "enable_warnings",
  "disable_warnings",
  "enable_info",
  "disable_info",
  "enable_metadata",
  "disable_metadata",
  "exec",
  "delimiter",
  "vertical_results",
  "horizontal_results",
  "query_vertical",
  "query_horizontal",
  "character_set",
  0
};

TYPELIB command_typelib= {array_elements(command_names),"",
			  command_names};

DYNAMIC_STRING ds_res;
static void die(const char *fmt, ...);
static void init_var_hash();
static VAR* var_from_env(const char *, const char *);
static byte* get_var_key(const byte* rec, uint* len,
			 my_bool __attribute__((unused)) t);
static VAR* var_init(VAR* v, const char *name, int name_len, const char *val,
		     int val_len);

static void var_free(void* v);

int dyn_string_cmp(DYNAMIC_STRING* ds, const char *fname);
void reject_dump(const char *record_file, char *buf, int size);

int close_connection(struct st_query* q);
static void set_charset(struct st_query*);
VAR* var_get(const char *var_name, const char** var_name_end, my_bool raw,
	     my_bool ignore_not_existing);
int eval_expr(VAR* v, const char *p, const char** p_end);
static int read_server_arguments(const char *name);

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
uint replace_strings(struct st_replace *rep, my_string *start,
		     uint *max_length, const char *from);
void free_replace();
static int insert_pointer_name(reg1 POINTER_ARRAY *pa,my_string name);
void free_pointer_array(POINTER_ARRAY *pa);
static int initialize_replace_buffer(void);
static void free_replace_buffer(void);
static void do_eval(DYNAMIC_STRING* query_eval, const char *query);
void str_to_file(const char *fname, char *str, int size);
int do_server_op(struct st_query* q,const char *op);

struct st_replace *glob_replace;
static char *out_buff;
static uint out_length;
static int eval_result = 0;

/* For column replace */
char *replace_column[MAX_COLUMNS];
uint max_replace_column= 0;

static void get_replace_column(struct st_query *q);
static void free_replace_column();

/* Disable functions that only exist in MySQL 4.0 */
#if MYSQL_VERSION_ID < 40000
void mysql_enable_rpl_parse(MYSQL* mysql __attribute__((unused))) {}
void mysql_disable_rpl_parse(MYSQL* mysql __attribute__((unused))) {}
int mysql_rpl_parse_enabled(MYSQL* mysql __attribute__((unused))) { return 1; }
my_bool mysql_rpl_probe(MYSQL *mysql __attribute__((unused))) { return 1; }
#endif
static void replace_dynstr_append_mem(DYNAMIC_STRING *ds, const char *val,
				      int len);

static void do_eval(DYNAMIC_STRING* query_eval, const char* query)
{
  const char* p;
  register char c;
  register int escaped = 0;
  VAR* v;
  DBUG_ENTER("do_eval");

  for (p= query; (c = *p); ++p)
  {
    switch(c) {
    case '$':
      if (escaped)
      {
	escaped = 0;
	dynstr_append_mem(query_eval, p, 1);
      }
      else
      {
	if (!(v = var_get(p, &p, 0, 0)))
	  die("Bad variable in eval");
	dynstr_append_mem(query_eval, v->str_val, v->str_val_len);
      }
      break;
    case '\\':
      if (escaped)
      {
	escaped = 0;
	dynstr_append_mem(query_eval, p, 1);
      }
      else
	escaped = 1;
      break;
    default:
      dynstr_append_mem(query_eval, p, 1);
      break;
    }
  }
  DBUG_VOID_RETURN;
}


static void close_cons()
{
  DBUG_ENTER("close_cons");
  if (last_result)
    mysql_free_result(last_result);
  for (--next_con; next_con >= cons; --next_con)
  {
    mysql_close(&next_con->mysql);
    my_free(next_con->name, MYF(MY_ALLOW_ZERO_PTR));
  }
  DBUG_VOID_RETURN;
}


static void close_files()
{
  DBUG_ENTER("close_files");
  for (; cur_file != file_stack ; cur_file--)
  {
    if (*cur_file != stdin && *cur_file)
      my_fclose(*cur_file,MYF(0));
  }
  DBUG_VOID_RETURN;
}


static void free_used_memory()
{
  uint i;
  DBUG_ENTER("free_used_memory");
#ifndef EMBEDDED_LIBRARY
  if (manager)
    mysql_manager_close(manager);
#endif
  close_cons();
  close_files();
  hash_free(&var_hash);

  for (i=0 ; i < q_lines.elements ; i++)
  {
    struct st_query **q= dynamic_element(&q_lines, i, struct st_query**);
    my_free((gptr) (*q)->query_buf,MYF(MY_ALLOW_ZERO_PTR));
    my_free((gptr) (*q),MYF(0));
  }
  for (i=0; i < 10; i++)
  {
    if (var_reg[i].alloced_len)
      my_free(var_reg[i].str_val, MYF(MY_WME));
  }
  while (embedded_server_arg_count > 1)
    my_free(embedded_server_args[--embedded_server_arg_count],MYF(0));
  delete_dynamic(&q_lines);
  dynstr_free(&ds_res);
  free_replace();
  free_replace_column();
  my_free(pass,MYF(MY_ALLOW_ZERO_PTR));
  free_defaults(default_argv);
  mysql_server_end();
  my_end(MY_CHECK_ERROR);
  DBUG_VOID_RETURN;
}

static void die(const char* fmt, ...)
{
  va_list args;
  DBUG_ENTER("die");
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "%s: ", my_progname);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);
  free_used_memory();
  exit(1);
}

/* Note that we will get some memory leaks when calling this! */

static void abort_not_supported_test()
{
  DBUG_ENTER("abort_not_supported_test");
  fprintf(stderr, "This test is not supported by this installation\n");
  if (!silent)
    printf("skipped\n");
  free_used_memory();
  exit(2);
}

static void verbose_msg(const char* fmt, ...)
{
  va_list args;
  DBUG_ENTER("verbose_msg");
  if (!verbose)
    DBUG_VOID_RETURN;

  va_start(args, fmt);

  fprintf(stderr, "%s: At line %u: ", my_progname, start_lineno);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  DBUG_VOID_RETURN;
}


void init_parser()
{
  parser.current_line = parser.read_lines = 0;
  memset(&var_reg,0, sizeof(var_reg));
}


int dyn_string_cmp(DYNAMIC_STRING* ds, const char* fname)
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
    fn_format(eval_file, eval_file,"","",4);
  }
  else
    fn_format(eval_file, fname,"","",4);

  if (!my_stat(eval_file, &stat_info, MYF(MY_WME)))
    die(NullS);
  if (!eval_result && (uint) stat_info.st_size != ds->length)
  {
    DBUG_PRINT("info",("Size differs:  result size: %u  file size: %u",
		       ds->length, stat_info.st_size));
    DBUG_RETURN(2);
  }
  if (!(tmp = (char*) my_malloc(stat_info.st_size + 1, MYF(MY_WME))))
    die(NullS);

  if ((fd = my_open(eval_file, O_RDONLY, MYF(MY_WME))) < 0)
    die(NullS);
  if (my_read(fd, (byte*)tmp, stat_info.st_size, MYF(MY_WME|MY_NABP)))
    die(NullS);
  tmp[stat_info.st_size] = 0;
  init_dynamic_string(&res_ds, "", 0, 65536);
  if (eval_result)
  {
    do_eval(&res_ds, tmp);
    res_ptr = res_ds.str;
    if ((res_len = res_ds.length) != ds->length)
    {
      res = 2;
      goto err;
    }
  }
  else
  {
    res_ptr = tmp;
    res_len = stat_info.st_size;
  }

  res = (memcmp(res_ptr, ds->str, res_len)) ?  1 : 0;

err:
  if (res && eval_result)
    str_to_file(fn_format(eval_file, fname, "", ".eval",2), res_ptr,
		res_len);

  my_free((gptr) tmp, MYF(0));
  my_close(fd, MYF(MY_WME));
  dynstr_free(&res_ds);

  DBUG_RETURN(res);
}

static int check_result(DYNAMIC_STRING* ds, const char* fname,
			my_bool require_option)
{
  int error = 0;
  int res=dyn_string_cmp(ds, fname);

  if (res && require_option)
    abort_not_supported_test();
  switch (res) {
  case 0:
    break; /* ok */
  case 2:
    verbose_msg("Result length mismatch");
    error = 1;
    break;
  case 1:
    verbose_msg("Result content mismatch");
    error = 1;
    break;
  default: /* impossible */
    die("Unknown error code from dyn_string_cmp()");
  }
  if (error)
    reject_dump(fname, ds->str, ds->length);
  return error;
}


VAR* var_get(const char* var_name, const char** var_name_end, my_bool raw,
	     my_bool ignore_not_existing)
{
  int digit;
  VAR* v;
  DBUG_ENTER("var_get");
  DBUG_PRINT("enter",("var_name: %s",var_name));

  if (*var_name != '$')
    goto err;
  digit = *++var_name - '0';
  if (digit < 0 || digit >= 10)
  {
    const char* save_var_name = var_name, *end;
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

    if (!(v = (VAR*) hash_search(&var_hash, save_var_name, length)) &&
        length < MAX_VAR_NAME)
    {
      char buff[MAX_VAR_NAME+1];
      strmake(buff, save_var_name, length);
      v= var_from_env(buff, "");
    }
    var_name--;					/* Point at last character */
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

static VAR* var_obtain(char* name, int len)
{
  VAR* v;
  if ((v = (VAR*)hash_search(&var_hash, name, len)))
    return v;
  v = var_init(0, name, len, "", 0);
  my_hash_insert(&var_hash, (byte*)v);
  return v;
}

int var_set(char* var_name, char* var_name_end, char* var_val,
	    char* var_val_end)
{
  int digit;
  VAR* v;
  if (*var_name++ != '$')
    {
      --var_name;
      *var_name_end = 0;
      die("Variable name in %s does not start with '$'", var_name);
    }
  digit = *var_name - '0';
  if (!(digit < 10 && digit >= 0))
    {
      v = var_obtain(var_name, var_name_end - var_name);
    }
  else
   v = var_reg + digit;

  return eval_expr(v, var_val, (const char**)&var_val_end);
}

int open_file(const char* name)
{
  char buff[FN_REFLEN];
  if (!test_if_hard_path(name))
  {
    strxmov(buff, opt_basedir, name, NullS);
    name=buff;
  }
  fn_format(buff,name,"","",4);

  if (*cur_file && cur_file == file_stack_end)
    die("Source directives are nesting too deep");
  if (!(*(cur_file+1) = my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(MY_WME))))
    die(NullS);
  cur_file++;
  *++lineno=1;

  return 0;
}


/* ugly long name, but we are following the convention */
int do_wait_for_slave_to_stop(struct st_query* q __attribute__((unused)))
{
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
  return 0;
}

int do_require_manager(struct st_query* a __attribute__((unused)))
{
  if (!manager)
    abort_not_supported_test();
  return 0;
}

#ifndef EMBEDDED_LIBRARY
int do_server_start(struct st_query* q)
{
  return do_server_op(q,"start");
}

int do_server_stop(struct st_query* q)
{
  return do_server_op(q,"stop");
}

int do_server_op(struct st_query* q,const char* op)
{
  char* p=q->first_argument;
  char com_buf[256],*com_p;
  if (!manager)
  {
    die("Manager is not initialized, manager commands are not possible");
  }
  com_p=strmov(com_buf,op);
  com_p=strmov(com_p,"_exec ");
  if (!*p)
    die("Missing server name in server_%s\n",op);
  while (*p && !my_isspace(charset_info,*p))
   *com_p++= *p++;
  *com_p++=' ';
  com_p=int10_to_str(manager_wait_timeout,com_p,10);
  *com_p++ = '\n';
  *com_p=0;
  if (mysql_manager_command(manager,com_buf,(int)(com_p-com_buf)))
    die("Error in command: %s(%d)",manager->last_error,manager->last_errno);
  while (!manager->eof)
  {
    if (mysql_manager_fetch_line(manager,com_buf,sizeof(com_buf)))
      die("Error fetching result line: %s(%d)", manager->last_error,
	  manager->last_errno);
  }

  return 0;
}
#endif

int do_source(struct st_query* q)
{
  char* p=q->first_argument, *name;
  if (!*p)
    die("Missing file name in source\n");
  name = p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  *p = 0;

  return open_file(name);
}

/*
  Execute given command.

  SYNOPSIS
    do_exec()
    q	called command

  DESCRIPTION
    If one uses --exec command [args] command in .test file
    we will execute the command and record its output.

  RETURN VALUES
    0	ok
    1	error
*/

int do_exec(struct st_query* q)
{
  int error= 0;
  DYNAMIC_STRING *ds;
  DYNAMIC_STRING ds_tmp;
  char buf[1024];
  FILE *res_file;
  char *cmd= q->first_argument;
  DBUG_ENTER("do_exec");

  while (*cmd && my_isspace(charset_info, *cmd))
    cmd++;
  if (!*cmd)
    die("Missing argument in exec\n");

  DBUG_PRINT("info", ("Executing '%s'", cmd));

  if (!(res_file= popen(cmd, "r")) && q->abort_on_error)
    die("popen() failed\n");

  if (disable_result_log)
  {
    while (fgets(buf, sizeof(buf), res_file))
    {
      buf[strlen(buf)-1]=0;
      DBUG_PRINT("exec_result",("%s", buf));
    }
  }
  else
  {
    if (q->record_file[0])
    {
      init_dynamic_string(&ds_tmp, "", 16384, 65536);
      ds= &ds_tmp;
    }
    else
      ds= &ds_res;

    while (fgets(buf, sizeof(buf), res_file))
      replace_dynstr_append_mem(ds, buf, strlen(buf));

    if (glob_replace)
      free_replace();

    if (record)
    {
      if (!q->record_file[0] && !result_file)
        die("At line %u: Missing result file", start_lineno);
      if (!result_file)
        str_to_file(q->record_file, ds->str, ds->length);
    }
    else if (q->record_file[0])
    {
      error= check_result(ds, q->record_file, q->require_file);
    }
    if (ds == &ds_tmp)
      dynstr_free(&ds_tmp);
  }
  pclose(res_file);
  
  DBUG_RETURN(error);
}


int var_query_set(VAR* v, const char* p, const char** p_end)
{
  char* end = (char*)((p_end && *p_end) ? *p_end : p + strlen(p));
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL* mysql = &cur_con->mysql;
  LINT_INIT(res);

  while (end > p && *end != '`')
    --end;
  if (p == end)
    die("Syntax error in query, missing '`'");
  ++p;

  if (mysql_real_query(mysql, p, (int)(end - p)) ||
      !(res = mysql_store_result(mysql)))
  {
    *end = 0;
    die("Error running query '%s': %s", p, mysql_error(mysql));
  }

  if ((row = mysql_fetch_row(res)) && row[0])
  {
    /*
      Concatenate all row results with tab in between to allow us to work
      with results from many columns (for example from SHOW VARIABLES)
    */
    DYNAMIC_STRING result;
    uint i;
    ulong *lengths;
    char *end;

    init_dynamic_string(&result, "", 16384, 65536);
    lengths= mysql_fetch_lengths(res);
    for (i=0; i < mysql_num_fields(res); i++)
    {
      if (row[0])
	dynstr_append_mem(&result, row[i], lengths[i]);
      dynstr_append_mem(&result, "\t", 1);
    }
    end= result.str + result.length-1;
    eval_expr(v, result.str, (const char**) &end);
    dynstr_free(&result);
  }
  else
    eval_expr(v, "", 0);

  mysql_free_result(res);
  return 0;
}

void var_copy(VAR* dest, VAR* src)
{
  dest->int_val=src->int_val;
  dest->int_dirty=src->int_dirty;
  if (dest->alloced_len < src->alloced_len &&
      !(dest->str_val=my_realloc(dest->str_val,src->alloced_len+1,
				 MYF(MY_WME))))
    die("Out of memory");
  dest->str_val_len=src->str_val_len;
  memcpy(dest->str_val,src->str_val,src->str_val_len+1);
}

int eval_expr(VAR* v, const char* p, const char** p_end)
{
  VAR* vp;
  if (*p == '$')
  {
    if ((vp = var_get(p,p_end,0,0)))
    {
      var_copy(v, vp);
      return 0;
    }
  }
  else if (*p == '`')
  {
    return var_query_set(v, p, p_end);
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
      return 0;
    }

  die("Invalid expr: %s", p);
  return 1;
}

int do_inc(struct st_query* q)
{
  char* p=q->first_argument;
  VAR* v;
  v = var_get(p, 0, 1, 0);
  v->int_val++;
  v->int_dirty = 1;
  return 0;
}

int do_dec(struct st_query* q)
{
  char* p=q->first_argument;
  VAR* v;
  v = var_get(p, 0, 1, 0);
  v->int_val--;
  v->int_dirty = 1;
  return 0;
}

int do_system(struct st_query* q)
{
  char* p=q->first_argument;
  VAR v;
  var_init(&v, 0, 0, 0, 0);
  eval_expr(&v, p, 0); /* NULL terminated */
  if (v.str_val_len)
  {
    char expr_buf[1024];
    if ((uint)v.str_val_len > sizeof(expr_buf) - 1)
      v.str_val_len = sizeof(expr_buf) - 1;
    memcpy(expr_buf, v.str_val, v.str_val_len);
    expr_buf[v.str_val_len] = 0;
    DBUG_PRINT("info", ("running system command '%s'", expr_buf));
    if (system(expr_buf) && q->abort_on_error)
      die("system command '%s' failed", expr_buf);
  }
  var_free(&v);
  return 0;
}

int do_echo(struct st_query* q)
{
  char* p=q->first_argument;
  VAR v;
  var_init(&v,0,0,0,0);
  eval_expr(&v, p, 0); /* NULL terminated */
  if (v.str_val_len)
  {
    fflush(stdout);
    write(1, v.str_val, v.str_val_len);
  }
  write(1, "\n", 1);
  var_free(&v);
  return 0;
}


int do_sync_with_master2(const char* p)
{
  MYSQL_RES* res;
  MYSQL_ROW row;
  MYSQL* mysql = &cur_con->mysql;
  char query_buf[FN_REFLEN+128];
  int offset= 0, tries= 0;
  int rpl_parse;

  if (!master_pos.file[0])
  {
    die("Line %u: Calling 'sync_with_master' without calling 'save_master_pos'", start_lineno);
  }
  rpl_parse = mysql_rpl_parse_enabled(mysql);
  mysql_disable_rpl_parse(mysql);

  if (*p)
    offset = atoi(p);

  sprintf(query_buf, "select master_pos_wait('%s', %ld)", master_pos.file,
	  master_pos.pos + offset);

wait_for_position:

  if (mysql_query(mysql, query_buf))
    die("line %u: failed in %s: %d: %s", start_lineno, query_buf,
	mysql_errno(mysql), mysql_error(mysql));

  if (!(last_result = res = mysql_store_result(mysql)))
    die("line %u: mysql_store_result() returned NULL for '%s'", start_lineno,
	query_buf);
  if (!(row = mysql_fetch_row(res)))
    die("line %u: empty result in %s", start_lineno, query_buf);
  if (!row[0])
  {
    /*
      It may be that the slave SQL thread has not started yet, though START
      SLAVE has been issued ?
    */
    if (tries++ == 3)
    {
      die("line %u: could not sync with master ('%s' returned NULL)", 
          start_lineno, query_buf);
    }
    sleep(1); /* So at most we will wait 3 seconds and make 4 tries */
    mysql_free_result(res);
    goto wait_for_position;
  }
  mysql_free_result(res);
  last_result=0;
  if (rpl_parse)
    mysql_enable_rpl_parse(mysql);

  return 0;
}

int do_sync_with_master(struct st_query* q)
{
  return do_sync_with_master2(q->first_argument);
}

int do_save_master_pos()
{
  MYSQL_RES* res;
  MYSQL_ROW row;
  MYSQL* mysql = &cur_con->mysql;
  const char *query;
  int rpl_parse;

  rpl_parse = mysql_rpl_parse_enabled(mysql);
  mysql_disable_rpl_parse(mysql);

  if (mysql_query(mysql, query= "show master status"))
    die("At line %u: failed in show master status: %d: %s", start_lineno,
	mysql_errno(mysql), mysql_error(mysql));

  if (!(last_result =res = mysql_store_result(mysql)))
    die("line %u: mysql_store_result() retuned NULL for '%s'", start_lineno,
	query);
  if (!(row = mysql_fetch_row(res)))
    die("line %u: empty result in show master status", start_lineno);
  strnmov(master_pos.file, row[0], sizeof(master_pos.file)-1);
  master_pos.pos = strtoul(row[1], (char**) 0, 10);
  mysql_free_result(res); last_result=0;

  if (rpl_parse)
    mysql_enable_rpl_parse(mysql);

  return 0;
}


int do_let(struct st_query* q)
{
  char* p=q->first_argument;
  char *var_name, *var_name_end, *var_val_start;
  if (!*p)
    die("Missing variable name in let\n");
  var_name = p;
  while (*p && (*p != '=' || my_isspace(charset_info,*p)))
    p++;
  var_name_end = p;
  if (*p == '=') p++;
  while (*p && my_isspace(charset_info,*p))
    p++;
  var_val_start = p;
  return var_set(var_name, var_name_end, var_val_start, q->end);
}

int do_rpl_probe(struct st_query* q __attribute__((unused)))
{
  DBUG_ENTER("do_rpl_probe");
  if (mysql_rpl_probe(&cur_con->mysql))
    die("Failed in mysql_rpl_probe(): '%s'", mysql_error(&cur_con->mysql));
  DBUG_RETURN(0);
}

int do_enable_rpl_parse(struct st_query* q __attribute__((unused)))
{
  mysql_enable_rpl_parse(&cur_con->mysql);
  return 0;
}

int do_disable_rpl_parse(struct st_query* q __attribute__((unused)))
{
  mysql_disable_rpl_parse(&cur_con->mysql);
  return 0;
}


int do_sleep(struct st_query* q, my_bool real_sleep)
{
  char *p=q->first_argument;
  while (*p && my_isspace(charset_info,*p))
    p++;
  if (!*p)
    die("Missing argument in sleep\n");
  if (opt_sleep && !real_sleep)
    my_sleep(opt_sleep * 1000000L);
  else
    my_sleep((ulong) (atof(p) * 1000000L));
  return 0;
}

static void get_file_name(char *filename, struct st_query* q)
{
  char* p=q->first_argument;
  strnmov(filename, p, FN_REFLEN);
  /* Remove end space */
  while (p > filename && my_isspace(charset_info,p[-1]))
    p--;
  p[0]=0;
}

static void set_charset(struct st_query* q)
{
  char* charset_name= q->first_argument;
  char* tmp;

  if (!charset_name || !*charset_name)
    die("Missing charset name in 'character_set'\n");
  /* Remove end space */
  tmp= charset_name;
  while (*tmp && !my_isspace(charset_info,*tmp))
    tmp++;
  *tmp= 0;

  charset_info= get_charset_by_csname(charset_name,MY_CS_PRIMARY,MYF(MY_WME));
  if (!charset_info)
    abort_not_supported_test();
}

static uint get_ints(uint *to,struct st_query* q)
{
  char* p=q->first_argument;
  long val;
  uint count=0;
  DBUG_ENTER("get_ints");

  if (!*p)
    die("Missing argument in %s\n", q->query);

  for (; (p=str2int(p,10,(long) INT_MIN, (long) INT_MAX, &val)) ; p++)
  {
    count++;
    *to++= (uint) val;
    if (*p != ',')
      break;
  }
  *to++=0;					/* End of data */
  DBUG_RETURN(count);
}

/*
  Get a string;  Return ptr to end of string
  Strings may be surrounded by " or '

  If string is a '$variable', return the value of the variable.
*/


static char *get_string(char **to_ptr, char **from_ptr,
			struct st_query* q)
{
  reg1 char c,sep;
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
    die("Wrong string argument in %s\n", q->query);

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


/*
  Get arguments for replace. The syntax is:
  replace from to [from to ...]
  Where each argument may be quoted with ' or "
  A argument may also be a variable, in which case the value of the
  variable is replaced.
*/

static void get_replace(struct st_query *q)
{
  uint i;
  char *from=q->first_argument;
  char *buff,*start;
  char word_end_chars[256],*pos;
  POINTER_ARRAY to_array,from_array;
  DBUG_ENTER("get_replace");

  free_replace();

  bzero((char*) &to_array,sizeof(to_array));
  bzero((char*) &from_array,sizeof(from_array));
  if (!*from)
    die("Missing argument in %s\n", q->query);
  start=buff=my_malloc(strlen(from)+1,MYF(MY_WME | MY_FAE));
  while (*from)
  {
    char *to=buff;
    to=get_string(&buff, &from, q);
    if (!*from)
      die("Wrong number of arguments to replace in %s\n", q->query);
    insert_pointer_name(&from_array,to);
    to=get_string(&buff, &from, q);
    insert_pointer_name(&to_array,to);
  }
  for (i=1,pos=word_end_chars ; i < 256 ; i++)
    if (my_isspace(charset_info,i))
      *pos++= i;
  *pos=0;					/* End pointer */
  if (!(glob_replace=init_replace((char**) from_array.typelib.type_names,
				  (char**) to_array.typelib.type_names,
				  (uint) from_array.typelib.count,
				  word_end_chars)) ||
      initialize_replace_buffer())
    die("Can't initialize replace from %s\n", q->query);
  free_pointer_array(&from_array);
  free_pointer_array(&to_array);
  my_free(start, MYF(0));
  DBUG_VOID_RETURN;
}

void free_replace()
{
  DBUG_ENTER("free_replace");
  if (glob_replace)
  {
    my_free((char*) glob_replace,MYF(0));
    glob_replace=0;
    free_replace_buffer();
  }
  DBUG_VOID_RETURN;
}

int select_connection(char *p)
{
  char* name;
  struct connection *con;
  DBUG_ENTER("select_connection");
  DBUG_PRINT("enter",("name: '%s'",p));

  if (!*p)
    die("Missing connection name in connect\n");
  name = p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  *p = 0;

  for (con = cons; con < next_con; con++)
  {
    if (!strcmp(con->name, name))
    {
      cur_con = con;
      DBUG_RETURN(0);
    }
  }
  die("connection '%s' not found in connection pool", name);
  DBUG_RETURN(1);				/* Never reached */
}

int close_connection(struct st_query* q)
{
  char* p=q->first_argument, *name;
  struct connection *con;
  DBUG_ENTER("close_connection");
  DBUG_PRINT("enter",("name: '%s'",p));

  if (!*p)
    die("Missing connection name in connect\n");
  name = p;
  while (*p && !my_isspace(charset_info,*p))
    p++;
  *p = 0;

  for (con = cons; con < next_con; con++)
  {
    if (!strcmp(con->name, name))
    {
#ifndef EMBEDDED_LIBRARY
      if (q->type == Q_DIRTY_CLOSE)
      {
	if (con->mysql.net.vio)
	{
	  vio_delete(con->mysql.net.vio);
	  con->mysql.net.vio = 0;
	}
      }
#endif
      mysql_close(&con->mysql);
      DBUG_RETURN(0);
    }
  }
  die("connection '%s' not found in connection pool", name);
  DBUG_RETURN(1);				/* Never reached */
}


/*
   This one now is a hack - we may want to improve in in the
   future to handle quotes. For now we assume that anything that is not
   a comma, a space or ) belongs to the argument. space is a chopper, comma or
   ) are delimiters/terminators
*/

char* safe_get_param(char* str, char** arg, const char* msg)
{
  DBUG_ENTER("safe_get_param");
  while (*str && my_isspace(charset_info,*str))
    str++;
  *arg = str;
  for (; *str && *str != ',' && *str != ')' ; str++)
  {
    if (my_isspace(charset_info,*str))
      *str = 0;
  }
  if (!*str)
    die(msg);

  *str++ = 0;
  DBUG_RETURN(str);
}

#ifndef EMBEDDED_LIBRARY
void init_manager()
{
  if (!(manager=mysql_manager_init(0)))
    die("Failed in mysql_manager_init()");
  if (!mysql_manager_connect(manager,manager_host,manager_user,
			     manager_pass,manager_port))
    die("Could not connect to MySQL manager: %s(%d)",manager->last_error,
	manager->last_errno);

}
#endif

int safe_connect(MYSQL* con, const char* host, const char* user,
		 const char* pass,
		 const char* db, int port, const char* sock)
{
  int con_error = 1;
  int i;
  for (i = 0; i < MAX_CON_TRIES; ++i)
  {
    if (mysql_real_connect(con, host,user, pass, db, port, sock,
			   CLIENT_MULTI_STATEMENTS))
    {
      con_error = 0;
      break;
    }
    sleep(CON_RETRY_SLEEP);
  }
  return con_error;
}


int do_connect(struct st_query* q)
{
  char* con_name, *con_user,*con_pass, *con_host, *con_port_str,
    *con_db, *con_sock;
  char* p=q->first_argument;
  char buff[FN_REFLEN];
  int con_port;
  int free_con_sock = 0;

  DBUG_ENTER("do_connect");
  DBUG_PRINT("enter",("connect: %s",p));

  if (*p != '(')
    die("Syntax error in connect - expected '(' found '%c'", *p);
  p++;
  p = safe_get_param(p, &con_name, "missing connection name");
  p = safe_get_param(p, &con_host, "missing connection host");
  p = safe_get_param(p, &con_user, "missing connection user");
  p = safe_get_param(p, &con_pass, "missing connection password");
  p = safe_get_param(p, &con_db, "missing connection db");
  if (!*p || *p == ';')				/* Default port and sock */
  {
    con_port=port;
    con_sock=(char*) unix_sock;
  }
  else
  {
    VAR* var_port, *var_sock;
    p = safe_get_param(p, &con_port_str, "missing connection port");
    if (*con_port_str == '$')
    {
      if (!(var_port = var_get(con_port_str, 0, 0, 0)))
	die("Unknown variable '%s'", con_port_str+1);
      con_port = var_port->int_val;
    }
    else
      con_port=atoi(con_port_str);
    p = safe_get_param(p, &con_sock, "missing connection socket");
    if (*con_sock == '$')
    {
      if (!(var_sock = var_get(con_sock, 0, 0, 0)))
	die("Unknown variable '%s'", con_sock+1);
      if (!(con_sock = (char*)my_malloc(var_sock->str_val_len+1, MYF(0))))
	die("Out of memory");
      free_con_sock = 1;
      memcpy(con_sock, var_sock->str_val, var_sock->str_val_len);
      con_sock[var_sock->str_val_len] = 0;
    }
  }

  if (next_con == cons_end)
    die("Connection limit exhausted - increase MAX_CONS in mysqltest.c");

  if (!mysql_init(&next_con->mysql))
    die("Failed on mysql_init()");
  if (opt_compress)
    mysql_options(&next_con->mysql,MYSQL_OPT_COMPRESS,NullS);
  mysql_options(&next_con->mysql, MYSQL_OPT_LOCAL_INFILE, 0);
  mysql_options(&next_con->mysql, MYSQL_SET_CHARSET_NAME, charset_name);

#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
    mysql_ssl_set(&next_con->mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
#endif
  if (con_sock && !free_con_sock && *con_sock && *con_sock != FN_LIBCHAR)
    con_sock=fn_format(buff, con_sock, TMPDIR, "",0);
  if (!con_db[0])
    con_db=db;
  /* Special database to allow one to connect without a database name */
  if (con_db && !strcmp(con_db,"*NO-ONE*"))
    con_db=0;
  if ((safe_connect(&next_con->mysql, con_host,
		    con_user, con_pass,
		    con_db, con_port, con_sock ? con_sock: 0)))
    die("Could not open connection '%s': %s", con_name,
	mysql_error(&next_con->mysql));

  if (!(next_con->name = my_strdup(con_name, MYF(MY_WME))))
    die(NullS);
  cur_con = next_con++;
  if (free_con_sock)
    my_free(con_sock, MYF(MY_WME));
  DBUG_RETURN(0);
}


int do_done(struct st_query* q)
{
  q->type = Q_END_BLOCK;
  if (cur_block == block_stack)
    die("Stray '}' - end of block before beginning");
  if (*block_ok--)
  {
    parser.current_line = *--cur_block;
  }
  else
  {
    ++parser.current_line;
    --cur_block;
  }
  return 0;
}

int do_while(struct st_query* q)
{
  char* p=q->first_argument;
  const char* expr_start, *expr_end;
  VAR v;
  if (cur_block == block_stack_end)
    die("Nesting too deeply");
  if (!*block_ok)
  {
    ++false_block_depth;
    *++block_ok = 0;
    *cur_block++ = parser.current_line++;
    return 0;
  }

  expr_start = strchr(p, '(');
  if (!expr_start)
    die("missing '(' in while");
  expr_end = strrchr(expr_start, ')');
  if (!expr_end)
    die("missing ')' in while");
  var_init(&v,0,0,0,0);
  eval_expr(&v, ++expr_start, &expr_end);
  *cur_block++ = parser.current_line++;
  if (!v.int_val)
  {
    *++block_ok = 0;
    false_block_depth++;
  }
  else
    *++block_ok = 1;
  var_free(&v);
  return 0;
}


/*
  Read characters from line buffer or file. This is needed to allow
  my_ungetc() to buffer MAX_DELIMITER characters for a file

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


my_bool end_of_query(int c)
{
  uint i;
  char tmp[MAX_DELIMITER];

  if (c != *delimiter)
    return 0;

  for (i= 1; i < delimiter_length &&
	 (c= my_getc(*cur_file)) == *(delimiter + i);
       i++)
    tmp[i]= c;

  if (i == delimiter_length)
    return 1;					/* Found delimiter */

  /* didn't find delimiter, push back things that we read */
  my_ungetc(c);
  while (i > 1)
    my_ungetc(tmp[--i]);
  return 0;
}


int read_line(char* buf, int size)
{
  int c;
  char* p= buf, *buf_end= buf + size - 1;
  int no_save= 0;
  enum {R_NORMAL, R_Q1, R_ESC_Q_Q1, R_ESC_Q_Q2,
	R_ESC_SLASH_Q1, R_ESC_SLASH_Q2,
	R_Q2, R_COMMENT, R_LINE_START} state= R_LINE_START;
  DBUG_ENTER("read_line");

  start_lineno= *lineno;
  for (; p < buf_end ;)
  {
    no_save= 0;
    c= my_getc(*cur_file);
    if (feof(*cur_file))
    {
  found_eof:
      if ((*cur_file) != stdin)
	my_fclose(*cur_file, MYF(0));
      cur_file--;
      lineno--;
      if (cur_file == file_stack)
	DBUG_RETURN(1);
      continue;
    }

    switch(state) {
    case R_NORMAL:
      /*  Only accept '{' in the beginning of a line */
      if (end_of_query(c))
      {
	*p= 0;
	DBUG_RETURN(0);
      }
      else if (c == '\'')
	state = R_Q1;
      else if (c == '"')
	state = R_Q2;
      else if (c == '\n')
      {
	state = R_LINE_START;
	(*lineno)++;
      }
      break;
    case R_COMMENT:
      if (c == '\n')
      {
	*p= 0;
	(*lineno)++;
	DBUG_RETURN(0);
      }
      break;
    case R_LINE_START:
      if (c == '#' || c == '-')
      {
	state = R_COMMENT;
      }
      else if (my_isspace(charset_info, c))
      {
	if (c == '\n')
	  start_lineno= ++*lineno;		/* Query hasn't started yet */
	no_save= 1;
      }
      else if (c == '}')
      {
	*buf++= '}';
	*buf= 0;
	DBUG_RETURN(0);
      }
      else if (end_of_query(c) || c == '{')
      {
	*p= 0;
	DBUG_RETURN(0);
      }
      else if (c == '\'')
	state= R_Q1;
      else if (c == '"')
	state= R_Q2;
      else
	state= R_NORMAL;
      break;

    case R_Q1:
      if (c == '\'')
	state= R_ESC_Q_Q1;
      else if (c == '\\')
	state= R_ESC_SLASH_Q1;
      break;
    case R_ESC_Q_Q1:
      if (end_of_query(c))
      {
	*p= 0;
	DBUG_RETURN(0);
      }
      if (c != '\'')
	state= R_NORMAL;
      else
	state= R_Q1;
      break;
    case R_ESC_SLASH_Q1:
      state= R_Q1;
      break;

    case R_Q2:
      if (c == '"')
	state= R_ESC_Q_Q2;
      else if (c == '\\')
	state= R_ESC_SLASH_Q2;
      break;
    case R_ESC_Q_Q2:
      if (end_of_query(c))
      {
	*p= 0;
	DBUG_RETURN(0);
      }
      if (c != '"')
	state= R_NORMAL;
      else
	state= R_Q2;
      break;
    case R_ESC_SLASH_Q2:
      state= R_Q2;
      break;
    }

    if (!no_save)
    {
      /* Could be a multibyte character */
      /* This code is based on the code in "sql_load.cc" */
#ifdef USE_MB
      int charlen = my_mbcharlen(charset_info, c);
      /* Not 100% correct that buf_end tell us anything if this is */
      /* a multibyte char or not but we give up if it doesn't fit */
      if ((charlen > 1) && (p + charlen) <= buf_end)
      {
	int i;
	char* mb_start = p;

	*p++ = c;

	for (i= 1; i < charlen; i++)
	{
	  if (feof(*cur_file))
	    goto found_eof;	/* FIXME: could we just break here?! */
	  c= my_getc(*cur_file);
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
  *p= 0;					/* Always end with \0 */
  DBUG_RETURN(feof(*cur_file));
}


static char read_query_buf[MAX_QUERY];

int read_query(struct st_query** q_ptr)
{
  char *p = read_query_buf, * p1 ;
  int expected_errno;
  struct st_query* q;
  DBUG_ENTER("read_query");

  if (parser.current_line < parser.read_lines)
  {
    get_dynamic(&q_lines, (gptr) q_ptr, parser.current_line) ;
    DBUG_RETURN(0);
  }
  if (!(*q_ptr= q= (struct st_query*) my_malloc(sizeof(*q), MYF(MY_WME))) ||
      insert_dynamic(&q_lines, (gptr) &q))
    die(NullS);

  q->record_file[0]= 0;
  q->require_file= 0;
  q->first_word_len= 0;

  q->type = Q_UNKNOWN;
  q->query_buf= q->query= 0;
  if (read_line(read_query_buf, sizeof(read_query_buf)))
  {
    DBUG_PRINT("warning",("too long query"));
    DBUG_RETURN(1);
  }
   DBUG_PRINT("info", ("query: %s", read_query_buf));
  if (*p == '#')
  {
    q->type = Q_COMMENT;
    /* This goto is to avoid losing the "expected error" info. */
    goto end;
  }
  memcpy((gptr) q->expected_errno, (gptr) global_expected_errno,
	 sizeof(global_expected_errno));
  q->expected_errors= global_expected_errors;
  q->abort_on_error= global_expected_errors == 0;
  bzero((gptr) global_expected_errno, sizeof(global_expected_errno));
  global_expected_errors=0;
  if (p[0] == '-' && p[1] == '-')
  {
    q->type= Q_COMMENT_WITH_COMMAND;
    p+= 2;					/* To calculate first word */
  }
  else
  {
    if (*p == '!')
    {
      q->abort_on_error= 0;
      p++;
      if (*p == '$')
      {
	expected_errno= 0;
	p++;
	for (; my_isdigit(charset_info, *p); p++)
	  expected_errno = expected_errno * 10 + *p - '0';
	q->expected_errno[0] = expected_errno;
	q->expected_errno[1] = 0;
	q->expected_errors=1;
      }
    }

    while (*p && my_isspace(charset_info, *p))
      p++ ;
    if (*p == '@')
    {
      p++;
      p1 = q->record_file;
      while (!my_isspace(charset_info, *p) &&
	     p1 < q->record_file + sizeof(q->record_file) - 1)
	*p1++ = *p++;
      *p1 = 0;
    }
  }

end:
  while (*p && my_isspace(charset_info, *p))
    p++;
  if (!(q->query_buf= q->query= my_strdup(p, MYF(MY_WME))))
    die(NullS);

  /* Calculate first word and first argument */
  for (p= q->query; *p && !my_isspace(charset_info, *p) ; p++) ;
  q->first_word_len= (uint) (p - q->query);
  while (*p && my_isspace(charset_info, *p))
    p++;
  q->first_argument= p;
  q->end= strend(q->query);
  parser.read_lines++;
  DBUG_RETURN(0);
}


static struct my_option my_long_options[] =
{
  {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"database", 'D', "Database to use.", (gptr*) &db, (gptr*) &db, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"basedir", 'b', "Basedir for tests.", (gptr*) &opt_basedir,
   (gptr*) &opt_basedir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"big-test", 'B', "Define BIG_TEST to 1.", (gptr*) &opt_big_test,
   (gptr*) &opt_big_test, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"compress", 'C', "Use the compressed server/client protocol.",
   (gptr*) &opt_compress, (gptr*) &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0,
   0, 0, 0},
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"host", 'h', "Connect to host.", (gptr*) &host, (gptr*) &host, 0,
   GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"manager-user", OPT_MANAGER_USER, "Undocumented: Used for debugging.",
   (gptr*) &manager_user, (gptr*) &manager_user, 0, GET_STR, REQUIRED_ARG, 0,
   0, 0, 0, 0, 0},
  {"manager-host", OPT_MANAGER_HOST, "Undocumented: Used for debugging.",
   (gptr*) &manager_host, (gptr*) &manager_host, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"manager-password", OPT_MANAGER_PASSWD, "Undocumented: Used for debugging.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"manager-port", OPT_MANAGER_PORT, "Undocumented: Used for debugging.",
   (gptr*) &manager_port, (gptr*) &manager_port, 0, GET_INT, REQUIRED_ARG,
   MYSQL_MANAGER_PORT, 0, 0, 0, 0, 0},
  {"manager-wait-timeout", OPT_MANAGER_WAIT_TIMEOUT,
   "Undocumented: Used for debugging.", (gptr*) &manager_wait_timeout,
   (gptr*) &manager_wait_timeout, 0, GET_INT, REQUIRED_ARG, 3, 0, 0, 0, 0, 0},
  {"password", 'p', "Password to use when connecting to server.",
   0, 0, 0, GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
  {"port", 'P', "Port number to use for connection.", (gptr*) &port,
   (gptr*) &port, 0, GET_INT, REQUIRED_ARG, MYSQL_PORT, 0, 0, 0, 0, 0},
  {"quiet", 's', "Suppress all normal output.", (gptr*) &silent,
   (gptr*) &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"record", 'r', "Record output of test_file into result file.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"result-file", 'R', "Read/Store result from/in this file.",
   (gptr*) &result_file, (gptr*) &result_file, 0, GET_STR, REQUIRED_ARG,
   0, 0, 0, 0, 0, 0},
  {"server-arg", 'A', "Send enbedded server this as a paramenter.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"server-file", 'F', "Read embedded server arguments from file.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Suppress all normal output. Synonym for --quiet.",
   (gptr*) &silent, (gptr*) &silent, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"skip-safemalloc", OPT_SKIP_SAFEMALLOC,
   "Don't use the memory allocation checking.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"sleep", 'T', "Sleep always this many seconds on sleep commands.",
   (gptr*) &opt_sleep, (gptr*) &opt_sleep, 0, GET_INT, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
  {"socket", 'S', "Socket file to use for connection.",
   (gptr*) &unix_sock, (gptr*) &unix_sock, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
   0, 0, 0},
#include "sslopt-longopts.h"
  {"test-file", 'x', "Read test from/in this file (default stdin).",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"tmpdir", 't', "Temporary directory where sockets are put.",
   0, 0, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"user", 'u', "User for login.", (gptr*) &user, (gptr*) &user, 0, GET_STR,
   REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"verbose", 'v', "Write more.", (gptr*) &verbose, (gptr*) &verbose, 0,
   GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Output version information and exit.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void print_version(void)
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
  case (int)OPT_MANAGER_PASSWD:
    my_free(manager_pass,MYF(MY_ALLOW_ZERO_PTR));
    manager_pass=my_strdup(argument, MYF(MY_FAE));
    while (*argument) *argument++= 'x';		/* Destroy argument */
    break;
  case 'x':
    {
      char buff[FN_REFLEN];
      if (!test_if_hard_path(argument))
      {
	strxmov(buff, opt_basedir, argument, NullS);
	argument= buff;
      }
      fn_format(buff, argument, "", "", 4);
      if (!(*++cur_file = my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(MY_WME))))
	die("Could not open %s: errno = %d", argument, errno);
      break;
    }
  case 'p':
    if (argument)
    {
      my_free(pass, MYF(MY_ALLOW_ZERO_PTR));
      pass= my_strdup(argument, MYF(MY_FAE));
      while (*argument) *argument++= 'x';		/* Destroy argument */
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
    embedded_server_args[embedded_server_arg_count++]=
      my_strdup(argument, MYF(MY_FAE));
    if (embedded_server_arg_count == MAX_SERVER_ARGS ||
	!embedded_server_args[embedded_server_arg_count-1])
    {
      die("Can't use server argument");
    }
    break;
  case 'F':
    if (read_server_arguments(argument))
      die(NullS);
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
    exit(1);
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
    db= *argv;
  if (tty_password)
    pass=get_tty_password(NullS);

  return 0;
}

char* safe_str_append(char* buf, const char* str, int size)
{
  int i,c ;
  for (i = 0; (c = *str++) &&  i < size - 1; i++)
    *buf++ = c;
  *buf = 0;
  return buf;
}

void str_to_file(const char* fname, char* str, int size)
{
  int fd;
  char buff[FN_REFLEN];
  if (!test_if_hard_path(fname))
  {
    strxmov(buff, opt_basedir, fname, NullS);
    fname=buff;
  }
  fn_format(buff,fname,"","",4);

  if ((fd = my_open(buff, O_WRONLY | O_CREAT | O_TRUNC,
		    MYF(MY_WME | MY_FFNF))) < 0)
    die("Could not open %s: errno = %d", buff, errno);
  if (my_write(fd, (byte*)str, size, MYF(MY_WME|MY_FNABP)))
    die("write failed");
  my_close(fd, MYF(0));
}

void reject_dump(const char* record_file, char* buf, int size)
{
  char reject_file[FN_REFLEN];
  str_to_file(fn_format(reject_file, record_file,"",".reject",2), buf, size);
}


/* Append the string to ds, with optional replace */

static void replace_dynstr_append_mem(DYNAMIC_STRING *ds, const char *val,
				      int len)
{
  if (glob_replace)
  {
    len=(int) replace_strings(glob_replace, &out_buff, &out_length, val);
    if (len == -1)
      die("Out of memory in replace\n");
    val=out_buff;
  }
  dynstr_append_mem(ds, val, len);
}


/*
  Append all results to the dynamic string separated with '\t'
  Values may be converted with 'replace_column'
*/

static void append_result(DYNAMIC_STRING *ds, MYSQL_RES *res)
{
  MYSQL_ROW row;
  uint num_fields= mysql_num_fields(res);
  MYSQL_FIELD *fields= !display_result_vertically ? 0 : mysql_fetch_fields(res);
  unsigned long *lengths;
  while ((row = mysql_fetch_row(res)))
  {
    uint i;
    lengths = mysql_fetch_lengths(res);
    for (i = 0; i < num_fields; i++)
    {
      const char *val= row[i];
      ulonglong len= lengths[i];

      if (i < max_replace_column && replace_column[i])
      {
	val= replace_column[i];
	len= strlen(val);
      }
      if (!val)
      {
	val= "NULL";
	len= 4;
      }
      if (!display_result_vertically)
      {
	if (i)
	  dynstr_append_mem(ds, "\t", 1);
	replace_dynstr_append_mem(ds, val, len);
      }
      else
      {
	dynstr_append(ds, fields[i].name);
	dynstr_append_mem(ds, "\t", 1);
	replace_dynstr_append_mem(ds, val, len);
	dynstr_append_mem(ds, "\n", 1);
      }
    }
    if (!display_result_vertically)
      dynstr_append_mem(ds, "\n", 1);
  }
  free_replace_column();
}


/*
* flags control the phased/stages of query execution to be performed
* if QUERY_SEND bit is on, the query will be sent. If QUERY_REAP is on
* the result will be read - for regular query, both bits must be on
*/

int run_query(MYSQL* mysql, struct st_query* q, int flags)
{
  MYSQL_RES* res= 0;
  uint i;
  int error= 0, err= 0, counter= 0;
  DYNAMIC_STRING *ds;
  DYNAMIC_STRING ds_tmp;
  DYNAMIC_STRING eval_query;
  char* query;
  int query_len, got_error_on_send= 0;
  DBUG_ENTER("run_query");
  DBUG_PRINT("enter",("flags: %d", flags));
  
  if (q->type != Q_EVAL)
  {
    query = q->query;
    query_len = strlen(query);
  }
  else
  {
    init_dynamic_string(&eval_query, "", 16384, 65536);
    do_eval(&eval_query, q->query);
    query = eval_query.str;
    query_len = eval_query.length;
  }
  DBUG_PRINT("enter", ("query: '%-.60s'", query));

  if (q->record_file[0])
  {
    init_dynamic_string(&ds_tmp, "", 16384, 65536);
    ds = &ds_tmp;
  }
  else
    ds= &ds_res;

  if (flags & QUERY_SEND)
  {
    got_error_on_send= mysql_send_query(mysql, query, query_len);
    if (got_error_on_send && !q->expected_errno[0])
      die("At line %u: unable to send query '%s' (mysql_errno=%d , errno=%d)",
	  start_lineno, query, mysql_errno(mysql), errno);
  }

  do
  {
    if ((flags & QUERY_SEND) && !disable_query_log && !counter)
    {
      replace_dynstr_append_mem(ds,query, query_len);
      dynstr_append_mem(ds, delimiter, delimiter_length);
      dynstr_append_mem(ds, "\n", 1);
    }
    if (!(flags & QUERY_REAP))
      DBUG_RETURN(0);

    if (got_error_on_send ||
	(!counter && (*mysql->methods->read_query_result)(mysql)) ||
	 (!(last_result= res= mysql_store_result(mysql)) &&
	  mysql_field_count(mysql)))
    {
      if (q->require_file)
      {
	abort_not_supported_test();
      }
      if (q->abort_on_error)
	die("At line %u: query '%s' failed: %d: %s", start_lineno, query,
	    mysql_errno(mysql), mysql_error(mysql));
      else
      {
	for (i=0 ; (uint) i < q->expected_errors ; i++)
	{
	  if ((q->expected_errno[i] == mysql_errno(mysql)))
	  {
	    if (i == 0 && q->expected_errors == 1)
	    {
	      /* Only log error if there is one possible error */
	      dynstr_append_mem(ds,"ERROR ",6);
	      replace_dynstr_append_mem(ds, mysql_sqlstate(mysql),
					strlen(mysql_sqlstate(mysql)));
	      dynstr_append_mem(ds, ": ", 2);
	      replace_dynstr_append_mem(ds,mysql_error(mysql),
					strlen(mysql_error(mysql)));
	      dynstr_append_mem(ds,"\n",1);
	    }
	    /* Don't log error if we may not get an error */
	    else if (q->expected_errno[0] != 0)
	      dynstr_append(ds,"Got one of the listed errors\n");
	    goto end;				/* Ok */
	  }
	}
	DBUG_PRINT("info",("i: %d  expected_errors: %d", i,
			   q->expected_errors));
	dynstr_append_mem(ds, "ERROR ",6);
	replace_dynstr_append_mem(ds, mysql_sqlstate(mysql),
				  strlen(mysql_sqlstate(mysql)));
	dynstr_append_mem(ds,": ",2);
	replace_dynstr_append_mem(ds, mysql_error(mysql),
				  strlen(mysql_error(mysql)));
	dynstr_append_mem(ds,"\n",1);
	if (i)
	{
	  verbose_msg("query '%s' failed with wrong errno %d instead of %d...",
		      q->query, mysql_errno(mysql), q->expected_errno[0]);
	  error= 1;
	  goto end;
	}
	verbose_msg("query '%s' failed: %d: %s", q->query, mysql_errno(mysql),
		    mysql_error(mysql));
	/*
	  if we do not abort on error, failure to run the query does
	  not fail the whole test case
	*/
	goto end;
      }
      /*{
	verbose_msg("failed in mysql_store_result for query '%s' (%d)", query,
	mysql_errno(mysql));
	error = 1;
	goto end;
	}*/
    }

    if (q->expected_errno[0])
    {
      error = 1;
      verbose_msg("query '%s' succeeded - should have failed with errno %d...",
		  q->query, q->expected_errno[0]);
      goto end;
    }

    if (!disable_result_log)
    {
      if (res)
      {
	MYSQL_FIELD *field, *field_end;
	uint num_fields= mysql_num_fields(res);

	if (display_metadata)
	{
	  dynstr_append(ds,"Catalog\tDatabase\tTable\tTable_alias\tColumn\tColumn_alias\tName\tType\tLength\tMax length\tIs_null\tFlags\tDecimals\tCharsetnr\n");
	  for (field= mysql_fetch_fields(res), field_end= field+num_fields ;
	       field < field_end ;
	       field++)
	  {
	    char buff[22];
	    dynstr_append_mem(ds, field->catalog, field->catalog_length);
	    dynstr_append_mem(ds, "\t", 1);
	    dynstr_append_mem(ds, field->db, field->db_length);
	    dynstr_append_mem(ds, "\t", 1);
	    dynstr_append_mem(ds, field->org_table, field->org_table_length);
	    dynstr_append_mem(ds, "\t", 1);
	    dynstr_append_mem(ds, field->table, field->table_length);
	    dynstr_append_mem(ds, "\t", 1);
	    dynstr_append_mem(ds, field->org_name, field->org_name_length);
	    dynstr_append_mem(ds, "\t", 1);
	    dynstr_append_mem(ds, field->name, field->name_length);
	    dynstr_append_mem(ds, "\t", 1);
	    int10_to_str((int) field->type, buff, 10);
	    dynstr_append(ds, buff);
	    dynstr_append_mem(ds, "\t", 1);
	    int10_to_str((int) field->length, buff, 10);
	    dynstr_append(ds, buff);
	    dynstr_append_mem(ds, "\t", 1);
	    int10_to_str((int) field->max_length, buff, 10);
	    dynstr_append(ds, buff);
	    dynstr_append_mem(ds, "\t", 1);
	    dynstr_append_mem(ds, (char*) (IS_NOT_NULL(field->flags) ?
					   "N" : "Y"), 1);
	    dynstr_append_mem(ds, "\t", 1);

	    int10_to_str((int) field->flags, buff, 10);
	    dynstr_append(ds, buff);
	    dynstr_append_mem(ds, "\t", 1);
	    int10_to_str((int) field->decimals, buff, 10);
	    dynstr_append(ds, buff);
	    dynstr_append_mem(ds, "\t", 1);
	    int10_to_str((int) field->charsetnr, buff, 10);
	    dynstr_append(ds, buff);
	    dynstr_append_mem(ds, "\n", 1);
	  }
	}
	if (!display_result_vertically)
	{
	  field= mysql_fetch_fields(res);
	  for (i = 0; i < num_fields; i++)
	  {
	    if (i)
	      dynstr_append_mem(ds, "\t", 1);
	    replace_dynstr_append_mem(ds, field[i].name,
				      strlen(field[i].name));
	  }
	  dynstr_append_mem(ds, "\n", 1);
	}
	append_result(ds, res);
      }

      /* Add all warnings to the result */
      if (!disable_warnings && mysql_warning_count(mysql))
      {
	MYSQL_RES *warn_res=0;
	uint count= mysql_warning_count(mysql);
	if (!mysql_real_query(mysql, "SHOW WARNINGS", 13))
	{
	  warn_res= mysql_store_result(mysql);
	}
	if (!warn_res)
	  verbose_msg("Warning count is %u but didn't get any warnings\n",
		      count);
	else
	{
	  dynstr_append_mem(ds, "Warnings:\n", 10);
	  append_result(ds, warn_res);
	  mysql_free_result(warn_res);
	}
      }
      if (!disable_info)
      {
	char buf[40];
	sprintf(buf,"affected rows: %lu\n",(ulong) mysql_affected_rows(mysql));
	dynstr_append(ds, buf);
	if (mysql_info(mysql))
	{
	  dynstr_append(ds, "info: ");
	  dynstr_append(ds, mysql_info(mysql));
	  dynstr_append_mem(ds, "\n", 1);
	}
      }
    }

    if (record)
    {
      if (!q->record_file[0] && !result_file)
	die("At line %u: Missing result file", start_lineno);
      if (!result_file)
	str_to_file(q->record_file, ds->str, ds->length);
    }
    else if (q->record_file[0])
    {
      error = check_result(ds, q->record_file, q->require_file);
    }
    if (res)
      mysql_free_result(res);
    last_result= 0;
    counter++;
  } while (!(err= mysql_next_result(mysql)));
  if (err >= 1)
    mysql_error(mysql);

end:
  free_replace();
  last_result=0;
  if (ds == &ds_tmp)
    dynstr_free(&ds_tmp);
  if (q->type == Q_EVAL)
    dynstr_free(&eval_query);
  DBUG_RETURN(error);
}


void get_query_type(struct st_query* q)
{
  char save;
  uint type;
  DBUG_ENTER("get_query_type");

  if (*q->query == '}')
  {
    q->type = Q_END_BLOCK;
    DBUG_VOID_RETURN;
  }
  if (q->type != Q_COMMENT_WITH_COMMAND)
    q->type = Q_QUERY;

  save=q->query[q->first_word_len];
  q->query[q->first_word_len]=0;
  type=find_type(q->query, &command_typelib, 1+2);
  q->query[q->first_word_len]=save;
  if (type > 0)
    q->type=(enum enum_commands) type;		/* Found command */
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

static VAR *var_init(VAR *v, const char *name, int name_len, const char *val,
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
						 + name_len, MYF(MY_WME))))
    die("Out of memory");

  tmp_var->name = (name) ? (char*) tmp_var + sizeof(*tmp_var) : 0;
  tmp_var->alloced = (v == 0);

  if (!(tmp_var->str_val = my_malloc(val_alloc_len+1, MYF(MY_WME))))
    die("Out of memory");

  memcpy(tmp_var->name, name, name_len);
  if (val)
  {
    memcpy(tmp_var->str_val, val, val_len);
    tmp_var->str_val[val_len]=0;
  }
  tmp_var->name_len = name_len;
  tmp_var->str_val_len = val_len;
  tmp_var->alloced_len = val_alloc_len;
  tmp_var->int_val = (val) ? atoi(val) : 0;
  tmp_var->int_dirty = 0;
  return tmp_var;
}

static void var_free(void *v)
{
  my_free(((VAR*) v)->str_val, MYF(MY_WME));
  if (((VAR*)v)->alloced)
   my_free((char*) v, MYF(MY_WME));
}


static VAR* var_from_env(const char *name, const char *def_val)
{
  const char *tmp;
  VAR *v;
  if (!(tmp = getenv(name)))
    tmp = def_val;

  v = var_init(0, name, 0, tmp, 0);
  my_hash_insert(&var_hash, (byte*)v);
  return v;
}


static void init_var_hash(MYSQL *mysql)
{
  VAR *v;
  DBUG_ENTER("init_var_hash");
  if (hash_init(&var_hash, charset_info, 
                1024, 0, 0, get_var_key, var_free, MYF(0)))
    die("Variable hash initialization failed");
  if (opt_big_test)
    my_hash_insert(&var_hash, (byte*) var_init(0,"BIG_TEST", 0, "1",0));
  v= var_init(0,"MAX_TABLES", 0, (sizeof(ulong) == 4) ? "31" : "62",0);
  my_hash_insert(&var_hash, (byte*) v);
  v= var_init(0,"SERVER_VERSION", 0, mysql_get_server_info(mysql), 0);
  my_hash_insert(&var_hash, (byte*) v);
  
  DBUG_VOID_RETURN;
}


int main(int argc, char **argv)
{
  int error = 0;
  struct st_query *q;
  my_bool require_file=0, q_send_flag=0;
  char save_file[FN_REFLEN];
  MY_INIT(argv[0]);
  {
  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);

  save_file[0]=0;
  TMPDIR[0]=0;
  memset(cons, 0, sizeof(cons));
  cons_end = cons + MAX_CONS;
  next_con = cons + 1;
  cur_con = cons;

  memset(file_stack, 0, sizeof(file_stack));
  memset(&master_pos, 0, sizeof(master_pos));
  file_stack_end = file_stack + MAX_INCLUDE_DEPTH;
  cur_file = file_stack;
  lineno   = lineno_stack;
  my_init_dynamic_array(&q_lines, sizeof(struct st_query*), INIT_Q_LINES,
		     INIT_Q_LINES);
  memset(block_stack, 0, sizeof(block_stack));
  block_stack_end = block_stack + BLOCK_STACK_DEPTH;
  memset(block_ok_stack, 0, sizeof(block_stack));
  cur_block = block_stack;
  block_ok = block_ok_stack;
  *block_ok = 1;
  init_dynamic_string(&ds_res, "", 0, 65536);
  parse_args(argc, argv);
  if (mysql_server_init(embedded_server_arg_count,
			embedded_server_args,
			(char**) embedded_server_groups))
    die("Can't initialize MySQL server");
  if (cur_file == file_stack)
    *++cur_file = stdin;
  *lineno=1;
#ifndef EMBEDDED_LIBRARY
  if (manager_host)
    init_manager();
#endif
  if (!( mysql_init(&cur_con->mysql)))
    die("Failed in mysql_init()");
  if (opt_compress)
    mysql_options(&cur_con->mysql,MYSQL_OPT_COMPRESS,NullS);
  mysql_options(&cur_con->mysql, MYSQL_OPT_LOCAL_INFILE, 0);
  mysql_options(&cur_con->mysql, MYSQL_SET_CHARSET_NAME, charset_name);

#ifdef HAVE_OPENSSL
  if (opt_use_ssl)
    mysql_ssl_set(&cur_con->mysql, opt_ssl_key, opt_ssl_cert, opt_ssl_ca,
		  opt_ssl_capath, opt_ssl_cipher);
#endif

  if (!(cur_con->name = my_strdup("default", MYF(MY_WME))))
    die("Out of memory");

  if (safe_connect(&cur_con->mysql, host, user, pass, db, port, unix_sock))
    die("Failed in mysql_real_connect(): %s", mysql_error(&cur_con->mysql));

  init_var_hash(&cur_con->mysql);

  while (!read_query(&q))
  {
    int current_line_inc = 1, processed = 0;
    if (q->type == Q_UNKNOWN || q->type == Q_COMMENT_WITH_COMMAND)
      get_query_type(q);
    if (*block_ok)
    {
      processed = 1;
      switch (q->type) {
      case Q_CONNECT: do_connect(q); break;
      case Q_CONNECTION: select_connection(q->first_argument); break;
      case Q_DISCONNECT:
      case Q_DIRTY_CLOSE:
	close_connection(q); break;
      case Q_RPL_PROBE: do_rpl_probe(q); break;
      case Q_ENABLE_RPL_PARSE:	 do_enable_rpl_parse(q); break;
      case Q_DISABLE_RPL_PARSE:  do_disable_rpl_parse(q); break;
      case Q_ENABLE_QUERY_LOG:	 disable_query_log=0; break;
      case Q_DISABLE_QUERY_LOG:  disable_query_log=1; break;
      case Q_ENABLE_RESULT_LOG:  disable_result_log=0; break;
      case Q_DISABLE_RESULT_LOG: disable_result_log=1; break;
      case Q_ENABLE_WARNINGS:    disable_warnings=0; break;
      case Q_DISABLE_WARNINGS:   disable_warnings=1; break;
      case Q_ENABLE_INFO:        disable_info=0; break;
      case Q_DISABLE_INFO:       disable_info=1; break;
      case Q_ENABLE_METADATA:    display_metadata=1; break;
      case Q_DISABLE_METADATA:   display_metadata=0; break;
      case Q_SOURCE: do_source(q); break;
      case Q_SLEEP: do_sleep(q, 0); break;
      case Q_REAL_SLEEP: do_sleep(q, 1); break;
      case Q_WAIT_FOR_SLAVE_TO_STOP: do_wait_for_slave_to_stop(q); break;
      case Q_REQUIRE_MANAGER: do_require_manager(q); break;
#ifndef EMBEDDED_LIBRARY
      case Q_SERVER_START: do_server_start(q); break;
      case Q_SERVER_STOP: do_server_stop(q); break;
#endif
      case Q_INC: do_inc(q); break;
      case Q_DEC: do_dec(q); break;
      case Q_ECHO: do_echo(q); break;
      case Q_SYSTEM: do_system(q); break;
      case Q_DELIMITER:
	strmake(delimiter, q->first_argument, sizeof(delimiter) - 1);
	delimiter_length= strlen(delimiter);
	break;
      case Q_DISPLAY_VERTICAL_RESULTS: display_result_vertically= TRUE; break;
      case Q_DISPLAY_HORIZONTAL_RESULTS: 
	display_result_vertically= FALSE; break;
      case Q_LET: do_let(q); break;
      case Q_EVAL_RESULT: eval_result = 1; break;
      case Q_EVAL:
	if (q->query == q->query_buf)
        {
	  q->query= q->first_argument;
          q->first_word_len= 0;
        }
	/* fall through */
      case Q_QUERY_VERTICAL:
      case Q_QUERY_HORIZONTAL:
      {
	my_bool old_display_result_vertically= display_result_vertically;
	if (!q->query[q->first_word_len])
	{
	  /* This happens when we use 'query_..' on it's own line */
	  q_send_flag=1;
          DBUG_PRINT("info",
                     ("query: '%s' first_word_len: %d  send_flag=1",
                      q->query, q->first_word_len));
	  break;
	}
	/* fix up query pointer if this is * first iteration for this line */
	if (q->query == q->query_buf)
	  q->query += q->first_word_len + 1;
	display_result_vertically= (q->type==Q_QUERY_VERTICAL);
	error|= run_query(&cur_con->mysql, q, QUERY_REAP|QUERY_SEND);
	display_result_vertically= old_display_result_vertically;
	break;
      }
      case Q_QUERY:
      case Q_REAP:
      {
	/*
	  We read the result always regardless of the mode for both full
	  query and read-result only (reap)
	*/
	int flags = QUERY_REAP;
	if (q->type != Q_REAP) /* for a full query, enable the send stage */
	  flags |= QUERY_SEND;
	if (q_send_flag)
	{
	  flags= QUERY_SEND;
	  q_send_flag=0;
	}
	if (save_file[0])
	{
	  strmov(q->record_file,save_file);
	  q->require_file=require_file;
	  save_file[0]=0;
	}
	error |= run_query(&cur_con->mysql, q, flags);
	break;
      }
      case Q_SEND:
	if (!q->query[q->first_word_len])
	{
	  /* This happens when we use 'send' on it's own line */
	  q_send_flag=1;
	  break;
	}
	/* fix up query pointer if this is * first iteration for this line */
	if (q->query == q->query_buf)
	  q->query += q->first_word_len;
	/*
	  run_query() can execute a query partially, depending on the flags
	  QUERY_SEND flag without QUERY_REAP tells it to just send the
	  query and read the result some time later when reap instruction
	  is given on this connection.
	 */
	error |= run_query(&cur_con->mysql, q, QUERY_SEND);
	break;
      case Q_RESULT:
	get_file_name(save_file,q);
	require_file=0;
	break;
      case Q_ERROR:
	global_expected_errors=get_ints(global_expected_errno,q);
	break;
      case Q_REQUIRE:
	get_file_name(save_file,q);
	require_file=1;
	break;
      case Q_REPLACE:
	get_replace(q);
	break;
      case Q_REPLACE_COLUMN:
	get_replace_column(q);
	break;
      case Q_SAVE_MASTER_POS: do_save_master_pos(); break;
      case Q_SYNC_WITH_MASTER: do_sync_with_master(q); break;
      case Q_SYNC_SLAVE_WITH_MASTER:
      {
	do_save_master_pos();
	if (*q->first_argument)
	  select_connection(q->first_argument);
	else
	{
	  char buf[] = "slave";
	  select_connection(buf);
	}
	do_sync_with_master2("");
	break;
      }
      case Q_COMMENT:				/* Ignore row */
      case Q_COMMENT_WITH_COMMAND:
	break;
      case Q_PING:
	(void) mysql_ping(&cur_con->mysql);
	break;
      case Q_CHARACTER_SET: 
	set_charset(q);
	break;
      case Q_EXEC: 
	(void) do_exec(q);
	break;
      default: processed = 0; break;
      }
    }

    if (!processed)
    {
      current_line_inc = 0;
      switch (q->type) {
      case Q_WHILE: do_while(q); break;
      case Q_END_BLOCK: do_done(q); break;
      default: current_line_inc = 1; break;
      }
    }

    parser.current_line += current_line_inc;
  }

  if (result_file && ds_res.length)
  {
    if (!record)
      error |= check_result(&ds_res, result_file, q->require_file);
    else
      str_to_file(result_file, ds_res.str, ds_res.length);
  }
  dynstr_free(&ds_res);

  if (!silent)
  {
    if (error)
      printf("not ok\n");
    else
      printf("ok\n");
  }

  free_used_memory();
  exit(error ? 1 : 0);
  return error ? 1 : 0;				/* Keep compiler happy */
  }
}


/*
  Read arguments for embedded server and put them into
  embedded_server_args_count and embedded_server_args[]
*/


static int read_server_arguments(const char *name)
{
  char argument[1024],buff[FN_REFLEN], *str=0;
  FILE *file;

  if (!test_if_hard_path(name))
  {
    strxmov(buff, opt_basedir, name, NullS);
    name=buff;
  }
  fn_format(buff,name,"","",4);

  if (!embedded_server_arg_count)
  {
    embedded_server_arg_count=1;
    embedded_server_args[0]= (char*) "";		/* Progname */
  }
  if (!(file=my_fopen(buff, O_RDONLY | FILE_BINARY, MYF(MY_WME))))
    return 1;
  while (embedded_server_arg_count < MAX_SERVER_ARGS &&
	 (str=fgets(argument,sizeof(argument), file)))
  {
    *(strend(str)-1)=0;				/* Remove end newline */
    if (!(embedded_server_args[embedded_server_arg_count]=
	  (char*) my_strdup(str,MYF(MY_WME))))
    {
      my_fclose(file,MYF(0));
      return 1;
    }
    embedded_server_arg_count++;
  }
  my_fclose(file,MYF(0));
  if (str)
  {
    fprintf(stderr,"Too many arguments in option file: %s\n",name);
    return 1;
  }
  return 0;
}

/****************************************************************************
* Handle replacement of strings
****************************************************************************/

#define PC_MALLOC		256	/* Bytes for pointers */
#define PS_MALLOC		512	/* Bytes for data */

#define SPACE_CHAR	256
#define START_OF_LINE	257
#define END_OF_LINE	258
#define LAST_CHAR_CODE	259

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

#ifndef WORD_BIT
#define WORD_BIT (8*sizeof(uint))
#endif


static int insert_pointer_name(reg1 POINTER_ARRAY *pa,my_string name)
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


	/* Code for replace rutines */

#define SET_MALLOC_HUNC 64

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


static int init_sets(REP_SETS *sets,uint states);
static REP_SET *make_new_set(REP_SETS *sets);
static void make_sets_invisible(REP_SETS *sets);
static void free_last_set(REP_SETS *sets);
static void free_sets(REP_SETS *sets);
static void set_bit(REP_SET *set, uint bit);
static void clear_bit(REP_SET *set, uint bit);
static void or_bits(REP_SET *to,REP_SET *from);
static void copy_bits(REP_SET *to,REP_SET *from);
static int cmp_bits(REP_SET *set1,REP_SET *set2);
static int get_next_bit(REP_SET *set,uint lastpos);
static int find_set(REP_SETS *sets,REP_SET *find);
static int find_found(FOUND_SET *found_set,uint table_offset,
			  int found_offset);
static uint start_at_word(my_string pos);
static uint end_of_word(my_string pos);
static uint replace_len(my_string pos);

static uint found_sets=0;


	/* Init a replace structure for further calls */

REPLACE *init_replace(my_string *from, my_string *to,uint count,
		      my_string word_end_chars)
{
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
      set_bit(start_states,states+1);
      if (!from[i][2])
      {
	start_states->table_offset=i;
	start_states->found_offset=1;
      }
    }
    else if (from[i][0] == '\\' && from[i][1] == '$')
    {
      set_bit(start_states,states);
      set_bit(word_states,states);
      if (!from[i][2] && start_states->table_offset == (uint) ~0)
      {
	start_states->table_offset=i;
	start_states->found_offset=0;
      }
    }
    else
    {
      set_bit(word_states,states);
      if (from[i][0] == '\\' && (from[i][1] == 'b' && from[i][2]))
	set_bit(start_states,states+1);
      else
	set_bit(start_states,states);
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
	      set_bit(new_set,i+1);		/* To next set */
	    else
	      set_bit(new_set,i);
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
	      clear_bit(new_set,i);
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


static int init_sets(REP_SETS *sets,uint states)
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

static void make_sets_invisible(REP_SETS *sets)
{
  sets->invisible=sets->count;
  sets->set+=sets->count;
  sets->count=0;
}

static REP_SET *make_new_set(REP_SETS *sets)
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

static void free_last_set(REP_SETS *sets)
{
  sets->count--;
  sets->extra++;
  return;
}

static void free_sets(REP_SETS *sets)
{
  my_free((gptr)sets->set_buffer,MYF(0));
  my_free((gptr)sets->bit_buffer,MYF(0));
  return;
}

static void set_bit(REP_SET *set, uint bit)
{
  set->bits[bit / WORD_BIT] |= 1 << (bit % WORD_BIT);
  return;
}

static void clear_bit(REP_SET *set, uint bit)
{
  set->bits[bit / WORD_BIT] &= ~ (1 << (bit % WORD_BIT));
  return;
}


static void or_bits(REP_SET *to,REP_SET *from)
{
  reg1 uint i;
  for (i=0 ; i < to->size_of_bits ; i++)
    to->bits[i]|=from->bits[i];
  return;
}

static void copy_bits(REP_SET *to,REP_SET *from)
{
  memcpy((byte*) to->bits,(byte*) from->bits,
	 (size_t) (sizeof(uint) * to->size_of_bits));
}

static int cmp_bits(REP_SET *set1,REP_SET *set2)
{
  return bcmp((byte*) set1->bits,(byte*) set2->bits,
	      sizeof(uint) * set1->size_of_bits);
}


	/* Get next set bit from set. */

static int get_next_bit(REP_SET *set,uint lastpos)
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
	   free given set, else put in given set in sets and return it's
	   position */

static int find_set(REP_SETS *sets,REP_SET *find)
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
	   Pos returned is -offset-2 in found_set_structure because it's is
	   saved in set->next and set->next[] >= 0 points to next set and
	   set->next[] == -1 is reserved for end without replaces.
	   */

static int find_found(FOUND_SET *found_set,uint table_offset, int found_offset)
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

static uint start_at_word(my_string pos)
{
  return (((!bcmp(pos,"\\b",2) && pos[2]) || !bcmp(pos,"\\^",2)) ? 1 : 0);
}

static uint end_of_word(my_string pos)
{
  my_string end=strend(pos);
  return ((end > pos+2 && !bcmp(end-2,"\\b",2)) ||
	  (end >= pos+2 && !bcmp(end-2,"\\$",2))) ?
	    1 : 0;
}


static uint replace_len(my_string str)
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


	/* Replace strings;  Return length of result string */

uint replace_strings(REPLACE *rep, my_string *start,uint *max_length,
		     const char *from)
{
  reg1 REPLACE *rep_pos;
  reg2 REPLACE_STRING *rep_str;
  my_string to,end,pos,new_str;

  end=(to= *start) + *max_length-1;
  rep_pos=rep+1;
  for (;;)
  {
    while (!rep_pos->found)
    {
      rep_pos= rep_pos->next[(uchar) *from];
      if (to == end)
      {
	(*max_length)+=8192;
	if (!(new_str=my_realloc(*start,*max_length,MYF(MY_WME))))
	  return (uint) -1;
	to=new_str+(to - *start);
	end=(*start=new_str)+ *max_length-1;
      }
      *to++= *from++;
    }
    if (!(rep_str = ((REPLACE_STRING*) rep_pos))->replace_string)
      return (uint) (to - *start)-1;
    to-=rep_str->to_offset;
    for (pos=rep_str->replace_string; *pos ; pos++)
    {
      if (to == end)
      {
	(*max_length)*=2;
	if (!(new_str=my_realloc(*start,*max_length,MYF(MY_WME))))
	  return (uint) -1;
	to=new_str+(to - *start);
	end=(*start=new_str)+ *max_length-1;
      }
      *to++= *pos;
    }
    if (!*(from-=rep_str->from_offset) && rep_pos->found != 2)
      return (uint) (to - *start);
    rep_pos=rep;
  }
}

static int initialize_replace_buffer(void)
{
  out_length=8192;
  if (!(out_buff=my_malloc(out_length,MYF(MY_WME))))
    return(1);
  return 0;
}

static void free_replace_buffer(void)
{
  my_free(out_buff,MYF(MY_WME));
}


/****************************************************************************
 Replace results for a column
*****************************************************************************/

static void free_replace_column()
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

/*
  Get arguments for replace_columns. The syntax is:
  replace-column column_number to_string [column_number to_string ...]
  Where each argument may be quoted with ' or "
  A argument may also be a variable, in which case the value of the
  variable is replaced.
*/

static void get_replace_column(struct st_query *q)
{
  char *from=q->first_argument;
  char *buff,*start;
  DBUG_ENTER("get_replace_columns");

  free_replace_column();
  if (!*from)
    die("Missing argument in %s\n", q->query);

  /* Allocate a buffer for results */
  start=buff=my_malloc(strlen(from)+1,MYF(MY_WME | MY_FAE));
  while (*from)
  {
    char *to;
    uint column_number;

    to= get_string(&buff, &from, q);
    if (!(column_number= atoi(to)) || column_number > MAX_COLUMNS)
      die("Wrong column number to replace_columns in %s\n", q->query);
    if (!*from)
      die("Wrong number of arguments to replace in %s\n", q->query);
    to= get_string(&buff, &from, q);
    my_free(replace_column[column_number-1], MY_ALLOW_ZERO_PTR);
    replace_column[column_number-1]= my_strdup(to, MYF(MY_WME | MY_FAE));
    set_if_bigger(max_replace_column, column_number);
  }
  my_free(start, MYF(0));
}

#ifdef __NETWARE__

/*
  Substitute environment variables with text.

  SYNOPSIS
    subst_env_var()
    arg			String that should be substitute

  DESCRIPTION
    This function takes a string as an input and replaces the
    environment variables, that starts with '$' character, with it value.

  NOTES
    Return string must be freed with my_free()

  RETURN
    String with environment variables replaced.
*/

static char *subst_env_var(const char *str)
{
  char *result;
  char *pos;

  result= pos= my_malloc(MAX_QUERY, MYF(MY_FAE));
  while (*str)
  {
    /*
      need this only when we want to provide the functionality of
      escaping through \ 'backslash'
      if ((result == pos && *str=='$') ||
          (result != pos && *str=='$' && str[-1] !='\\'))
    */
    if (*str == '$')
    {
      char env_var[256], *env_pos= env_var, *subst;

      /* Search for end of environment variable */
      for (str++;
           *str && !isspace(*str) && *str != '\\' && *str != '/' &&
             *str != '$';
           str++)
        *env_pos++= *str;
      *env_pos= 0;

      if (!(subst= getenv(env_var)))
      {
        my_free(result, MYF(0));
        die("MYSQLTEST.NLM: Environment variable %s is not defined\n",
            env_var);
      }

      /* get the string to be substitued for env_var  */
      pos= strmov(pos, subst);
      /* Process delimiter in *str again */
    }
    else
      *pos++= *str++;
  }
  *pos= 0;
  return result;
}


/*
  popen replacement for Netware

  SYNPOSIS
    my_popen()
    name		Command to execute (with possible env variables)
    mode		Mode for popen.

  NOTES
    Environment variable expansion does not take place for popen function
    on NetWare, so we use this function to wrap around popen to do this.

    For the moment we ignore 'mode' and always use 'r0'

  RETURN
    # >= 0	File handle
    -1		Error
*/

#undef popen                                    /* Remove wrapper */

FILE *my_popen(const char *cmd, const char *mode __attribute__((unused)))
{
  char *subst_cmd;
  FILE *res_file;

  subst_cmd= subst_env_var(cmd);
  res_file= popen(subst_cmd, "r0");
  my_free(subst_cmd, MYF(0));
  return res_file;
}

#endif /* __NETWARE__ */
