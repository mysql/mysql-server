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

#define MTEST_VERSION "1.6"

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql.h>
#include <mysql_version.h>
#include <m_ctype.h>
#include <my_config.h>
#include <my_dir.h>
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

typedef
  struct
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
  char* str_val;
  int str_val_len;
  int int_val;
  int alloced_len;
  int int_dirty; /* do not update string if int is updated until first read */
} VAR;

VAR var_reg[10];
/*Perl/shell-like variable registers */

struct connection cons[MAX_CONS];
struct connection* cur_con, *next_con, *cons_end;

/* this should really be called command */
struct st_query
{
  char *query, *first_argument;
  int first_word_len;
  my_bool abort_on_error, require_file;
  uint expected_errno[MAX_EXPECTED_ERRORS];
  char record_file[FN_REFLEN];
  /* Add new commands before Q_UNKNOWN */
  enum { Q_CONNECTION=1, Q_QUERY, Q_CONNECT,
	 Q_SLEEP, Q_INC, Q_DEC,Q_SOURCE,
	 Q_DISCONNECT,Q_LET, Q_ECHO, Q_WHILE, Q_END_BLOCK,
	 Q_SYSTEM, Q_RESULT, Q_REQUIRE, Q_SAVE_MASTER_POS,
	 Q_SYNC_WITH_MASTER, Q_ERROR, Q_SEND, Q_REAP, Q_DIRTY_CLOSE,
	 Q_UNKNOWN, Q_COMMENT, Q_COMMENT_WITH_COMMAND} type;
};

const char *command_names[] = {
"connection", "query","connect","sleep","inc","dec","source","disconnect",
"let","echo","while","end","system","result", "require", "save_master_pos",
 "sync_with_master", "error", "send", "reap", "dirty_close", 0
};

TYPELIB command_typelib= {array_elements(command_names),"",
			  command_names};

DYNAMIC_STRING ds_res;

int dyn_string_cmp(DYNAMIC_STRING* ds, const char* fname);
void reject_dump(const char* record_file, char* buf, int size);

int close_connection(struct st_query* q);
VAR* var_get(char* var_name, char* var_name_end, int raw);

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
  for (i=0 ; i < q_lines.elements ; i++)
  {
    struct st_query **q= dynamic_element(&q_lines, i, struct st_query**);
    my_free((gptr) (*q)->query,MYF(MY_ALLOW_ZERO_PTR));
    my_free((gptr) (*q),MYF(0));
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

VAR* var_get(char* var_name, char* var_name_end, int raw)
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
    --var_name;
    goto err;
  }
  v = var_reg + digit;
  if (!raw && v->int_dirty)
  {
    sprintf(v->str_val, "%d", v->int_val);
    v->int_dirty = 0;
  }
  return v;
err:
  if (var_name_end)
    *var_name_end = 0;
  die("Unsupported variable name: %s", var_name);
  return 0;
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
      *var_name_end = 0;
      die("Unsupported variable name: %s", var_name);
    }
  v = var_reg + digit;
  if (v->alloced_len < (val_len = (int)(var_val_end - var_val)+1))
    {
      v->alloced_len = (val_len < MIN_VAR_ALLOC) ? MIN_VAR_ALLOC : val_len;
     if (!(v->str_val =
	  v->str_val ? my_realloc(v->str_val, v->alloced_len,  MYF(MY_WME)) :
	   my_malloc(v->alloced_len, MYF(MY_WME))))
	 die("Out of memory");
    }
  memcpy(v->str_val, var_val, val_len-1);
  v->str_val_len = val_len;
  v->str_val[val_len] = 0;
  v->int_val = atoi(v->str_val);
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


int eval_expr(VAR* v, char* p, char* p_end)
{
  VAR* vp;
  if (*p == '$')
    {
      if ((vp = var_get(p,p_end,0)))
	{
	  memcpy(v, vp, sizeof(VAR));
	  return 0;
	}
    }
  else
    {
      v->str_val = p;
      v->str_val_len = p_end ? p_end - p : strlen(p);
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
  if (v.str_val_len > 1)
    {
      char expr_buf[512];
      if ((uint)v.str_val_len > sizeof(expr_buf) - 1)
	v.str_val_len = sizeof(expr_buf) - 1;
      memcpy(expr_buf, v.str_val, v.str_val_len);
      expr_buf[v.str_val_len] = 0;
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
  if (v.str_val_len > 1)
    {
      fflush(stdout);
      write(1, v.str_val, v.str_val_len - 1);
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
      
  return 0;
}

int do_save_master_pos()
{
  MYSQL_RES* res;
  MYSQL_ROW row;
  MYSQL* mysql = &cur_con->mysql;
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
      char c;
      char *p_end;
      p++;
      p_end = p + 6;

      for(;p <= p_end; ++p)
      {
	c = *p - '0';
	if (c < 10 && c >= 0)
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

  while (*p && isspace(*p)) p++;
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
  while(*str && isspace(*str)) str++;
  *arg = str;
  while(*str && *str != ',' && *str != ')')
    {
      if (isspace(*str)) *str = 0;
      str++;
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
  p = safe_get_param(p, &con_port_str, "missing connection port");
  p = safe_get_param(p, &con_sock, "missing connection scoket");
  if (next_con == cons_end)
    die("Connection limit exhausted - incread MAX_CONS in mysqltest.c");

  if (!mysql_init(&next_con->mysql))
    die("Failed on mysql_init()");
  con_sock=fn_format(buff, con_sock, TMPDIR,"",0);
  if (!mysql_real_connect(&next_con->mysql, con_host, con_user, con_pass,
			 con_db, atoi(con_port_str), con_sock, 0))
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
  char* expr_start, *expr_end;
  VAR v;
  if (cur_block == block_stack_end)
	die("Nesting too deep");
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
  eval_expr(&v, ++expr_start, --expr_end);
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
  int c, expected_errno;
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
  q->query=0;
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
      while(!isspace(c = *p) &&
	    p1 < q->record_file + sizeof(q->record_file) - 1)
	*p1++ = *p++;
      *p1 = 0;
    }
  }
  while (*p && isspace(*p)) p++;
  if (!(q->query=my_strdup(p,MYF(MY_WME))))
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
  {"debug",    optional_argument, 0, '#'},
  {"database", required_argument, 0, 'D'},
  {"help", no_argument, 0, '?'},
  {"host", required_argument, 0, 'h'},
  {"password", optional_argument, 0, 'p'},
  {"port", required_argument, 0, 'P'},
  {"quiet", no_argument, 0, 'q'},
  {"record", no_argument, 0, 'r'},
  {"result-file", required_argument, 0, 'R'},
  {"silent", no_argument, 0, 'q'},
  {"sleep",  required_argument, 0, 'T'},
  {"socket", required_argument, 0, 'S'},
  {"tmpdir", required_argument, 0, 't'},
  {"user", required_argument, 0, 'u'},
  {"verbose", no_argument, 0, 'v'},
  {"version", no_argument, 0, 'V'},
  {0, 0,0,0}
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

  while((c = getopt_long(argc, argv, "h:p::u:P:D:S:R:t:T:#:?rvVq",
			 long_options, &option_index)) != EOF)
    {
      switch(c)	{
      case '#':
	DBUG_PUSH(optarg ? optarg : "d:t:o,/tmp/mysqltest.trace");
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
        if(!host) {
          my_free(host, MYF(MY_ALLOW_ZERO_PTR));
          host=my_strdup("127.0.0.1", MYF(MY_WME));
        };
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


int run_query(MYSQL* mysql, struct st_query* q, int flags)
{
  MYSQL_RES* res = 0;
  MYSQL_FIELD* fields;
  MYSQL_ROW row;
  int num_fields,i, error = 0;
  unsigned long* lengths;
  char* val;
  int len;
  int q_error = 0 ;
  DYNAMIC_STRING *ds;
  DYNAMIC_STRING ds_tmp;
  DBUG_ENTER("run_query");

  if ( q->record_file[0])
  {
    init_dynamic_string(&ds_tmp, "", 16384, 65536);
    ds = &ds_tmp;
  }
  else
    ds= &ds_res;
  
  if((flags & QUERY_SEND) &&
    (q_error = mysql_send_query(mysql, q->query)))
    die("At line %u: unable to send query '%s'", start_lineno, q->query);
  if(!(flags & QUERY_REAP))
    return 0;
  
  if (mysql_reap_query(mysql))
  {
    if (q->require_file)
      abort_not_supported_test();
    if (q->abort_on_error)
      die("At line %u: query '%s' failed: %d: %s", start_lineno, q->query,
	  mysql_errno(mysql), mysql_error(mysql));
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
  }

  if (q->expected_errno[0])
  {
    error = 1;
    verbose_msg("query '%s' succeeded - should have failed with errno %d...",
		q->query, q->expected_errno[0]);
    goto end;
  }


  if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql))
  {
    if (q->require_file)
      abort_not_supported_test();
    if (q->abort_on_error)
      die("At line %u: Failed in mysql_store_result for query '%s' (%d)",
	  start_lineno, q->query, mysql_errno(mysql));
    else
    {
      verbose_msg("failed in mysql_store_result for query '%s' (%d)", q->query,
		  mysql_errno(mysql));
      error = 1;
      goto end;
    }
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
      dynstr_append_mem(ds, val, len);
    }

    dynstr_append_mem(ds, "\n", 1);
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

end:
  if (res) mysql_free_result(res);
  if (ds == &ds_tmp)
    dynstr_free(&ds_tmp);
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
  type=find_type(q->query, &command_typelib, 1);
  q->query[q->first_word_len]=save;
  if (type > 0)
    q->type=type;				/* Found command */
}


int main(int argc, char** argv)
{
  int error = 0;
  struct st_query* q;
  my_bool require_file=0;
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

  for(;!read_query(&q);)
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
      case Q_SOURCE: do_source(q); break;
      case Q_SLEEP: do_sleep(q); break;
      case Q_INC: do_inc(q); break;
      case Q_DEC: do_dec(q); break;
      case Q_ECHO: do_echo(q); break;
      case Q_SYSTEM: do_system(q); break;
      case Q_LET: do_let(q); break;
      case Q_QUERY:
      case Q_REAP:	
      {
	int flags = QUERY_REAP;
	if(q->type == Q_QUERY)
	  flags |= QUERY_SEND;
	
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
	q->query += q->first_word_len;
	error |= run_query(&cur_con->mysql, q, QUERY_SEND);
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
      case Q_SAVE_MASTER_POS: do_save_master_pos(q); break;	
      case Q_SYNC_WITH_MASTER: do_sync_with_master(q); break;	
      case Q_COMMENT:				/* Ignore row */
      case Q_COMMENT_WITH_COMMAND:
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
  exit(error);
  return error;
}
