/* Copyright (C) 2000-2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation.

   There are special exceptions to the terms and conditions of the GPL as it
   is applied to this software. View the full text of the exception in file
   EXCEPTIONS-CLIENT in the directory of this software distribution.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#if defined(__WIN__) || defined(_WIN32) || defined(_WIN64)
#include <winsock.h>
#include <odbcinst.h>
#endif
#include <my_sys.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "errmsg.h"
#include <violite.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#ifdef	 HAVE_PWD_H
#include <pwd.h>
#endif
#if !defined(MSDOS) && !defined(__WIN__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SELECT_H
#include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#endif /* !defined(MSDOS) && !defined(__WIN__) */
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#if defined(THREAD) && !defined(__WIN__)
#include <my_pthread.h>				/* because of signal()	*/
#endif
#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif

#undef net_buffer_length
#undef max_allowed_packet
static my_bool	mysql_client_init=0;
uint		mysql_port=0;
my_string	mysql_unix_port=0;
ulong 		net_buffer_length=8192;
ulong		max_allowed_packet= 1024L*1024L*1024L;
ulong		net_read_timeout=  CLIENT_NET_READ_TIMEOUT;
ulong		net_write_timeout= CLIENT_NET_WRITE_TIMEOUT;

#define CLIENT_CAPABILITIES	(CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_TRANSACTIONS)

#ifdef __WIN__
#define CONNECT_TIMEOUT 20
#else
#define CONNECT_TIMEOUT 0
#endif

#if defined(MSDOS) || defined(__WIN__)
/* socket_errno is defined in my_global.h for all platforms */
#define perror(A)
#else
#include <errno.h>
#define SOCKET_ERROR -1
#endif /* __WIN__ */

static MYSQL_DATA *read_rows (MYSQL *mysql,MYSQL_FIELD *fields,
			      uint field_count);
static int read_one_row(MYSQL *mysql,uint fields,MYSQL_ROW row,
			ulong *lengths);
static void end_server(MYSQL *mysql);
static void read_user_name(char *name);
static void append_wild(char *to,char *end,const char *wild);
static my_bool mysql_reconnect(MYSQL *mysql);
static int send_file_to_server(MYSQL *mysql,const char *filename);
static sig_handler pipe_sig_handler(int sig);
static ulong mysql_sub_escape_string(CHARSET_INFO *charset_info, char *to,
				     const char *from, ulong length);

static my_bool org_my_init_done=0;

int STDCALL mysql_server_init(int argc __attribute__((unused)),
			      char **argv __attribute__((unused)),
			      char **groups __attribute__((unused)))
{
  return (int) mysql_once_init();
}

void STDCALL mysql_server_end()
{
  /* If library called my_init(), free memory allocated by it */
  if (!org_my_init_done)
  {
    my_end(0);
#ifndef THREAD
  /* Remove TRACING, if enabled by mysql_debug() */
    DBUG_POP();
#endif
  }
  else
    mysql_thread_end();
  mysql_client_init= org_my_init_done= 0;
}

static MYSQL_PARAMETERS mysql_internal_parameters=
{&max_allowed_packet, &net_buffer_length};

MYSQL_PARAMETERS *STDCALL mysql_get_parameters(void)
{
  return &mysql_internal_parameters;
}

my_bool STDCALL mysql_thread_init()
{
#ifdef THREAD
    return my_thread_init();
#else
    return 0;
#endif
}

void STDCALL mysql_thread_end()
{
#ifdef THREAD
    my_thread_end();
#endif
}

/*
  Let the user specify that we don't want SIGPIPE;  This doesn't however work
  with threaded applications as we can have multiple read in progress.
*/

#if !defined(__WIN__) && defined(SIGPIPE) && !defined(THREAD)
#define init_sigpipe_variables  sig_return old_signal_handler=(sig_return) 0;
#define set_sigpipe(mysql)     if ((mysql)->client_flag & CLIENT_IGNORE_SIGPIPE) old_signal_handler=signal(SIGPIPE,pipe_sig_handler)
#define reset_sigpipe(mysql) if ((mysql)->client_flag & CLIENT_IGNORE_SIGPIPE) signal(SIGPIPE,old_signal_handler);
#else
#define init_sigpipe_variables
#define set_sigpipe(mysql)
#define reset_sigpipe(mysql)
#endif

static MYSQL* spawn_init(MYSQL* parent, const char* host,
			 unsigned int port,
			 const char* user,
			 const char* passwd);

#if !(defined(__WIN__) || defined(OS2) || defined(__NETWARE__))
static int wait_for_data(my_socket fd, uint timeout);
#endif

/****************************************************************************
  A modified version of connect().  my_connect() allows you to specify
  a timeout value, in seconds, that we should wait until we
  derermine we can't connect to a particular host.  If timeout is 0,
  my_connect() will behave exactly like connect().

  Base version coded by Steve Bernacki, Jr. <steve@navinet.net>
*****************************************************************************/

int my_connect(my_socket fd, const struct sockaddr *name, uint namelen,
	       uint timeout)
{
#if defined(__WIN__) || defined(OS2) || defined(__NETWARE__)
  return connect(fd, (struct sockaddr*) name, namelen);
#else
  int flags, res, s_err;

  /*
    If they passed us a timeout of zero, we should behave
    exactly like the normal connect() call does.
  */

  if (timeout == 0)
    return connect(fd, (struct sockaddr*) name, namelen);

  flags = fcntl(fd, F_GETFL, 0);	  /* Set socket to not block */
#ifdef O_NONBLOCK
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);  /* and save the flags..  */
#endif

  res= connect(fd, (struct sockaddr*) name, namelen);
  s_err= errno;			/* Save the error... */
  fcntl(fd, F_SETFL, flags);
  if ((res != 0) && (s_err != EINPROGRESS))
  {
    errno= s_err;			/* Restore it */
    return(-1);
  }
  if (res == 0)				/* Connected quickly! */
    return(0);
  return wait_for_data(fd, timeout);
#endif
}


/*
  Wait up to timeout seconds for a connection to be established.

  We prefer to do this with poll() as there is no limitations with this.
  If not, we will use select()
*/

#if !(defined(__WIN__) || defined(OS2) || defined(__NETWARE__))

static int wait_for_data(my_socket fd, uint timeout)
{
#ifdef HAVE_POLL
  struct pollfd ufds;
  int res;

  ufds.fd= fd;
  ufds.events= POLLIN | POLLPRI;
  if (!(res= poll(&ufds, 1, (int) timeout*1000)))
  {
    errno= EINTR;
    return -1;
  }
  if (res < 0 || !(ufds.revents & (POLLIN | POLLPRI)))
    return -1;
  return 0;
#else
  SOCKOPT_OPTLEN_TYPE s_err_size = sizeof(uint);
  fd_set sfds;
  struct timeval tv;
  time_t start_time, now_time;
  int res, s_err;

  if (fd >= FD_SETSIZE)				/* Check if wrong error */
    return 0;					/* Can't use timeout */

  /*
    Our connection is "in progress."  We can use the select() call to wait
    up to a specified period of time for the connection to suceed.
    If select() returns 0 (after waiting howevermany seconds), our socket
    never became writable (host is probably unreachable.)  Otherwise, if
    select() returns 1, then one of two conditions exist:
   
    1. An error occured.  We use getsockopt() to check for this.
    2. The connection was set up sucessfully: getsockopt() will
    return 0 as an error.
   
    Thanks goes to Andrew Gierth <andrew@erlenstar.demon.co.uk>
    who posted this method of timing out a connect() in
    comp.unix.programmer on August 15th, 1997.
  */

  FD_ZERO(&sfds);
  FD_SET(fd, &sfds);
  /*
    select could be interrupted by a signal, and if it is, 
    the timeout should be adjusted and the select restarted
    to work around OSes that don't restart select and 
    implementations of select that don't adjust tv upon
    failure to reflect the time remaining
   */
  start_time = time(NULL);
  for (;;)
  {
    tv.tv_sec = (long) timeout;
    tv.tv_usec = 0;
#if defined(HPUX10) && defined(THREAD)
    if ((res = select(fd+1, NULL, (int*) &sfds, NULL, &tv)) > 0)
      break;
#else
    if ((res = select(fd+1, NULL, &sfds, NULL, &tv)) > 0)
      break;
#endif
    if (res == 0)					/* timeout */
      return -1;
    now_time=time(NULL);
    timeout-= (uint) (now_time - start_time);
    if (errno != EINTR || (int) timeout <= 0)
      return -1;
  }

  /*
    select() returned something more interesting than zero, let's
    see if we have any errors.  If the next two statements pass,
    we've got an open socket!
  */

  s_err=0;
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0)
    return(-1);

  if (s_err)
  {						/* getsockopt could succeed */
    errno = s_err;
    return(-1);					/* but return an error... */
  }
  return (0);					/* ok */
#endif /* HAVE_POLL */
}
#endif /* defined(__WIN__) || defined(OS2) || defined(__NETWARE__) */

/*
  Create a named pipe connection
*/

#ifdef __WIN__

HANDLE create_named_pipe(NET *net, uint connect_timeout, char **arg_host,
			 char **arg_unix_socket)
{
  HANDLE hPipe=INVALID_HANDLE_VALUE;
  char pipe_name[1024];
  DWORD dwMode;
  int i;
  my_bool testing_named_pipes=0;
  char *host= *arg_host, *unix_socket= *arg_unix_socket;

  if ( ! unix_socket || (unix_socket)[0] == 0x00)
    unix_socket = mysql_unix_port;
  if (!host || !strcmp(host,LOCAL_HOST))
    host=LOCAL_HOST_NAMEDPIPE;

  
  pipe_name[sizeof(pipe_name)-1]= 0;		/* Safety if too long string */
  strxnmov(pipe_name, sizeof(pipe_name)-1, "\\\\", host, "\\pipe\\",
	   unix_socket, NullS);
  DBUG_PRINT("info",("Server name: '%s'.  Named Pipe: %s", host, unix_socket));

  for (i=0 ; i < 100 ; i++)			/* Don't retry forever */
  {
    if ((hPipe = CreateFile(pipe_name,
			    GENERIC_READ | GENERIC_WRITE,
			    0,
			    NULL,
			    OPEN_EXISTING,
			    0,
			    NULL )) != INVALID_HANDLE_VALUE)
      break;
    if (GetLastError() != ERROR_PIPE_BUSY)
    {
      net->last_errno=CR_NAMEDPIPEOPEN_ERROR;
      sprintf(net->last_error,ER(net->last_errno),host, unix_socket,
	      (ulong) GetLastError());
      return INVALID_HANDLE_VALUE;
    }
    /* wait for for an other instance */
    if (! WaitNamedPipe(pipe_name, connect_timeout*1000) )
    {
      net->last_errno=CR_NAMEDPIPEWAIT_ERROR;
      sprintf(net->last_error,ER(net->last_errno),host, unix_socket,
	      (ulong) GetLastError());
      return INVALID_HANDLE_VALUE;
    }
  }
  if (hPipe == INVALID_HANDLE_VALUE)
  {
    net->last_errno=CR_NAMEDPIPEOPEN_ERROR;
    sprintf(net->last_error,ER(net->last_errno),host, unix_socket,
	    (ulong) GetLastError());
    return INVALID_HANDLE_VALUE;
  }
  dwMode = PIPE_READMODE_BYTE | PIPE_WAIT;
  if ( !SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL) )
  {
    CloseHandle( hPipe );
    net->last_errno=CR_NAMEDPIPESETSTATE_ERROR;
    sprintf(net->last_error,ER(net->last_errno),host, unix_socket,
	    (ulong) GetLastError());
    return INVALID_HANDLE_VALUE;
  }
  *arg_host=host ; *arg_unix_socket=unix_socket;	/* connect arg */
  return (hPipe);
}
#endif


/*****************************************************************************
  read a packet from server. Give error message if socket was down
  or packet is an error message
*****************************************************************************/

ulong
net_safe_read(MYSQL *mysql)
{
  NET *net= &mysql->net;
  ulong len=0;
  init_sigpipe_variables

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(mysql);
  if (net->vio != 0)
    len=my_net_read(net);
  reset_sigpipe(mysql);

  if (len == packet_error || len == 0)
  {
    DBUG_PRINT("error",("Wrong connection or packet. fd: %s  len: %d",
			vio_description(net->vio),len));
    end_server(mysql);
    net->last_errno=(net->last_errno == ER_NET_PACKET_TOO_LARGE ?
		     CR_NET_PACKET_TOO_LARGE:
		     CR_SERVER_LOST);
    strmov(net->last_error,ER(net->last_errno));
    return (packet_error);
  }
  if (net->read_pos[0] == 255)
  {
    if (len > 3)
    {
      char *pos=(char*) net->read_pos+1;
      net->last_errno=uint2korr(pos);
      pos+=2;
      len-=2;
      (void) strmake(net->last_error,(char*) pos,
		     min((uint) len,(uint) sizeof(net->last_error)-1));
    }
    else
    {
      net->last_errno=CR_UNKNOWN_ERROR;
      (void) strmov(net->last_error,ER(net->last_errno));
    }
    DBUG_PRINT("error",("Got error: %d (%s)", net->last_errno,
			net->last_error));
    return(packet_error);
  }
  return len;
}


/* Get the length of next field. Change parameter to point at fieldstart */
static ulong
net_field_length(uchar **packet)
{
  reg1 uchar *pos= *packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (ulong) *pos;
  }
  if (*pos == 251)
  {
    (*packet)++;
    return NULL_LENGTH;
  }
  if (*pos == 252)
  {
    (*packet)+=3;
    return (ulong) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+=4;
    return (ulong) uint3korr(pos+1);
  }
  (*packet)+=9;					/* Must be 254 when here */
  return (ulong) uint4korr(pos+1);
}

/* Same as above, but returns ulonglong values */

static my_ulonglong
net_field_length_ll(uchar **packet)
{
  reg1 uchar *pos= *packet;
  if (*pos < 251)
  {
    (*packet)++;
    return (my_ulonglong) *pos;
  }
  if (*pos == 251)
  {
    (*packet)++;
    return (my_ulonglong) NULL_LENGTH;
  }
  if (*pos == 252)
  {
    (*packet)+=3;
    return (my_ulonglong) uint2korr(pos+1);
  }
  if (*pos == 253)
  {
    (*packet)+=4;
    return (my_ulonglong) uint3korr(pos+1);
  }
  (*packet)+=9;					/* Must be 254 when here */
#ifdef NO_CLIENT_LONGLONG
  return (my_ulonglong) uint4korr(pos+1);
#else
  return (my_ulonglong) uint8korr(pos+1);
#endif
}


static void free_rows(MYSQL_DATA *cur)
{
  if (cur)
  {
    free_root(&cur->alloc,MYF(0));
    my_free((gptr) cur,MYF(0));
  }
}


int
simple_command(MYSQL *mysql,enum enum_server_command command, const char *arg,
	       ulong length, my_bool skipp_check)
{
  NET *net= &mysql->net;
  int result= -1;
  init_sigpipe_variables

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(mysql);
  if (mysql->net.vio == 0)
  {						/* Do reconnect if possible */
    if (mysql_reconnect(mysql))
      goto end;
  }
  if (mysql->status != MYSQL_STATUS_READY)
  {
    strmov(net->last_error,ER(mysql->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    goto end;
  }

  mysql->net.last_error[0]=0;
  mysql->net.last_errno=0;
  mysql->info=0;
  mysql->affected_rows= ~(my_ulonglong) 0;
  net_clear(net);			/* Clear receive buffer */
  if (!arg)
    arg="";

  if (net_write_command(net,(uchar) command,arg,
			length ? length : (ulong) strlen(arg)))
  {
    DBUG_PRINT("error",("Can't send command to server. Error: %d",
			socket_errno));
    if (net->last_errno == ER_NET_PACKET_TOO_LARGE)
    {
      net->last_errno=CR_NET_PACKET_TOO_LARGE;
      strmov(net->last_error,ER(net->last_errno));
      goto end;
    }
    end_server(mysql);
    if (mysql_reconnect(mysql))
      goto end;
    if (net_write_command(net,(uchar) command,arg,
			  length ? length : (ulong) strlen(arg)))
    {
      net->last_errno=CR_SERVER_GONE_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto end;
    }
  }
  result=0;
  if (!skipp_check)
    result= ((mysql->packet_length=net_safe_read(mysql)) == packet_error ?
	     -1 : 0);
 end:
  reset_sigpipe(mysql);
  return result;
}


static void free_old_query(MYSQL *mysql)
{
  DBUG_ENTER("free_old_query");
  if (mysql->fields)
    free_root(&mysql->field_alloc,MYF(0));
  init_alloc_root(&mysql->field_alloc,8192,0);	/* Assume rowlength < 8192 */
  mysql->fields=0;
  mysql->field_count=0;				/* For API */
  DBUG_VOID_RETURN;
}

#if defined(HAVE_GETPWUID) && defined(NO_GETPWUID_DECL)
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif


#if defined(__NETWARE__)
/* Default to value of USER on NetWare, if unset use "UNKNOWN_USER" */
static void read_user_name(char *name)
{
  char *str=getenv("USER");
  strmake(name, str ? str : "UNKNOWN_USER", USERNAME_LENGTH);
}

#elif !defined(MSDOS) && ! defined(VMS) && !defined(__WIN__) && !defined(OS2)

static void read_user_name(char *name)
{
  DBUG_ENTER("read_user_name");
  if (geteuid() == 0)
    (void) strmov(name,"root");		/* allow use of surun */
  else
  {
#ifdef HAVE_GETPWUID
    struct passwd *skr;
    const char *str;
    if ((str=getlogin()) == NULL)
    {
      if ((skr=getpwuid(geteuid())) != NULL)
	str=skr->pw_name;
      else if (!(str=getenv("USER")) && !(str=getenv("LOGNAME")) &&
	       !(str=getenv("LOGIN")))
	str="UNKNOWN_USER";
    }
    (void) strmake(name,str,USERNAME_LENGTH);
#elif HAVE_CUSERID
    (void) cuserid(name);
#else
    strmov(name,"UNKNOWN_USER");
#endif
  }
  DBUG_VOID_RETURN;
}

#else /* If MSDOS || VMS */

static void read_user_name(char *name)
{
  char *str=getenv("USER");		/* ODBC will send user variable */
  strmake(name,str ? str : "ODBC", USERNAME_LENGTH);
}

#endif

#ifdef __WIN__
static my_bool is_NT(void)
{
  char *os=getenv("OS");
  return (os && !strcmp(os, "Windows_NT")) ? 1 : 0;
}
#endif

/*
  Expand wildcard to a sql string
*/

static void
append_wild(char *to, char *end, const char *wild)
{
  end-=5;					/* Some extra */
  if (wild && wild[0])
  {
    to=strmov(to," like '");
    while (*wild && to < end)
    {
      if (*wild == '\\' || *wild == '\'')
	*to++='\\';
      *to++= *wild++;
    }
    if (*wild)					/* Too small buffer */
      *to++='%';				/* Nicer this way */
    to[0]='\'';
    to[1]=0;
  }
}



/**************************************************************************
  Init debugging if MYSQL_DEBUG environment variable is found
**************************************************************************/

void STDCALL
mysql_debug(const char *debug __attribute__((unused)))
{
#ifndef DBUG_OFF
  char	*env;
  if (_db_on_)
    return;					/* Already using debugging */
  if (debug)
  {
    DEBUGGER_ON;
    DBUG_PUSH(debug);
  }
  else if ((env = getenv("MYSQL_DEBUG")))
  {
    DEBUGGER_ON;
    DBUG_PUSH(env);
#if !defined(_WINVER) && !defined(WINVER)
    puts("\n-------------------------------------------------------");
    puts("MYSQL_DEBUG found. libmysql started with the following:");
    puts(env);
    puts("-------------------------------------------------------\n");
#else
    {
      char buff[80];
      buff[sizeof(buff)-1]= 0;
      strxnmov(buff,sizeof(buff)-1,"libmysql: ", env, NullS);
      MessageBox((HWND) 0,"Debugging variable MYSQL_DEBUG used",buff,MB_OK);
    }
#endif
  }
#endif
}


/**************************************************************************
  Close the server connection if we get a SIGPIPE
   ARGSUSED
**************************************************************************/

static sig_handler
pipe_sig_handler(int sig __attribute__((unused)))
{
  DBUG_PRINT("info",("Hit by signal %d",sig));
#ifdef DONT_REMEMBER_SIGNAL
  (void) signal(SIGPIPE,pipe_sig_handler);
#endif
}


/**************************************************************************
  Shut down connection
**************************************************************************/

static void
end_server(MYSQL *mysql)
{
  DBUG_ENTER("end_server");
  if (mysql->net.vio != 0)
  {
    init_sigpipe_variables
    DBUG_PRINT("info",("Net: %s", vio_description(mysql->net.vio)));
    set_sigpipe(mysql);
    vio_delete(mysql->net.vio);
    reset_sigpipe(mysql);
    mysql->net.vio= 0;          /* Marker */
  }
  net_end(&mysql->net);
  free_old_query(mysql);
  DBUG_VOID_RETURN;
}


void STDCALL
mysql_free_result(MYSQL_RES *result)
{
  DBUG_ENTER("mysql_free_result");
  DBUG_PRINT("enter",("mysql_res: %lx",result));
  if (result)
  {
    if (result->handle && result->handle->status == MYSQL_STATUS_USE_RESULT)
    {
      DBUG_PRINT("warning",("Not all rows in set were read; Ignoring rows"));
      for (;;)
      {
	ulong pkt_len;
	if ((pkt_len=net_safe_read(result->handle)) == packet_error)
	  break;
	if (pkt_len == 1 && result->handle->net.read_pos[0] == 254)
	  break;				/* End of data */
      }
      result->handle->status=MYSQL_STATUS_READY;
    }
    free_rows(result->data);
    if (result->fields)
      free_root(&result->field_alloc,MYF(0));
    if (result->row)
      my_free((gptr) result->row,MYF(0));
    my_free((gptr) result,MYF(0));
  }
  DBUG_VOID_RETURN;
}


/****************************************************************************
  Get options from my.cnf
****************************************************************************/

static const char *default_options[]=
{
  "port","socket","compress","password","pipe", "timeout", "user",
  "init-command", "host", "database", "debug", "return-found-rows",
  "ssl-key" ,"ssl-cert" ,"ssl-ca" ,"ssl-capath",
  "character-sets-dir", "default-character-set", "interactive-timeout",
  "connect-timeout", "local-infile", "disable-local-infile",
  "replication-probe", "enable-reads-from-master", "repl-parse-query",
  "ssl-cipher", "max-allowed-packet",
  NullS
};

static TYPELIB option_types={array_elements(default_options)-1,
			     "options",default_options};

static void mysql_read_default_options(struct st_mysql_options *options,
				       const char *filename,const char *group)
{
  int argc;
  char *argv_buff[1],**argv;
  const char *groups[3];
  DBUG_ENTER("mysql_read_default_options");
  DBUG_PRINT("enter",("file: %s  group: %s",filename,group ? group :"NULL"));

  argc=1; argv=argv_buff; argv_buff[0]= (char*) "client";
  groups[0]= (char*) "client"; groups[1]= (char*) group; groups[2]=0;

  load_defaults(filename, groups, &argc, &argv);
  if (argc != 1)				/* If some default option */
  {
    char **option=argv;
    while (*++option)
    {
      /* DBUG_PRINT("info",("option: %s",option[0])); */
      if (option[0][0] == '-' && option[0][1] == '-')
      {
	char *end=strcend(*option,'=');
	char *opt_arg=0;
	if (*end)
	{
	  opt_arg=end+1;
	  *end=0;				/* Remove '=' */
	}
	/* Change all '_' in variable name to '-' */
	for (end= *option ; *(end= strcend(end,'_')) ; )
	  *end= '-';
	switch (find_type(*option+2,&option_types,2)) {
	case 1:				/* port */
	  if (opt_arg)
	    options->port=atoi(opt_arg);
	  break;
	case 2:				/* socket */
	  if (opt_arg)
	  {
	    my_free(options->unix_socket,MYF(MY_ALLOW_ZERO_PTR));
	    options->unix_socket=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 3:				/* compress */
	  options->compress=1;
	  break;
	case 4:				/* password */
	  if (opt_arg)
	  {
	    my_free(options->password,MYF(MY_ALLOW_ZERO_PTR));
	    options->password=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 5:				/* pipe */
	  options->named_pipe=1;	/* Force named pipe */
	  break;
	case 20:			/* connect_timeout */
	case 6:				/* timeout */
	  if (opt_arg)
	    options->connect_timeout=atoi(opt_arg);
	  break;
	case 7:				/* user */
	  if (opt_arg)
	  {
	    my_free(options->user,MYF(MY_ALLOW_ZERO_PTR));
	    options->user=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 8:				/* init-command */
	  if (opt_arg)
	  {
	    my_free(options->init_command,MYF(MY_ALLOW_ZERO_PTR));
	    options->init_command=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 9:				/* host */
	  if (opt_arg)
	  {
	    my_free(options->host,MYF(MY_ALLOW_ZERO_PTR));
	    options->host=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 10:			/* database */
	  if (opt_arg)
	  {
	    my_free(options->db,MYF(MY_ALLOW_ZERO_PTR));
	    options->db=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
	case 11:			/* debug */
	  mysql_debug(opt_arg ? opt_arg : "d:t:o,/tmp/client.trace");
	  break;
	case 12:			/* return-found-rows */
	  options->client_flag|=CLIENT_FOUND_ROWS;
	  break;
#ifdef HAVE_OPENSSL
	case 13:			/* ssl_key */
	  my_free(options->ssl_key, MYF(MY_ALLOW_ZERO_PTR));
          options->ssl_key = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case 14:			/* ssl_cert */
	  my_free(options->ssl_cert, MYF(MY_ALLOW_ZERO_PTR));
          options->ssl_cert = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case 15:			/* ssl_ca */
	  my_free(options->ssl_ca, MYF(MY_ALLOW_ZERO_PTR));
          options->ssl_ca = my_strdup(opt_arg, MYF(MY_WME));
          break;
	case 16:			/* ssl_capath */
	  my_free(options->ssl_capath, MYF(MY_ALLOW_ZERO_PTR));
          options->ssl_capath = my_strdup(opt_arg, MYF(MY_WME));
          break;
#else
	case 13:				/* Ignore SSL options */
	case 14:
	case 15:
	case 16:
	  break;
#endif /* HAVE_OPENSSL */
	case 17:			/* charset-lib */
	  my_free(options->charset_dir,MYF(MY_ALLOW_ZERO_PTR));
          options->charset_dir = my_strdup(opt_arg, MYF(MY_WME));
	  break;
	case 18:
	  my_free(options->charset_name,MYF(MY_ALLOW_ZERO_PTR));
          options->charset_name = my_strdup(opt_arg, MYF(MY_WME));
	  break;
	case 19:				/* Interactive-timeout */
	  options->client_flag|= CLIENT_INTERACTIVE;
	  break;
	case 21:
	  if (!opt_arg || atoi(opt_arg) != 0)
	    options->client_flag|= CLIENT_LOCAL_FILES;
	  else
	    options->client_flag&= ~CLIENT_LOCAL_FILES;
	  break;
	case 22:
	  options->client_flag&= ~CLIENT_LOCAL_FILES;
          break;
	case 23:  /* replication probe */
	  options->rpl_probe= 1;
	  break;
	case 24: /* enable-reads-from-master */
	  options->no_master_reads= 0;
	  break;
	case 25: /* repl-parse-query */
	  options->rpl_parse= 1;
	  break;
	case 27:
          if (opt_arg)
            options->max_allowed_packet= atoi(opt_arg);
	  break;
	default:
	  DBUG_PRINT("warning",("unknown option: %s",option[0]));
	}
      }
    }
  }
  free_defaults(argv);
  DBUG_VOID_RETURN;
}


/***************************************************************************
  Change field rows to field structs
***************************************************************************/

static MYSQL_FIELD *
unpack_fields(MYSQL_DATA *data,MEM_ROOT *alloc,uint fields,
	      my_bool default_value, my_bool long_flag_protocol)
{
  MYSQL_ROWS	*row;
  MYSQL_FIELD	*field,*result;
  DBUG_ENTER("unpack_fields");

  field=result=(MYSQL_FIELD*) alloc_root(alloc,
					 (uint) sizeof(MYSQL_FIELD)*fields);
  if (!result)
    DBUG_RETURN(0);

  for (row=data->data; row ; row = row->next,field++)
  {
    field->org_table= field->table=  strdup_root(alloc,(char*) row->data[0]);
    field->name=   strdup_root(alloc,(char*) row->data[1]);
    field->length= (uint) uint3korr(row->data[2]);
    field->type=   (enum enum_field_types) (uchar) row->data[3][0];
    if (long_flag_protocol)
    {
      field->flags=   uint2korr(row->data[4]);
      field->decimals=(uint) (uchar) row->data[4][2];
    }
    else
    {
      field->flags=   (uint) (uchar) row->data[4][0];
      field->decimals=(uint) (uchar) row->data[4][1];
    }
    if (INTERNAL_NUM_FIELD(field))
      field->flags|= NUM_FLAG;
    if (default_value && row->data[5])
      field->def=strdup_root(alloc,(char*) row->data[5]);
    else
      field->def=0;
    field->max_length= 0;
  }
  free_rows(data);				/* Free old data */
  DBUG_RETURN(result);
}


/* Read all rows (fields or data) from server */

static MYSQL_DATA *read_rows(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
			     uint fields)
{
  uint	field;
  ulong pkt_len;
  ulong len;
  uchar *cp;
  char	*to, *end_to;
  MYSQL_DATA *result;
  MYSQL_ROWS **prev_ptr,*cur;
  NET *net = &mysql->net;
  DBUG_ENTER("read_rows");

  if ((pkt_len= net_safe_read(mysql)) == packet_error)
    DBUG_RETURN(0);
  if (!(result=(MYSQL_DATA*) my_malloc(sizeof(MYSQL_DATA),
				       MYF(MY_WME | MY_ZEROFILL))))
  {
    net->last_errno=CR_OUT_OF_MEMORY;
    strmov(net->last_error,ER(net->last_errno));
    DBUG_RETURN(0);
  }
  init_alloc_root(&result->alloc,8192,0);	/* Assume rowlength < 8192 */
  result->alloc.min_malloc=sizeof(MYSQL_ROWS);
  prev_ptr= &result->data;
  result->rows=0;
  result->fields=fields;

  while (*(cp=net->read_pos) != 254 || pkt_len != 1)
  {
    result->rows++;
    if (!(cur= (MYSQL_ROWS*) alloc_root(&result->alloc,
					    sizeof(MYSQL_ROWS))) ||
	!(cur->data= ((MYSQL_ROW)
		      alloc_root(&result->alloc,
				     (fields+1)*sizeof(char *)+pkt_len))))
    {
      free_rows(result);
      net->last_errno=CR_OUT_OF_MEMORY;
      strmov(net->last_error,ER(net->last_errno));
      DBUG_RETURN(0);
    }
    *prev_ptr=cur;
    prev_ptr= &cur->next;
    to= (char*) (cur->data+fields+1);
    end_to=to+pkt_len-1;
    for (field=0 ; field < fields ; field++)
    {
      if ((len=(ulong) net_field_length(&cp)) == NULL_LENGTH)
      {						/* null field */
	cur->data[field] = 0;
      }
      else
      {
	cur->data[field] = to;
        if (len > (ulong) (end_to - to))
        {
          free_rows(result);
          net->last_errno=CR_MALFORMED_PACKET;
          strmov(net->last_error,ER(net->last_errno));
          DBUG_RETURN(0);
        }
	memcpy(to,(char*) cp,len); to[len]=0;
	to+=len+1;
	cp+=len;
	if (mysql_fields)
	{
	  if (mysql_fields[field].max_length < len)
	    mysql_fields[field].max_length=len;
	}
      }
    }
    cur->data[field]=to;			/* End of last field */
    if ((pkt_len=net_safe_read(mysql)) == packet_error)
    {
      free_rows(result);
      DBUG_RETURN(0);
    }
  }
  *prev_ptr=0;					/* last pointer is null */
  DBUG_PRINT("exit",("Got %d rows",result->rows));
  DBUG_RETURN(result);
}


/*
  Read one row. Uses packet buffer as storage for fields.
  When next packet is read, the previous field values are destroyed
*/


static int
read_one_row(MYSQL *mysql,uint fields,MYSQL_ROW row, ulong *lengths)
{
  uint field;
  ulong pkt_len,len;
  uchar *pos,*prev_pos, *end_pos;

  if ((pkt_len=net_safe_read(mysql)) == packet_error)
    return -1;
  if (pkt_len == 1 && mysql->net.read_pos[0] == 254)
    return 1;				/* End of data */
  prev_pos= 0;				/* allowed to write at packet[-1] */
  pos=mysql->net.read_pos;
  end_pos=pos+pkt_len;
  for (field=0 ; field < fields ; field++)
  {
    if ((len=(ulong) net_field_length(&pos)) == NULL_LENGTH)
    {						/* null field */
      row[field] = 0;
      *lengths++=0;
    }
    else
    {
      if (len > (ulong) (end_pos - pos))
      {
        mysql->net.last_errno=CR_UNKNOWN_ERROR;
        strmov(mysql->net.last_error,ER(mysql->net.last_errno));
        return -1;
      }
      row[field] = (char*) pos;
      pos+=len;
      *lengths++=len;
    }
    if (prev_pos)
      *prev_pos=0;				/* Terminate prev field */
    prev_pos=pos;
  }
  row[field]=(char*) prev_pos+1;		/* End of last field */
  *prev_pos=0;					/* Terminate last field */
  return 0;
}

/* perform query on master */
int STDCALL mysql_master_query(MYSQL *mysql, const char *q,
			       unsigned long length)
{
  DBUG_ENTER("mysql_master_query");
  if (mysql_master_send_query(mysql, q, length))
    DBUG_RETURN(1);
  DBUG_RETURN(mysql_read_query_result(mysql));
}

int STDCALL mysql_master_send_query(MYSQL *mysql, const char *q,
				    unsigned long length)
{
  MYSQL *master = mysql->master;
  DBUG_ENTER("mysql_master_send_query");
  if (!master->net.vio && !mysql_real_connect(master,0,0,0,0,0,0,0))
    DBUG_RETURN(1);
  mysql->last_used_con = master;
  DBUG_RETURN(simple_command(master, COM_QUERY, q, length, 1));
}


/* perform query on slave */  
int STDCALL mysql_slave_query(MYSQL *mysql, const char *q,
			      unsigned long length)
{
  DBUG_ENTER("mysql_slave_query");
  if (mysql_slave_send_query(mysql, q, length))
    DBUG_RETURN(1);
  DBUG_RETURN(mysql_read_query_result(mysql));
}

int STDCALL mysql_slave_send_query(MYSQL *mysql, const char *q,
				   unsigned long length)
{
  MYSQL* last_used_slave, *slave_to_use = 0;
  DBUG_ENTER("mysql_slave_send_query");

  if ((last_used_slave = mysql->last_used_slave))
    slave_to_use = last_used_slave->next_slave;
  else
    slave_to_use = mysql->next_slave;
  /*
    Next_slave is always safe to use - we have a circular list of slaves
    if there are no slaves, mysql->next_slave == mysql
  */
  mysql->last_used_con = mysql->last_used_slave = slave_to_use;
  if (!slave_to_use->net.vio && !mysql_real_connect(slave_to_use, 0,0,0,
						   0,0,0,0))
    DBUG_RETURN(1);
  DBUG_RETURN(simple_command(slave_to_use, COM_QUERY, q, length, 1));
}


/* enable/disable parsing of all queries to decide
   if they go on master or slave */
void STDCALL mysql_enable_rpl_parse(MYSQL* mysql)
{
  mysql->options.rpl_parse = 1;
}

void STDCALL mysql_disable_rpl_parse(MYSQL* mysql)
{
  mysql->options.rpl_parse = 0;
}

/* get the value of the parse flag */  
int STDCALL mysql_rpl_parse_enabled(MYSQL* mysql)
{
  return mysql->options.rpl_parse;
}

/*  enable/disable reads from master */
void STDCALL mysql_enable_reads_from_master(MYSQL* mysql)
{
  mysql->options.no_master_reads = 0;
}

void STDCALL mysql_disable_reads_from_master(MYSQL* mysql)
{
  mysql->options.no_master_reads = 1;
}

/* get the value of the master read flag */  
int STDCALL mysql_reads_from_master_enabled(MYSQL* mysql)
{
  return !(mysql->options.no_master_reads);
}


/*
  We may get an error while doing replication internals.
  In this case, we add a special explanation to the original
  error
*/

static void expand_error(MYSQL* mysql, int error)
{
  char tmp[MYSQL_ERRMSG_SIZE];
  char *p;
  uint err_length;
  strmake(tmp, mysql->net.last_error, MYSQL_ERRMSG_SIZE-1);
  p = strmake(mysql->net.last_error, ER(error), MYSQL_ERRMSG_SIZE-1);
  err_length= (uint) (p - mysql->net.last_error);
  strmake(p, tmp, MYSQL_ERRMSG_SIZE-1 - err_length);
  mysql->net.last_errno = error;
}

/*
  This function assumes we have just called SHOW SLAVE STATUS and have
  read the given result and row
*/

static int get_master(MYSQL* mysql, MYSQL_RES* res, MYSQL_ROW row)
{
  MYSQL* master;
  DBUG_ENTER("get_master");
  if (mysql_num_fields(res) < 3)
    DBUG_RETURN(1); /* safety */

  /* use the same username and password as the original connection */
  if (!(master = spawn_init(mysql, row[0], atoi(row[2]), 0, 0)))
    DBUG_RETURN(1);
  mysql->master = master;
  DBUG_RETURN(0);
}


/*
  Assuming we already know that mysql points to a master connection,
  retrieve all the slaves
*/

static int get_slaves_from_master(MYSQL* mysql)
{
  MYSQL_RES* res = 0;
  MYSQL_ROW row;
  int error = 1;
  int has_auth_info;
  int port_ind;
  DBUG_ENTER("get_slaves_from_master");

  if (!mysql->net.vio && !mysql_real_connect(mysql,0,0,0,0,0,0,0))
  {
    expand_error(mysql, CR_PROBE_MASTER_CONNECT);
    DBUG_RETURN(1);
  }

  if (mysql_query(mysql, "SHOW SLAVE HOSTS") ||
      !(res = mysql_store_result(mysql)))
  {
    expand_error(mysql, CR_PROBE_SLAVE_HOSTS);
    DBUG_RETURN(1);
  }

  switch (mysql_num_fields(res)) {
  case 5:
    has_auth_info = 0;
    port_ind=2;
    break;
  case 7:
    has_auth_info = 1;
    port_ind=4;
    break;
  default:
    goto err;
  }

  while ((row = mysql_fetch_row(res)))
  {
    MYSQL* slave;
    const char* tmp_user, *tmp_pass;

    if (has_auth_info)
    {
      tmp_user = row[2];
      tmp_pass = row[3];
    }
    else
    {
      tmp_user = mysql->user;
      tmp_pass = mysql->passwd;
    }

    if (!(slave = spawn_init(mysql, row[1], atoi(row[port_ind]),
			    tmp_user, tmp_pass)))
      goto err;

    /* Now add slave into the circular linked list */
    slave->next_slave = mysql->next_slave;
    mysql->next_slave = slave;
  }
  error = 0;
err:
  if (res)
   mysql_free_result(res);
  DBUG_RETURN(error);
}


int STDCALL mysql_rpl_probe(MYSQL* mysql)
{
  MYSQL_RES *res= 0;
  MYSQL_ROW row;
  int error = 1;
  DBUG_ENTER("mysql_rpl_probe");

  /*
    First determine the replication role of the server we connected to
    the most reliable way to do this is to run SHOW SLAVE STATUS and see
    if we have a non-empty master host. This is still not fool-proof -
    it is not a sin to have a master that has a dormant slave thread with
    a non-empty master host. However, it is more reliable to check 
    for empty master than whether the slave thread is actually running
  */
  if (mysql_query(mysql, "SHOW SLAVE STATUS") ||
      !(res = mysql_store_result(mysql)))
  {
    expand_error(mysql, CR_PROBE_SLAVE_STATUS);
    DBUG_RETURN(1);
  }

  row= mysql_fetch_row(res);
  /*
    Check master host for emptiness/NULL
    For MySQL 4.0 it's enough to check for row[0]
  */
  if (row && row[0] && *(row[0]))
  {
    /* this is a slave, ask it for the master */
    if (get_master(mysql, res, row) || get_slaves_from_master(mysql))
      goto err;
  }
  else
  {
    mysql->master = mysql;
    if (get_slaves_from_master(mysql))
      goto err;
  }

  error = 0;
err:
  if (res)
    mysql_free_result(res);
  DBUG_RETURN(error);
}


/*
  Make a not so fool-proof decision on where the query should go, to
  the master or the slave. Ideally the user should always make this
  decision himself with mysql_master_query() or mysql_slave_query().
  However, to be able to more easily port the old code, we support the
  option of an educated guess - this should work for most applications,
  however, it may make the wrong decision in some particular cases. If
  that happens, the user would have to change the code to call
  mysql_master_query() or mysql_slave_query() explicitly in the place
  where we have made the wrong decision
*/

enum mysql_rpl_type
STDCALL mysql_rpl_query_type(const char* q, int len)
{
  const char *q_end= q + len;
  for (; q < q_end; ++q)
  {
    char c;
    if (isalpha(c=*q))
    {
      switch(tolower(c)) {
      case 'i':  /* insert */
      case 'u':  /* update or unlock tables */
      case 'l':  /* lock tables or load data infile */
      case 'd':  /* drop or delete */
      case 'a':  /* alter */
	return MYSQL_RPL_MASTER;
      case 'c':  /* create or check */
	return tolower(q[1]) == 'h' ? MYSQL_RPL_ADMIN : MYSQL_RPL_MASTER ;
      case 's': /* select or show */
	return tolower(q[1]) == 'h' ? MYSQL_RPL_ADMIN : MYSQL_RPL_SLAVE;
      case 'f': /* flush */
      case 'r': /* repair */
      case 'g': /* grant */
	return MYSQL_RPL_ADMIN;
      default:
	return MYSQL_RPL_SLAVE;
      }
    }
  }
  return MYSQL_RPL_MASTER;		/* By default, send to master */
}


/****************************************************************************
  Init MySQL structure or allocate one
****************************************************************************/

MYSQL * STDCALL
mysql_init(MYSQL *mysql)
{
  if (mysql_once_init())
    return 0;
  if (!mysql)
  {
    if (!(mysql=(MYSQL*) my_malloc(sizeof(*mysql),MYF(MY_WME | MY_ZEROFILL))))
      return 0;
    mysql->free_me=1;
  }
  else
    bzero((char*) (mysql),sizeof(*(mysql)));
  mysql->options.connect_timeout=CONNECT_TIMEOUT;
  mysql->last_used_con = mysql->next_slave = mysql->master = mysql;

  /*
    By default, we are a replication pivot. The caller must reset it
    after we return if this is not the case.
  */
  mysql->rpl_pivot = 1; 

/*
  Only enable LOAD DATA INFILE by default if configured with
  --enable-local-infile
*/
#ifdef ENABLED_LOCAL_INFILE
  mysql->options.client_flag|= CLIENT_LOCAL_FILES;
#endif
  return mysql;
}


/*
  Initialize the MySQL library

  SYNOPSIS
    mysql_once_init()

  NOTES
    Can't be static on NetWare
    This function is called by mysql_init() and indirectly called
    by mysql_query(), so one should never have to call this from an
    outside program.

  RETURN
    0  ok
    1  could not initialize environment (out of memory or thread keys)
*/

int mysql_once_init(void)
{
  if (!mysql_client_init)
  {
    mysql_client_init=1;
    org_my_init_done=my_init_done;
    if (my_init())				/* Will init threads */
      return 1;
    init_client_errs();
    if (!mysql_port)
    {
      mysql_port = MYSQL_PORT;
#ifndef MSDOS
      {
	struct servent *serv_ptr;
	char	*env;
	if ((serv_ptr = getservbyname("mysql", "tcp")))
	  mysql_port = (uint) ntohs((ushort) serv_ptr->s_port);
	if ((env = getenv("MYSQL_TCP_PORT")))
	  mysql_port =(uint) atoi(env);
      }
#endif
    }
    if (!mysql_unix_port)
    {
      char *env;
#ifdef __WIN__
      mysql_unix_port = (char*) MYSQL_NAMEDPIPE;
#else
      mysql_unix_port = (char*) MYSQL_UNIX_ADDR;
#endif
      if ((env = getenv("MYSQL_UNIX_PORT")))
	mysql_unix_port = env;
    }
    mysql_debug(NullS);
#if defined(SIGPIPE) && !defined(__WIN__)
    (void) signal(SIGPIPE, SIG_IGN);
#endif
  }
#ifdef THREAD
  else
  {
    if (my_thread_init())         /* Init if new thread */
      return 1;
  }
#endif
  return 0;
}


/**************************************************************************
  Fill in SSL part of MYSQL structure and set 'use_ssl' flag.
  NB! Errors are not reported until you do mysql_real_connect.
**************************************************************************/

#define strdup_if_not_null(A) (A) == 0 ? 0 : my_strdup((A),MYF(MY_WME))

int STDCALL
mysql_ssl_set(MYSQL *mysql __attribute__((unused)) ,
	      const char *key __attribute__((unused)), 
	      const char *cert __attribute__((unused)),
	      const char *ca __attribute__((unused)), 
	      const char *capath __attribute__((unused)),
	      const char *cipher __attribute__((unused)))
{
#ifdef HAVE_OPENSSL
  mysql->options.ssl_key=    strdup_if_not_null(key);
  mysql->options.ssl_cert=   strdup_if_not_null(cert);
  mysql->options.ssl_ca=     strdup_if_not_null(ca);
  mysql->options.ssl_capath= strdup_if_not_null(capath);
  mysql->options.ssl_cipher= strdup_if_not_null(cipher);
#endif
  return 0;
}


/**************************************************************************
  Free strings in the SSL structure and clear 'use_ssl' flag.
  NB! Errors are not reported until you do mysql_real_connect.
**************************************************************************/

static int
mysql_ssl_free(MYSQL *mysql __attribute__((unused)))
{
#ifdef HAVE_OPENSSL
  my_free(mysql->options.ssl_key, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_cert, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_ca, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_capath, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_cipher, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->connector_fd,MYF(MY_ALLOW_ZERO_PTR));
  mysql->options.ssl_key = 0;
  mysql->options.ssl_cert = 0;
  mysql->options.ssl_ca = 0;
  mysql->options.ssl_capath = 0;
  mysql->options.ssl_cipher= 0;
  mysql->options.use_ssl = FALSE;
  mysql->connector_fd = 0;
#endif /* HAVE_OPENSSL */
  return 0;
}

/**************************************************************************
  Connect to sql server
  If host == 0 then use localhost
**************************************************************************/

#ifdef USE_OLD_FUNCTIONS
MYSQL * STDCALL
mysql_connect(MYSQL *mysql,const char *host,
	      const char *user, const char *passwd)
{
  MYSQL *res;
  mysql=mysql_init(mysql);			/* Make it thread safe */
  {
    DBUG_ENTER("mysql_connect");
    if (!(res=mysql_real_connect(mysql,host,user,passwd,NullS,0,NullS,0)))
    {
      if (mysql->free_me)
	my_free((gptr) mysql,MYF(0));
    }
    DBUG_RETURN(res);
  }
}
#endif


#ifdef CHECK_LICENSE
/*
  Check server side variable 'license'.
  If the variable does not exist or does not contain 'Commercial', 
  we're talking to non-commercial server from commercial client.
  SYNOPSIS
    check_license()
  RETURN VALUE
    0  success
   !0  network error or the server is not commercial.
       Error code is saved in mysql->net.last_errno.
*/

static int check_license(MYSQL *mysql)
{
  MYSQL_ROW row;
  MYSQL_RES *res;
  NET *net= &mysql->net;
  static const char query[]= "SELECT @@license";
  static const char required_license[]= STRINGIFY_ARG(LICENSE);

  if (mysql_real_query(mysql, query, sizeof(query)-1))
  {
    if (net->last_errno == ER_UNKNOWN_SYSTEM_VARIABLE)
    {
      net->last_errno= CR_WRONG_LICENSE;
      sprintf(net->last_error, ER(net->last_errno), required_license);
    }
    return 1;
  }
  if (!(res= mysql_use_result(mysql)))
    return 1;
  row= mysql_fetch_row(res);
  /* 
    If no rows in result set, or column value is NULL (none of these
    two is ever true for server variables now), or column value
    mismatch, set wrong license error.
  */
  if (!net->last_errno &&
      (!row || !row[0] ||
       strncmp(row[0], required_license, sizeof(required_license))))
  {
    net->last_errno= CR_WRONG_LICENSE;
    sprintf(net->last_error, ER(net->last_errno), required_license);
  }
  mysql_free_result(res);
  return net->last_errno;
}
#endif /* CHECK_LICENSE */

/*
  The following union is used to force a struct to be double allgined.
  This is to avoid warings with gethostname_r() on Linux itanium systems
*/

typedef union
{
  double tmp;
  char   buff[GETHOSTBYNAME_BUFF_SIZE];
} gethostbyname_buff;


/*
  Note that the mysql argument must be initialized with mysql_init()
  before calling mysql_real_connect !
*/

MYSQL * STDCALL
mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		   const char *passwd, const char *db,
		   uint port, const char *unix_socket,uint client_flag)
{
  char		buff[NAME_LEN+USERNAME_LENGTH+100],charset_name_buff[16];
  char		*end,*host_info,*charset_name;
  my_socket	sock;
  in_addr_t	ip_addr;
  struct	sockaddr_in sock_addr;
  ulong		pkt_length;
  NET		*net= &mysql->net;
#ifdef __WIN__
  HANDLE	hPipe=INVALID_HANDLE_VALUE;
#endif
#ifdef HAVE_SYS_UN_H
  struct	sockaddr_un UNIXaddr;
#endif
  init_sigpipe_variables
  DBUG_ENTER("mysql_real_connect");
  LINT_INIT(host_info);

  DBUG_PRINT("enter",("host: %s  db: %s  user: %s",
		      host ? host : "(Null)",
		      db ? db : "(Null)",
		      user ? user : "(Null)"));

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(mysql);
  net->vio = 0;				/* If something goes wrong */
  /* use default options */
  if (mysql->options.my_cnf_file || mysql->options.my_cnf_group)
  {
    mysql_read_default_options(&mysql->options,
			       (mysql->options.my_cnf_file ?
				mysql->options.my_cnf_file : "my"),
			       mysql->options.my_cnf_group);
    my_free(mysql->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.my_cnf_file=mysql->options.my_cnf_group=0;
  }

  /* Some empty-string-tests are done because of ODBC */
  if (!host || !host[0])
    host=mysql->options.host;
  if (!user || !user[0])
    user=mysql->options.user;
  if (!passwd)
  {
    passwd=mysql->options.password;
#ifndef DONT_USE_MYSQL_PWD
    if (!passwd)
      passwd=getenv("MYSQL_PWD");  /* get it from environment (haneke) */
#endif
  }
  if (!db || !db[0])
    db=mysql->options.db;
  if (!port)
    port=mysql->options.port;
  if (!unix_socket)
    unix_socket=mysql->options.unix_socket;

  mysql->reconnect=1;			/* Reconnect as default */
  mysql->server_status=SERVER_STATUS_AUTOCOMMIT;

  /*
  ** Grab a socket and connect it to the server
  */

#if defined(HAVE_SYS_UN_H)
  if ((!host || !strcmp(host,LOCAL_HOST)) && (unix_socket || mysql_unix_port))
  {
    host=LOCAL_HOST;
    if (!unix_socket)
      unix_socket=mysql_unix_port;
    host_info=(char*) ER(CR_LOCALHOST_CONNECTION);
    DBUG_PRINT("info",("Using UNIX sock '%s'",unix_socket));
    if ((sock = socket(AF_UNIX,SOCK_STREAM,0)) == SOCKET_ERROR)
    {
      net->last_errno=CR_SOCKET_CREATE_ERROR;
      sprintf(net->last_error,ER(net->last_errno),socket_errno);
      goto error;
    }
    net->vio = vio_new(sock, VIO_TYPE_SOCKET, TRUE);
    bzero((char*) &UNIXaddr,sizeof(UNIXaddr));
    UNIXaddr.sun_family = AF_UNIX;
    strmake(UNIXaddr.sun_path, unix_socket, sizeof(UNIXaddr.sun_path)-1);
    if (my_connect(sock,(struct sockaddr *) &UNIXaddr, sizeof(UNIXaddr),
		 mysql->options.connect_timeout) <0)
    {
      DBUG_PRINT("error",("Got error %d on connect to local server",socket_errno));
      net->last_errno=CR_CONNECTION_ERROR;
      sprintf(net->last_error,ER(net->last_errno),unix_socket,socket_errno);
      goto error;
    }
  }
  else
#elif defined(__WIN__)
  {
    if ((unix_socket ||
	 !host && is_NT() ||
	 host && !strcmp(host,LOCAL_HOST_NAMEDPIPE) ||
	 mysql->options.named_pipe || !have_tcpip))
    {
      sock=0;
      if ((hPipe=create_named_pipe(net, mysql->options.connect_timeout,
				   (char**) &host, (char**) &unix_socket)) ==
	  INVALID_HANDLE_VALUE)
      {
	DBUG_PRINT("error",
		   ("host: '%s'  socket: '%s'  named_pipe: %d  have_tcpip: %d",
		    host ? host : "<null>",
		    unix_socket ? unix_socket : "<null>",
		    (int) mysql->options.named_pipe,
		    (int) have_tcpip));
	if (mysql->options.named_pipe ||
	    (host && !strcmp(host,LOCAL_HOST_NAMEDPIPE)) ||
	    (unix_socket && !strcmp(unix_socket,MYSQL_NAMEDPIPE)))
	{
	  net->last_errno= CR_SERVER_LOST;
	  strmov(net->last_error,ER(net->last_errno));
	  goto error;		/* User only requested named pipes */
	}
	/* Try also with TCP/IP */
      }
      else
      {
	net->vio=vio_new_win32pipe(hPipe);
	sprintf(host_info=buff, ER(CR_NAMEDPIPE_CONNECTION), host,
		unix_socket);
      }
    }
  }
  if (hPipe == INVALID_HANDLE_VALUE)
#endif
  {
    unix_socket=0;				/* This is not used */
    if (!port)
      port=mysql_port;
    if (!host)
      host=LOCAL_HOST;
    sprintf(host_info=buff,ER(CR_TCP_CONNECTION),host);
    DBUG_PRINT("info",("Server name: '%s'.  TCP sock: %d", host,port));
    /* _WIN64 ;  Assume that the (int) range is enough for socket() */
    if ((sock = (my_socket) socket(AF_INET,SOCK_STREAM,0)) == SOCKET_ERROR)
    {
      net->last_errno=CR_IPSOCK_ERROR;
      sprintf(net->last_error,ER(net->last_errno),socket_errno);
      goto error;
    }
    net->vio = vio_new(sock,VIO_TYPE_TCPIP,FALSE);
    bzero((char*) &sock_addr,sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;

    /*
    ** The server name may be a host name or IP address
    */

    if ((int) (ip_addr = inet_addr(host)) != (int) INADDR_NONE)
    {
      memcpy_fixed(&sock_addr.sin_addr,&ip_addr,sizeof(ip_addr));
    }
    else
    {
      int tmp_errno;
      struct hostent tmp_hostent,*hp;
      gethostbyname_buff buff2;
      hp = my_gethostbyname_r(host,&tmp_hostent,buff2.buff,sizeof(buff2),
			      &tmp_errno);
      if (!hp)
      {
	my_gethostbyname_r_free();
	net->last_errno=CR_UNKNOWN_HOST;
	sprintf(net->last_error, ER(CR_UNKNOWN_HOST), host, tmp_errno);
	goto error;
      }
      memcpy(&sock_addr.sin_addr, hp->h_addr,
             min(sizeof(sock_addr.sin_addr), (size_t) hp->h_length));
      my_gethostbyname_r_free();
    }
    sock_addr.sin_port = (ushort) htons((ushort) port);
    if (my_connect(sock,(struct sockaddr *) &sock_addr, sizeof(sock_addr),
		 mysql->options.connect_timeout) <0)
    {
      DBUG_PRINT("error",("Got error %d on connect to '%s'",socket_errno,host));
      net->last_errno= CR_CONN_HOST_ERROR;
      sprintf(net->last_error ,ER(CR_CONN_HOST_ERROR), host, socket_errno);
      goto error;
    }
  }

  if (!net->vio || my_net_init(net, net->vio))
  {
    vio_delete(net->vio);
    net->vio = 0;
    net->last_errno=CR_OUT_OF_MEMORY;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }
  vio_keepalive(net->vio,TRUE);

  /* Get version info */
  mysql->protocol_version= PROTOCOL_VERSION;	/* Assume this */
  if (mysql->options.connect_timeout &&
      vio_poll_read(net->vio, mysql->options.connect_timeout))
  {
    net->last_errno= CR_SERVER_LOST;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }
  if ((pkt_length=net_safe_read(mysql)) == packet_error)
    goto error;

  /* Check if version of protocoll matches current one */

  mysql->protocol_version= net->read_pos[0];
  DBUG_DUMP("packet",(char*) net->read_pos,10);
  DBUG_PRINT("info",("mysql protocol version %d, server=%d",
		     PROTOCOL_VERSION, mysql->protocol_version));
  if (mysql->protocol_version != PROTOCOL_VERSION)
  {
    net->last_errno= CR_VERSION_ERROR;
    sprintf(net->last_error, ER(CR_VERSION_ERROR), mysql->protocol_version,
	    PROTOCOL_VERSION);
    goto error;
  }
  end=strend((char*) net->read_pos+1);
  mysql->thread_id=uint4korr(end+1);
  end+=5;
  strmake(mysql->scramble_buff,end,8);
  end+=9;
  if (pkt_length >= (uint) (end+1 - (char*) net->read_pos))
    mysql->server_capabilities=uint2korr(end);
  if (pkt_length >= (uint) (end+18 - (char*) net->read_pos))
  {
    /* New protocol with 16 bytes to describe server characteristics */
    mysql->server_language=end[2];
    mysql->server_status=uint2korr(end+3);
  }

  /* Set character set */
  if ((charset_name=mysql->options.charset_name))
  {
    const char *save=charsets_dir;
    if (mysql->options.charset_dir)
      charsets_dir=mysql->options.charset_dir;
    mysql->charset=get_charset_by_name(mysql->options.charset_name,
                                       MYF(MY_WME));
    charsets_dir=save;
  }
  else if (mysql->server_language)
  {
    charset_name=charset_name_buff;
    sprintf(charset_name,"%d",mysql->server_language);	/* In case of errors */
    if (!(mysql->charset =
	  get_charset((uint8) mysql->server_language, MYF(0))))
      mysql->charset = default_charset_info; /* shouldn't be fatal */

  }
  else
    mysql->charset=default_charset_info;

  if (!mysql->charset)
  {
    net->last_errno=CR_CANT_READ_CHARSET;
    if (mysql->options.charset_dir)
      sprintf(net->last_error,ER(net->last_errno),
              charset_name ? charset_name : "unknown",
              mysql->options.charset_dir);
    else
    {
      char cs_dir_name[FN_REFLEN];
      get_charsets_dir(cs_dir_name);
      sprintf(net->last_error,ER(net->last_errno),
              charset_name ? charset_name : "unknown",
              cs_dir_name);
    }
    goto error;
  }

  /* Save connection information */
  if (!user) user="";
  if (!passwd) passwd="";
  if (!my_multi_malloc(MYF(0),
		       &mysql->host_info, (uint) strlen(host_info)+1,
		       &mysql->host,      (uint) strlen(host)+1,
		       &mysql->unix_socket,unix_socket ?
		       (uint) strlen(unix_socket)+1 : (uint) 1,
		       &mysql->server_version,
		       (uint) (end - (char*) net->read_pos),
		       NullS) ||
      !(mysql->user=my_strdup(user,MYF(0))) ||
      !(mysql->passwd=my_strdup(passwd,MYF(0))))
  {
    strmov(net->last_error, ER(net->last_errno=CR_OUT_OF_MEMORY));
    goto error;
  }
  strmov(mysql->host_info,host_info);
  strmov(mysql->host,host);
  if (unix_socket)
    strmov(mysql->unix_socket,unix_socket);
  else
    mysql->unix_socket=0;
  strmov(mysql->server_version,(char*) net->read_pos+1);
  mysql->port=port;
  client_flag|=mysql->options.client_flag;

  /* Send client information for access check */
  client_flag|=CLIENT_CAPABILITIES;

#ifdef HAVE_OPENSSL
  if (mysql->options.ssl_key || mysql->options.ssl_cert ||
      mysql->options.ssl_ca || mysql->options.ssl_capath ||
      mysql->options.ssl_cipher)
    mysql->options.use_ssl= 1;
  if (mysql->options.use_ssl)
    client_flag|=CLIENT_SSL;
#endif /* HAVE_OPENSSL */
  if (db)
    client_flag|=CLIENT_CONNECT_WITH_DB;
#ifdef HAVE_COMPRESS
  if ((mysql->server_capabilities & CLIENT_COMPRESS) &&
      (mysql->options.compress || (client_flag & CLIENT_COMPRESS)))
    client_flag|=CLIENT_COMPRESS;		/* We will use compression */
  else
#endif
    client_flag&= ~CLIENT_COMPRESS;

#ifdef HAVE_OPENSSL
  if ((mysql->server_capabilities & CLIENT_SSL) &&
      (mysql->options.use_ssl || (client_flag & CLIENT_SSL)))
  {
    DBUG_PRINT("info", ("Changing IO layer to SSL"));
    client_flag |= CLIENT_SSL;
  }
  else
  {
    if (client_flag & CLIENT_SSL)
    {
      DBUG_PRINT("info", ("Leaving IO layer intact because server doesn't support SSL"));
    }
    client_flag &= ~CLIENT_SSL;
  }
#endif /* HAVE_OPENSSL */

  int2store(buff,client_flag);
  mysql->client_flag=client_flag;

#ifdef HAVE_OPENSSL
  /*
    Oops.. are we careful enough to not send ANY information without
    encryption?
  */
  if (client_flag & CLIENT_SSL)
  {
    struct st_mysql_options *options= &mysql->options;
    if (my_net_write(net,buff,(uint) (2)) || net_flush(net))
    {
      net->last_errno= CR_SERVER_LOST;
      strmov(net->last_error,ER(net->last_errno));
      goto error;
    }
    /* Do the SSL layering. */
    if (!(mysql->connector_fd=
	  (gptr) new_VioSSLConnectorFd(options->ssl_key,
				       options->ssl_cert,
				       options->ssl_ca,
				       options->ssl_capath,
				       options->ssl_cipher)))
    {
      net->last_errno= CR_SSL_CONNECTION_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto error;
    }
    DBUG_PRINT("info", ("IO layer change in progress..."));
    if(sslconnect((struct st_VioSSLConnectorFd*)(mysql->connector_fd),
	        mysql->net.vio, (long) (mysql->options.connect_timeout)))
    {
      net->last_errno= CR_SSL_CONNECTION_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto error;    
    }
    DBUG_PRINT("info", ("IO layer change done!"));
  }
#endif /* HAVE_OPENSSL */

  DBUG_PRINT("info",("Server version = '%s'  capabilites: %ld  status: %d  client_flag: %d",
		     mysql->server_version,mysql->server_capabilities,
		     mysql->server_status, client_flag));

  /* This needs to be changed as it's not useful with big packets */
  int3store(buff+2,max_allowed_packet);
  if (user && user[0])
    strmake(buff+5,user,32);			/* Max user name */
  else
    read_user_name((char*) buff+5);
#ifdef _CUSTOMCONFIG_
#include "_cust_libmysql.h";
#endif
  DBUG_PRINT("info",("user: %s",buff+5));
  end=scramble(strend(buff+5)+1, mysql->scramble_buff, passwd,
	       (my_bool) (mysql->protocol_version == 9));
  if (db && (mysql->server_capabilities & CLIENT_CONNECT_WITH_DB))
  {
    end=strmake(end+1,db,NAME_LEN);
    mysql->db=my_strdup(db,MYF(MY_WME));
    db=0;
  }
  if (my_net_write(net,buff,(ulong) (end-buff)) || net_flush(net))
  {
    net->last_errno= CR_SERVER_LOST;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }
  if (net_safe_read(mysql) == packet_error)
    goto error;
  if (client_flag & CLIENT_COMPRESS)		/* We will use compression */
    net->compress=1;
  if (mysql->options.max_allowed_packet)
    net->max_packet_size= mysql->options.max_allowed_packet;
#ifdef CHECK_LICENSE 
  if (check_license(mysql))
    goto error;
#endif
  if (db && mysql_select_db(mysql,db))
    goto error;
  if (mysql->options.init_command)
  {
    my_bool reconnect=mysql->reconnect;
    mysql->reconnect=0;
    if (mysql_query(mysql,mysql->options.init_command))
      goto error;
    mysql_free_result(mysql_use_result(mysql));
    mysql->reconnect=reconnect;
  }

  if (mysql->options.rpl_probe && mysql_rpl_probe(mysql))
    goto error;

  DBUG_PRINT("exit",("Mysql handler: %lx",mysql));
  reset_sigpipe(mysql);
  DBUG_RETURN(mysql);

error:
  reset_sigpipe(mysql);
  DBUG_PRINT("error",("message: %u (%s)",net->last_errno,net->last_error));
  {
    /* Free alloced memory */
    my_bool free_me=mysql->free_me;
    end_server(mysql);
    mysql->free_me=0;
    mysql_close(mysql);
    mysql->free_me=free_me;
  }
  DBUG_RETURN(0);
}


/* needed when we move MYSQL structure to a different address */

static void mysql_fix_pointers(MYSQL* mysql, MYSQL* old_mysql)
{
  MYSQL *tmp, *tmp_prev;
  if (mysql->master == old_mysql)
    mysql->master = mysql;
  if (mysql->last_used_con == old_mysql)
    mysql->last_used_con = mysql;
  if (mysql->last_used_slave == old_mysql)
    mysql->last_used_slave = mysql;
  for (tmp_prev = mysql, tmp = mysql->next_slave;
       tmp != old_mysql;tmp = tmp->next_slave)
  {
    tmp_prev = tmp;
  }
  tmp_prev->next_slave = mysql;
}


static my_bool mysql_reconnect(MYSQL *mysql)
{
  MYSQL tmp_mysql;
  DBUG_ENTER("mysql_reconnect");

  if (!mysql->reconnect ||
      (mysql->server_status & SERVER_STATUS_IN_TRANS) || !mysql->host_info)
  {
   /* Allow reconnect next time */
    mysql->server_status&= ~SERVER_STATUS_IN_TRANS;
    mysql->net.last_errno= CR_SERVER_GONE_ERROR;
    strmov(mysql->net.last_error, ER(mysql->net.last_errno));
    DBUG_RETURN(1);
  }
  mysql_init(&tmp_mysql);
  tmp_mysql.options=mysql->options;
  bzero((char*) &mysql->options,sizeof(mysql->options));
  tmp_mysql.rpl_pivot = mysql->rpl_pivot;
  if (!mysql_real_connect(&tmp_mysql,mysql->host,mysql->user,mysql->passwd,
			  mysql->db, mysql->port, mysql->unix_socket,
			  mysql->client_flag))
  {
    mysql->net.last_errno= tmp_mysql.net.last_errno;
    strmov(mysql->net.last_error, tmp_mysql.net.last_error);
    DBUG_RETURN(1);
  }
  tmp_mysql.free_me=mysql->free_me;
  mysql->free_me=0;
  mysql_close(mysql);
  *mysql=tmp_mysql;
  mysql_fix_pointers(mysql, &tmp_mysql); /* adjust connection pointers */  
  net_clear(&mysql->net);
  mysql->affected_rows= ~(my_ulonglong) 0;
  DBUG_RETURN(0);
}


/**************************************************************************
  Change user and database 
**************************************************************************/

my_bool	STDCALL mysql_change_user(MYSQL *mysql, const char *user, 
				  const char *passwd, const char *db)
{
  char buff[512],*pos=buff;
  DBUG_ENTER("mysql_change_user");

  if (!user)
    user="";
  if (!passwd)
    passwd="";

  pos=strmov(pos,user)+1;
  pos=scramble(pos, mysql->scramble_buff, passwd,
	       (my_bool) (mysql->protocol_version == 9));
  pos=strmov(pos+1,db ? db : "");
  if (simple_command(mysql,COM_CHANGE_USER, buff,(ulong) (pos-buff),0))
    DBUG_RETURN(1);

  my_free(mysql->user,MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->passwd,MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));

  mysql->user=  my_strdup(user,MYF(MY_WME));
  mysql->passwd=my_strdup(passwd,MYF(MY_WME));
  mysql->db=    db ? my_strdup(db,MYF(MY_WME)) : 0;
  DBUG_RETURN(0);
}


/**************************************************************************
  Set current database
**************************************************************************/

int STDCALL
mysql_select_db(MYSQL *mysql, const char *db)
{
  int error;
  DBUG_ENTER("mysql_select_db");
  DBUG_PRINT("enter",("db: '%s'",db));

  if ((error=simple_command(mysql,COM_INIT_DB,db,(ulong) strlen(db),0)))
    DBUG_RETURN(error);
  my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));
  mysql->db=my_strdup(db,MYF(MY_WME));
  DBUG_RETURN(0);
}


/*************************************************************************
  Send a QUIT to the server and close the connection
  If handle is alloced by mysql connect free it.
*************************************************************************/

void STDCALL
mysql_close(MYSQL *mysql)
{
  DBUG_ENTER("mysql_close");
  if (mysql)					/* Some simple safety */
  {
    if (mysql->net.vio != 0)
    {
      free_old_query(mysql);
      mysql->status=MYSQL_STATUS_READY; /* Force command */
      mysql->reconnect=0;
      simple_command(mysql,COM_QUIT,NullS,0,1);
      end_server(mysql);			/* Sets mysql->net.vio= 0 */
    }
    my_free((gptr) mysql->host_info,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->user,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->passwd,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.init_command,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.user,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.host,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.password,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.unix_socket,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.db,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.charset_dir,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.charset_name,MYF(MY_ALLOW_ZERO_PTR));
#ifdef HAVE_OPENSSL
    mysql_ssl_free(mysql);
#endif /* HAVE_OPENSSL */
    /* Clear pointers for better safety */
    mysql->host_info=mysql->user=mysql->passwd=mysql->db=0;
    bzero((char*) &mysql->options,sizeof(mysql->options));

    /* free/close slave list */
    if (mysql->rpl_pivot)
    {
      MYSQL* tmp;
      for (tmp = mysql->next_slave; tmp != mysql; )
      {
	/* trick to avoid following freed pointer */
	MYSQL* tmp1 = tmp->next_slave;
	mysql_close(tmp);
	tmp = tmp1;
      }
      mysql->rpl_pivot=0;
    }
    if (mysql != mysql->master)
      mysql_close(mysql->master);
    if (mysql->free_me)
      my_free((gptr) mysql,MYF(0));
  }
  DBUG_VOID_RETURN;
}


/**************************************************************************
  Do a query. If query returned rows, free old rows.
  Read data by mysql_store_result or by repeat call of mysql_fetch_row
**************************************************************************/

int STDCALL
mysql_query(MYSQL *mysql, const char *query)
{
  return mysql_real_query(mysql,query, (uint) strlen(query));
}


static MYSQL* spawn_init(MYSQL* parent, const char* host,
			 unsigned int port, const char* user,
			 const char* passwd)
{
  MYSQL* child;
  DBUG_ENTER("spawn_init");
  if (!(child= mysql_init(0)))
    DBUG_RETURN(0);

  child->options.user= my_strdup((user) ? user :
				 (parent->user ? parent->user :
				  parent->options.user), MYF(0));
  child->options.password= my_strdup((passwd) ? passwd :
				     (parent->passwd ?
				      parent->passwd :
				      parent->options.password), MYF(0));
  child->options.port= port;
  child->options.host= my_strdup((host) ? host :
				 (parent->host ?
				  parent->host :
				  parent->options.host), MYF(0));
  if (parent->db)
    child->options.db= my_strdup(parent->db, MYF(0));
  else if (parent->options.db)
    child->options.db= my_strdup(parent->options.db, MYF(0));

  /*
    rpl_pivot is set to 1 in mysql_init();  Reset it as we are not doing
    replication here
  */
  child->rpl_pivot= 0;
  DBUG_RETURN(child);
}


int
STDCALL mysql_set_master(MYSQL* mysql, const char* host,
			 unsigned int port, const char* user,
			 const char* passwd)
{
  if (mysql->master != mysql && !mysql->master->rpl_pivot)
    mysql_close(mysql->master);
  if (!(mysql->master = spawn_init(mysql, host, port, user, passwd)))
    return 1;
  return 0;
}

int
STDCALL mysql_add_slave(MYSQL* mysql, const char* host,
					   unsigned int port,
					   const char* user,
					   const char* passwd)
{
  MYSQL* slave;
  if (!(slave = spawn_init(mysql, host, port, user, passwd)))
    return 1;
  slave->next_slave = mysql->next_slave;
  mysql->next_slave = slave;
  return 0;
}


/*
  Send the query and return so we can do something else.
  Needs to be followed by mysql_read_query_result() when we want to
  finish processing it.
*/  

int STDCALL
mysql_send_query(MYSQL* mysql, const char* query, ulong length)
{
  DBUG_ENTER("mysql_send_query");
  DBUG_PRINT("enter",("rpl_parse: %d  rpl_pivot: %d",
		      mysql->options.rpl_parse, mysql->rpl_pivot));

  if (mysql->options.rpl_parse && mysql->rpl_pivot)
  {
    switch (mysql_rpl_query_type(query, length)) {
    case MYSQL_RPL_MASTER:
      DBUG_RETURN(mysql_master_send_query(mysql, query, length));
    case MYSQL_RPL_SLAVE:
      DBUG_RETURN(mysql_slave_send_query(mysql, query, length));
    case MYSQL_RPL_ADMIN:
      break;					/* fall through */
    }
  }

  mysql->last_used_con = mysql;
  DBUG_RETURN(simple_command(mysql, COM_QUERY, query, length, 1));
}


int STDCALL mysql_read_query_result(MYSQL *mysql)
{
  uchar *pos;
  ulong field_count;
  MYSQL_DATA *fields;
  ulong length;
  DBUG_ENTER("mysql_read_query_result");

  /* read from the connection which we actually used, which
     could differ from the original connection if we have slaves
   */
  mysql = mysql->last_used_con;

  if ((length = net_safe_read(mysql)) == packet_error)
    DBUG_RETURN(-1);
  free_old_query(mysql);			/* Free old result */
get_info:
  pos=(uchar*) mysql->net.read_pos;
  if ((field_count= net_field_length(&pos)) == 0)
  {
    mysql->affected_rows= net_field_length_ll(&pos);
    mysql->insert_id=	  net_field_length_ll(&pos);
    if (mysql->server_capabilities & CLIENT_TRANSACTIONS)
    {
      mysql->server_status=uint2korr(pos); pos+=2;
    }
    if (pos < mysql->net.read_pos+length && net_field_length(&pos))
      mysql->info=(char*) pos;
    DBUG_RETURN(0);
  }
  if (field_count == NULL_LENGTH)		/* LOAD DATA LOCAL INFILE */
  {
    int error=send_file_to_server(mysql,(char*) pos);
    if ((length=net_safe_read(mysql)) == packet_error || error)
      DBUG_RETURN(-1);
    goto get_info;				/* Get info packet */
  }
  if (!(mysql->server_status & SERVER_STATUS_AUTOCOMMIT))
    mysql->server_status|= SERVER_STATUS_IN_TRANS;

  mysql->extra_info= net_field_length_ll(&pos); /* Maybe number of rec */
  if (!(fields=read_rows(mysql,(MYSQL_FIELD*) 0,5)))
    DBUG_RETURN(-1);
  if (!(mysql->fields=unpack_fields(fields,&mysql->field_alloc,
				    (uint) field_count,0,
				    (my_bool) test(mysql->server_capabilities &
						   CLIENT_LONG_FLAG))))
    DBUG_RETURN(-1);
  mysql->status=MYSQL_STATUS_GET_RESULT;
  mysql->field_count= (uint) field_count;
  DBUG_RETURN(0);
}


int STDCALL
mysql_real_query(MYSQL *mysql, const char *query, ulong length)
{
  DBUG_ENTER("mysql_real_query");
  DBUG_PRINT("enter",("handle: %lx",mysql));
  DBUG_PRINT("query",("Query = '%-.4096s'",query));

  if (mysql_send_query(mysql,query,length))
    DBUG_RETURN(-1);
  DBUG_RETURN(mysql_read_query_result(mysql));
}


static int
send_file_to_server(MYSQL *mysql, const char *filename)
{
  int fd, readcount, result= -1;
  uint packet_length=MY_ALIGN(mysql->net.max_packet-16,IO_SIZE);
  char *buf, tmp_name[FN_REFLEN];
  DBUG_ENTER("send_file_to_server");

  if (!(buf=my_malloc(packet_length,MYF(0))))
  {
    strmov(mysql->net.last_error, ER(mysql->net.last_errno=CR_OUT_OF_MEMORY));
    DBUG_RETURN(-1);
  }

  fn_format(tmp_name,filename,"","",4);		/* Convert to client format */
  if ((fd = my_open(tmp_name,O_RDONLY, MYF(0))) < 0)
  {
    my_net_write(&mysql->net,"",0);		/* Server needs one packet */
    net_flush(&mysql->net);
    mysql->net.last_errno=EE_FILENOTFOUND;
    my_snprintf(mysql->net.last_error,sizeof(mysql->net.last_error)-1,
		EE(mysql->net.last_errno),tmp_name, errno);
    goto err;
  }

  while ((readcount = (int) my_read(fd,(byte*) buf,packet_length,MYF(0))) > 0)
  {
    if (my_net_write(&mysql->net,buf,readcount))
    {
      DBUG_PRINT("error",("Lost connection to MySQL server during LOAD DATA of local file"));
      mysql->net.last_errno=CR_SERVER_LOST;
      strmov(mysql->net.last_error,ER(mysql->net.last_errno));
      goto err;
    }
  }
  /* Send empty packet to mark end of file */
  if (my_net_write(&mysql->net,"",0) || net_flush(&mysql->net))
  {
    mysql->net.last_errno=CR_SERVER_LOST;
    sprintf(mysql->net.last_error,ER(mysql->net.last_errno),errno);
    goto err;
  }
  if (readcount < 0)
  {
    mysql->net.last_errno=EE_READ; /* the errmsg for not entire file read */
    my_snprintf(mysql->net.last_error,sizeof(mysql->net.last_error)-1,
		tmp_name,errno);
    goto err;
  }
  result=0;					/* Ok */

err:
  if (fd >= 0)
    (void) my_close(fd,MYF(0));
  my_free(buf,MYF(0));
  DBUG_RETURN(result);
}


/**************************************************************************
  Alloc result struct for buffered results. All rows are read to buffer.
  mysql_data_seek may be used.
**************************************************************************/

MYSQL_RES * STDCALL
mysql_store_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  DBUG_ENTER("mysql_store_result");

  /* read from the actually used connection */
  mysql = mysql->last_used_con;

  if (!mysql->fields)
    DBUG_RETURN(0);
  if (mysql->status != MYSQL_STATUS_GET_RESULT)
  {
    strmov(mysql->net.last_error,
	   ER(mysql->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    DBUG_RETURN(0);
  }
  mysql->status=MYSQL_STATUS_READY;		/* server is ready */
  if (!(result=(MYSQL_RES*) my_malloc((uint) (sizeof(MYSQL_RES)+
					      sizeof(ulong) *
					      mysql->field_count),
				      MYF(MY_WME | MY_ZEROFILL))))
  {
    mysql->net.last_errno=CR_OUT_OF_MEMORY;
    strmov(mysql->net.last_error, ER(mysql->net.last_errno));
    DBUG_RETURN(0);
  }
  result->eof=1;				/* Marker for buffered */
  result->lengths=(ulong*) (result+1);
  if (!(result->data=read_rows(mysql,mysql->fields,mysql->field_count)))
  {
    my_free((gptr) result,MYF(0));
    DBUG_RETURN(0);
  }
  mysql->affected_rows= result->row_count= result->data->rows;
  result->data_cursor=	result->data->data;
  result->fields=	mysql->fields;
  result->field_alloc=	mysql->field_alloc;
  result->field_count=	mysql->field_count;
  result->current_field=0;
  result->current_row=0;			/* Must do a fetch first */
  mysql->fields=0;				/* fields is now in result */
  DBUG_RETURN(result);				/* Data fetched */
}


/**************************************************************************
  Alloc struct for use with unbuffered reads. Data is fetched by domand
  when calling to mysql_fetch_row.
  mysql_data_seek is a noop.

  No other queries may be specified with the same MYSQL handle.
  There shouldn't be much processing per row because mysql server shouldn't
  have to wait for the client (and will not wait more than 30 sec/packet).
**************************************************************************/

MYSQL_RES * STDCALL
mysql_use_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  DBUG_ENTER("mysql_use_result");

  mysql = mysql->last_used_con;

  if (!mysql->fields)
    DBUG_RETURN(0);
  if (mysql->status != MYSQL_STATUS_GET_RESULT)
  {
    strmov(mysql->net.last_error,
	   ER(mysql->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    DBUG_RETURN(0);
  }
  if (!(result=(MYSQL_RES*) my_malloc(sizeof(*result)+
				      sizeof(ulong)*mysql->field_count,
				      MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(0);
  result->lengths=(ulong*) (result+1);
  if (!(result->row=(MYSQL_ROW)
	my_malloc(sizeof(result->row[0])*(mysql->field_count+1), MYF(MY_WME))))
  {					/* Ptrs: to one row */
    my_free((gptr) result,MYF(0));
    DBUG_RETURN(0);
  }
  result->fields=	mysql->fields;
  result->field_alloc=	mysql->field_alloc;
  result->field_count=	mysql->field_count;
  result->current_field=0;
  result->handle=	mysql;
  result->current_row=	0;
  mysql->fields=0;			/* fields is now in result */
  mysql->status=MYSQL_STATUS_USE_RESULT;
  DBUG_RETURN(result);			/* Data is read to be fetched */
}



/**************************************************************************
  Return next field of the query results
**************************************************************************/

MYSQL_FIELD * STDCALL
mysql_fetch_field(MYSQL_RES *result)
{
  if (result->current_field >= result->field_count)
    return(NULL);
  return &result->fields[result->current_field++];
}


/**************************************************************************
   Return next row of the query results
**************************************************************************/

MYSQL_ROW STDCALL
mysql_fetch_row(MYSQL_RES *res)
{
  DBUG_ENTER("mysql_fetch_row");
  if (!res->data)
  {						/* Unbufferred fetch */
    if (!res->eof)
    {
      if (!(read_one_row(res->handle,res->field_count,res->row, res->lengths)))
      {
	res->row_count++;
	DBUG_RETURN(res->current_row=res->row);
      }
      else
      {
	DBUG_PRINT("info",("end of data"));
	res->eof=1;
	res->handle->status=MYSQL_STATUS_READY;
	/* Don't clear handle in mysql_free_results */
	res->handle=0;
      }
    }
    DBUG_RETURN((MYSQL_ROW) NULL);
  }
  {
    MYSQL_ROW tmp;
    if (!res->data_cursor)
    {
      DBUG_PRINT("info",("end of data"));
      DBUG_RETURN(res->current_row=(MYSQL_ROW) NULL);
    }
    tmp = res->data_cursor->data;
    res->data_cursor = res->data_cursor->next;
    DBUG_RETURN(res->current_row=tmp);
  }
}

/**************************************************************************
  Get column lengths of the current row
  If one uses mysql_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

ulong * STDCALL
mysql_fetch_lengths(MYSQL_RES *res)
{
  ulong *lengths,*prev_length;
  byte *start;
  MYSQL_ROW column,end;

  if (!(column=res->current_row))
    return 0;					/* Something is wrong */
  if (res->data)
  {
    start=0;
    prev_length=0;				/* Keep gcc happy */
    lengths=res->lengths;
    for (end=column+res->field_count+1 ; column != end ; column++,lengths++)
    {
      if (!*column)
      {
	*lengths=0;				/* Null */
	continue;
      }
      if (start)				/* Found end of prev string */
	*prev_length= (ulong) (*column-start-1);
      start= *column;
      prev_length=lengths;
    }
  }
  return res->lengths;
}

/**************************************************************************
  Move to a specific row and column
**************************************************************************/

void STDCALL
mysql_data_seek(MYSQL_RES *result, my_ulonglong row)
{
  MYSQL_ROWS	*tmp=0;
  DBUG_PRINT("info",("mysql_data_seek(%ld)",(long) row));
  if (result->data)
    for (tmp=result->data->data; row-- && tmp ; tmp = tmp->next) ;
  result->current_row=0;
  result->data_cursor = tmp;
}

/*************************************************************************
  put the row or field cursor one a position one got from mysql_row_tell()
  This doesn't restore any data. The next mysql_fetch_row or
  mysql_fetch_field will return the next row or field after the last used
*************************************************************************/

MYSQL_ROW_OFFSET STDCALL
mysql_row_seek(MYSQL_RES *result, MYSQL_ROW_OFFSET row)
{
  MYSQL_ROW_OFFSET return_value=result->data_cursor;
  result->current_row= 0;
  result->data_cursor= row;
  return return_value;
}


MYSQL_FIELD_OFFSET STDCALL
mysql_field_seek(MYSQL_RES *result, MYSQL_FIELD_OFFSET field_offset)
{
  MYSQL_FIELD_OFFSET return_value=result->current_field;
  result->current_field=field_offset;
  return return_value;
}

/*****************************************************************************
  List all databases
*****************************************************************************/

MYSQL_RES * STDCALL
mysql_list_dbs(MYSQL *mysql, const char *wild)
{
  char buff[255];
  DBUG_ENTER("mysql_list_dbs");

  append_wild(strmov(buff,"show databases"),buff+sizeof(buff),wild);
  if (mysql_query(mysql,buff))
    DBUG_RETURN(0);
  DBUG_RETURN (mysql_store_result(mysql));
}


/*****************************************************************************
  List all tables in a database
  If wild is given then only the tables matching wild is returned
*****************************************************************************/

MYSQL_RES * STDCALL
mysql_list_tables(MYSQL *mysql, const char *wild)
{
  char buff[255];
  DBUG_ENTER("mysql_list_tables");

  append_wild(strmov(buff,"show tables"),buff+sizeof(buff),wild);
  if (mysql_query(mysql,buff))
    DBUG_RETURN(0);
  DBUG_RETURN (mysql_store_result(mysql));
}


/**************************************************************************
  List all fields in a table
  If wild is given then only the fields matching wild is returned
  Instead of this use query:
  show fields in 'table' like "wild"
**************************************************************************/

MYSQL_RES * STDCALL
mysql_list_fields(MYSQL *mysql, const char *table, const char *wild)
{
  MYSQL_RES *result;
  MYSQL_DATA *query;
  char	     buff[257],*end;
  DBUG_ENTER("mysql_list_fields");
  DBUG_PRINT("enter",("table: '%s'  wild: '%s'",table,wild ? wild : ""));

  LINT_INIT(query);

  end=strmake(strmake(buff, table,128)+1,wild ? wild : "",128);
  if (simple_command(mysql,COM_FIELD_LIST,buff,(ulong) (end-buff),1) ||
      !(query = read_rows(mysql,(MYSQL_FIELD*) 0,6)))
    DBUG_RETURN(NULL);

  free_old_query(mysql);
  if (!(result = (MYSQL_RES *) my_malloc(sizeof(MYSQL_RES),
					 MYF(MY_WME | MY_ZEROFILL))))
  {
    free_rows(query);
    DBUG_RETURN(NULL);
  }
  result->field_alloc=mysql->field_alloc;
  mysql->fields=0;
  result->field_count = (uint) query->rows;
  result->fields= unpack_fields(query,&result->field_alloc,
				result->field_count,1,
				(my_bool) test(mysql->server_capabilities &
					       CLIENT_LONG_FLAG));
  result->eof=1;
  DBUG_RETURN(result);
}

/* List all running processes (threads) in server */

MYSQL_RES * STDCALL
mysql_list_processes(MYSQL *mysql)
{
  MYSQL_DATA *fields;
  uint field_count;
  uchar *pos;
  DBUG_ENTER("mysql_list_processes");

  LINT_INIT(fields);
  if (simple_command(mysql,COM_PROCESS_INFO,0,0,0))
    DBUG_RETURN(0);
  free_old_query(mysql);
  pos=(uchar*) mysql->net.read_pos;
  field_count=(uint) net_field_length(&pos);
  if (!(fields = read_rows(mysql,(MYSQL_FIELD*) 0,5)))
    DBUG_RETURN(NULL);
  if (!(mysql->fields=unpack_fields(fields,&mysql->field_alloc,field_count,0,
				    (my_bool) test(mysql->server_capabilities &
						   CLIENT_LONG_FLAG))))
    DBUG_RETURN(0);
  mysql->status=MYSQL_STATUS_GET_RESULT;
  mysql->field_count=field_count;
  DBUG_RETURN(mysql_store_result(mysql));
}


#ifdef USE_OLD_FUNCTIONS
int  STDCALL
mysql_create_db(MYSQL *mysql, const char *db)
{
  DBUG_ENTER("mysql_createdb");
  DBUG_PRINT("enter",("db: %s",db));
  DBUG_RETURN(simple_command(mysql,COM_CREATE_DB,db, (ulong) strlen(db),0));
}


int  STDCALL
mysql_drop_db(MYSQL *mysql, const char *db)
{
  DBUG_ENTER("mysql_drop_db");
  DBUG_PRINT("enter",("db: %s",db));
  DBUG_RETURN(simple_command(mysql,COM_DROP_DB,db,(ulong) strlen(db),0));
}
#endif


int STDCALL
mysql_shutdown(MYSQL *mysql)
{
  DBUG_ENTER("mysql_shutdown");
  DBUG_RETURN(simple_command(mysql,COM_SHUTDOWN,0,0,0));
}


int STDCALL
mysql_refresh(MYSQL *mysql,uint options)
{
  uchar bits[1];
  DBUG_ENTER("mysql_refresh");
  bits[0]= (uchar) options;
  DBUG_RETURN(simple_command(mysql,COM_REFRESH,(char*) bits,1,0));
}

int STDCALL
mysql_kill(MYSQL *mysql,ulong pid)
{
  char buff[12];
  DBUG_ENTER("mysql_kill");
  int4store(buff,pid);
  DBUG_RETURN(simple_command(mysql,COM_PROCESS_KILL,buff,4,0));
}


int STDCALL
mysql_dump_debug_info(MYSQL *mysql)
{
  DBUG_ENTER("mysql_dump_debug_info");
  DBUG_RETURN(simple_command(mysql,COM_DEBUG,0,0,0));
}

const char * STDCALL
mysql_stat(MYSQL *mysql)
{
  DBUG_ENTER("mysql_stat");
  if (simple_command(mysql,COM_STATISTICS,0,0,0))
    return mysql->net.last_error;
  mysql->net.read_pos[mysql->packet_length]=0;	/* End of stat string */
  if (!mysql->net.read_pos[0])
  {
    mysql->net.last_errno=CR_WRONG_HOST_INFO;
    strmov(mysql->net.last_error, ER(mysql->net.last_errno));
    return mysql->net.last_error;
  }
  DBUG_RETURN((char*) mysql->net.read_pos);
}


int STDCALL
mysql_ping(MYSQL *mysql)
{
  DBUG_ENTER("mysql_ping");
  DBUG_RETURN(simple_command(mysql,COM_PING,0,0,0));
}


const char * STDCALL
mysql_get_server_info(MYSQL *mysql)
{
  return((char*) mysql->server_version);
}


const char * STDCALL
mysql_get_host_info(MYSQL *mysql)
{
  return(mysql->host_info);
}


uint STDCALL
mysql_get_proto_info(MYSQL *mysql)
{
  return (mysql->protocol_version);
}

const char * STDCALL
mysql_get_client_info(void)
{
  return (char*) MYSQL_SERVER_VERSION;
}

ulong STDCALL mysql_get_client_version(void)
{
  return MYSQL_VERSION_ID;
}


int STDCALL
mysql_options(MYSQL *mysql,enum mysql_option option, const char *arg)
{
  DBUG_ENTER("mysql_option");
  DBUG_PRINT("enter",("option: %d",(int) option));
  switch (option) {
  case MYSQL_OPT_CONNECT_TIMEOUT:
    mysql->options.connect_timeout= *(uint*) arg;
    break;
  case MYSQL_OPT_COMPRESS:
    mysql->options.compress= 1;			/* Remember for connect */
    break;
  case MYSQL_OPT_NAMED_PIPE:
    mysql->options.named_pipe=1;		/* Force named pipe */
    break;
  case MYSQL_OPT_LOCAL_INFILE:			/* Allow LOAD DATA LOCAL ?*/
    if (!arg || test(*(uint*) arg))
      mysql->options.client_flag|= CLIENT_LOCAL_FILES;
    else
      mysql->options.client_flag&= ~CLIENT_LOCAL_FILES;
    break;
  case MYSQL_INIT_COMMAND:
    my_free(mysql->options.init_command,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.init_command=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_READ_DEFAULT_FILE:
    my_free(mysql->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.my_cnf_file=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_READ_DEFAULT_GROUP:
    my_free(mysql->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.my_cnf_group=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_SET_CHARSET_DIR:
    my_free(mysql->options.charset_dir,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.charset_dir=my_strdup(arg,MYF(MY_WME));
    break;
  case MYSQL_SET_CHARSET_NAME:
    my_free(mysql->options.charset_name,MYF(MY_ALLOW_ZERO_PTR));
    mysql->options.charset_name=my_strdup(arg,MYF(MY_WME));
    break;
  default:
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/****************************************************************************
  Functions to get information from the MySQL structure
  These are functions to make shared libraries more usable.
****************************************************************************/

/* MYSQL_RES */
my_ulonglong STDCALL mysql_num_rows(MYSQL_RES *res)
{
  return res->row_count;
}

unsigned int STDCALL mysql_num_fields(MYSQL_RES *res)
{
  return res->field_count;
}

my_bool STDCALL mysql_eof(MYSQL_RES *res)
{
  return res->eof;
}

MYSQL_FIELD * STDCALL mysql_fetch_field_direct(MYSQL_RES *res,uint fieldnr)
{
  return &(res)->fields[fieldnr];
}

MYSQL_FIELD * STDCALL mysql_fetch_fields(MYSQL_RES *res)
{
  return (res)->fields;
}

MYSQL_ROW_OFFSET STDCALL mysql_row_tell(MYSQL_RES *res)
{
  return res->data_cursor;
}

MYSQL_FIELD_OFFSET STDCALL mysql_field_tell(MYSQL_RES *res)
{
  return (res)->current_field;
}

/* MYSQL */

unsigned int STDCALL mysql_field_count(MYSQL *mysql)
{
  return mysql->last_used_con->field_count;
}

my_ulonglong STDCALL mysql_affected_rows(MYSQL *mysql)
{
  return mysql->last_used_con->affected_rows;
}

my_ulonglong STDCALL mysql_insert_id(MYSQL *mysql)
{
  return mysql->last_used_con->insert_id;
}

uint STDCALL mysql_errno(MYSQL *mysql)
{
  return mysql->net.last_errno;
}

const char * STDCALL mysql_error(MYSQL *mysql)
{
  return mysql->net.last_error;
}

const char *STDCALL mysql_info(MYSQL *mysql)
{
  return mysql->info;
}

ulong STDCALL mysql_thread_id(MYSQL *mysql)
{
  return (mysql)->thread_id;
}

const char * STDCALL mysql_character_set_name(MYSQL *mysql)
{
  return mysql->charset->name;
}


uint STDCALL mysql_thread_safe(void)
{
#ifdef THREAD
  return 1;
#else
  return 0;
#endif
}

/****************************************************************************
  Some support functions
****************************************************************************/

/*
  Functions called my my_net_init() to set some application specific variables
*/

void my_net_local_init(NET *net)
{
  net->max_packet=   (uint) net_buffer_length;
  net->read_timeout= (uint) net_read_timeout;
  net->write_timeout=(uint) net_write_timeout;
  net->retry_count=  1;
  net->max_packet_size= max(net_buffer_length, max_allowed_packet);
}

/*
  This function is used to create HEX string that you
  can use in a SQL statement in of the either ways:
    INSERT INTO blob_column VALUES (0xAABBCC);  (any MySQL version)
    INSERT INTO blob_column VALUES (X'AABBCC'); (4.1 and higher)
  
  The string in "from" is encoded to a HEX string.
  The result is placed in "to" and a terminating null byte is appended.
  
  The string pointed to by "from" must be "length" bytes long.
  You must allocate the "to" buffer to be at least length*2+1 bytes long.
  Each character needs two bytes, and you need room for the terminating
  null byte. When mysql_hex_string() returns, the contents of "to" will
  be a null-terminated string. The return value is the length of the
  encoded string, not including the terminating null character.
*/

ulong mysql_hex_string(char *to, const char *from, ulong length)
{
  char *to0= to;
  const char *end;
            
  for (end= from + length; from < end; from++)
  {
    *to++= _dig_vec[((unsigned char) *from) >> 4];
    *to++= _dig_vec[((unsigned char) *from) & 0x0F];
  }
  *to= '\0';
  return (ulong) (to-to0);
}

/*
  Add escape characters to a string (blob?) to make it suitable for a insert
  to should at least have place for length*2+1 chars
  Returns the length of the to string
*/

ulong STDCALL
mysql_escape_string(char *to,const char *from,ulong length)
{
  return mysql_sub_escape_string(default_charset_info,to,from,length);
}

ulong STDCALL
mysql_real_escape_string(MYSQL *mysql, char *to,const char *from,
			 ulong length)
{
  return mysql_sub_escape_string(mysql->charset,to,from,length);
}


static ulong
mysql_sub_escape_string(CHARSET_INFO *charset_info, char *to,
			const char *from, ulong length)
{
  const char *to_start=to;
  const char *end;
#ifdef USE_MB
  my_bool use_mb_flag=use_mb(charset_info);
#endif
  for (end=from+length; from != end ; from++)
  {
#ifdef USE_MB
    int l;
    if (use_mb_flag && (l = my_ismbchar(charset_info, from, end)))
    {
      while (l--)
	  *to++ = *from++;
      from--;
      continue;
    }
#endif
    switch (*from) {
    case 0:				/* Must be escaped for 'mysql' */
      *to++= '\\';
      *to++= '0';
      break;
    case '\n':				/* Must be escaped for logs */
      *to++= '\\';
      *to++= 'n';
      break;
    case '\r':
      *to++= '\\';
      *to++= 'r';
      break;
    case '\\':
      *to++= '\\';
      *to++= '\\';
      break;
    case '\'':
      *to++= '\\';
      *to++= '\'';
      break;
    case '"':				/* Better safe than sorry */
      *to++= '\\';
      *to++= '"';
      break;
    case '\032':			/* This gives problems on Win32 */
      *to++= '\\';
      *to++= 'Z';
      break;
    default:
      *to++= *from;
    }
  }
  *to=0;
  return (ulong) (to-to_start);
}


char * STDCALL
mysql_odbc_escape_string(MYSQL *mysql,
			 char *to, ulong to_length,
			 const char *from, ulong from_length,
			 void *param,
			 char * (*extend_buffer)
			 (void *, char *, ulong *))
{
  char *to_end=to+to_length-5;
  const char *end;
#ifdef USE_MB
  my_bool use_mb_flag=use_mb(mysql->charset);
#endif

  for (end=from+from_length; from != end ; from++)
  {
    if (to >= to_end)
    {
      to_length = (ulong) (end-from)+512;	/* We want this much more */
      if (!(to=(*extend_buffer)(param, to, &to_length)))
	return to;
      to_end=to+to_length-5;
    }
#ifdef USE_MB
    {
      int l;
      if (use_mb_flag && (l = my_ismbchar(mysql->charset, from, end)))
      {
	while (l--)
	  *to++ = *from++;
	from--;
	continue;
      }
    }
#endif
    switch (*from) {
    case 0:				/* Must be escaped for 'mysql' */
      *to++= '\\';
      *to++= '0';
      break;
    case '\n':				/* Must be escaped for logs */
      *to++= '\\';
      *to++= 'n';
      break;
    case '\r':
      *to++= '\\';
      *to++= 'r';
      break;
    case '\\':
      *to++= '\\';
      *to++= '\\';
      break;
    case '\'':
      *to++= '\\';
      *to++= '\'';
      break;
    case '"':				/* Better safe than sorry */
      *to++= '\\';
      *to++= '"';
      break;
    case '\032':			/* This gives problems on Win32 */
      *to++= '\\';
      *to++= 'Z';
      break;
    default:
      *to++= *from;
    }
  }
  return to;
}

void STDCALL
myodbc_remove_escape(MYSQL *mysql,char *name)
{
  char *to;
#ifdef USE_MB
  my_bool use_mb_flag=use_mb(mysql->charset);
  char *end;
  LINT_INIT(end);
  if (use_mb_flag)
    for (end=name; *end ; end++) ;
#endif

  for (to=name ; *name ; name++)
  {
#ifdef USE_MB
    int l;
    if (use_mb_flag && (l = my_ismbchar( mysql->charset, name , end ) ) )
    {
      while (l--)
	*to++ = *name++;
      name--;
      continue;
    }
#endif
    if (*name == '\\' && name[1])
      name++;
    *to++= *name;
  }
  *to=0;
}
