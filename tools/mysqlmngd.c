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

#define MNGD_VERSION "1.0"
#define MNGD_GREETING "MySQL Server Management Daemon v.1.0"

#define LOG_ERR  1
#define LOG_WARN 2
#define LOG_INFO 3
#define LOG_DEBUG 4

#ifndef MNGD_PORT
#define MNGD_PORT  23546
#endif

#ifndef MNGD_MAX_CMD_LEN
#define MNGD_MAX_CMD_LEN 16384
#endif

#ifndef MNGD_LOG_FILE
#define MNGD_LOG_FILE "/var/log/mysqlmngd.log"
#endif

#ifndef MNGD_BACK_LOG
#define MNGD_BACK_LOG 50
#endif

#ifndef MAX_USER_NAME
#define MAX_USER_NAME 16
#endif

/* Variable naming convention - if starts with mngd_, either is set
   directly by the user, or used closely in ocnjunction with a variable
   set by the user
*/

uint mngd_port = MNGD_PORT;
FILE* errfp;
const char* mngd_log_file = MNGD_LOG_FILE;
pthread_mutex_t lock_log, lock_shutdown;
int mngd_sock = -1;
struct sockaddr_in mngd_addr;
ulong mngd_bind_addr = INADDR_ANY;
int mngd_back_log = MNGD_BACK_LOG;
int in_shutdown = 0, shutdown_requested=0;
const char* mngd_greeting = MNGD_GREETING;
uint mngd_max_cmd_len = MNGD_MAX_CMD_LEN;

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

struct mngd_thd
{
  Vio* vio;
  char user[MAX_USER_NAME];
  int priv_flags;
  char* cmd_buf;
  int fatal,finished;
};

struct mngd_thd* mngd_thd_new(Vio* vio);
void mngd_thd_free(struct mngd_thd* thd);

typedef int (*mngd_cmd_handler)(struct mngd_thd*,char*,char*);


struct mngd_cmd
{
  const char* name;
  const char* help;
  mngd_cmd_handler handler_func;
  int len;
};

#define HANDLE_DECL(com) static int handle_ ## com (struct mngd_thd* thd,\
 char* args_start,char* args_end)

#define HANDLE_NOARG_DECL(com) static int handle_ ## com \
  (struct mngd_thd* thd, char* __attribute__((unused)) args_start,\
 char* __attribute__((unused)) args_end)


HANDLE_NOARG_DECL(ping);
HANDLE_NOARG_DECL(quit);
HANDLE_NOARG_DECL(help);
HANDLE_NOARG_DECL(shutdown);

struct mngd_cmd commands[] =
{
  {"ping", "Check if this server is alive", handle_ping,4},
  {"quit", "Finish session", handle_quit,4},
  {"shutdown", "Shutdown this server", handle_shutdown,8},
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
  {"version", no_argument, 0, 'V'},
  {0, 0, 0, 0}
};

static void die(const char* fmt,...);
static void print_time(FILE* fp);
static void clean_up();
static struct mngd_cmd* lookup_cmd(char* s,int len);
static void client_msg(Vio* vio,int err_code,const char* fmt,...);
static void client_msg_pre(Vio* vio,int err_code,const char* fmt,...);
static void client_msg_raw(Vio* vio,int err_code,int pre,const char* fmt,
			    va_list args);
static int authenticate(struct mngd_thd* thd);
static char* read_line(struct mngd_thd* thd); /* returns pointer to end of
						 line
					      */
static pthread_handler_decl(process_connection,arg);
static int exec_line(struct mngd_thd* thd,char* buf,char* buf_end);

static int exec_line(struct mngd_thd* thd,char* buf,char* buf_end)
{
  char* p=buf;
  struct mngd_cmd* cmd;
  for (;p<buf_end && !isspace(*p);p++)
    *p=tolower(*p);
  if (!(cmd=lookup_cmd(buf,(int)(p-buf))))
  {
    client_msg(thd->vio,MSG_CLIENT_ERR,
	       "Unrecognized command, type help to see list of supported\
 commands");
    return 1;
  }
  return cmd->handler_func(thd,p,buf_end);
}

static struct mngd_cmd* lookup_cmd(char* s,int len)
{
  struct mngd_cmd* cmd = commands;
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
  struct mngd_cmd* cmd = commands;
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

static int authenticate(struct mngd_thd* thd)
{
  char* buf_end;
  client_msg(thd->vio,MSG_INFO, mngd_greeting);
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
#endif

static pthread_handler_decl(process_connection,arg)
{
  struct mngd_thd* thd = (struct mngd_thd*)arg;
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
  mngd_thd_free(thd);
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

static char* read_line(struct mngd_thd* thd)
{
  char* p=thd->cmd_buf;
  char* buf_end = thd->cmd_buf + mngd_max_cmd_len;
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

struct mngd_thd* mngd_thd_new(Vio* vio)
{
  struct mngd_thd* tmp;
  if (!(tmp=(struct mngd_thd*)my_malloc(sizeof(*tmp)+mngd_max_cmd_len,
					MYF(0))))
  {
    log_err("Out of memory in mngd_thd_new");
    return 0;
  }
  tmp->vio=vio;
  tmp->user[0]=0;
  tmp->priv_flags=0;
  tmp->fatal=tmp->finished=0;
  tmp->cmd_buf=(char*)tmp+sizeof(*tmp);
  return tmp;
}

void mngd_thd_free(struct mngd_thd* thd)
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
  if (mngd_sock)
    close(mngd_sock);
  log_info("Ended");
  if (errfp != stderr)
    fclose(errfp);
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,MNGD_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

void usage()
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

int parse_args(int argc, char **argv)
{
  int c, option_index = 0;
  while ((c=getopt_long(argc,argv,"P:?#:Vl:b:B:g:m:",
			long_options,&option_index)) != EOF)
  {
    switch (c)
      {
      case '#':
	DBUG_PUSH(optarg ? optarg : "d:t:O,/tmp/mysqlmgrd.trace");
	break;
      case 'P':
	mngd_port=atoi(optarg);
	break;
      case 'm':
	mngd_max_cmd_len=atoi(optarg);
	break;
      case 'g':
	mngd_greeting=optarg;
      case 'b':
	mngd_bind_addr = inet_addr(optarg);
	break;
      case 'B':
	mngd_back_log = atoi(optarg);
	break;
      case 'l':
	mngd_log_file=optarg;
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

int init_server()
{
  int arg=1;
  log_info("Started");
  if ((mngd_sock=socket(PF_INET,SOCK_STREAM,0)) < 0)
    die("Could not create socket");
  bzero((char*)&mngd_addr, sizeof(mngd_addr));
  mngd_addr.sin_family = AF_INET;
  mngd_addr.sin_addr.s_addr = mngd_bind_addr;
  mngd_addr.sin_port = htons(mngd_port);
  setsockopt(mngd_sock,SOL_SOCKET, SO_REUSEADDR,(char*)&arg,sizeof(arg));
  if (bind(mngd_sock,(struct sockaddr*)&mngd_addr, sizeof(mngd_addr)) < 0)
    die("Could not bind");
  if (listen(mngd_sock,mngd_back_log) < 0)
    die("Could not listen");

  return 0;
}

int run_server_loop()
{
  pthread_t th;
  struct mngd_thd *thd;
  int client_sock,len;
  Vio* vio;

  for (;!shutdown_requested;)
  {
    len=sizeof(struct sockaddr_in);
    if ((client_sock=accept(mngd_sock,(struct sockaddr*)&mngd_addr,&len))<0)
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
    if (!(thd=mngd_thd_new(vio)))
    {
      log_err("Could not create thread object");
      vio_close(vio);
      continue;
    }
    
    if (authenticate(thd))
    {
      client_msg(vio,MSG_ACCESS, "Access denied");
      mngd_thd_free(thd);
      continue;
    }
    if (shutdown_requested)
      break;
    if (pthread_create(&th,0,process_connection,(void*)thd))
    {
      client_msg(vio,MSG_INTERNAL_ERR,"Could not create thread, errno=%d",
		 errno);
      mngd_thd_free(thd);
      continue;
    }
  }
  return 0;
}

FILE* open_log_stream()
{
  FILE* fp;
  if (!(fp=fopen(mngd_log_file,"a")))
    die("Could not open log file '%s'", mngd_log_file);
  return fp;
}

int daemonize()
{
  switch (fork())
    {
    case -1:
      die("Cannot fork");
    case 0:
      errfp = open_log_stream();
      close(0);
      close(1);
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
  return daemonize();
}



