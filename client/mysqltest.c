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

#define MTEST_VERSION "1.2"

#include "global.h"
#include "my_sys.h"
#include "m_string.h"
#include "mysql.h"
#include "mysql_version.h"
#include "my_config.h"
#include "my_dir.h"
#include "mysqld_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_QUERY  65536
#define PAD_SIZE	128
#define MAX_CONS   1024
#define MAX_INCLUDE_DEPTH 16
#define LAZY_GUESS_BUF_SIZE 8192
#define INIT_Q_LINES	  1024
#define MIN_VAR_ALLOC	  32
#define BLOCK_STACK_DEPTH  32

int record = 0, verbose = 0, silent = 0;
static char *db = 0, *pass=0;
const char* user = 0, *host = 0, *unix_sock = 0;
int port = 0;
static uint start_lineno, *lineno;

static const char *load_default_groups[]= { "mysqltest","client",0 };

FILE* file_stack[MAX_INCLUDE_DEPTH];
FILE** cur_file;
FILE** file_stack_end;
uint lineno_stack[MAX_INCLUDE_DEPTH];

int block_stack[BLOCK_STACK_DEPTH];
int *cur_block, *block_stack_end;

DYNAMIC_ARRAY q_lines;

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
struct query
{
  char q[MAX_QUERY];
  int first_word_len;
  my_bool abort_on_error, require_file;
  uint expected_errno;
  char record_file[FN_REFLEN];
  /* Add new commands before Q_UNKNOWN */
  enum { Q_CONNECTION=1, Q_QUERY, Q_CONNECT,
	 Q_SLEEP, Q_INC, Q_DEC,Q_SOURCE,
	 Q_DISCONNECT,Q_LET, Q_ECHO, Q_WHILE, Q_END_BLOCK,
	 Q_SYSTEM, Q_RESULT, Q_REQUIRE,
	 Q_UNKNOWN, Q_COMMENT, Q_COMMENT_WITH_COMMAND} type;
};

const char *command_names[] = {
"connection", "query","connect","sleep","inc","dec","source","disconnect",
"let","echo","while","end","system","result", "require",0
};

TYPELIB command_typelib= {array_elements(command_names),"",
			  command_names};


#define DS_CHUNK   16384

typedef struct dyn_string
{
  char* str;
  int len,max_len;
} DYN_STRING;

DYN_STRING ds_res;

void dyn_string_init(DYN_STRING* ds);
void dyn_string_end(DYN_STRING* ds);
void dyn_string_append(DYN_STRING* ds, const char* str, int len);
int dyn_string_cmp(DYN_STRING* ds, const char* fname);
void reject_dump(const char* record_file, char* buf, int size);

int close_connection(struct query* q);
VAR* var_get(char* var_name, char* var_name_end, int raw);

static void close_cons()
{
  for(--next_con; next_con >= cons; --next_con)
    {
      mysql_close(&next_con->mysql);
      my_free(next_con->name, MYF(MY_ALLOW_ZERO_PTR));
    }
}

static void die(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s: ", my_progname);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  close_cons();
  exit(1);
}

static void abort_not_supported_test()
{
  fprintf(stderr, "This test is not supported by this installation\n");
  if (!silent)
    printf("skipped\n");
  close_cons();
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

void dyn_string_init(DYN_STRING* ds)
{
  if (!(ds->str = (char*)my_malloc(DS_CHUNK, MYF(0))))
    die("Out of memory");
  ds->len = 0;
  ds->max_len = DS_CHUNK;
}

void dyn_string_end(DYN_STRING* ds)
{
  my_free(ds->str, MYF(0));
  memset(ds, 0, sizeof(*ds)); /* safety */
}

void dyn_string_append(DYN_STRING* ds, const char* str, int len)
{
  int new_len;
  if (!len)
    len = strlen(str);
  new_len = ds->len + len;
  if (new_len > ds->max_len)
    {
      int new_alloc_len = (new_len & ~(DS_CHUNK-1)) + DS_CHUNK;
      char* tmp = (char*) my_malloc(new_alloc_len, MYF(0));
      if (!tmp)
	die("Out of memory");
      memcpy(tmp, ds->str, ds->len);
      memcpy(tmp + ds->len, str, len);
      my_free((gptr)ds->str, MYF(0));
      ds->str = tmp;
      ds->len = new_len;
      ds->max_len = new_alloc_len;
    }
  else
    {
      memcpy(ds->str + ds->len, str, len);
      ds->len += len;
    }
}


int dyn_string_cmp(DYN_STRING* ds, const char* fname)
{
  MY_STAT stat_info;
  char *tmp;
  int res;
  int fd;
  if (!my_stat(fname, &stat_info, MYF(MY_WME)))
    die("Could not stat %s: errno =%d", fname, errno);
  if (stat_info.st_size != ds->len)
    return 2;
  if (!(tmp = (char*) my_malloc(ds->len, MYF(0))))
    die("Out of memory");
  if ((fd = my_open(fname, O_RDONLY, MYF(MY_WME))) < 0)
    die("Could not open %s: errno = %d", fname, errno);
  if (my_read(fd, (byte*)tmp, stat_info.st_size, MYF(MY_WME|MY_NABP)))
    die("read failed");
  res = (memcmp(tmp, ds->str, stat_info.st_size)) ?  1 : 0;
  my_free((gptr)tmp, MYF(0));
  my_close(fd, MYF(0));
  return res;
}

static int check_result(DYN_STRING* ds, const char* fname,
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
    reject_dump(fname, ds->str, ds->len);
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
  if (*cur_file && ++cur_file == file_stack_end)
    die("Source directives are nesting too deep");
  if (!(*cur_file = my_fopen(name, O_RDONLY, MYF(MY_WME))))
    die("Could not read '%s': errno %d\n", name, errno);
  *++lineno=1;

  return 0;
}

int do_source(struct query* q)
{
  char* p, *name;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  if (!*p)
    die("Missing file name in source\n");
  name = p;
  while(*p && !isspace(*p))
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

int do_inc(struct query* q)
{
  char* p;
  VAR* v;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  v = var_get(p, 0, 1);
  v->int_val++;
  v->int_dirty = 1;
  return 0;
}

int do_dec(struct query* q)
{
  char* p;
  VAR* v;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  v = var_get(p, 0, 1);
  v->int_val--;
  v->int_dirty = 1;
  return 0;
}

int do_system(struct query* q)
{
  char* p;
  VAR v;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
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

int do_echo(struct query* q)
{
  char* p;
  VAR v;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  eval_expr(&v, p, 0); /* NULL terminated */
  if (v.str_val_len > 1)
    {
      fflush(stdout);
      write(1, v.str_val, v.str_val_len - 1);
    }
  write(1, "\n", 1);
  return 0;
}

int do_let(struct query* q)
{
  char* p, *var_name, *var_name_end, *var_val_start;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
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

int do_sleep(struct query* q)
{
  char* p, *arg;
  struct timeval t;
  int dec_mul = 1000000;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  if (!*p)
    die("Missing argument in sleep\n");
  arg = p;
  t.tv_sec = atoi(arg);
  t.tv_usec = 0;
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
  *p = 0;
  t.tv_usec *= dec_mul;
  return select(0,0,0,0, &t);
}

static void get_file_name(char *filename, struct query* q)
{
  char *p = (char*) q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  strnmov(filename, p, FN_REFLEN);
  /* Remove end space */
  while (p > filename && isspace(p[-1]))
    p--;
  p[0]=0;
}


int select_connection(struct query* q)
{
  char* p, *name;
  struct connection *con;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  if (!*p)
    die("Missing connection name in connect\n");
  name = p;
  while(*p && !isspace(*p))
    p++;
  *p = 0;

  for(con = cons; con < next_con; con++)
    if (!strcmp(con->name, name))
      {
	cur_con = con;
	return 0;
      }

  die("connection '%s' not found in connection pool", name);
  return 1;
}

int close_connection(struct query* q)
{
  char* p, *name;
  struct connection *con;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  if (!*p)
    die("Missing connection name in connect\n");
  name = p;
  while(*p && !isspace(*p))
    p++;
  *p = 0;

  for(con = cons; con < next_con; con++)
    if (!strcmp(con->name, name))
      {
	mysql_close(&con->mysql);
	return 0;
      }

  die("connection '%s' not found in connection pool", name);
  return 1;
}


/* this one now is a hack - we may want to improve in in the
   future to handle quotes. For now we assume that anything that is not
   a comma, a space or ) belongs to the argument. space is a chopper, comma or
   ) are delimiters/terminators
*/
char* safe_get_param(char* str, char** arg, const char* msg)
{
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
  return str;
}

int do_connect(struct query* q)
{
  char* con_name, *con_user,*con_pass, *con_host, *con_port_str,
    *con_db, *con_sock;
  char* p;

  p = q->q + q->first_word_len;

  while(*p && isspace(*p)) p++;
  if (*p != '(')
    die("Syntax error in connect - expeected '(' found '%c'", *p);
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
  if (!mysql_real_connect(&next_con->mysql, con_host, con_user, con_pass,
			 con_db, atoi(con_port_str), con_sock, 0))
    die("Could not open connection '%s': %s", con_name,
	mysql_error(&next_con->mysql));

  if (!(next_con->name = my_strdup(con_name, MYF(MY_WME))))
    die("Out of memory");
  cur_con = next_con++;

  return 0;
}

int do_done(struct query* q)
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

int do_while(struct query* q)
{
  char *p = q->q + q->first_word_len;
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

int read_query(struct query** q_ptr)
{
  char* p = read_query_buf, * p1 ;
  int c, expected_errno;
  struct query* q;

  if (parser.current_line < parser.read_lines)
  {
    get_dynamic(&q_lines, (gptr)q_ptr, parser.current_line) ;
    return 0;
  }
  if (!(*q_ptr=q=(struct query*)my_malloc(sizeof(*q), MYF(MY_WME)))
     || insert_dynamic(&q_lines, (gptr)&q)
     )
    die("Out of memory");

  q->record_file[0] = 0;
  q->require_file=0;
  q->abort_on_error = 1;
  q->first_word_len = 0;
  q->expected_errno = 0;
  q->type = Q_UNKNOWN;
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
	q->expected_errno = expected_errno;
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
  while(*p && isspace(*p)) p++;
  /* Calculate first word */
  p1 = q->q;
  while(*p && !isspace(*p))
    *p1++ = *p++;

  q->first_word_len = p1 - q->q;
  strmov(p1, p);
  parser.read_lines++;
  return 0;
}

struct option long_options[] =
{
  {"verbose", no_argument, 0, 'v'},
  {"version", no_argument, 0, 'V'},
  {"silent", no_argument, 0, 'q'},
  {"quiet", no_argument, 0, 'q'},
  {"record", no_argument, 0, 'r'},
  {"result-file", required_argument, 0, 'R'},
  {"help", no_argument, 0, '?'},
  {"user", required_argument, 0, 'u'},
  {"password", optional_argument, 0, 'p'},
  {"host", required_argument, 0, 'h'},
  {"socket", required_argument, 0, 'S'},
  {"database", required_argument, 0, 'D'},
  {"port", required_argument, 0, 'P'},
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
  -?, --help               Display this help and exit.\n\
  -h, --host=...           Connect to host.\n\
  -u, --user=...           User for login.\n\
  -p[password], --password[=...]\n\
                           Password to use when connecting to server.\n\
  -D, --database=...       Database to use.\n\
  -P, --port=...           Port number to use for connection.\n\
  -S, --socket=...         Socket file to use for connection.\n\
  -r, --record             Record output of test_file into result file.\n\
  -R, --result-file=...    Read/Store result from/in this file\n\
  -v, --verbose            Write more.\n\
  -q, --quiet, --silent    Suppress all normal output.\n\
  -V, --version            Output version information and exit.\n\n");
}

int parse_args(int argc, char **argv)
{
  int c, option_index = 0;
  my_bool tty_password=0;

  load_defaults("my",load_default_groups,&argc,&argv);
  while((c = getopt_long(argc, argv, "h:p::u:P:D:S:R:?rvVq",
			 long_options, &option_index)) != EOF)
    {
      switch(c)
	{
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
	case 'V':
	  print_version();
	  exit(0);
	case '?':
	  usage();
	  exit(0);
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
  if (strlen(record_file) >= FN_REFLEN-8)
    die("too long path name for reject");
  strmov(strmov(reject_file, record_file),".reject");
  str_to_file(reject_file, buf, size);
}


int run_query(MYSQL* mysql, struct query* q)
{
  MYSQL_RES* res = 0;
  MYSQL_FIELD* fields;
  MYSQL_ROW row;
  int num_fields,i, error = 0;
  unsigned long* lengths;
  char* val;
  int len;
  DYN_STRING *ds = &ds_res;
  DYN_STRING ds_tmp;
  dyn_string_init(&ds_tmp);

  if ( q->record_file[0])
  {
    ds = &ds_tmp;
  }

  if (mysql_query(mysql, q->q))
  {
    if (q->require_file)
      abort_not_supported_test();
    if (q->abort_on_error)
      die("At line %u: query '%s' failed: %d: %s", start_lineno, q->q,
	  mysql_errno(mysql), mysql_error(mysql));
    else
    {
      if (q->expected_errno)
      {
	error = (q->expected_errno != mysql_errno(mysql));
	if (error)
	  verbose_msg("query '%s' failed with wrong errno\
 %d instead of %d", q->q, mysql_errno(mysql), q->expected_errno);
	goto end;
      }

      verbose_msg("query '%s' failed: %d: %s", q->q, mysql_errno(mysql),
		  mysql_error(mysql));
      /* if we do not abort on error, failure to run the query does
	 not fail the whole test case
      */
      goto end;
    }
  }

  if (q->expected_errno)
  {
    error = 1;
    verbose_msg("query '%s' succeeded - should have failed with errno %d",
		q->q, q->expected_errno);
    goto end;
  }


  if (!(res = mysql_store_result(mysql)) && mysql_field_count(mysql))
  {
    if (q->require_file)
      abort_not_supported_test();
    if (q->abort_on_error)
      die("At line %u: Failed in mysql_store_result for query '%s' (%d)",
	  start_lineno, q->q, mysql_errno(mysql));
    else
    {
      verbose_msg("failed in mysql_store_result for query '%s' (%d)", q->q,
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
      dyn_string_append(ds, "\t", 1);
    dyn_string_append(ds, fields[i].name, 0);
  }

  dyn_string_append(ds, "\n", 1);


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
	dyn_string_append(ds, "\t", 1);
      dyn_string_append(ds, val, len);
    }

    dyn_string_append(ds, "\n", 1);
  }

  if (record)
  {
    if (!q->record_file[0] && !result_file)
      die("At line %u: Missing result file", start_lineno);
    if (!result_file)
      str_to_file(q->record_file, ds->str, ds->len);
  }
  else if (q->record_file[0])
  {
    error = check_result(ds, q->record_file, q->require_file);
  }

end:
  if (res) mysql_free_result(res);
  return error;
}


void get_query_type(struct query* q)
{
  char save;
  uint type;
  if (*q->q == '}')
  {
    q->type = Q_END_BLOCK;
    return;
  }
  if (q->type != Q_COMMENT_WITH_COMMAND)
    q->type = Q_QUERY;

  save=q->q[q->first_word_len];
  q->q[q->first_word_len]=0;
  type=find_type(q->q, &command_typelib, 0);
  q->q[q->first_word_len]=save;
  if (type > 0)
    q->type=type;				/* Found command */
}



int main(int argc, char** argv)
{
  int error = 0;
  struct query* q;
  my_bool require_file=0;
  char save_file[FN_REFLEN];
  save_file[0]=0;

  MY_INIT(argv[0]);
  memset(cons, 0, sizeof(cons));
  cons_end = cons + MAX_CONS;
  next_con = cons + 1;
  cur_con = cons;

  memset(file_stack, 0, sizeof(file_stack));
  file_stack_end = file_stack + MAX_INCLUDE_DEPTH;
  cur_file = file_stack;
  lineno   = lineno_stack;
  init_dynamic_array(&q_lines, sizeof(struct query*), INIT_Q_LINES,
		     INIT_Q_LINES);
  memset(block_stack, 0, sizeof(block_stack));
  block_stack_end = block_stack + BLOCK_STACK_DEPTH;
  cur_block = block_stack;
  dyn_string_init(&ds_res);
  parse_args(argc, argv);
  if (!*cur_file)
    *cur_file = stdin;
  *lineno=1;

  if (!( mysql_init(&cur_con->mysql)))
    die("Failed in mysql_init()");

  mysql_options(&cur_con->mysql, MYSQL_READ_DEFAULT_GROUP, "mysql");

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
      case Q_DISCONNECT: close_connection(q); break;
      case Q_SOURCE: do_source(q); break;
      case Q_SLEEP: do_sleep(q); break;
      case Q_INC: do_inc(q); break;
      case Q_DEC: do_dec(q); break;
      case Q_ECHO: do_echo(q); break;
      case Q_SYSTEM: do_system(q); break;
      case Q_LET: do_let(q); break;
      case Q_QUERY:
      {
	if (save_file[0])
	{
	  strmov(q->record_file,save_file);
	  q->require_file=require_file;
	  save_file[0]=0;
	}
	error |= run_query(&cur_con->mysql, q); break;
      }
      case Q_RESULT:
	get_file_name(save_file,q);
	require_file=0;
	break;
      case Q_REQUIRE:
	get_file_name(save_file,q);
	require_file=1;
	break;
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

  close_cons();

  if (result_file && ds_res.len)
  {
    if(!record)
      error |= check_result(&ds_res, result_file, q->require_file);
    else
      str_to_file(result_file, ds_res.str, ds_res.len);
  }
  dyn_string_end(&ds_res);

  if (!silent) {
    if(error)
      printf("not ok\n");
    else
      printf("ok\n");
  }

  exit(error);
  return error;
}
