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
 *
 **/

#define MTEST_VERSION "1.0"

#include "global.h"
#include "my_sys.h"
#include "m_string.h"
#include "mysql.h"
#include "mysql_version.h"
#include "my_config.h"
#include "mysqld_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_QUERY  16384
#define MAX_RECORD_FILE 128
#define PAD_SIZE        128
#define MAX_CONS   1024
#define MAX_INCLUDE_DEPTH 16
#define LAZY_GUESS_BUF_SIZE 8192
#define INIT_Q_LINES      1024
#define MIN_VAR_ALLOC     32
#define BLOCK_STACK_DEPTH  32

int record = 0, verbose = 0, silent = 0;
const char* record_mode = "r";
static char *db = 0, *pass=0;
const char* user = 0, *host = 0, *unix_sock = 0;
int port = 0;
static const char *load_default_groups[]= { "mysqltest","client",0 };

FILE* file_stack[MAX_INCLUDE_DEPTH];
FILE** cur_file;
FILE** file_stack_end;

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

/* this should really be called command*/
struct query
{
  char q[MAX_QUERY];
  int has_result_set;
  int first_word_len;
  int abort_on_error;
  uint expected_errno;
  char record_file[MAX_RECORD_FILE];
  enum {Q_CONNECTION, Q_QUERY, Q_CONNECT,
	Q_SLEEP, Q_INC, Q_DEC,Q_SOURCE,
	Q_DISCONNECT,Q_LET, Q_ECHO, Q_WHILE, Q_END_BLOCK,
	Q_UNKNOWN} type;
};

static void die(const char* fmt, ...);
int close_connection(struct query* q);
VAR* var_get(char* var_name, char* var_name_end, int raw);

void init_parser()
{
  parser.current_line = parser.read_lines = 0;
  memset(&var_reg,0, sizeof(var_reg));
}

int hex_val(int c)
{
  if(isdigit(c))
    return c - '0';
  else if((c = tolower(c)) >= 'a' && c <= 'f')
    return c - 'a' + 10;
  else
    return -1;
}

VAR* var_get(char* var_name, char* var_name_end, int raw)
{
  int digit;
  VAR* v;
  if(*var_name++ != '$')
    {
      --var_name;
      goto err;
    }
  digit = *var_name - '0';
  if(!(digit < 10 && digit >= 0))
    {
      --var_name;
      goto err;
    }
  v = var_reg + digit;
  if(!raw && v->int_dirty)
    {
      sprintf(v->str_val, "%d", v->int_val);
      v->int_dirty = 0;
    }
  return v;
 err:
  if(var_name_end)
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
  if(*var_name++ != '$')
    {
      --var_name;
      *var_name_end = 0;
      die("Variable name in %s does not start with '$'", var_name);
    }
  digit = *var_name - '0';
  if(!(digit < 10 && digit >= 0))
    {
      *var_name_end = 0;
      die("Unsupported variable name: %s", var_name);
    }
  v = var_reg + digit;
  if(v->alloced_len < (val_len = (int)(var_val_end - var_val)+1))
    {
      v->alloced_len = (val_len < MIN_VAR_ALLOC) ? MIN_VAR_ALLOC : val_len;
     if(!(v->str_val =
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
  if(*cur_file && ++cur_file == file_stack_end)
    die("Source directives are nesting too deep");
  if(!(*cur_file = fopen(name, "r")))
    die("Could not read '%s': errno %d\n", name, errno);
  
  return 0;
}

int do_source(struct query* q)
{
  char* p, *name;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  if(!*p)
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
  if((vp = var_get(p,p_end,0)))
    {
      memcpy(v, vp, sizeof(VAR));
      return 0;
    }
  if(p_end)
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


int do_echo(struct query* q)
{
  char* p;
  VAR v;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  eval_expr(&v, p, 0); /* NULL terminated */
  if(v.str_val_len > 1)
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
  if(!*p)
    die("Missing variable name in let\n");
  var_name = p;
  while(*p && (*p != '=' || isspace(*p)))
    p++;
  var_name_end = p;
  if(*p == '=') p++;
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
  if(!*p)
    die("Missing agument in sleep\n");
  arg = p;
  t.tv_sec = atoi(arg);
  t.tv_usec = 0;
  while(*p && *p != '.' && !isspace(*p))
    p++;
  if(*p == '.')
    {
      char c;
      char *p_end;
      p++;
      p_end = p + 6;
      
      for(;p <= p_end; ++p)
	{
	  c = *p - '0';
	  if(c < 10 && c >= 0)
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


int select_connection(struct query* q)
{
  char* p, *name;
  struct connection *con;
  p = (char*)q->q + q->first_word_len;
  while(*p && isspace(*p)) p++;
  if(!*p)
    die("Missing connection name in connect\n");
  name = p;
  while(*p && !isspace(*p))
    p++;
  *p = 0;
  
  for(con = cons; con < next_con; con++)
    if(!strcmp(con->name, name))
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
  if(!*p)
    die("Missing connection name in connect\n");
  name = p;
  while(*p && !isspace(*p))
    p++;
  *p = 0;
  
  for(con = cons; con < next_con; con++)
    if(!strcmp(con->name, name))
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
      if(isspace(*str)) *str = 0;
      str++;
    }
  if(!*str)
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
  if(*p != '(')
    die("Syntax error in connect - expeected '(' found '%c'", *p);
  p++;
  p = safe_get_param(p, &con_name, "missing connection name");
  p = safe_get_param(p, &con_host, "missing connection host");
  p = safe_get_param(p, &con_user, "missing connection user");
  p = safe_get_param(p, &con_pass, "missing connection password");
  p = safe_get_param(p, &con_db, "missing connection db");
  p = safe_get_param(p, &con_port_str, "missing connection port");
  p = safe_get_param(p, &con_sock, "missing connection scoket");
  if(next_con == cons_end)
    die("Connection limit exhausted - incread MAX_CONS in mysqltest.c");

  if(!mysql_init(&next_con->mysql))
    die("Failed on mysql_init()");
  if(!mysql_real_connect(&next_con->mysql, con_host, con_user, con_pass,
			 con_db, atoi(con_port_str), con_sock, 0))
    die("Could not open connection '%s': %s", con_name,
	mysql_error(&next_con->mysql));

  if(!(next_con->name = my_strdup(con_name, MYF(MY_WME))))
    die("Out of memory");
  cur_con = next_con++;

  return 0;
}

int do_done(struct query* q)
{
  q->type = Q_END_BLOCK;
  if(cur_block == block_stack)
    die("Stray '}' - end of block before beginning");
  if(block_ok)
    parser.current_line = *--cur_block;
  else
    {
      if(!--false_block_depth)
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
  if(cur_block == block_stack_end)
	die("Nesting too deep");
  if(!block_ok)
    {
      ++false_block_depth;
      return 0;
    }  
  expr_start = strchr(p, '(');
  if(!expr_start)
    die("missing '(' in while");
  expr_end = strrchr(expr_start, ')');
  if(!expr_end)
    die("missing ')' in while");
  eval_expr(&v, ++expr_start, --expr_end);
  *cur_block++ = parser.current_line++; 
  if(!v.int_val)
    {
      block_ok = 0;
      false_block_depth = 1;
    }
  return 0;	
}

void close_cons()
{
  for(--next_con; next_con >= cons; --next_con)
    {
      mysql_close(&next_con->mysql);
      my_free(next_con->name, MYF(0));
    }
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
	  if(c == '\\')
	    {
	      state = ST_ESCAPED;
	    }
	  else
	    *p_dest++ = c;
	  break;
	case ST_ESCAPED:
	  if((val = hex_val(c)) > 0)
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
	  if((val = hex_val(c)) > 0)
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
  char* p = buf, *buf_end = buf + size;
  int no_save = 0;
  enum {R_NORMAL, R_Q1, R_ESC_Q_Q1, R_ESC_Q_Q2,
	R_ESC_SLASH_Q1, R_ESC_SLASH_Q2,
	R_Q2, R_COMMENT, R_LINE_START} state = R_LINE_START;
  
  for(; p < buf_end ;)
    {
      no_save = 0;
      c = fgetc(*cur_file);
      if(feof(*cur_file))
	{
          fclose(*cur_file);
	  
	  if(cur_file == file_stack)
	   return 1;
	  else
	    {
	      cur_file--;
	      continue;
	    }
	}
      
      switch(state)
	{
	case R_NORMAL:
	  if(c == ';' || c == '{') /*  '{' allows some interesting syntax
				    *  but we don't care, as long as the
				    *  correct sytnax gets parsed right */
	     {
	      *p = 0;
	      return 0;
	     }
	   else if(c == '\'')
             state = R_Q1;
	   else if(c == '"')
             state = R_Q2;
	   else if(c == '\n')
	     state = R_LINE_START;
	     
	   break;
	case R_COMMENT:
	  no_save = 1;
	  if(c == '\n')
	    state = R_LINE_START;
	  break;
	  
	case R_LINE_START:
	  if(c == '#')
	    {
	     state = R_COMMENT;
	     no_save = 1;
	    }
	  else if(isspace(c))
	    no_save = 1;
	  else if(c == '}')
	    {
	      *buf++ = '}';
	      *buf = 0;
	      return 0;
	    }
	  else if(c == ';' || c == '{')
	    {
	      *p = 0;
	      return 0;
	    }
	  else
	    state = R_NORMAL;
	  break;
	  
	case R_Q1:
	  if(c == '\'')
	    state = R_ESC_Q_Q1;
	  else if(c == '\\')
	    state = R_ESC_SLASH_Q1;
	  break;
	case R_ESC_Q_Q1:
           if(c == ';')
	     {
	      *p = 0;
	      return 0;
	     }
	  if(c != '\'')
	    state = R_NORMAL;
	  break;
	case R_ESC_SLASH_Q1:
	  state = R_Q1;
	  break;
	  
	case R_Q2:
	  if(c == '"')
	    state = R_ESC_Q_Q2;
	  else if(c == '\\')
	    state = R_ESC_SLASH_Q2;
	  break;
	case R_ESC_Q_Q2:
           if(c == ';')
	     {
	      *p = 0;
	      return 0;
	     }
	  if(c != '"')
	    state = R_NORMAL;
	  break;
	case R_ESC_SLASH_Q2:
	  state = R_Q2;
	  break;
       }

      if(!no_save)
	*p++ = c;
    }
  return feof(*cur_file); 
}

int read_query(struct query** q_ptr)
{
  char buf[MAX_QUERY];
  char* p = buf,* p1 ;
  int c, expected_errno;
  struct query* q;
  if(parser.current_line < parser.read_lines)
    {
      get_dynamic(&q_lines, (gptr)q_ptr, parser.current_line) ;
      return 0;
    }
  if(!(*q_ptr=q=(struct query*)my_malloc(sizeof(*q), MYF(MY_WME)))
     || insert_dynamic(&q_lines, (gptr)&q)
     )
    die("Out of memory");
  
  q->record_file[0] = 0;
  q->abort_on_error = 1;
  q->has_result_set = 0;
  q->first_word_len = 0;
  q->expected_errno = 0;
  q->type = Q_UNKNOWN;
  if(read_line(buf, sizeof(buf)))
    return 1;
  if(*p == '!')
    {
     q->abort_on_error = 0;
     p++;
     if(*p == '$')
       {
	 expected_errno = 0;
	 p++;
	 for(;isdigit(*p);p++)
	   expected_errno = expected_errno * 10 + *p - '0';
	 q->expected_errno = expected_errno;
       }
    }

  while(*p && isspace(*p)) p++ ;
  if(*p == '@')
    {
      q->has_result_set = 1;
      p++;
      p1 = q->record_file;
      while(!isspace(c = *p) &&
	    p1 < q->record_file + sizeof(q->record_file) - 1)
	*p1++ = *p++;
      *p1 = 0;      	
	
    }

  while(*p && isspace(*p)) p++;
  p1 = q->q;
  while(*p && !isspace(*p))
    *p1++ = *p++;
  
  q->first_word_len = p1 - q->q;
  strcpy(p1, p);
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
  {"help", no_argument, 0, '?'},
  {"user", required_argument, 0, 'u'},
  {"password", optional_argument, 0, 'p'},
  {"host", required_argument, 0, 'h'},
  {"socket", required_argument, 0, 'S'},
  {"database", required_argument, 0, 'D'},
  {"port", required_argument, 0, 'P'},
  {0, 0,0,0}
};

void die(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s: ", my_progname);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

void verbose_msg(const char* fmt, ...)
{
  va_list args;

  if(!verbose) return;
  
  va_start(args, fmt);
  
  fprintf(stderr, "%s: ", my_progname);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,MTEST_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

void usage()
{
  print_version();
  printf("MySQL AB, by Sasha & Matt\n");
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
  -v, --verbose            Write more.\n\
  -q, --quiet, --silent    Suppress all normal output.\n\
  -V, --version            Output version information and exit.\n\n");
}

int parse_args(int argc, char **argv)
{
  int c, option_index = 0;
  my_bool tty_password=0;

  load_defaults("my",load_default_groups,&argc,&argv);
  while((c = getopt_long(argc, argv, "h:p::u:P:D:S:?rvVq",
			 long_options, &option_index)) != EOF)
    {
      switch(c)
	{
	case 'v':
	  verbose = 1;
	  break;
	case 'r':
	  record = 1;
	  record_mode = "w";
	  break;
	case 'u':
	  user = optarg;
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
	  exit(0);
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
    {
      my_free(db,MYF(MY_ALLOW_ZERO_PTR));
      db=my_strdup(*argv,MYF(MY_WME));
    }
  if (tty_password)
    pass=get_tty_password(NullS);

  return 0;
}

char* safe_str_append(char* buf, char* str, int size)
{
  int i,c ;
  for(i = 0; (c = *str++) &&  i < size - 1; i++)
    *buf++ = c;
  *buf = 0;
  return buf;
}

void reject_dump(char* record_file, char* buf, int size)
{
  char reject_file[MAX_RECORD_FILE+16];
  char* p;
  FILE* freject;
  
  p = reject_file;
  p = safe_str_append(p, record_file, sizeof(reject_file));
  p = safe_str_append(p, (char*)".reject", reject_file - p +
		      sizeof(reject_file));

  if(!(freject = fopen(reject_file, "w")))
    die("Could not open reject file %s, error %d", reject_file, errno);
  fwrite(buf, size, 1, freject);
  fclose(freject);
}

int run_query(MYSQL* mysql, struct query* q)
{
  MYSQL_RES* res = 0;
  MYSQL_FIELD* fields;
  MYSQL_ROW row;
  int num_fields,i, error = 0;
  FILE* frecord = 0;
  char* res_buf = 0, *p_res_buf = 0, *res_buf_end = 0, *record_buf = 0;
  struct stat info;
  unsigned long* lengths;
  char* val;
  int len;
  int guess_result_size;

  if(q->record_file[0])
    {
      if(!(frecord = fopen(q->record_file, record_mode)))
	die("Error %d opening record file '%s'", errno, q->record_file);
      if(!record)
	{
	  if(stat(q->record_file, &info))
	    die("Error %d on stat of record file '%s'", errno, q->record_file);
	  /* let the developer be lazy and generate a .reject file
           * by touching the the result file and running the test
	   * in that case, we need a buffer large enough to hold the
	   * entire rejected result
	   */
	  guess_result_size = (info.st_size ? info.st_size :
			       LAZY_GUESS_BUF_SIZE) + PAD_SIZE;
	  /* if we guess wrong, the result will be truncated */
	  /* if the master result file is 0 length, the developer */
	  /* wants to generate reject file, edit it, and then move into
	   * the master result location - in this case we should just
	   * allocate a buffer that is large enough for the kind of result
	   * that a human would want to examine - hope 8 K is enough
	   * if this is a real test, the result should be exactly the length
	   * of the master file if it is correct. If it is wrong and is
	   * longer, we should be able to tell what is wrong by looking
	   * at the first "correct" number of bytes
	   */
	  if(!(p_res_buf = res_buf =
	       (char*)malloc(guess_result_size)))
	    die("malloc() failed trying to allocate %d bytes",
		guess_result_size);
	  res_buf_end = res_buf + guess_result_size;;
	}
    } 	


  
  if(mysql_query(mysql, q->q))
    {
      if(q->abort_on_error)
	die("query '%s' failed: %s", q->q, mysql_error(mysql));
      else
	{
	  if(q->expected_errno)
	    {
	      error = (q->expected_errno != mysql_errno(mysql));
	      if(error)
       	        verbose_msg("query '%s' failed with wrong errno\
 %d instead of %d", q->q, mysql_errno(mysql), q->expected_errno);
	      goto end;
	    }
	  
 	  verbose_msg("query '%s' failed: %s", q->q, mysql_error(mysql));
	  /* if we do not abort on error, failure to run the query does
	     not fail the whole test case
	  */  
	  goto end;
	}
    }

  if(q->expected_errno)
    {
      error = 1;
      verbose_msg("query '%s' succeeded - should have failed with errno %d",
		  q->q, q->expected_errno);
      goto end;
    }
  
  if(!q->has_result_set)
    goto end;

  if(!(res = mysql_store_result(mysql)))
    {
     if(q->abort_on_error)
       die("failed in mysql_store_result for query '%s'", q->q);
     else
       {
         verbose_msg("failed in mysql_store_result for query '%s'", q->q);
	 error = 1;
         goto end;
       }
    }
  
  if(!frecord)
      goto end;
  
  fields =  mysql_fetch_fields(res);
  num_fields =  mysql_num_fields(res);
  for( i = 0; i < num_fields; i++)
    {
      if(record)
	fprintf(frecord, "%s\t", fields[i].name);
      else
	{
	  p_res_buf = safe_str_append(p_res_buf, fields[i].name,
				      res_buf_end - p_res_buf - 1);
	  *p_res_buf++ = '\t';
	}
    }

  if(record)
    fputc('\n', frecord);
  else if(res_buf_end > p_res_buf) 
    *p_res_buf++ = '\n';

  while((row = mysql_fetch_row(res)))
  {
    lengths = mysql_fetch_lengths(res);
    for(i = 0; i < num_fields; i++)
      {
	 val = (char*)row[i];
	 len = lengths[i];
	 
	if(!val)
	  {
	    val = (char*)"NULL";
	    len = 4;
	  }
	if(record)
	  {
	    fwrite(val, len, 1, frecord);
	    fputc('\t', frecord);
	  }
	else
	  {
	    if(p_res_buf + len + 1 < res_buf_end)
	      {
		 memcpy(p_res_buf, val, len);
		 p_res_buf += len;
		 *p_res_buf++ = '\t';
	      }
	    
	  }
      }

    if(record)
      fputc('\n', frecord);
    else if(res_buf_end > p_res_buf) 
    *p_res_buf++ = '\n';
      
  }

  if(!record && frecord) 
    {
      if( (len = p_res_buf - res_buf) != info.st_size)
	{
          verbose_msg("Result length mismatch: actual  %d, expected = %d ", len,
	      info.st_size);
          reject_dump(q->record_file, res_buf, len);
	  error = 1;
	}
      else
	{
	  if(!(record_buf = (char*)malloc(info.st_size)))
	    die("malloc() failed allocating %d bytes", info.st_size);
	  fread(record_buf, info.st_size, 1, frecord);
	  if(memcmp(record_buf, res_buf, len))
	    {
	      verbose_msg("Result not the same as the record");
	      reject_dump(q->record_file, res_buf, len);
	      error = 1;
	    }
	}
    }

  
 end:
  if(res_buf) free(res_buf);
  if(record_buf) free(record_buf);
  if(res) mysql_free_result(res);
  if(frecord) fclose(frecord);
  return error;
}

int check_first_word(struct query* q, const char* word, int len)
{
  const char* p, *p1, *end;

  if(len != q->first_word_len)
    return 0;

  
  p = word;
  end = p + len;
  p1 = q->q;

  for(; p < end; p++, p1++)
    if(tolower(*p) != tolower(*p1))
      return 0;

  return 1;
}

void get_query_type(struct query* q)
{
  if(*q->q == '}')
    {
      q->type = Q_END_BLOCK;
      return;
    }
  q->type = Q_QUERY;
  switch(q->first_word_len)
    {
    case 3:
      if(check_first_word(q, "inc", 3))
	q->type = Q_INC;
      else if(check_first_word(q, "dec", 3))
	q->type = Q_DEC;
      else if(check_first_word(q, "let", 3))
	q->type = Q_LET;
      break;
    case 4:
      if(check_first_word(q, "echo", 4))
	q->type = Q_ECHO;
      break;
    case 5:
      if(check_first_word(q, "sleep", 5))
	q->type = Q_SLEEP;
      else if(check_first_word(q, "while", 5))
	q->type = Q_WHILE;
      break;
    case 6:
      if(check_first_word(q, "source", 6))
	q->type = Q_SOURCE;
      break;
    case 7:
      if(check_first_word(q, "connect", 7))
	q->type = Q_CONNECT;
      break;
    case 10:
      if(check_first_word(q, "connection", 10))
	q->type = Q_CONNECTION;
      else if(check_first_word(q, "disconnect", 10))
	q->type = Q_DISCONNECT;
      break;
      
    }
}

int main(int argc, char** argv)
{
  int error = 0;
  struct query* q;

  MY_INIT(argv[0]);
  memset(cons, 0, sizeof(cons));
  cons_end = cons + MAX_CONS;
  next_con = cons + 1;
  cur_con = cons;
  
  memset(file_stack, 0, sizeof(file_stack));
  file_stack_end = file_stack + MAX_INCLUDE_DEPTH;
  cur_file = file_stack;
  init_dynamic_array(&q_lines, sizeof(struct query*), INIT_Q_LINES,
		     INIT_Q_LINES);
  memset(block_stack, 0, sizeof(block_stack));
  block_stack_end = block_stack + BLOCK_STACK_DEPTH;
  cur_block = block_stack;
  
  parse_args(argc, argv);
  if(!*cur_file)
    *cur_file = stdin;

  
  
  if(!( mysql_init(&cur_con->mysql)))
    die("Failed in mysql_init()");

  mysql_options(&cur_con->mysql, MYSQL_READ_DEFAULT_GROUP, "mysql");
  
  if(!mysql_real_connect(&cur_con->mysql, host,
			 user, pass, db, port, unix_sock,
     0))
    die("Failed in mysql_real_connect(): %s", mysql_error(&cur_con->mysql));
  cur_con->name = my_strdup("default", MYF(MY_WME));
  if(!cur_con->name)
    die("Out of memory");

  for(;!read_query(&q);)
    {
      int current_line_inc = 1, processed = 0;
      if(q->type == Q_UNKNOWN)
	get_query_type(q);
      if(block_ok)
	{
	  processed = 1;
	  switch(q->type)
	    {
	    case Q_CONNECT: do_connect(q); break;
	    case Q_CONNECTION: select_connection(q); break;
	    case Q_DISCONNECT: close_connection(q); break;
	    case Q_SOURCE: do_source(q); break;
	    case Q_SLEEP: do_sleep(q); break;
	    case Q_INC: do_inc(q); break;
	    case Q_DEC: do_dec(q); break;
	    case Q_ECHO: do_echo(q); break;
	    case Q_LET: do_let(q); break;
	    case Q_QUERY: error |= run_query(&cur_con->mysql, q); break;
	    default: processed = 0; break;
	    }
	}
      
      if(!processed)
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
  
  if (!silent) {
    if(error)
      printf("not ok\n");
    else
      printf("ok\n");
  }
  
  exit(error);
  return error;
}
