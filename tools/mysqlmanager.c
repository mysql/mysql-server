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

/* MySQL server management daemon
 *
 * Written by:
 *   Sasha Pachev <sasha@mysql.com>
 **/

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
#include <my_pthread.h>

#define MANAGER_VERSION "1.0"
#define MANAGER_GREETING "MySQL Server Management Daemon v.1.0"

#define LOG_ERR  1
#define LOG_WARN 2
#define LOG_INFO 3
#define LOG_DEBUG 4

#ifndef MANAGER_PORT
#define MANAGER_PORT  23546
#endif

#ifndef MANAGER_MAX_CMD_LEN
#define MANAGER_MAX_CMD_LEN 16384
#endif

#ifndef MANAGER_LOG_FILE
#define MANAGER_LOG_FILE "/var/log/mysqlmanager.log"
#endif

#ifndef MANAGER_BACK_LOG
#define MANAGER_BACK_LOG 50
#endif

#ifndef MAX_USER_NAME
#define MAX_USER_NAME 16
#endif

/* Variable naming convention - if starts with manager_, either is set
   directly by the user, or used closely in ocnjunction with a variable
   set by the user
*/

uint manager_port = MANAGER_PORT;
FILE* errfp;
const char* manager_log_file = MANAGER_LOG_FILE;
pthread_mutex_t lock_log,lock_shutdown,lock_exec_hash;
int manager_sock = -1;
struct sockaddr_in manager_addr;
ulong manager_bind_addr = INADDR_ANY;
int manager_back_log = MANAGER_BACK_LOG;
int in_shutdown = 0, shutdown_requested=0;
const char* manager_greeting = MANAGER_GREETING;
uint manager_max_cmd_len = MANAGER_MAX_CMD_LEN;
int one_thread = 0; /* for debugging */

/* messages */

#define MAX_CLIENT_MSG_LEN  256
#define NET_BLOCK    2048
#define ESCAPE_CHAR '\\'
#define EOL_CHAR '\n'

#define MSG_OK           200
#define MSG_INFO         250
#define MSG_ACCESS       401
#define MSG_CLIENT_ERR   450
#define MSG_INTERNAL_ERR 500

/* access flags */

#define PRIV_SHUTDOWN 1

struct manager_thd
{
  Vio* vio;
  char user[MAX_USER_NAME];
  int priv_flags;
  char* cmd_buf;
  int fatal,finished;
};

HASH exec_hash;

static struct manager_thd* manager_thd_new(Vio* vio);
static struct manager_exec* manager_exec_new(char* arg_start,char* arg_end);
static void manager_exec_print(Vio* vio,struct manager_exec* e);
static void manager_thd_free(struct manager_thd* thd);
static void manager_exec_free(void* e);
static char* arg_strmov(char* dest, const char* src, int n);
static byte* get_exec_key(const byte* e, uint* len,
			  my_bool __attribute__((unused)) t);
static uint tokenize_args(char* arg_start,char** arg_end);
static void init_arg_array(char* arg_str,char** args,uint arg_count);
typedef int (*manager_cmd_handler)(struct manager_thd*,char*,char*);


struct manager_cmd
{
  const char* name;
  const char* help;
  manager_cmd_handler handler_func;
  int len;
};

struct manager_exec
{
  char* ident;
  int ident_len;
  const char* error;
  char* bin_path;
  char** args;
  char con_user[16];
  char con_pass[16];
  int con_port;
  char con_sock[FN_REFLEN];
  char* data_buf; 
};

#define HANDLE_DECL(com) static int handle_ ## com (struct manager_thd* thd,\
 char* args_start,char* args_end)

#define HANDLE_NOARG_DECL(com) static int handle_ ## com \
  (struct manager_thd* thd, char* __attribute__((unused)) args_start,\
 char* __attribute__((unused)) args_end)


HANDLE_NOARG_DECL(ping);
HANDLE_NOARG_DECL(quit);
HANDLE_NOARG_DECL(help);
HANDLE_NOARG_DECL(shutdown);
HANDLE_DECL(def_exec);
HANDLE_NOARG_DECL(show_exec);

struct manager_cmd commands[] =
{
  {"ping", "Check if this server is alive", handle_ping,4},
  {"quit", "Finish session", handle_quit,4},
  {"shutdown", "Shutdown this server", handle_shutdown,8},
  {"def_exec", "Define executable entry", handle_def_exec,8},
  {"show_exec","Show defined executable entries",handle_show_exec,9},
  {"help", "Print this message", handle_help,4},
  {0,0,0,0}
};

struct option long_options[] =
{
  {"debug", optional_argument, 0, '#'},
  {"help",  no_argument, 0, 'h'},
  {"port",  required_argument, 0, 'P'},
  {"log",  required_argument, 0, 'l'},
  {"bind-address", required_argument, 0, 'b'},
  {"tcp-backlog", required_argument, 0, 'B'},
  {"greeting", required_argument, 0, 'g'},
  {"max-command-len",required_argument,0,'m'},
  {"one-thread",no_argument,0,'d'},
  {"version", no_argument, 0, 'V'},
  {0, 0, 0, 0}
};

static void die(const char* fmt,...);
static void print_time(FILE* fp);
static void clean_up();
static struct manager_cmd* lookup_cmd(char* s,int len);
static void client_msg(Vio* vio,int err_code,const char* fmt,...);
static void client_msg_pre(Vio* vio,int err_code,const char* fmt,...);
static void client_msg_raw(Vio* vio,int err_code,int pre,const char* fmt,
			    va_list args);
static int authenticate(struct manager_thd* thd);
static char* read_line(struct manager_thd* thd); /* returns pointer to end of
						 line
					      */
static pthread_handler_decl(process_connection,arg);
static int exec_line(struct manager_thd* thd,char* buf,char* buf_end);

static int exec_line(struct manager_thd* thd,char* buf,char* buf_end)
{
  char* p=buf;
  struct manager_cmd* cmd;
  for (;p<buf_end && !isspace(*p);p++)
    *p=tolower(*p);
  if (!(cmd=lookup_cmd(buf,(int)(p-buf))))
  {
    client_msg(thd->vio,MSG_CLIENT_ERR,
	       "Unrecognized command, type help to see list of supported\
 commands");
    return 1;
  }
  for (;p<buf_end && isspace(*p);p++);
  return cmd->handler_func(thd,p,buf_end);
}

static struct manager_cmd* lookup_cmd(char* s,int len)
{
  struct manager_cmd* cmd = commands;
  for (;cmd->name;cmd++)
  {
    if (cmd->len == len && !memcmp(cmd->name,s,len))
      return cmd;
  }
  return 0;
}

HANDLE_NOARG_DECL(ping)
{
  client_msg(thd->vio,MSG_OK,"Server management daemon is alive");
  return 0;
}

HANDLE_NOARG_DECL(quit)
{
  client_msg(thd->vio,MSG_OK,"Goodbye");
  thd->finished=1;
  return 0;
}

HANDLE_NOARG_DECL(help)
{
  struct manager_cmd* cmd = commands;
  Vio* vio = thd->vio;
  client_msg_pre(vio,MSG_INFO,"Available commands:");
  for (;cmd->name;cmd++)
  {
    client_msg_pre(vio,MSG_INFO,"%s - %s", cmd->name, cmd->help);
  }
  client_msg_pre(vio,MSG_INFO,"End of help");
  return 0;
}

HANDLE_NOARG_DECL(shutdown)
{
  client_msg(thd->vio,MSG_OK,"Shutdown started, goodbye");
  thd->finished=1;
  shutdown_requested = 1;
  return 0;
}

HANDLE_DECL(def_exec)
{
  struct manager_exec* e=0;
  const char* error=0;
  if (!(e=manager_exec_new(args_start,args_end)))
  {
    error="Out of memory";
    goto err;
  }
  if (e->error)
  {
    error=e->error;
    goto err;
  }
  pthread_mutex_lock(&lock_exec_hash);
  hash_insert(&exec_hash,(byte*)e);
  pthread_mutex_unlock(&lock_exec_hash);
  client_msg(thd->vio,MSG_OK,"Exec definition created");
  return 0;
err:
  client_msg(thd->vio,MSG_CLIENT_ERR,error);
  if (e)
    manager_exec_free(e);
  return 1;
}

HANDLE_NOARG_DECL(show_exec)
{
  uint i;
  client_msg_pre(thd->vio,MSG_INFO,"Exec_def\tArguments");
  pthread_mutex_lock(&lock_exec_hash);
  for (i=0;i<exec_hash.records;i++)
  {
    struct manager_exec* e=(struct manager_exec*)hash_element(&exec_hash,i);
    manager_exec_print(thd->vio,e);
  }
  pthread_mutex_unlock(&lock_exec_hash);
  client_msg(thd->vio,MSG_INFO,"End");
  return 0;
}

static char* arg_strmov(char* dest, const char* src, int n)
{
  char* dest_end = dest+n-1;
  char c;
  for (;dest<dest_end && (c=*src++);)
  {
    if (c=='%')
      *dest++='%';
    *dest++=c;
  }
  return dest;
}

static void manager_exec_print(Vio* vio,struct manager_exec* e)
{
  char buf[MAX_CLIENT_MSG_LEN];
  char* p=buf,*buf_end=buf+sizeof(buf);
  char** args=e->args;
  
  p=arg_strmov(p,e->ident,(int)(buf_end-p)-2);
  *p++='\t';
  for(;p<buf_end && *args;args++)
  {
    p=arg_strmov(p,*args,(int)(buf_end-p)-2);
    *p++='\t';
  }
  *p=0;
  client_msg_pre(vio,MSG_INFO,buf);
  return;
}

static int authenticate(struct manager_thd* thd)
{
  char* buf_end;
  client_msg(thd->vio,MSG_INFO, manager_greeting);
  if (!(buf_end=read_line(thd)))
    return -1;
  client_msg(thd->vio,MSG_OK,"OK");
  return 0;
}

static void print_time(FILE* fp)
{
  struct tm now;
  time_t t;
  time(&t);
  localtime_r(&t,&now);
  fprintf(fp,"[%d-%02d-%02d %02d:%02d:%02d] ", now.tm_year+1900,
	  now.tm_mon+1,now.tm_mday,now.tm_hour,now.tm_min,
	  now.tm_sec);
}

static void die(const char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  if (fmt)
  {
    if (errfp==stderr)
      fprintf(errfp, "%s: ", my_progname);
    else
      {
	print_time(errfp);
	fprintf(errfp,"Fatal error: ");
      }
    vfprintf(errfp, fmt, args);
    if (errno)
      fprintf(errfp, " errno=%d", errno);
    fprintf(errfp, "\n");
    fflush(errfp);
  }
  va_end(args);
  clean_up();
  exit(1);
}

void print_msg_type(int msg_type)
{
  const char* msg;
  switch (msg_type)
  {
  case LOG_ERR: msg = "ERROR"; break;
  case LOG_WARN: msg = "WARNING"; break;
  case LOG_INFO: msg = "INFO"; break;
#ifndef DBUG_OFF    
  case LOG_DEBUG: msg = "DEBUG"; break;
#endif    
  default: msg = "UNKNOWN TYPE"; break;
  }
  fprintf(errfp," %s: ", msg); 
}

static void log_msg(const char* fmt, int msg_type, va_list args)
{
  pthread_mutex_lock(&lock_log);
  print_time(errfp);
  print_msg_type(msg_type);
  vfprintf(errfp,fmt,args);
  fputc('\n',errfp);
  fflush(errfp);
  pthread_mutex_unlock(&lock_log);
}

#define LOG_MSG_FUNC(type,TYPE) inline static void log_ ## type  \
 (const char* fmt,...) { \
  va_list args; \
  va_start(args,fmt); \
  log_msg(fmt,LOG_ ## TYPE,args);\
 }

LOG_MSG_FUNC(err,ERR)
LOG_MSG_FUNC(warn,WARN)
LOG_MSG_FUNC(info,INFO)

#ifndef DBUG_OFF
LOG_MSG_FUNC(debug,DEBUG)
#else
inline void log_debug(char* __attribute__((unused)) fmt,...) {}
#endif

static pthread_handler_decl(process_connection,arg)
{
  struct manager_thd* thd = (struct manager_thd*)arg;
  my_thread_init();
  pthread_detach_this_thread();
  for (;!thd->finished;)
  {
    char* buf_end;
    if ((!(buf_end=read_line(thd)) || exec_line(thd,thd->cmd_buf,buf_end))
	&& thd->fatal)
    {
      log_err("Thread aborted");
      break;
    }
  }
  manager_thd_free(thd);
  pthread_exit(0);
}

static void client_msg_raw(Vio* vio, int err_code, int pre, const char* fmt,
			   va_list args)
{
  char buf[MAX_CLIENT_MSG_LEN],*p,*buf_end;
  p=buf;
  buf_end=buf+sizeof(buf);
  p=int10_to_str(err_code,p,10);
  if (pre)
    *p++='-';
  *p++=' ';
  p+=my_vsnprintf(p,buf_end-p,fmt,args);
  if (p>buf_end-2)
    p=buf_end - 2;
  *p++='\r';
  *p++='\n';
  if (vio_write(vio,buf,(uint)(p-buf))<=0)
    log_err("Failed writing to client: errno=%d");
}

static void client_msg(Vio* vio, int err_code, const char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  client_msg_raw(vio,err_code,0,fmt,args);
}

static void client_msg_pre(Vio* vio, int err_code, const char* fmt, ...)
{
  va_list args;
  va_start(args,fmt);
  client_msg_raw(vio,err_code,1,fmt,args);
}

static char* read_line(struct manager_thd* thd)
{
  char* p=thd->cmd_buf;
  char* buf_end = thd->cmd_buf + manager_max_cmd_len;
  int escaped = 0;
  for (;p<buf_end;)
  {
    int len,read_len;
    char *block_end,*p_back;
    read_len = min(NET_BLOCK,(uint)(buf_end-p));
    if ((len=vio_read(thd->vio,p,read_len))<=0)
    {
      log_err("Error reading command from client");
      return 0;
    }
    block_end=p+len;
    /* a trick to unescape in place */
    for (p_back=p;p<block_end;p++)
    {
      char c=*p;
      if (c==ESCAPE_CHAR)
      {
	if (!escaped)
	{
	  escaped=1;
	  continue;
	}
	else
	  escaped=0;
      }
      if (c==EOL_CHAR && !escaped)
	break;
      *p_back++=c;
      escaped=0;
    }
    if (p!=block_end)
    {
      *p_back=0;
      return p_back;
    }
  }
  client_msg(thd->vio,MSG_CLIENT_ERR,"Command line too long");
  return 0;
}

struct manager_thd* manager_thd_new(Vio* vio)
{
  struct manager_thd* tmp;
  if (!(tmp=(struct manager_thd*)my_malloc(sizeof(*tmp)+manager_max_cmd_len,
					MYF(0))))
  {
    log_err("Out of memory in manager_thd_new");
    return 0;
  }
  tmp->vio=vio;
  tmp->user[0]=0;
  tmp->priv_flags=0;
  tmp->fatal=tmp->finished=0;
  tmp->cmd_buf=(char*)tmp+sizeof(*tmp);
  return tmp;
}

static void manager_thd_free(struct manager_thd* thd)
{
  if (thd->vio)
    vio_close(thd->vio);
  my_free((byte*)thd->vio,MYF(0));
}

static void clean_up()
{
  pthread_mutex_lock(&lock_shutdown);
  if (in_shutdown)
  {
    pthread_mutex_unlock(&lock_shutdown);
    return;
  }
  in_shutdown = 1;
  pthread_mutex_unlock(&lock_shutdown);
  log_info("Shutdown started");
  if (manager_sock)
    close(manager_sock);
  log_info("Ended");
  if (errfp != stderr)
    fclose(errfp);
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,MANAGER_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage()
{
  print_version();
  printf("MySQL AB, by Sasha\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Manages instances of MySQL server.\n\n");
  printf("Usage: %s [OPTIONS]", my_progname);
  printf("\n\
  -?, --help               Display this help and exit.\n");
#ifndef DBUG_OFF
  puts("\
  -#, --debug=[...]        Output debug log. Often this is 'd:t:o,filename`");
#endif
  printf("\
  -P, --port=...           Port number to listen on.\n\
  -l, --log=...            Path to log file.\n\
  -b, --bind-address=...   Address to listen on.\n\
  -B, --tcp-backlog==...   Size of TCP/IP listen queue.\n\
  -g, --greeting=          Set greeting on connect \n\
  -m, --max-command-len    Maximum command length \n\
  -V, --version            Output version information and exit.\n\n");
}

static int parse_args(int argc, char **argv)
{
  int c, option_index = 0;
  while ((c=getopt_long(argc,argv,"P:?#:Vl:b:B:g:m:d",
			long_options,&option_index)) != EOF)
  {
    switch (c)
      {
      case '#':
	DBUG_PUSH(optarg ? optarg : "d:t:O,/tmp/mysqlmgrd.trace");
	break;
      case 'd':
	one_thread=1;
	break;
      case 'P':
	manager_port=atoi(optarg);
	break;
      case 'm':
	manager_max_cmd_len=atoi(optarg);
	break;
      case 'g':
	manager_greeting=optarg;
      case 'b':
	manager_bind_addr = inet_addr(optarg);
	break;
      case 'B':
	manager_back_log = atoi(optarg);
	break;
      case 'l':
	manager_log_file=optarg;
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
  return 0;
}

static int init_server()
{
  int arg=1;
  log_info("Started");
  if ((manager_sock=socket(PF_INET,SOCK_STREAM,0)) < 0)
    die("Could not create socket");
  bzero((char*)&manager_addr, sizeof(manager_addr));
  manager_addr.sin_family = AF_INET;
  manager_addr.sin_addr.s_addr = manager_bind_addr;
  manager_addr.sin_port = htons(manager_port);
  setsockopt(manager_sock,SOL_SOCKET, SO_REUSEADDR,(char*)&arg,sizeof(arg));
  if (bind(manager_sock,(struct sockaddr*)&manager_addr, sizeof(manager_addr)) < 0)
    die("Could not bind");
  if (listen(manager_sock,manager_back_log) < 0)
    die("Could not listen");

  return 0;
}

static int run_server_loop()
{
  pthread_t th;
  struct manager_thd *thd;
  int client_sock,len;
  Vio* vio;

  for (;!shutdown_requested;)
  {
    len=sizeof(struct sockaddr_in);
    if ((client_sock=accept(manager_sock,(struct sockaddr*)&manager_addr,&len))<0)
    {
      if (shutdown_requested)
	break;
      if (errno != EAGAIN)
      {
	log_warn("Error in accept, errno=%d", errno);
	sleep(1); /* avoid tying up CPU if accept is broken */
      }
      continue;
    }
    if (shutdown_requested)
      break;
    if (!(vio=vio_new(client_sock,VIO_TYPE_TCPIP,FALSE)))
    {
      log_err("Could not create I/O object");
      close(client_sock);
      continue;
    }
    if (!(thd=manager_thd_new(vio)))
    {
      log_err("Could not create thread object");
      vio_close(vio);
      continue;
    }
    
    if (authenticate(thd))
    {
      client_msg(vio,MSG_ACCESS, "Access denied");
      manager_thd_free(thd);
      continue;
    }
    if (shutdown_requested)
      break;
    if (one_thread)
    {
      process_connection((void*)thd);
      manager_thd_free(thd);
      continue;
    }
    else if (pthread_create(&th,0,process_connection,(void*)thd))
    {
      client_msg(vio,MSG_INTERNAL_ERR,"Could not create thread, errno=%d",
		 errno);
      manager_thd_free(thd);
      continue;
    }
  }
  return 0;
}

static FILE* open_log_stream()
{
  FILE* fp;
  if (!(fp=fopen(manager_log_file,"a")))
    die("Could not open log file '%s'", manager_log_file);
  return fp;
}

static byte* get_exec_key(const byte* e, uint* len,
			 my_bool __attribute__((unused)) t)
{
  register const char* key;
  key = ((struct manager_exec*)e)->ident;
  *len = ((struct manager_exec*)e)->ident_len;
  return (byte*)key;
}

static void init_arg_array(char* arg_str,char** args,uint arg_count)
{
  char* p = arg_str;
  for (;arg_count>0;arg_count--)
  {
    *args++=p;
    p += strlen(p)+1;
  }
  *args=0;
}

static uint tokenize_args(char* arg_start,char** arg_end)
{
  char* p, *p_write,*p_end;
  uint arg_count=0;
  int quoted=0,escaped=0,last_space=0;
  p_end=*arg_end;
  p_write=p=arg_start;
  for(;p<p_end;p++)
  {
    char c = *p;
    switch (c)
    {
    case ' ':
    case '\r':
    case '\n':  
      if (!quoted)
      {
	if (!last_space)
	{
	  *p_write++=0;
	  arg_count++;
	  last_space=1;
	}
      }
      else
	*p_write++=c;
      escaped=0;
      break;
    case '"':
      if (!escaped)
	quoted=!quoted;
      else
	*p_write++=c;
      last_space=0;
      escaped=0;
      break;
    case '\\':
      if (!escaped)
	escaped=1;
      else
      {
	*p_write++=c;
	escaped=0;
      }
      last_space=0;
      break;
    default:
      escaped=last_space=0;
      *p_write++=c;
      break;
    }
  }
  if (!last_space && p_write>arg_start)
    arg_count++;
  *p_write=0;
  *arg_end=p_write;
  log_debug("arg_count=%d,arg_start='%s'",arg_count,arg_start);
  return arg_count;
}


static struct manager_exec* manager_exec_new(char* arg_start,char* arg_end)
{
  struct manager_exec* tmp;
  char* first_arg;
  uint arg_len,num_args;
  num_args=tokenize_args(arg_start,&arg_end);
  arg_len=(uint)(arg_end-arg_start)+1; /* include \0 terminator*/
  if (!(tmp=(struct manager_exec*)my_malloc(sizeof(*tmp)+arg_len+
					    sizeof(char*)*num_args,MYF(0))))
    return 0;
  if (num_args<2)
  {
    tmp->error="Too few arguments";
    return tmp;
  }
  tmp->data_buf=(char*)tmp+sizeof(*tmp);
  memcpy(tmp->data_buf,arg_start,arg_len);
  tmp->args=(char**)(tmp->data_buf+arg_len);
  tmp->ident=tmp->data_buf;
  tmp->ident_len=strlen(tmp->ident);
  first_arg=tmp->ident+tmp->ident_len+1;
  init_arg_array(first_arg,tmp->args,num_args-1);
  strmov(tmp->con_user,"root");
  tmp->con_pass[0]=0;
  tmp->con_sock[0]=0;
  tmp->con_port=MYSQL_PORT;
  tmp->bin_path=tmp->args[0];
  tmp->error=0;
  return tmp;
}


static void manager_exec_free(void* e)
{
  my_free(e,MYF(0));
}

static void init_globals()
{
  if (hash_init(&exec_hash,1024,0,0,get_exec_key,manager_exec_free,MYF(0)))
    die("Exec hash initialization failed");
}

static int daemonize()
{
  switch (fork())
    {
    case -1:
      die("Cannot fork");
    case 0:
      errfp = open_log_stream();
      init_globals();
      close(0);
      close(1);
      close(2);
      init_server();
      run_server_loop();
      clean_up();
      break;
    default:
      break;
    }
  return 0;
}

int main(int argc, char** argv)
{
  MY_INIT(argv[0]);
  errfp = stderr;
  parse_args(argc,argv);
  pthread_mutex_init(&lock_log,0);
  pthread_mutex_init(&lock_shutdown,0);
  pthread_mutex_init(&lock_exec_hash,0);
  if (one_thread)
  {
    init_globals();
    init_server();
    run_server_loop();
    clean_up();
    return 0;
  }
  else
    return daemonize();
}









