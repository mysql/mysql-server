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

/* mysqltest test tool
 * See man page for more information.
 *
 * Written by:
 *   Sasha Pachev <sasha@mysql.com>
 *   Matt Wagner  <matt@mysql.com>
 *   Monty
 **/

/**********************************************************************
  TODO:

- Print also the queries that returns a result to the log file;  This makes
  it much easier to find out what's wrong.

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

#define MTEST_VERSION "1.9"

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <mysql_version.h>
#include <m_ctype.h>
#include <my_config.h>
#include <my_dir.h>
#include <hash.h>
#include <mysqld_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <violite.h>

#define MAX_QUERY  65536
#define PAD_SIZE	128
#define MAX_CONS   1024
#define MAX_INCLUDE_DEPTH 16
#define LAZY_GUESS_BUF_SIZE 8192
#define INIT_Q_LINES	  1024
#define MIN_VAR_ALLOC	  32
#define BLOCK_STACK_DEPTH  32
#define MAX_EXPECTED_ERRORS 10
#define QUERY_SEND  1
#define QUERY_REAP  2
#define CON_RETRY_SLEEP 1 /* how long to sleep before trying to connect again*/
#define MAX_CON_TRIES   2 /* sometimes in a test the client starts before
			   * the server - to solve the problem, we try again
			   * after some sleep if connection fails the first
			   * time */

static int record = 0, verbose = 0, silent = 0, opt_sleep=0;
static char *db = 0, *pass=0;
const char* user = 0, *host = 0, *unix_sock = 0;
static int port = 0;
static uint start_lineno, *lineno;

static char **default_argv;
static const char *load_default_groups[]= { "mysqltest","client",0 };

static FILE* file_stack[MAX_INCLUDE_DEPTH];
static FILE** cur_file;
static FILE** file_stack_end;
static uint lineno_stack[MAX_INCLUDE_DEPTH];
static char TMPDIR[FN_REFLEN];

static int block_stack[BLOCK_STACK_DEPTH];
static int *cur_block, *block_stack_end;
static uint global_expected_errno[MAX_EXPECTED_ERRORS];

DYNAMIC_ARRAY q_lines;

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

PARSER parser;
MASTER_POS master_pos;
int block_ok = 1; /* set to 0 if the current block should not be executed */
int false_block_depth = 0;
const char* result_file = 0; /* if set, all results are concated and
				compared against this file*/

typedef struct
{
  char* name;
  int name_len;
  char* str_val;
  int str_val_len;
  int int_val;
  int alloced_len;
  int int_dirty; /* do not update string if int is updated until first read */
} VAR;

VAR var_reg[10];
/*Perl/shell-like variable registers */
HASH var_hash;

struct connection cons[MAX_CONS];
struct connection* cur_con, *next_con, *cons_end;

  /* Add new commands before Q_UNKNOWN !*/

enum enum_commands {
Q_CONNECTION=1,     Q_QUERY, 
Q_CONNECT,          Q_SLEEP, 
Q_INC,              Q_DEC,
Q_SOURCE,           Q_DISCONNECT,
Q_LET,              Q_ECHO, 
Q_WHILE,            Q_END_BLOCK,
Q_SYSTEM,           Q_RESULT, 
Q_REQUIRE,          Q_SAVE_MASTER_POS,
Q_SYNC_WITH_MASTER, Q_ERROR, 
Q_SEND,             Q_REAP, 
Q_DIRTY_CLOSE,      Q_REPLACE,
Q_PING,             Q_EVAL,
Q_RPL_PROBE,        Q_ENABLE_RPL_PARSE,
Q_DISABLE_RPL_PARSE,
Q_UNKNOWN,                             /* Unknown command.   */
Q_COMMENT,                             /* Comments, ignored. */
Q_COMMENT_WITH_COMMAND
};

/* this should really be called command */
struct st_query
{
  char *query, *query_buf,*first_argument;
  int first_word_len;
  my_bool abort_on_error, require_file;
  uint expected_errno[MAX_EXPECTED_ERRORS];
  char record_file[FN_REFLEN];
  enum enum_commands type;
};

const char *command_names[] = {
  "connection",       "query",
  "connect",          "sleep",
  "inc",              "dec",
  "source",           "disconnect",
  "let",              "echo",
  "while",            "end",
  "system",           "result",
  "require",          "save_master_pos",
  "sync_with_master", "error",
  "send",             "reap", 
  "dirty_close",      "replace_result",
  "ping",             "eval",
  "rpl_probe",        "enable_rpl_parse",
  "disable_rpl_parse",
  0
};

TYPELIB command_typelib= {array_elements(command_names),"",
			  command_names};

DYNAMIC_STRING ds_res;
static void die(const char* fmt, ...);
static void init_var_hash();
static byte* get_var_key(const byte* rec, uint* len,
			 my_bool __attribute__((unused)) t);
static VAR* var_init(const char* name, int name_len, const char* val,
		     int val_len);

static void var_free(void* v);

int dyn_string_cmp(DYNAMIC_STRING* ds, const char* fname);
void reject_dump(const char* record_file, char* buf, int size);

int close_connection(struct st_query* q);
VAR* var_get(const char* var_name, const char** var_name_end, int raw);
int eval_expr(VAR* v, const char* p, const char** p_end);

/* Definitions for replace */

typedef struct st_pointer_array {		/* when using array-strings */
  TYPELIB typelib;				/* Pointer to strings */
  byte	*str;					/* Strings is here */
  int7	*flag;					/* Flag about each var. */
  uint  array_allocs,max_count,length,max_length;
} POINTER_ARRAY;

struct st_replace;
struct st_replace *init_replace(my_string *from, my_string *to, uint count,
				my_string word_end_chars);
uint replace_strings(struct st_replace *rep, my_string *start,
		     uint *max_length, my_string from);
static int insert_pointer_name(reg1 POINTER_ARRAY *pa,my_string name);
void free_pointer_array(POINTER_ARRAY *pa);
static int initialize_replace_buffer(void);
static void free_replace_buffer(void);
static void do_eval(DYNAMIC_STRING* query_eval, const char* query);

struct st_replace *glob_replace;
static char *out_buff;
static uint out_length;

static void do_eval(DYNAMIC_STRING* query_eval, const char* query)
{
  const char* p;
  register char c;
  register int escaped = 0;
  VAR* v;
  
  for(p = query; (c = *p); ++p)
    {
      switch(c)
	{
	case '$':
	  if(escaped)
	    {
	      escaped = 0;
   	      dynstr_append_mem(query_eval, p, 1);
	    }
	  else
	    {
	      if(!(v = var_get(p, &p, 0)))
		die("Bad variable in eval");
	      dynstr_append_mem(query_eval, v->str_val, v->str_val_len);
	    }
	  break;
	case '\\':
	  if(escaped)
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
}

static void close_cons()
{
  DBUG_ENTER("close_cons");
  for (--next_con; next_con >= cons; --next_con)
  {
    mysql_close(&next_con->mysql);
    my_free(next_con->name, MYF(MY_ALLOW_ZERO_PTR));
  }
  DBUG_VOID_RETURN;
}

static void close_files()
{
  do
  {
    if (*cur_file != stdin)
      my_fclose(*cur_file,MYF(0));
  } while (cur_file-- != file_stack);
}

static void free_used_memory()
{
  uint i;
  DBUG_ENTER("free_used_memory");
  close_cons();
  close_files();
  hash_free(&var_hash);
  
  for (i=0 ; i < q_lines.elements ; i++)
  {
    struct st_query **q= dynamic_element(&q_lines, i, struct st_query**);
    my_free((gptr) (*q)->query_buf,MYF(MY_ALLOW_ZERO_PTR));
    my_free((gptr) (*q),MYF(0));
  }
  for(i=0; i < 10; i++)
    {
      if(var_reg[i].alloced_len)
	my_free(var_reg[i].str_val, MYF(MY_WME));
    }
  delete_dynamic(&q_lines);
  dynstr_free(&ds_res);
  my_free(pass,MYF(MY_ALLOW_ZERO_PTR));
  free_defaults(default_argv);
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
  if (!verbose) return;

  va_start(args, fmt);

  fprintf(stderr, "%s: At line %u: ", my_progname, start_lineno);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}


void init_parser()
{
  parser.current_line = parser.read_lines = 0;
  memset(&var_reg,0, sizeof(var_reg));
}

int hex_val(int c)
{
  if (isdigit(c))
    return c - '0';
  else if ((c = tolower(c)) >= 'a' && c <= 'f')
    return c - 'a' + 10;
  else
    return -1;
}

int dyn_string_cmp(DYNAMIC_STRING* ds, const char* fname)
{
  MY_STAT stat_info;
  char *tmp;
  int res;
  int fd;
  DBUG_ENTER("dyn_string_cmp");

  if (!my_stat(fname, &stat_info, MYF(MY_WME)))
    die(NullS);
  if (stat_info.st_size != ds->length)
    DBUG_RETURN(2);
  if (!(tmp = (char*) my_malloc(ds->length, MYF(MY_WME))))
    die(NullS);
  if ((fd = my_open(fname, O_RDONLY, MYF(MY_WME))) < 0)
    die(NullS);
  if (my_read(fd, (byte*)tmp, stat_info.st_size, MYF(MY_WME|MY_NABP)))
    die(NullS);
  res = (memcmp(tmp, ds->str, stat_info.st_size)) ?  1 : 0;
  my_free((gptr) tmp, MYF(0));
  my_close(fd, MYF(MY_WME));

  DBUG_RETURN(res);
}

static int check_result(DYNAMIC_STRING* ds, const char* fname,
			my_bool require_option)
{
  int error = 0;
  int res=dyn_string_cmp(ds, fname);

  if (res && require_option)
    abort_not_supported_test();
  switch (res)
  {
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

VAR* var_get(const char* var_name, const char** var_name_end, int raw)
{
  int digit;
  VAR* v;
  if (*var_name++ != '$')
  {
    --var_name;
    goto err;
  }
  digit = *var_name - '0';
  if (!(digit < 10 && digit >= 0))
  {
    const char* save_var_name = var_name, *end;
    end = (var_name_end) ? *var_name_end : 0;
    while(isalnum(*var_name) || *var_name == '_')
      {
	if(end && var_name == end)
	  break;
        ++var_name;
      }
    if(var_name == save_var_name)
      die("Empty variable");
    
    if(!(v = (VAR*)hash_search(&var_hash, save_var_name,
			       var_name - save_var_name)))
      {
	if (end)
	  *(char*)end = 0;
        die("Variable '%s' used uninitialized", save_var_name);
      }
    --var_name;
  }
  else 
   v = var_reg + digit;
  
  if (!raw && v->int_dirty)
  {
    sprintf(v->str_val, "%d", v->int_val);
    v->int_dirty = 0;
    v->str_val_len = strlen(v->str_val);
  }
  if(var_name_end)
    *var_name_end = var_name  ;
  return v;
err:
  if (var_name_end)
    *var_name_end = 0;
  die("Unsupported variable name: %s", var_name);
  return 0;
}

static VAR* var_obtain(char* name, int len)
{
  VAR* v;
  if((v = (VAR*)hash_search(&var_hash, name, len)))
    return v;
  v = var_init(name, len, "", 0);
  hash_insert(&var_hash, (byte*)v);
  return v;
}

int var_set(char* var_name, char* var_name_end, char* var_val,
	    char* var_val_end)
{
  int digit;
  int val_len;
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
  if (v->alloced_len < (val_len = (int)(var_val_end - var_val)+1))
    {
      v->alloced_len = (val_len < MIN_VAR_ALLOC) ? MIN_VAR_ALLOC : val_len;
     if (!(v->str_val =
	  v->str_val ? my_realloc(v->str_val, v->alloced_len,  MYF(MY_WME)) :
	   my_malloc(v->alloced_len, MYF(MY_WME))))
	 die("Out of memory");
    }
  val_len--;
  memcpy(v->str_val, var_val, val_len);
  v->str_val_len = val_len;
  v->str_val[val_len] = 0;
  v->int_val = atoi(v->str_val);
  v->int_dirty=0;
  return 0;
}

int open_file(const char* name)
{
  if (*cur_file && cur_file == file_stack_end)
    die("Source directives are nesting too deep");
  if (!(*(cur_file+1) = my_fopen(name, O_RDONLY, MYF(MY_WME))))
    die(NullS);
  cur_file++;
  *++lineno=1;

  return 0;
}

int do_source(struct st_query* q)
{
  char* p=q->first_argument, *name;
  if (!*p)
    die("Missing file name in source\n");
  name = p;
  while (*p && !isspace(*p))
    p++;
  *p = 0;

  return open_file(name);
}


int eval_expr(VAR* v, const char* p, const char** p_end)
{
  VAR* vp;
  if (*p == '$')
    {
      if ((vp = var_get(p,p_end,0)))
	{
	  memcpy(v, vp, sizeof(*v));
	  return 0;
	}
    }
  else
    {
      v->str_val = (char*)p;
      v->str_val_len = (p_end && *p_end) ? (int) (*p_end - p) : (int) strlen(p);
      v->int_val=atoi(p);
      v->int_dirty=0;
      return 0;
    }

  if (p_end)
    *p_end = 0;
  die("Invalid expr: %s", p);
  return 1;
}

int do_inc(struct st_query* q)
{
  char* p=q->first_argument;
  VAR* v;
  v = var_get(p, 0, 1);
  v->int_val++;
  v->int_dirty = 1;
  return 0;
}

int do_dec(struct st_query* q)
{
  char* p=q->first_argument;
  VAR* v;
  v = var_get(p, 0, 1);
  v->int_val--;
  v->int_dirty = 1;
  return 0;
}

int do_system(struct st_query* q)
{
  char* p=q->first_argument;
  VAR v;
  eval_expr(&v, p, 0); /* NULL terminated */
  if (v.str_val_len)
    {
      char expr_buf[512];
      if ((uint)v.str_val_len > sizeof(expr_buf) - 1)
	v.str_val_len = sizeof(expr_buf) - 1;
      memcpy(expr_buf, v.str_val, v.str_val_len);
      expr_buf[v.str_val_len] = 0;
      DBUG_PRINT("info", ("running system command '%s'", expr_buf));
      if (system(expr_buf) && q->abort_on_error)
	die("system command '%s' failed", expr_buf);
    }
  return 0;
}

int do_echo(struct st_query* q)
{
  char* p=q->first_argument;
  VAR v;
  eval_expr(&v, p, 0); /* NULL terminated */
  if (v.str_val_len)
  {
    fflush(stdout);
    write(1, v.str_val, v.str_val_len);
  }
  write(1, "\n", 1);
  return 0;
}

int do_sync_with_master(struct st_query* q)
{
  MYSQL_RES* res;
  MYSQL_ROW row;
  MYSQL* mysql = &cur_con->mysql;
  char query_buf[FN_REFLEN+128];
  int offset = 0;
  char* p = q->first_argument;
  int rpl_parse;

  rpl_parse = mysql_rpl_parse_enabled(mysql);
  mysql_disable_rpl_parse(mysql);
  
  if(*p)
    offset = atoi(p);
  
  sprintf(query_buf, "select master_pos_wait('%s', %ld)", master_pos.file,
	  master_pos.pos + offset);
  if(mysql_query(mysql, query_buf))
    die("At line %u: failed in %s: %d: %s", start_lineno, query_buf,
	mysql_errno(mysql), mysql_error(mysql));

  if(!(res = mysql_store_result(mysql)))
    die("line %u: mysql_store_result() retuned NULL", start_lineno);
  if(!(row = mysql_fetch_row(res)))
    die("line %u: empty result in %s", start_lineno, query_buf);
  if(!row[0])
    die("Error on slave while syncing with master");
  mysql_free_result(res);

  if(rpl_parse)
    mysql_enable_rpl_parse(mysql);
  
  return 0;
}

int do_save_master_pos()
{
  MYSQL_RES* res;
  MYSQL_ROW row;
  MYSQL* mysql = &cur_con->mysql;
  int rpl_parse;

  rpl_parse = mysql_rpl_parse_enabled(mysql);
  mysql_disable_rpl_parse(mysql);
  
  if(mysql_query(mysql, "show master status"))
    die("At line %u: failed in show master status: %d: %s", start_lineno,
	mysql_errno(mysql), mysql_error(mysql));

  if(!(res = mysql_store_result(mysql)))
    die("line %u: mysql_store_result() retuned NULL", start_lineno);
  if(!(row = mysql_fetch_row(res)))
    die("line %u: empty result in show master status", start_lineno);
  strncpy(master_pos.file, row[0], sizeof(master_pos.file));
  master_pos.pos = strtoul(row[1], (char**) 0, 10); 
  mysql_free_result(res);
  
  if(rpl_parse)
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
  while(*p && (*p != '=' || isspace(*p)))
    p++;
  var_name_end = p;
  if (*p == '=') p++;
  while(*p && isspace(*p))
    p++;
  var_val_start = p;
  while(*p && !isspace(*p))
    p++;
  return var_set(var_name, var_name_end, var_val_start, p);
}

int do_rpl_probe(struct st_query* __attribute__((unused)) q)
{
  if(mysql_rpl_probe(&cur_con->mysql))
    die("Failed in mysql_rpl_probe(): %s", mysql_error(&cur_con->mysql));
  return 0;
}

int do_enable_rpl_parse(struct st_query* __attribute__((unused)) q)
{
  mysql_enable_rpl_parse(&cur_con->mysql);
  return 0;
}

int do_disable_rpl_parse(struct st_query* __attribute__((unused)) q)
{
  mysql_disable_rpl_parse(&cur_con->mysql);
  return 0;
}


int do_sleep(struct st_query* q)
{
  char* p=q->first_argument;
  struct timeval t;
  int dec_mul = 1000000;
  while(*p && isspace(*p)) p++;
  if (!*p)
    die("Missing argument in sleep\n");
  t.tv_usec = 0;
  if (opt_sleep)
    t.tv_sec = opt_sleep;
  else
  {
    t.tv_sec = atoi(p);
    while(*p && *p != '.' && !isspace(*p))
      p++;
    if (*p == '.')
    {
      int c;
      char *p_end;
      p++;
      p_end = p + 6;

      for(;p <= p_end; ++p)
      {
	c = (int) (*p - '0');
	if (c < 10 && (int) c >= 0)
	{
	  t.tv_usec = t.tv_usec * 10 + c;
	  dec_mul /= 10;
	}
	else
	  break;
      }
    }
  }
  t.tv_usec *= dec_mul;
  return select(0,0,0,0, &t);
}

static void get_file_name(char *filename, struct st_query* q)
{
  char* p=q->first_argument;
  strnmov(filename, p, FN_REFLEN);
  /* Remove end space */
  while (p > filename && isspace(p[-1]))
    p--;
  p[0]=0;
}


static void get_ints(uint *to,struct st_query* q)
{
  char* p=q->first_argument;
  long val;
  DBUG_ENTER("get_ints");

  if (!*p)
    die("Missing argument in %s\n", q->query);

  for (; (p=str2int(p,10,(long) INT_MIN, (long) INT_MAX, &val)) ; p++)
  {
    *to++= (uint) val;
    if (*p != ',')
      break;
  }
  *to++=0;					/* End of data */
  DBUG_VOID_RETURN;
}

/*
  Get a string;  Return ptr to end of string
  Strings may be surrounded by " or '
*/


static void get_string(char **to_ptr, char **from_ptr,
		       struct st_query* q)
{
  reg1 char c,sep;
  char *to= *to_ptr, *from= *from_ptr;
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

  while (isspace(*from))		/* Point to next string */
    from++;

  *to++ =0;				/* End of string marker */
  *to_ptr= to;
  *from_ptr= from;
}


/*
  Get arguments for replace. The syntax is:
  replace from to [from to ...]
  Where each argument may be quoted with ' or "
*/

static void get_replace(struct st_query *q)
{
  uint i;
  char *from=q->first_argument;
  char *buff,*start;
  char word_end_chars[256],*pos;
  POINTER_ARRAY to_array,from_array;
  DBUG_ENTER("get_replace");

  bzero((char*) &to_array,sizeof(to_array));
  bzero((char*) &from_array,sizeof(from_array));
  if (!*from)
    die("Missing argument in %s\n", q->query);
  start=buff=my_malloc(strlen(from)+1,MYF(MY_WME | MY_FAE));
  while (*from)
  {
    char *to=buff;
    get_string(&buff, &from, q);
    if (!*from)
      die("Wrong number of arguments to replace in %s\n", q->query);
    insert_pointer_name(&from_array,to);
    to=buff;
    get_string(&buff, &from, q);
    insert_pointer_name(&to_array,to);
  }
  for (i=1,pos=word_end_chars ; i < 256 ; i++)
    if (isspace(i))
      *pos++=i;
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
}

void free_replace()
{
  DBUG_ENTER("free_replace");
  my_free((char*) glob_replace,MYF(0));
  glob_replace=0;
  free_replace_buffer();
  DBUG_VOID_RETURN;
}


int select_connection(struct st_query* q)
{
  char* p=q->first_argument, *name;
  struct connection *con;
  DBUG_ENTER("select_connection");
  DBUG_PRINT("enter",("name: '%s'",p));

  if (!*p)
    die("Missing connection name in connect\n");
  name = p;
  while(*p && !isspace(*p))
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
  while(*p && !isspace(*p))
    p++;
  *p = 0;

  for(con = cons; con < next_con; con++)
  {
    if (!strcmp(con->name, name))
    {
      if(q->type == Q_DIRTY_CLOSE)
	{
	  if(con->mysql.net.vio)
	    {
	      vio_delete(con->mysql.net.vio);
	      con->mysql.net.vio = 0;
	    }
	}
		
      mysql_close(&con->mysql);
      DBUG_RETURN(0);
    }
  }
  die("connection '%s' not found in connection pool", name);
  DBUG_RETURN(1);				/* Never reached */
}


/* this one now is a hack - we may want to improve in in the
   future to handle quotes. For now we assume that anything that is not
   a comma, a space or ) belongs to the argument. space is a chopper, comma or
   ) are delimiters/terminators
*/

char* safe_get_param(char* str, char** arg, const char* msg)
{
  DBUG_ENTER("safe_get_param");
  while (*str && isspace(*str)) str++;
  *arg = str;
  for (; *str && *str != ',' && *str != ')' ; str++)
  {
    if (isspace(*str)) *str = 0;
  }
  if (!*str)
    die(msg);

  *str++ = 0;
  DBUG_RETURN(str);
}


int do_connect(struct st_query* q)
{
  char* con_name, *con_user,*con_pass, *con_host, *con_port_str,
    *con_db, *con_sock;
  char* p=q->first_argument;
  char buff[FN_REFLEN];
  int con_port;
  int i, con_error;
  
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
    p = safe_get_param(p, &con_port_str, "missing connection port");
    con_port=atoi(con_port_str);
    p = safe_get_param(p, &con_sock, "missing connection socket");
  }
  if (next_con == cons_end)
    die("Connection limit exhausted - increase MAX_CONS in mysqltest.c");

  if (!mysql_init(&next_con->mysql))
    die("Failed on mysql_init()");
  if (con_sock)
    con_sock=fn_format(buff, con_sock, TMPDIR, "",0);
  if (!con_db[0])
    con_db=db;
  con_error = 1;
  for (i = 0; i < MAX_CON_TRIES; ++i)
    {
      if(mysql_real_connect(&next_con->mysql, con_host,
			    con_user, con_pass,
			    con_db, con_port, con_sock, 0))
	{
	  con_error = 0;
	  break;
	}
      sleep(CON_RETRY_SLEEP);
    }

  if(con_error)
    die("Could not open connection '%s': %s", con_name,
	mysql_error(&next_con->mysql));

  if (!(next_con->name = my_strdup(con_name, MYF(MY_WME))))
    die(NullS);
  cur_con = next_con++;

  DBUG_RETURN(0);
}

int do_done(struct st_query* q)
{
  q->type = Q_END_BLOCK;
  if (cur_block == block_stack)
    die("Stray '}' - end of block before beginning");
  if (block_ok)
    parser.current_line = *--cur_block;
  else
    {
      if (!--false_block_depth)
	block_ok = 1;
      ++parser.current_line;
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
  if (!block_ok)
    {
      ++false_block_depth;
      return 0;
    }
  expr_start = strchr(p, '(');
  if (!expr_start)
    die("missing '(' in while");
  expr_end = strrchr(expr_start, ')');
  if (!expr_end)
    die("missing ')' in while");
  eval_expr(&v, ++expr_start, &expr_end);
  *cur_block++ = parser.current_line++;
  if (!v.int_val)
    {
      block_ok = 0;
      false_block_depth = 1;
    }
  return 0;
}


int safe_copy_unescape(char* dest, char* src, int size)
{
  register char* p_dest = dest, *p_src = src;
  register int c, val;
  enum { ST_NORMAL, ST_ESCAPED, ST_HEX2} state = ST_NORMAL ;

  size--; /* just to make life easier */

  for(; p_dest - size < dest && p_src - size < src
	&& (c = *p_src) != '\n' && c; ++p_src )
    {
      switch(state)
	{
	case ST_NORMAL:
	  if (c == '\\')
	    {
	      state = ST_ESCAPED;
	    }
	  else
	    *p_dest++ = c;
	  break;
	case ST_ESCAPED:
	  if ((val = hex_val(c)) > 0)
	    {
	      *p_dest = val;
	      state = ST_HEX2;
	    }
	  else
	    {
	      state = ST_NORMAL;
	      *p_dest++ = c;
	    }
	  break;
	case ST_HEX2:
	  if ((val = hex_val(c)) > 0)
	    {
	      *p_dest = (*p_dest << 4) + val;
	      p_dest++;
	    }
	  else
	    *p_dest++ = c;

	  state = ST_NORMAL;
	  break;

	}
    }

  *p_dest = 0;
  return (p_dest - dest);
}


int read_line(char* buf, int size)
{
  int c;
  char* p = buf, *buf_end = buf + size-1;
  int no_save = 0;
  enum {R_NORMAL, R_Q1, R_ESC_Q_Q1, R_ESC_Q_Q2,
	R_ESC_SLASH_Q1, R_ESC_SLASH_Q2,
	R_Q2, R_COMMENT, R_LINE_START} state = R_LINE_START;

  start_lineno= *lineno;
  for (; p < buf_end ;)
  {
    no_save = 0;
    c = fgetc(*cur_file);
    if (feof(*cur_file))
    {
      if ((*cur_file) != stdin)
	my_fclose(*cur_file,MYF(0));

      if (cur_file == file_stack)
	return 1;
      else
      {
	cur_file--;
	lineno--;
	continue;
      }
    }

    switch(state) {
    case R_NORMAL:
      /*  Only accept '{' in the beginning of a line */
      if (c == ';')
      {
	*p = 0;
	return 0;
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
	*p=0;
	(*lineno)++;
	return 0;
      }
      break;
    case R_LINE_START:
      if (c == '#' || c == '-')
      {
	state = R_COMMENT;
      }
      else if (isspace(c))
      {
	if (c == '\n')
	  start_lineno= ++*lineno;		/* Query hasn't started yet */
	no_save = 1;
      }
      else if (c == '}')
      {
	*buf++ = '}';
	*buf = 0;
	return 0;
      }
      else if (c == ';' || c == '{')
      {
	*p = 0;
	return 0;
      }
      else
	state = R_NORMAL;
      break;

    case R_Q1:
      if (c == '\'')
	state = R_ESC_Q_Q1;
      else if (c == '\\')
	state = R_ESC_SLASH_Q1;
      break;
    case R_ESC_Q_Q1:
      if (c == ';')
      {
	*p = 0;
	return 0;
      }
      if (c != '\'')
	state = R_NORMAL;
      else
	state = R_Q1;
      break;
    case R_ESC_SLASH_Q1:
      state = R_Q1;
      break;

    case R_Q2:
      if (c == '"')
	state = R_ESC_Q_Q2;
      else if (c == '\\')
	state = R_ESC_SLASH_Q2;
      break;
    case R_ESC_Q_Q2:
      if (c == ';')
      {
	*p = 0;
	return 0;
      }
      if (c != '"')
	state = R_NORMAL;
      else
	state = R_Q2;
      break;
    case R_ESC_SLASH_Q2:
      state = R_Q2;
      break;
    }

    if (!no_save)
      *p++ = c;
  }
  *p=0;						/* Always end with \0 */
  return feof(*cur_file);
}

static char read_query_buf[MAX_QUERY];

int read_query(struct st_query** q_ptr)
{
  char *p = read_query_buf, * p1 ;
  int expected_errno;
  struct st_query* q;

  if (parser.current_line < parser.read_lines)
  {
    get_dynamic(&q_lines, (gptr) q_ptr, parser.current_line) ;
    return 0;
  }
  if (!(*q_ptr=q=(struct st_query*) my_malloc(sizeof(*q), MYF(MY_WME))) ||
      insert_dynamic(&q_lines, (gptr) &q))
    die(NullS);

  q->record_file[0] = 0;
  q->require_file=0;
  q->first_word_len = 0;
  memcpy((gptr) q->expected_errno, (gptr) global_expected_errno,
	 sizeof(global_expected_errno));
  q->abort_on_error = global_expected_errno[0] == 0;
  bzero((gptr) global_expected_errno,sizeof(global_expected_errno));
  q->type = Q_UNKNOWN;
  q->query_buf=q->query=0;
  if (read_line(read_query_buf, sizeof(read_query_buf)))
    return 1;

  if (*p == '#')
  {
    q->type = Q_COMMENT;
  }
  else if (p[0] == '-' && p[1] == '-')
  {
    q->type = Q_COMMENT_WITH_COMMAND;
    p+=2;					/* To calculate first word */
  }
  else
  {
    if (*p == '!')
    {
      q->abort_on_error = 0;
      p++;
      if (*p == '$')
      {
	expected_errno = 0;
	p++;
	for (;isdigit(*p);p++)
	  expected_errno = expected_errno * 10 + *p - '0';
	q->expected_errno[0] = expected_errno;
	q->expected_errno[1] = 0;
      }
    }

    while(*p && isspace(*p)) p++ ;
    if (*p == '@')
    {
      p++;
      p1 = q->record_file;
      while (!isspace(*p) &&
	     p1 < q->record_file + sizeof(q->record_file) - 1)
	*p1++ = *p++;
      *p1 = 0;
    }
  }
  while (*p && isspace(*p)) p++;
  if (!(q->query_buf=q->query=my_strdup(p,MYF(MY_WME))))
    die(NullS);

  /* Calculate first word and first argument */
  for (p=q->query; *p && !isspace(*p) ; p++) ;
  q->first_word_len = (uint) (p - q->query);
  while (*p && isspace(*p)) p++;
  q->first_argument=p;

  parser.read_lines++;
  return 0;
}


struct option long_options[] =
{
  {"debug",       optional_argument, 0, '#'},
  {"database",    required_argument, 0, 'D'},
  {"help",        no_argument,       0, '?'},
  {"host",        required_argument, 0, 'h'},
  {"password",    optional_argument, 0, 'p'},
  {"port",        required_argument, 0, 'P'},
  {"quiet",       no_argument,       0, 'q'},
  {"record",      no_argument,       0, 'r'},
  {"result-file", required_argument, 0, 'R'},
  {"silent",      no_argument,       0, 'q'},
  {"sleep",       required_argument, 0, 'T'},
  {"socket",      required_argument, 0, 'S'},
  {"test-file",   required_argument, 0, 'x'},
  {"tmpdir",      required_argument, 0, 't'},
  {"user",        required_argument, 0, 'u'},
  {"verbose",     no_argument,       0, 'v'},
  {"version",     no_argument,       0, 'V'},
  {0, 0, 0, 0}
};


static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,MTEST_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

void usage()
{
  print_version();
  printf("MySQL AB, by Sasha, Matt & Monty\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Runs a test against the mysql server and compares output with a results file.\n\n");
  printf("Usage: %s [OPTIONS] [database] < test_file\n", my_progname);
  printf("\n\
  -?, --help               Display this help and exit.\n");
#ifndef DBUG_OFF
  puts("\
  -#, --debug=[...]        Output debug log. Often this is 'd:t:o,filename`");
#endif
  printf("\
  -h, --host=...           Connect to host.\n\
  -u, --user=...           User for login.\n\
  -p[password], --password[=...]\n\
                           Password to use when connecting to server.\n\
  -D, --database=...       Database to use.\n\
  -P, --port=...           Port number to use for connection.\n\
  -S, --socket=...         Socket file to use for connection.\n\
  -t, --tmpdir=...	   Temporary directory where sockets are put\n\
  -T, --sleep=#		   Sleep always this many seconds on sleep commands\n\
  -r, --record             Record output of test_file into result file.\n\
  -R, --result-file=...    Read/Store result from/in this file.\n\
  -x, --test-file=...      Read test from/in this file (default stdin).\n\
  -v, --verbose            Write more.\n\
  -q, --quiet, --silent    Suppress all normal output.\n\
  -V, --version            Output version information and exit.\n\
  --no-defaults            Don't read default options from any options file.\n\n");
}

int parse_args(int argc, char **argv)
{
  int c, option_index = 0;
  my_bool tty_password=0;

  load_defaults("my",load_default_groups,&argc,&argv);
  default_argv= argv;

  while((c = getopt_long(argc, argv, "h:p::u:P:D:S:R:x:t:T:#:?rvVq",
			 long_options, &option_index)) != EOF)
    {
      switch(c)	{
      case '#':
	DBUG_PUSH(optarg ? optarg : "d:t:O,/tmp/mysqltest.trace");
	break;
      case 'v':
	verbose = 1;
	break;
      case 'r':
	record = 1;
	break;
      case 'u':
	user = optarg;
	break;
      case 'R':
	result_file = optarg;
	break;
      case 'x':
      if (!(*cur_file = my_fopen(optarg, O_RDONLY, MYF(MY_WME))))
	  die("Could not open %s: errno = %d", optarg, errno);
	break;
      case 'p':
	if (optarg)
	{
	  my_free(pass,MYF(MY_ALLOW_ZERO_PTR));
	  pass=my_strdup(optarg,MYF(MY_FAE));
	  while (*optarg) *optarg++= 'x';		/* Destroy argument */
	}
	else
	  tty_password=1;
	break;
      case 'P':
	port = atoi(optarg);
	break;
      case 'S':
	unix_sock = optarg;
	break;
      case 'D':
	db = optarg;
	break;
      case 'h':
	host = optarg;
	break;
      case 'q':
	silent = 1;
	break;
      case 't':
	strnmov(TMPDIR,optarg,sizeof(TMPDIR));
	break;
      case 'T':
	opt_sleep=atoi(optarg);
	break;
      case 'V':
	print_version();
	exit(0);
      case '?':
	usage();
	exit(1);				/* Unknown option */
      default:
	usage();
	exit(1);
      }
    }

  argc-=optind;
  argv+=optind;
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
  for(i = 0; (c = *str++) &&  i < size - 1; i++)
    *buf++ = c;
  *buf = 0;
  return buf;
}

void str_to_file(const char* fname, char* str, int size)
{
  int fd;
  if ((fd = my_open(fname, O_WRONLY | O_CREAT | O_TRUNC,
		    MYF(MY_WME | MY_FFNF))) < 0)
    die("Could not open %s: errno = %d", fname, errno);
  if (my_write(fd, (byte*)str, size, MYF(MY_WME|MY_FNABP)))
    die("write failed");
  my_close(fd, MYF(0));
}

void reject_dump(const char* record_file, char* buf, int size)
{
  char reject_file[FN_REFLEN];
  str_to_file(fn_format(reject_file, record_file,"",".reject",2), buf, size);
}

/* flags control the phased/stages of query execution to be performed 
* if QUERY_SEND bit is on, the query will be sent. If QUERY_REAP is on
* the result will be read - for regular query, both bits must be on
*/
int run_query(MYSQL* mysql, struct st_query* q, int flags)
{
  MYSQL_RES* res = 0;
  MYSQL_FIELD* fields;
  MYSQL_ROW row;
  int num_fields,i, error = 0;
  unsigned long* lengths;
  char* val;
  int len;
  DYNAMIC_STRING *ds;
  DYNAMIC_STRING ds_tmp;
  DYNAMIC_STRING eval_query;
  char* query;
  int query_len;
  DBUG_ENTER("run_query");

  if(q->type != Q_EVAL)
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
  
  if ( q->record_file[0])
  {
    init_dynamic_string(&ds_tmp, "", 16384, 65536);
    ds = &ds_tmp;
  }
  else
    ds= &ds_res;
  
  if ((flags & QUERY_SEND) && mysql_send_query(mysql, query, query_len))
    die("At line %u: unable to send query '%s'", start_lineno, query);
  if(!(flags & QUERY_REAP))
    return 0;
  
  if (mysql_read_query_result(mysql) ||
      (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql)))
  {
    if (q->require_file)
      abort_not_supported_test();
    if (q->abort_on_error)
      die("At line %u: query '%s' failed: %d: %s", start_lineno, query,
	  mysql_errno(mysql), mysql_error(mysql));
      /*die("At line %u: Failed in mysql_store_result for query '%s' (%d)",
	  start_lineno, query, mysql_errno(mysql));*/
    else
    {
      for (i=0 ; q->expected_errno[i] ; i++)
      {
	if ((q->expected_errno[i] == mysql_errno(mysql)))
	  goto end;				/* Ok */
      }
      if (i)
      {
	verbose_msg("query '%s' failed with wrong errno\
 %d instead of %d...", q->query, mysql_errno(mysql), q->expected_errno[0]);
	goto end;
      }
      verbose_msg("query '%s' failed: %d: %s", q->query, mysql_errno(mysql),
		  mysql_error(mysql));
      /* if we do not abort on error, failure to run the query does
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

  if (!res) goto end;

  fields =  mysql_fetch_fields(res);
  num_fields =	mysql_num_fields(res);
  for( i = 0; i < num_fields; i++)
  {
    if (i)
      dynstr_append_mem(ds, "\t", 1);
    dynstr_append(ds, fields[i].name);
  }

  dynstr_append_mem(ds, "\n", 1);


  while((row = mysql_fetch_row(res)))
  {
    lengths = mysql_fetch_lengths(res);
    for(i = 0; i < num_fields; i++)
    {
      val = (char*)row[i];
      len = lengths[i];

      if (!val)
      {
	val = (char*)"NULL";
	len = 4;
      }

      if (i)
	dynstr_append_mem(ds, "\t", 1);
      if (glob_replace)
      {
	len=(int) replace_strings(glob_replace, &out_buff, &out_length, val);
	if (len == -1)
	  die("Out of memory in replace\n");
	val=out_buff;
      }
      dynstr_append_mem(ds, val, len);
    }

    dynstr_append_mem(ds, "\n", 1);
  }
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
    error = check_result(ds, q->record_file, q->require_file);
  }

end:
  if (res) mysql_free_result(res);
  if (ds == &ds_tmp)
    dynstr_free(&ds_tmp);
  if(q->type == Q_EVAL)
    dynstr_free(&eval_query);
  DBUG_RETURN(error);
}


void get_query_type(struct st_query* q)
{
  char save;
  uint type;
  if (*q->query == '}')
  {
    q->type = Q_END_BLOCK;
    return;
  }
  if (q->type != Q_COMMENT_WITH_COMMAND)
    q->type = Q_QUERY;

  save=q->query[q->first_word_len];
  q->query[q->first_word_len]=0;
  type=find_type(q->query, &command_typelib, 1+2);
  q->query[q->first_word_len]=save;
  if (type > 0)
    q->type=(enum enum_commands) type;		/* Found command */
}

static byte* get_var_key(const byte* var, uint* len,
			 my_bool __attribute__((unused)) t)
{
  register char* key;
  key = ((VAR*)var)->name;
  *len = ((VAR*)var)->name_len;
  return (byte*)key;
}

static VAR* var_init(const char* name, int name_len, const char* val,
		     int val_len)
{
  int val_alloc_len;
  VAR* tmp_var;
  if(!name_len)
    name_len = strlen(name);
  if(!val_len)
    val_len = strlen(val) ;
  val_alloc_len = val_len + 16; /* room to grow */
  if(!(tmp_var = (VAR*)my_malloc(sizeof(*tmp_var) + val_alloc_len
				 + name_len, MYF(MY_WME))))
    die("Out of memory");
  tmp_var->name = (char*)tmp_var + sizeof(*tmp_var);
  tmp_var->str_val = tmp_var->name + name_len;
  memcpy(tmp_var->name, name, name_len);
  memcpy(tmp_var->str_val, val, val_len + 1);
  tmp_var->name_len = name_len;
  tmp_var->str_val_len = val_len;
  tmp_var->alloced_len = val_alloc_len;
  tmp_var->int_val = atoi(val);
  tmp_var->int_dirty = 0;
  return tmp_var;
}

static void var_free(void* v)
{
  my_free(v, MYF(MY_WME));
}


static void var_from_env(const char* name, const char* def_val)
{
  const char* tmp;
  VAR* v;
  if(!(tmp = getenv(name)))
    tmp = def_val;
    
  v = var_init(name, 0, tmp, 0); 
  hash_insert(&var_hash, (byte*)v);
}

static void init_var_hash()
{
  if(hash_init(&var_hash, 1024, 0, 0, get_var_key, var_free, MYF(0)))
    die("Variable hash initialization failed");
  var_from_env("MASTER_MYPORT", "9306");
  var_from_env("SLAVE_MYPORT", "9307");
  var_from_env("MYSQL_TEST_DIR", "");
}

int main(int argc, char** argv)
{
  int error = 0;
  struct st_query* q;
  my_bool require_file=0, q_send_flag=0;
  char save_file[FN_REFLEN];
  MY_INIT(argv[0]);

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
  init_dynamic_array(&q_lines, sizeof(struct st_query*), INIT_Q_LINES,
		     INIT_Q_LINES);
  memset(block_stack, 0, sizeof(block_stack));
  block_stack_end = block_stack + BLOCK_STACK_DEPTH;
  cur_block = block_stack;
  init_dynamic_string(&ds_res, "", 0, 65536);
  parse_args(argc, argv);
  init_var_hash();
  if (!*cur_file)
    *cur_file = stdin;
  *lineno=1;

  if (!( mysql_init(&cur_con->mysql)))
    die("Failed in mysql_init()");
  cur_con->name = my_strdup("default", MYF(MY_WME));
  if (!cur_con->name)
    die("Out of memory");

  if (!mysql_real_connect(&cur_con->mysql, host,
			 user, pass, db, port, unix_sock,
     0))
    die("Failed in mysql_real_connect(): %s", mysql_error(&cur_con->mysql));

  while (!read_query(&q))
  {
    int current_line_inc = 1, processed = 0;
    if (q->type == Q_UNKNOWN || q->type == Q_COMMENT_WITH_COMMAND)
      get_query_type(q);
    if (block_ok)
    {
      processed = 1;
      switch (q->type) {
      case Q_CONNECT: do_connect(q); break;
      case Q_CONNECTION: select_connection(q); break;
      case Q_DISCONNECT:
      case Q_DIRTY_CLOSE:	
	close_connection(q); break;
      case Q_RPL_PROBE: do_rpl_probe(q); break;
      case Q_ENABLE_RPL_PARSE: do_enable_rpl_parse(q); break;
      case Q_DISABLE_RPL_PARSE: do_disable_rpl_parse(q); break;
      case Q_SOURCE: do_source(q); break;
      case Q_SLEEP: do_sleep(q); break;
      case Q_INC: do_inc(q); break;
      case Q_DEC: do_dec(q); break;
      case Q_ECHO: do_echo(q); break;
      case Q_SYSTEM: do_system(q); break;
      case Q_LET: do_let(q); break;
      case Q_EVAL:
        if (q->query == q->query_buf)
	  q->query += q->first_word_len;
        /* fall through */
      case Q_QUERY:
      case Q_REAP:	
      {
	int flags = QUERY_REAP; /* we read the result always regardless
				* of the mode for both full query and
				* read-result only ( reap) */
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
	error |= run_query(&cur_con->mysql, q, QUERY_SEND);
	/* run query can execute a query partially, depending on the flags
	 * QUERY_SEND flag without QUERY_REAP tells it to just send the
	 * query and read the result some time later when reap instruction
	 * is given on this connection
	 */
	break;
      case Q_RESULT:
	get_file_name(save_file,q);
	require_file=0;
	break;
      case Q_ERROR:
	get_ints(global_expected_errno,q);
	break;
      case Q_REQUIRE:
	get_file_name(save_file,q);
	require_file=1;
	break;
      case Q_REPLACE:
	get_replace(q);
	break;
      case Q_SAVE_MASTER_POS: do_save_master_pos(); break;	
      case Q_SYNC_WITH_MASTER: do_sync_with_master(q); break;	
      case Q_COMMENT:				/* Ignore row */
      case Q_COMMENT_WITH_COMMAND: 
      case Q_PING:
	(void) mysql_ping(&cur_con->mysql);
	break;
      default: processed = 0; break;
      }
    }

    if (!processed)
    {
      current_line_inc = 0;
      switch(q->type)
      {
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

  if (!silent) {
    if(error)
      printf("not ok\n");
    else
      printf("ok\n");
  }

  free_used_memory();
  exit(error ? 1 : 0);
  return error ? 1 : 0;				/* Keep compiler happy */
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
  bool   found;
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
  uint  *bits;				/* Pointer to used sets */
  short	next[LAST_CHAR_CODE];		/* Pointer to next sets */
  uint	found_len;			/* Best match to date */
  int	found_offset;
  uint  table_offset;
  uint  size_of_bits;			/* For convinience */
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
		     my_string from)
{
  reg1 REPLACE *rep_pos;
  reg2 REPLACE_STRING *rep_str;
  my_string to,end,pos,new;

  end=(to= *start) + *max_length-1;
  rep_pos=rep+1;
  for(;;)
  {
    while (!rep_pos->found)
    {
      rep_pos= rep_pos->next[(uchar) *from];
      if (to == end)
      {
	(*max_length)+=8192;
	if (!(new=my_realloc(*start,*max_length,MYF(MY_WME))))
	  return (uint) -1;
	to=new+(to - *start);
	end=(*start=new)+ *max_length-1;
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
	if (!(new=my_realloc(*start,*max_length,MYF(MY_WME))))
	  return (uint) -1;
	to=new+(to - *start);
	end=(*start=new)+ *max_length-1;
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
