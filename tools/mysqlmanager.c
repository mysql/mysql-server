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

#include <my_global.h>
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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <violite.h>
#include <my_pthread.h>
#include <md5.h>

#define MANAGER_VERSION "1.0"
#define MANAGER_GREETING "MySQL Server Management Daemon v. 1.0" 

#define LOG_ERR  1
#define LOG_WARN 2
#define LOG_INFO 3
#define LOG_DEBUG 4

#define CHILD_START 1
#define CHILD_STOP  2

#ifndef MANAGER_PORT
#define MANAGER_PORT  23546
#endif

#ifndef MANAGER_CONNECT_RETRIES
#define MANAGER_CONNECT_RETRIES 5
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

#ifndef MANAGER_PW_FILE
#define MANAGER_PW_FILE "/etc/mysqlmanager.passwd"
#endif

#ifndef MAX_HOST
#define MAX_HOST 128
#endif

#ifndef MAX_LAUNCHER_MSG
#define MAX_LAUNCHER_MSG 256
#endif

/* Variable naming convention - if starts with manager_, either is set
   directly by the user, or used closely in ocnjunction with a variable
   set by the user
*/

#if defined(__i386__) && defined(HAVE_LINUXTHREADS)
#define DO_STACKTRACE 1
#endif

uint manager_port = MANAGER_PORT;
FILE* errfp;
const char* manager_log_file = MANAGER_LOG_FILE;
pthread_mutex_t lock_log,lock_shutdown,lock_exec_hash,lock_launch_thd;
pthread_cond_t cond_launch_thd;
pthread_t loop_th,launch_msg_th;
int manager_sock = -1;
uchar* stack_bottom=0;
struct sockaddr_in manager_addr;
ulong manager_bind_addr = INADDR_ANY;
int manager_back_log = MANAGER_BACK_LOG;
int in_shutdown = 0, shutdown_requested=0;
int manager_connect_retries=MANAGER_CONNECT_RETRIES;
const char* manager_greeting = MANAGER_GREETING;
uint manager_max_cmd_len = MANAGER_MAX_CMD_LEN;
const char* manager_pw_file=MANAGER_PW_FILE;
int one_thread = 0; /* for debugging */

typedef enum {PARAM_STDOUT,PARAM_STDERR} PARAM_TYPE;

/* messages */

#define MAX_CLIENT_MSG_LEN  256
#define NET_BLOCK    2048
#define MD5_LEN      16
#define ESCAPE_CHAR '\\'
#define EOL_CHAR '\n'

/* access flags */

#define PRIV_SHUTDOWN 1

struct manager_thd
{
  Vio* vio;
  char user[MAX_USER_NAME+1];
  int priv_flags;
  char* cmd_buf;
  int fatal,finished;
};

struct manager_user
{
  char user[MAX_USER_NAME+1];
  char md5_pass[MD5_LEN];
  int user_len;
  const char* error;
};

HASH exec_hash,user_hash;
struct manager_exec* cur_launch_exec=0;

static struct manager_thd* manager_thd_new(Vio* vio);

static struct manager_exec* manager_exec_new(char* arg_start,char* arg_end);
static void manager_exec_print(Vio* vio,struct manager_exec* e);
static void manager_thd_free(struct manager_thd* thd);
static void manager_exec_free(void* e);
static void manager_exec_connect(struct manager_exec* e);
static int manager_exec_launch(struct manager_exec* e);
static struct manager_exec* manager_exec_by_pid(pid_t pid);

static struct manager_user* manager_user_new(char* buf);
static void manager_user_free(void* u);

static char* arg_strmov(char* dest, const char* src, int n);
static byte* get_exec_key(const byte* e, uint* len,
			  my_bool __attribute__((unused)) t);
static byte* get_user_key(const byte* u, uint* len,
			  my_bool __attribute__((unused)) t);
static uint tokenize_args(char* arg_start,char** arg_end);
static void init_arg_array(char* arg_str,char** args,uint arg_count);
static int hex_val(char c);
static int open_and_dup(int fd,char* path);
static void update_req_len(struct manager_exec* e);

typedef int (*manager_cmd_handler)(struct manager_thd*,char*,char*);

static void handle_child(int __attribute__((unused)) sig);
static void handle_sigpipe(int __attribute__((unused)) sig);

/* exec() in a threaded application is full of problems
   to solve this, we fork off a launcher at the very start
   and communicate with it through a pipe
*/
static void fork_launcher();
static void run_launcher_loop();
int to_launcher_pipe[2],from_launcher_pipe[2];
pid_t launcher_pid;
int in_segfault=0;

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
  pid_t pid;
  int exit_code;
  pthread_mutex_t lock;
  pthread_cond_t cond;
  pthread_t th;
  char con_sock[FN_REFLEN];
  char con_host[MAX_HOST];
  char stderr_path[FN_REFLEN];
  char stdout_path[FN_REFLEN];
  MYSQL mysql;
  char* data_buf;
  int req_len;
  int start_wait_timeout;
  int stderr_path_size,stdout_path_size,data_buf_size;
  int num_args;
};

static int set_exec_param(struct manager_thd* thd, char* args_start,
			  char* args_end, PARAM_TYPE param_type);

#define HANDLE_DECL(com) static int com (struct manager_thd* thd, char* args_start,char* args_end)
#define HANDLE_NOARG_DECL(com) static int com \
  (struct manager_thd* thd, char* __attribute__((unused)) args_start,\
 char* __attribute__((unused)) args_end)


HANDLE_NOARG_DECL(handle_ping);
HANDLE_NOARG_DECL(handle_quit);
HANDLE_NOARG_DECL(handle_help);
HANDLE_NOARG_DECL(handle_shutdown);
HANDLE_DECL(handle_def_exec);
HANDLE_DECL(handle_start_exec);
HANDLE_DECL(handle_stop_exec);
HANDLE_DECL(handle_set_exec_con);
HANDLE_DECL(handle_set_exec_stdout);
HANDLE_DECL(handle_set_exec_stderr);
HANDLE_NOARG_DECL(handle_show_exec);
HANDLE_DECL(handle_query);


struct manager_cmd commands[] =
{
  {"ping", "Check if this server is alive", handle_ping,4},
  {"quit", "Finish session", handle_quit,4},
  {"shutdown", "Shutdown this server", handle_shutdown,8},
  {"def_exec", "Define executable entry", handle_def_exec,8},
  {"start_exec", "Launch process defined by executable entry",
   handle_start_exec,10},
  {"stop_exec", "Stop process defined by executable entry",
   handle_stop_exec,9},
  {"set_exec_con", "Set connection parameters for executable entry",
   handle_set_exec_con,12},
  {"set_exec_stdout", "Set stdout path for executable entry",
   handle_set_exec_stdout,15},
  {"set_exec_stderr", "Set stderr path for executable entry",
   handle_set_exec_stderr,15},
  {"query","Run query against MySQL server",handle_query,5},
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
  {"connect-retries",required_argument,0,'C'},
  {"password-file",required_argument,0,'p'},
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
static pthread_handler_decl(process_launcher_messages,
			    __attribute__((unused)) arg);
static int exec_line(struct manager_thd* thd,char* buf,char* buf_end);

#ifdef DO_STACKTRACE
void print_stacktrace();
#endif

static void handle_segfault(int sig)
{
  if (in_segfault)
    exit(1);
  in_segfault=1;
  fprintf(errfp,"Got fatal signal %d\n",sig);
#ifdef DO_STACKTRACE
  print_stacktrace();
#endif
  exit(1);
}

static void handle_sigpipe(int __attribute__((unused)) sig)
{
  signal(SIGPIPE,handle_sigpipe);
}

#ifdef DO_STACKTRACE

#define MAX_DEPTH 25
#define SIGRETURN_FRAME_COUNT 1

void print_stacktrace()
{
  uchar** fp;
  int i;
  LINT_INIT(fp);
  fprintf(errfp,"Fatal errror, stacktrace follows:\n");
#ifdef __i386__  
  __asm__ __volatile__("movl %%ebp,%0" :"=r"(fp) :"r"(fp));
#endif
  if (!fp)
  {
    fprintf(errfp,"frame points is NULL, cannot trace stack\n");
    return;
  }
  for(i=0;i<MAX_DEPTH && fp<(uchar**)stack_bottom;i++)
  {
#ifdef __i386__    
    uchar** new_fp = (uchar**)*fp;
    fprintf(errfp, "%p\n", i == SIGRETURN_FRAME_COUNT ?
	    *(fp+17) : *(fp+1));
#endif /* __386__ */
    if (new_fp <= fp )
    {
      fprintf(errfp, "New value of fp=%p failed sanity check,\
 terminating stack trace!\n", new_fp);
      return;
    }
    fp = new_fp;
  }
  fprintf(errfp,"Stack trace successful\n");
  fflush(errfp);
}
#endif

static int exec_line(struct manager_thd* thd,char* buf,char* buf_end)
{
  char* p=buf;
  struct manager_cmd* cmd;
  for (;p<buf_end && !isspace(*p);p++)
    *p=tolower(*p);
  if (!(cmd=lookup_cmd(buf,(int)(p-buf))))
  {
    client_msg(thd->vio,MANAGER_CLIENT_ERR,
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

HANDLE_NOARG_DECL(handle_ping)
{
  client_msg(thd->vio,MANAGER_OK,"Server management daemon is alive");
  return 0;
}

HANDLE_NOARG_DECL(handle_quit)
{
  client_msg(thd->vio,MANAGER_OK,"Goodbye");
  thd->finished=1;
  return 0;
}

HANDLE_NOARG_DECL(handle_help)
{
  struct manager_cmd* cmd = commands;
  Vio* vio = thd->vio;
  client_msg_pre(vio,MANAGER_INFO,"Available commands:");
  for (;cmd->name;cmd++)
  {
    client_msg_pre(vio,MANAGER_INFO,"%s - %s", cmd->name, cmd->help);
  }
  client_msg_pre(vio,MANAGER_INFO,"End of help");
  return 0;
}

HANDLE_NOARG_DECL(handle_shutdown)
{
  client_msg(thd->vio,MANAGER_OK,"Shutdown started, goodbye");
  thd->finished=1;
  shutdown_requested = 1;
  if (!one_thread)
  {
    kill(launcher_pid,SIGTERM);
    pthread_kill(loop_th,SIGTERM);
  }
  return 0;
}

HANDLE_DECL(handle_set_exec_con)
{
  int num_args;
  const char* error=0;
  struct manager_exec* e;
  char* arg_p;
  if ((num_args=tokenize_args(args_start,&args_end))<2)
  {
    error="Too few arguments";
    goto err;
  }
  arg_p=args_start;
  pthread_mutex_lock(&lock_exec_hash);
  if (!(e=(struct manager_exec*)hash_search(&exec_hash,arg_p,
					    strlen(arg_p))))
  {
    pthread_mutex_unlock(&lock_exec_hash);
    error="Exec definition entry does not exist";
    goto err;
  }
  arg_p+=strlen(arg_p)+1;
  arg_p+=(strnmov(e->con_user,arg_p,sizeof(e->con_user))-e->con_user)+1;
  if (num_args >= 3)
  {
    arg_p+=(strnmov(e->con_host,arg_p,sizeof(e->con_host))-e->con_host)+1;
    if (num_args == 4)
    {
      if (!(e->con_port=atoi(arg_p)))
	strnmov(e->con_sock,arg_p,sizeof(e->con_sock));
      else
	e->con_sock[0]=0;
    }
    else if(num_args > 4)
    {
      pthread_mutex_unlock(&lock_exec_hash);
      error="Too many arguments";
      goto err;
    }
  }
  pthread_mutex_unlock(&lock_exec_hash);
  client_msg(thd->vio,MANAGER_OK,"Entry updated");
  return 0;
err:
  client_msg(thd->vio,MANAGER_CLIENT_ERR,error);
  return 1;
}

HANDLE_DECL(handle_set_exec_stdout)
{
  return set_exec_param(thd,args_start,args_end,PARAM_STDOUT);
}

HANDLE_DECL(handle_set_exec_stderr)
{
  return set_exec_param(thd,args_start,args_end,PARAM_STDERR);
}

static int set_exec_param(struct manager_thd* thd, char* args_start,
			  char* args_end, PARAM_TYPE param_type)
{
  int num_args;
  const char* error=0;
  struct manager_exec* e;
  char* arg_p;
  char* param;
  int param_size;
  
  if ((num_args=tokenize_args(args_start,&args_end))<2)
  {
    error="Too few arguments";
    goto err;
  }
  arg_p=args_start;
  pthread_mutex_lock(&lock_exec_hash);
  if (!(e=(struct manager_exec*)hash_search(&exec_hash,arg_p,
					    strlen(arg_p))))
  {
    pthread_mutex_unlock(&lock_exec_hash);
    error="Exec definition entry does not exist";
    goto err;
  }
  arg_p+=strlen(arg_p)+1;
  param_size=strlen(arg_p)+1;
  switch (param_type)
  {
  case PARAM_STDOUT:
    param=e->stdout_path;
    e->req_len+=(param_size-e->stdout_path_size);
    e->stdout_path_size=param_size;
    break;
  case PARAM_STDERR:
    param=e->stderr_path;
    e->req_len+=(param_size-e->stderr_path_size);
    e->stderr_path_size=param_size;
    break;
  default:
    error="Internal error";
    goto err;
  }
  strnmov(param,arg_p,FN_REFLEN);
  pthread_mutex_unlock(&lock_exec_hash);
  client_msg(thd->vio,MANAGER_OK,"Entry updated");
  return 0;
err:
  client_msg(thd->vio,MANAGER_CLIENT_ERR,error);
  return 1;
}


HANDLE_DECL(handle_start_exec)
{
  int num_args;
  struct manager_exec* e;
  int ident_len;
  const char* error=0;
  struct timespec t;
  if ((num_args=tokenize_args(args_start,&args_end))<1)
  {
    error="Too few arguments";
    goto err;
  }
  ident_len=strlen(args_start);
  pthread_mutex_lock(&lock_exec_hash);
  if (!(e=(struct manager_exec*)hash_search(&exec_hash,args_start,
					    ident_len)))
  {
    pthread_mutex_unlock(&lock_exec_hash);
    error="Exec definition entry does not exist";
    goto err;
  }
  pthread_mutex_unlock(&lock_exec_hash);
  manager_exec_launch(e);
  if ((error=e->error))
    goto err;
  pthread_mutex_lock(&e->lock);
  t.tv_sec=time(0)+(e->start_wait_timeout=atoi(args_start+ident_len+1));
  t.tv_nsec=0;
  if (!e->pid)
    pthread_cond_timedwait(&e->cond,&e->lock,&t);
  if (!e->pid)
  {
    pthread_mutex_unlock(&e->lock);
    error="Process failed to start withing alotted time";
    goto err;
  }
  mysql_close(&e->mysql);
  manager_exec_connect(e);
  error=e->error;
  pthread_mutex_unlock(&e->lock);
  if (error)
    goto err;
  client_msg(thd->vio,MANAGER_OK,"'%s' started",e->ident);
  return 0;
err:
  client_msg(thd->vio,MANAGER_CLIENT_ERR,error);
  return 1;
}

HANDLE_DECL(handle_stop_exec)
{
  int num_args;
  struct timespec abstime;
  struct manager_exec* e;
  int ident_len;
  const char* error=0;
  if ((num_args=tokenize_args(args_start,&args_end))<2)
  {
    error="Too few arguments";
    goto err;
  }
  ident_len=strlen(args_start);
  abstime.tv_sec=time(0)+atoi(args_start+1+ident_len);
  abstime.tv_nsec=0;
  pthread_mutex_lock(&lock_exec_hash);
  if (!(e=(struct manager_exec*)hash_search(&exec_hash,args_start,
					    ident_len)))
  {
    pthread_mutex_unlock(&lock_exec_hash);
    error="Exec definition entry does not exist";
    goto err;
  }
  pthread_mutex_unlock(&lock_exec_hash);
  pthread_mutex_lock(&e->lock);
  e->th=pthread_self();
  if (!e->pid)
  {
    e->th=0;
    pthread_mutex_unlock(&e->lock);
    error="Process not running";
    goto err;
  }
  if (mysql_shutdown(&e->mysql))
  {
    e->th=0;
    pthread_mutex_unlock(&e->lock);
    error="Could not send shutdown command";
    goto err;
  }
  if (e->pid)
    pthread_cond_timedwait(&e->cond,&e->lock,&abstime);
  if (e->pid)
    error="Process failed to terminate within alotted time";
  e->th=0;
  pthread_mutex_unlock(&e->lock);
  if (!error)
  {
    client_msg(thd->vio,MANAGER_OK,"'%s' terminated",e->ident);
    return 0;
  }
err:
  client_msg(thd->vio,MANAGER_CLIENT_ERR,error);
  return 1;
}

HANDLE_DECL(query)
{
  const char* error=0;
  struct manager_exec* e;
  MYSQL_RES* res=0;
  MYSQL_ROW row;
  MYSQL_FIELD* fields;
  int num_fields,i,ident_len;
  char* ident,*query;
  query=ident=args_start;
  while (!isspace(*query))
    query++;
  if (query == ident)
  {
    error="Missing server identifier";
    goto err;
  }
  ident_len=(int)(query-ident);
  while (query<args_end && isspace(*query))
    query++;
  if (query == args_end)
  {
    error="Missing query";
    goto err;
  }
  pthread_mutex_lock(&lock_exec_hash);
  if (!(e=(struct manager_exec*)hash_search(&exec_hash,ident,
					    ident_len)))
  {
    pthread_mutex_unlock(&lock_exec_hash);
    error="Exec definition entry does not exist";
    goto err;
  }
  pthread_mutex_unlock(&lock_exec_hash);
  pthread_mutex_lock(&e->lock);
  if (!e->pid)
  {
    error="Process is not running";
    pthread_mutex_unlock(&e->lock);
    goto err;
  }
  
  if (mysql_query(&e->mysql,query))
  {
    error=mysql_error(&e->mysql);
    pthread_mutex_unlock(&e->lock);
    goto err;
  }
  if ((res=mysql_store_result(&e->mysql)))
  {
    char buf[MAX_CLIENT_MSG_LEN],*p,*buf_end;
    fields=mysql_fetch_fields(res);
    num_fields=mysql_num_fields(res);
    p=buf;
    buf_end=buf+sizeof(buf);
    for (i=0;i<num_fields && p<buf_end-2;i++)
    {
      p=arg_strmov(p,fields[i].name,buf_end-p-2);
      *p++='\t';
    }
    *p=0;
    client_msg_pre(thd->vio,MANAGER_OK,buf);
    
    while ((row=mysql_fetch_row(res)))
    {
      p=buf;
      for (i=0;i<num_fields && p<buf_end-2;i++)
      {
	p=arg_strmov(p,row[i],buf_end-p-2);
	*p++='\t';
      }
      *p=0;
      client_msg_pre(thd->vio,MANAGER_OK,buf);
    }
  }
  pthread_mutex_unlock(&e->lock);
  client_msg(thd->vio,MANAGER_OK,"End");
  return 0;
err:
  client_msg(thd->vio,MANAGER_CLIENT_ERR,error);
  return 1;
}

HANDLE_DECL(handle_def_exec)
{
  struct manager_exec* e=0,*old_e;
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
  if ((old_e=(struct manager_exec*)hash_search(&exec_hash,(byte*)e->ident,
					       e->ident_len)))
  {
    strnmov(e->stdout_path,old_e->stdout_path,sizeof(e->stdout_path));
    strnmov(e->stderr_path,old_e->stderr_path,sizeof(e->stderr_path));
    strnmov(e->con_user,old_e->con_user,sizeof(e->con_user));
    strnmov(e->con_host,old_e->con_host,sizeof(e->con_host));
    strnmov(e->con_sock,old_e->con_sock,sizeof(e->con_sock));
    e->con_port=old_e->con_port;
    update_req_len(e);
    hash_delete(&exec_hash,(byte*)old_e);
  }
  hash_insert(&exec_hash,(byte*)e);
  pthread_mutex_unlock(&lock_exec_hash);
  client_msg(thd->vio,MANAGER_OK,"Exec definition created");
  return 0;
err:
  client_msg(thd->vio,MANAGER_CLIENT_ERR,error);
  if (e)
    manager_exec_free(e);
  return 1;
}

HANDLE_NOARG_DECL(handle_show_exec)
{
  uint i;
  client_msg_pre(thd->vio,MANAGER_INFO,"Exec_def\tPid\tExit_status\tCon_info\
\tStdout\tStderr\tArguments");
  pthread_mutex_lock(&lock_exec_hash);
  for (i=0;i<exec_hash.records;i++)
  {
    struct manager_exec* e=(struct manager_exec*)hash_element(&exec_hash,i);
    manager_exec_print(thd->vio,e);
  }
  pthread_mutex_unlock(&lock_exec_hash);
  client_msg(thd->vio,MANAGER_INFO,"End");
  return 0;
}

static struct manager_exec* manager_exec_by_pid(pid_t pid)
{
  struct manager_exec* e;
  uint i;
  pthread_mutex_lock(&lock_exec_hash);
  for (i=0;i<exec_hash.records;i++)
  {
    e=(struct manager_exec*)hash_element(&exec_hash,i);
    if (e->pid==pid)
    {
      pthread_mutex_unlock(&lock_exec_hash);
      return e;
    }
  }
 pthread_mutex_unlock(&lock_exec_hash);
 return 0;					 
}

static void manager_exec_connect(struct manager_exec* e)
{
  int i;
  int connect_retries;
  
  if (!(connect_retries=e->start_wait_timeout))
    connect_retries=manager_connect_retries;
  
  for (i=0;i<connect_retries;i++)
  {
    if (mysql_real_connect(&e->mysql,e->con_host,e->con_user,e->con_pass,0,
			   e->con_port,e->con_sock,0))
      return;
    sleep(1);
  }
  e->error="Could not connect to MySQL server withing the number of tries";
}

static int manager_exec_launch(struct manager_exec* e)
{
  if (one_thread)
  {
    pid_t tmp_pid;
    switch ((tmp_pid=fork()))
    {
    case -1:
      e->error="Cannot fork";
      return 1;
    case 0:
    {
      int err_code;
      close(manager_sock);
      err_code=execv(e->bin_path,e->args);
      exit(err_code);
    }
    default:
      e->pid=tmp_pid;
      manager_exec_connect(e);
      return 0;
    }
  }
  else
  {
    if (my_write(to_launcher_pipe[1],(byte*)&e->req_len,
		 sizeof(int),MYF(MY_NABP))||
	my_write(to_launcher_pipe[1],(byte*)&e->num_args,
		 sizeof(int),MYF(MY_NABP)) ||
	my_write(to_launcher_pipe[1],e->stdout_path,e->stdout_path_size,
		 MYF(MY_NABP)) ||
	my_write(to_launcher_pipe[1],e->stderr_path,e->stderr_path_size,
		 MYF(MY_NABP)) ||
	my_write(to_launcher_pipe[1],e->data_buf,e->data_buf_size,
		 MYF(MY_NABP)))
    {
      e->error="Failed write request to launcher";
      return 1;
    }
  }
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
  char buf[MAX_MYSQL_MANAGER_MSG];
  char* p=buf,*buf_end=buf+sizeof(buf)-1;
  char** args=e->args;
  
  p=arg_strmov(p,e->ident,(int)(buf_end-p)-1);
  *p++='\t';
  if (p>buf_end-15)
    goto end;
  p=int10_to_str(e->pid,p,10);
  *p++='\t';
  p=int10_to_str(e->exit_code,p,10);
  *p++='\t';
  
  p=arg_strmov(p,e->con_user,(int)(buf_end-p)-1);
  *p++='@';
  if (p==buf_end)
    goto end;
  p=arg_strmov(p,e->con_host,(int)(buf_end-p)-11);
  *p++=':';
  if (p==buf_end-10)
    goto end;
  if (e->con_sock[0])
  {
    p=arg_strmov(p,e->con_sock,(int)(buf_end-p)-1);
  }
  else
  {
    p=int10_to_str(e->con_port,p,10);
  }
  *p++='\t';
  p=arg_strmov(p,e->stdout_path,(int)(buf_end-p)-1);
  if (p==buf_end-1)
    goto end;
  *p++='\t';
  p=arg_strmov(p,e->stderr_path,(int)(buf_end-p)-1);
  if (p==buf_end-1)
    goto end;
  *p++='\t';
  
  for(;p<buf_end && *args;args++)
  {
    p=arg_strmov(p,*args,(int)(buf_end-p)-1);
    *p++='\t';
  }
end:  
  *p=0;
  client_msg_pre(vio,MANAGER_INFO,buf);
  return;
}

static int authenticate(struct manager_thd* thd)
{
  char* buf_end,*buf,*p,*p_end;
  my_MD5_CTX context;
  uchar digest[MD5_LEN];
  struct manager_user* u;
  char c;
  
  client_msg(thd->vio,MANAGER_INFO, manager_greeting);
  if (!(buf_end=read_line(thd)))
    return -1;
  for (buf=thd->cmd_buf,p=thd->user,p_end=p+MAX_USER_NAME;
       buf<buf_end && (c=*buf) && p<p_end; buf++,p++)
  {
    if (isspace(c))
    {
      *p=0;
      break;
    }
    else
      *p=c;
  }
  if (p==p_end || buf==buf_end)
    return 1;
  if (!(u=(struct manager_user*)hash_search(&user_hash,thd->user,
					    (uint)(p-thd->user))))
    return 1;
  for (;isspace(*buf) && buf<buf_end;buf++) /* empty */;
  
  my_MD5Init(&context);
  my_MD5Update(&context,buf,(uint)(buf_end-buf));
  my_MD5Final(digest,&context);
  if (memcmp(u->md5_pass,digest,MD5_LEN))
    return 1;
  client_msg(thd->vio,MANAGER_OK,"OK");
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

#define LOG_MSG_FUNC(type,TYPE) inline static void type  \
 (const char* fmt,...) { \
  va_list args; \
  va_start(args,fmt); \
  log_msg(fmt,TYPE,args);\
 }

LOG_MSG_FUNC(log_err,LOG_ERR)
LOG_MSG_FUNC(log_warn,LOG_WARN)
LOG_MSG_FUNC(log_info,LOG_INFO)

#ifndef DBUG_OFF
LOG_MSG_FUNC(log_debug,LOG_DEBUG)
#else
inline void log_debug(char* __attribute__((unused)) fmt,...) {}
#endif

static pthread_handler_decl(process_launcher_messages,
			    __attribute__((unused)) arg)
{
  my_thread_init();
  for (;!in_shutdown;)
  {
    pid_t pid;
    struct manager_exec* e;
    char buf[MAX_LAUNCHER_MSG];
    if (read(from_launcher_pipe[0],buf,MAX_LAUNCHER_MSG)<0)
    {
      log_err("error reading launcher message");
      sleep(1);
      continue;
    }
    switch (buf[0])
    {
    case CHILD_START:
    {
      char* ident=buf+1;
      int ident_len=strlen(ident);
      memcpy(&pid,ident+ident_len+1,sizeof(pid));
      log_debug("process message - ident=%s,ident_len=%d,pid=%d",ident,
		ident_len,pid);
      pthread_mutex_lock(&lock_exec_hash);
      log_debug("hash has %d records",exec_hash.records);
      e=(struct manager_exec*)hash_search(&exec_hash,ident,ident_len);
      if (e)
      {
	pthread_mutex_lock(&e->lock);
	e->pid=pid;
	pthread_cond_broadcast(&e->cond);
	pthread_mutex_unlock(&e->lock);
      }
      pthread_mutex_unlock(&lock_exec_hash);
      log_debug("unlocked mutex");
      break;
    }
    case CHILD_STOP:
      memcpy(&pid,buf+1,sizeof(pid));
      e=manager_exec_by_pid(pid);
      if (e)
      {
	pthread_mutex_lock(&e->lock);
	e->pid=0;
	memcpy(&e->exit_code,buf+1+sizeof(pid),sizeof(int));
	pthread_cond_broadcast(&e->cond);
	pthread_mutex_unlock(&e->lock);
      }
      break;
    default:
      log_err("Got invalid launcher message");
      break;
    }
  }
  return 0;
}

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
  return 0;					/* Don't get cc warning */
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
    log_err("Failed writing to client: errno=%d",errno);
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
      thd->fatal=1;
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
  client_msg(thd->vio,MANAGER_CLIENT_ERR,"Command line too long");
  return 0;
}

static void handle_child(int __attribute__((unused)) sig)
{
  pid_t child;
  int child_status;
  
  for(;(child=waitpid(-1,&child_status,WNOHANG))>0;)
  {
    char msg_buf[1+sizeof(int)+sizeof(int)];
    msg_buf[0]=CHILD_STOP;
    memcpy(msg_buf+1,&child,sizeof(int));
    memcpy(msg_buf+1+sizeof(int),&child_status,sizeof(int));
    if (write(from_launcher_pipe[1],msg_buf,sizeof(msg_buf))!=sizeof(msg_buf))
      log_err("launcher: error writing message on child exit"); 
  }
  signal(SIGCHLD,handle_child);
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
  hash_free(&exec_hash);
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
  -d, --one-thread         Use one thread ( for debugging)  \n\
  -C, --connect-retries    Number of attempts to establish MySQL connection \n\
  -m, --max-command-len    Maximum command length \n\
  -V, --version            Output version information and exit.\n\n");
}

static int parse_args(int argc, char **argv)
{
  int c, option_index = 0;
  while ((c=getopt_long(argc,argv,"P:?#:Vl:b:B:g:m:dC:p:",
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
      case 'p':
	manager_pw_file=optarg;
	break;
      case 'C':
        manager_connect_retries=atoi(optarg);
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
  int client_sock;
  uint len;
  Vio* vio;

  for (;!shutdown_requested;)
  {
    len=sizeof(struct sockaddr_in);
    if ((client_sock=accept(manager_sock,(struct sockaddr*)&manager_addr,
			    &len)) <0)
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
      client_msg(vio,MANAGER_ACCESS, "Access denied");
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
      client_msg(vio,MANAGER_INTERNAL_ERR,"Could not create thread, errno=%d",
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

static byte* get_user_key(const byte* u, uint* len,
			  my_bool __attribute__((unused)) t)
{
  register const char* key;
  key = ((struct manager_user*)u)->user;
  *len = ((struct manager_user*)u)->user_len;
  return (byte*)key;
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

static void update_req_len(struct manager_exec* e)
{
  e->req_len=e->data_buf_size+
    (e->stdout_path_size=strlen(e->stdout_path)+1)+
    (e->stderr_path_size=strlen(e->stderr_path)+1);
 }

static struct manager_exec* manager_exec_new(char* arg_start,char* arg_end)
{
  struct manager_exec* tmp;
  char* first_arg;
  uint arg_len,num_args;
  num_args=tokenize_args(arg_start,&arg_end);
  arg_len=(uint)(arg_end-arg_start)+1; /* include \0 terminator*/
  if (!(tmp=(struct manager_exec*)my_malloc(sizeof(*tmp)+arg_len+
					    sizeof(char*)*num_args,
					    MYF(MY_ZEROFILL))))
    return 0;
  if (num_args<2)
  {
    tmp->error="Too few arguments";
    return tmp;
  }
  tmp->data_buf=(char*)tmp+sizeof(*tmp);
  memcpy(tmp->data_buf,arg_start,arg_len);
  tmp->data_buf_size=arg_len;
  tmp->args=(char**)(tmp->data_buf+arg_len);
  tmp->num_args=num_args; 
  tmp->ident=tmp->data_buf;
  tmp->ident_len=strlen(tmp->ident);
  first_arg=tmp->ident+tmp->ident_len+1;
  init_arg_array(first_arg,tmp->args,num_args-1);
  strmov(tmp->con_user,"root");
  tmp->con_port=MYSQL_PORT;
  memcpy(tmp->con_host,"localhost",10);
  tmp->bin_path=tmp->args[0];
  tmp->stdout_path_size=tmp->stderr_path_size=1;
  tmp->req_len=tmp->data_buf_size+2;
  pthread_mutex_init(&tmp->lock,0);
  pthread_cond_init(&tmp->cond,0);
  mysql_init(&tmp->mysql);
  return tmp;
}

static void manager_exec_free(void* e)
{
  mysql_close(&((struct manager_exec*)e)->mysql);
  my_free(e,MYF(0));
}

static int hex_val(char c)
{
  if (isdigit(c))
    return c-'0';
  c=tolower(c);
  return c-'a'+10;
}

static struct manager_user* manager_user_new(char* buf)
{
  struct manager_user* tmp;
  char* p,*user_end,*p_end;
  char c;
  if (!(tmp=(struct manager_user*)my_malloc(sizeof(*tmp),MYF(0))))
    return 0;
  p=tmp->user;
  tmp->error=0;
  user_end=p+MAX_USER_NAME;
  for (;(c=*buf) && p<user_end;buf++)
  {
    if (c == ':')
    {
      *p=0;
      tmp->user_len=p-tmp->user;
      buf++;
      break;
    }
    else
      *p++=c;
  }
  if (!c)
    tmp->error="Missing ':'";
  if (p == user_end)
    tmp->error="Username too long";
  if (tmp->error)
    return tmp;
  if (strlen(buf) < 2*MD5_LEN)
  {
    tmp->error="Invalid MD5 sum, too short";
    return tmp;
  }
  p=tmp->md5_pass;
  p_end=p+MD5_LEN;
  for (; p<p_end;p++,buf+=2)
  {
    *p=hex_val(*buf)*16+hex_val(buf[1]);
  }
  
  return tmp;
}

static void manager_user_free(void* u)
{
  my_free((gptr)u,MYF(0));
}

static void init_user_hash()
{
  FILE* f;
  char buf[80];
  int line_num=1;
  if (hash_init(&user_hash,1024,0,0,get_user_key,manager_user_free,MYF(0)))
    die("Could not initialize user hash");
  if (!(f=fopen(manager_pw_file,"r")))
    die("Could not open password file '%s'", manager_pw_file);
  for (;;line_num++)
  {
    struct manager_user* u;
    if (!fgets(buf,sizeof(buf),f) || feof(f))
      break;
    if (buf[0] == '#')
      continue;
    if (!(u=manager_user_new(buf)))
      die("Out of memory while reading user line");
    if (u->error)
    {
      die("Error on line %d of '%s': %s",line_num,manager_pw_file, u->error);
    }
    else
    {
      hash_insert(&user_hash,(gptr)u);
    }
  }
  fclose(f);
}

static void init_globals()
{
  if (hash_init(&exec_hash,1024,0,0,get_exec_key,manager_exec_free,MYF(0)))
    die("Exec hash initialization failed");
  if (!one_thread)
  {
    fork_launcher();
    if (pthread_create(&launch_msg_th,0,process_launcher_messages,0))
      die("Could not start launcher message handler thread");
  }
  init_user_hash();
  loop_th=pthread_self();
  signal(SIGPIPE,handle_sigpipe);
}

static int open_and_dup(int fd,char* path)
{
  int old_fd;
  if ((old_fd=my_open(path,O_WRONLY|O_APPEND|O_CREAT,MYF(0)))<0)
  {
    log_err("Could not open '%s' for append, errno=%d",path,errno);
    return 1;
  }
  if (dup2(old_fd,fd)<0)
  {
    log_err("Failed in dup2(), errno=%d",errno);
    return 1;
  }
  my_close(old_fd,MYF(0));
  return 0;
}

static void run_launcher_loop()
{
  for (;;)
  {
    int req_len,ident_len,num_args;
    char* request_buf=0;
    pid_t pid;
    char* exec_path,*ident,*stdout_path,*stderr_path;
    char** args=0;
    
    if (my_read(to_launcher_pipe[0],(byte*)&req_len,
		sizeof(int),MYF(MY_NABP|MY_FULL_IO)) ||
	my_read(to_launcher_pipe[0],(byte*)&num_args,
		sizeof(int),MYF(MY_NABP|MY_FULL_IO)) ||
	!(request_buf=(char*)my_malloc(req_len+sizeof(pid)+2,MYF(0))) ||
	!(args=(char**)my_malloc(num_args*sizeof(char*),MYF(0))) ||
	my_read(to_launcher_pipe[0],request_buf,req_len,
		MYF(MY_NABP|MY_FULL_IO)))
    {
      log_err("launcher: Error reading request");
      my_free((gptr)request_buf,MYF(MY_ALLOW_ZERO_PTR));
      my_free((gptr)args,MYF(MY_ALLOW_ZERO_PTR));
      sleep(1);
      continue;
    }
    stdout_path=request_buf;
    stderr_path=stdout_path+strlen(stdout_path)+1;
    request_buf=stderr_path+strlen(stderr_path); /* black magic */
    ident=request_buf+1;
    ident_len=strlen(ident);
    exec_path=ident+ident_len+1;
    log_debug("num_args=%d,req_len=%d,ident=%s,ident_len=%d,exec_path=%s,\
stdout_path=%s,stderr_path=%s",
	      num_args,
	      req_len,ident,ident_len,exec_path,stdout_path,stderr_path);
    init_arg_array(exec_path,args,num_args-1);    
        
    switch ((pid=fork()))
    {
    case -1:
      log_err("launcher: cannot fork");
      sleep(1);
      break;
    case 0:
      if (open_and_dup(1,stdout_path) || open_and_dup(2,stderr_path))
	exit(1);
      if (execv(exec_path,args))
	log_err("launcher: cannot exec %s",exec_path);
      exit(1);
    default:
      request_buf[0]=CHILD_START;
      memcpy(request_buf+ident_len+2,&pid,sizeof(pid));
      if (write(from_launcher_pipe[1],request_buf,ident_len+2+sizeof(pid))<0)
	log_err("launcher: error sending launch status report");
      break;
    }
    my_free((gptr)(stdout_path),MYF(0));
    my_free((gptr)args,MYF(0));
  }
}

static void fork_launcher()
{
  if (pipe(to_launcher_pipe) || pipe(from_launcher_pipe))
    die("Could not create launcher pipes");
  switch ((launcher_pid=fork()))
  {
  case 0:
    signal(SIGCHLD,handle_child);
    run_launcher_loop();
    exit(0);
  case -1: die("Could not fork the launcher");
  default: return;
  }
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
  char c;
  stack_bottom= (uchar *) &c;
  MY_INIT(argv[0]);
  errfp = stderr;
  parse_args(argc,argv);
  pthread_mutex_init(&lock_log,0);
  pthread_mutex_init(&lock_shutdown,0);
  pthread_mutex_init(&lock_exec_hash,0);
  pthread_mutex_init(&lock_launch_thd,0);
  pthread_cond_init(&cond_launch_thd,0);
#ifdef DO_STACKTRACE
  signal(SIGSEGV,handle_segfault);
#endif
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









