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

/*
 mini MySQL client to be included into the server to do server to server
 commincation by Sasha Pachev

 Note: all file-global symbols must begin with mc_ , even the static ones, just
 in case we decide to make them external at some point
 */

#if defined(__WIN__)
#include <winsock.h>
#include <odbcinst.h>
/* Disable alarms */
typedef my_bool ALARM;
#define thr_alarm_init(A) (*(A))=0
#define thr_alarm_in_use(A) (*(A))
#define thr_end_alarm(A)
#define thr_alarm(A,B,C) local_thr_alarm((A),(B),(C))
inline int local_thr_alarm(my_bool *A,int B __attribute__((unused)),ALARM *C __attribute__((unused)))
{
  *A=1;
  return 0;
}
#define thr_got_alarm(A) 0
#endif

#include <my_global.h>
#include <mysql_embed.h>
#include <mysql_com.h>
#include <violite.h>
#include <my_sys.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "mini_client.h"
#include "mysql_version.h"
#include "mysqld_error.h"
#include "errmsg.h"

#ifdef EMBEDDED_LIBRARY
#define net_read_timeout net_read_timeout1
#define net_write_timeout net_write_timeout1
#endif

#if defined( OS2) && defined( MYSQL_SERVER)
#undef  ER
#define ER CER
#endif

extern ulong net_read_timeout;

extern "C" {					// Because of SCO 3.2V4.2
#include <sys/stat.h>
#include <signal.h>
#ifdef	 HAVE_PWD_H
#include <pwd.h>
#endif
#if !defined(MSDOS) && !defined(__WIN__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SELECT_H
#  include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#if defined(THREAD)
#include <my_pthread.h>				/* because of signal()	*/
#include <thr_alarm.h>
#endif
#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif

}

static void mc_free_rows(MYSQL_DATA *cur);
static MYSQL_FIELD *unpack_fields(MYSQL_DATA *data,MEM_ROOT *alloc,uint fields,
		  	          my_bool default_value,
				  my_bool long_flag_protocol);

static void mc_end_server(MYSQL *mysql);
static int mc_sock_connect(File s, const struct sockaddr *name, uint namelen, uint to);
static void mc_free_old_query(MYSQL *mysql);
static int mc_send_file_to_server(MYSQL *mysql, const char *filename);
static my_ulonglong mc_net_field_length_ll(uchar **packet);
static ulong mc_net_field_length(uchar **packet);
static int mc_read_one_row(MYSQL *mysql,uint fields,MYSQL_ROW row,
			   ulong *lengths);
static MYSQL_DATA *mc_read_rows(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
				uint fields);



#define CLIENT_CAPABILITIES	(CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_LOCAL_FILES)

#if defined(MSDOS) || defined(__WIN__)
#define perror(A)
#else
#include <errno.h>
#define SOCKET_ERROR -1
#endif

#ifdef __WIN__
static my_bool is_NT(void)
{
  char *os=getenv("OS");
  return (os && !strcmp(os, "Windows_NT")) ? 1 : 0;
}
#endif

/*
** Create a named pipe connection
*/

#ifdef __WIN__

HANDLE create_named_pipe(NET *net, uint connect_timeout, char **arg_host,
			 char **arg_unix_socket)
{
  HANDLE hPipe=INVALID_HANDLE_VALUE;
  char szPipeName [ 257 ];
  DWORD dwMode;
  int i;
  my_bool testing_named_pipes=0;
  char *host= *arg_host, *unix_socket= *arg_unix_socket;

  if (!host || !strcmp(host,LOCAL_HOST))
    host=LOCAL_HOST_NAMEDPIPE;

  sprintf( szPipeName, "\\\\%s\\pipe\\%s", host, unix_socket);
  DBUG_PRINT("info",("Server name: '%s'.  Named Pipe: %s",
		     host, unix_socket));

  for (i=0 ; i < 100 ; i++)			/* Don't retry forever */
  {
    if ((hPipe = CreateFile(szPipeName,
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
    if (! WaitNamedPipe(szPipeName, connect_timeout*1000) )
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


/****************************************************************************
** Init MySQL structure or allocate one
****************************************************************************/

MYSQL * STDCALL
mc_mysql_init(MYSQL *mysql)
{
  init_client_errs();
  if (!mysql)
  {
    if (!(mysql=(MYSQL*) my_malloc(sizeof(*mysql),MYF(MY_WME | MY_ZEROFILL))))
      return 0;
    mysql->free_me=1;
    mysql->net.vio = 0;
  }
  else
    bzero((char*) (mysql),sizeof(*(mysql)));
#ifdef __WIN__
  mysql->options.connect_timeout=20;
#endif
  return mysql;
}

/**************************************************************************
** Shut down connection
**************************************************************************/

static void
mc_end_server(MYSQL *mysql)
{
  DBUG_ENTER("mc_end_server");
  if (mysql->net.vio != 0)
  {
    DBUG_PRINT("info",("Net: %s", vio_description(mysql->net.vio)));
    vio_delete(mysql->net.vio);
    mysql->net.vio= 0;          /* Marker */
  }
  net_end(&mysql->net);
  mc_free_old_query(mysql);
  DBUG_VOID_RETURN;
}

static void mc_free_old_query(MYSQL *mysql)
{
  DBUG_ENTER("mc_free_old_query");
  if (mysql->fields)
    free_root(&mysql->field_alloc,MYF(0));
  else
    init_alloc_root(&mysql->field_alloc,8192,0); /* Assume rowlength < 8192 */
  mysql->fields=0;
  mysql->field_count=0;				/* For API */
  DBUG_VOID_RETURN;
}


/****************************************************************************
* A modified version of connect().  mc_sock_connect() allows you to specify
* a timeout value, in seconds, that we should wait until we
* derermine we can't connect to a particular host.  If timeout is 0,
* mc_sock_connect() will behave exactly like connect().
*
* Base version coded by Steve Bernacki, Jr. <steve@navinet.net>
*****************************************************************************/

static int mc_sock_connect(my_socket s, const struct sockaddr *name,
			   uint namelen, uint to)
{
#if defined(__WIN__) || defined(OS2)
  return connect(s, (struct sockaddr*) name, namelen);
#else
  int flags, res, s_err;
  SOCKOPT_OPTLEN_TYPE s_err_size = sizeof(uint);
  fd_set sfds;
  struct timeval tv;

  /* If they passed us a timeout of zero, we should behave
   * exactly like the normal connect() call does.
   */

  if (to == 0)
    return connect(s, (struct sockaddr*) name, namelen);

  flags = fcntl(s, F_GETFL, 0);		  /* Set socket to not block */
#ifdef O_NONBLOCK
  fcntl(s, F_SETFL, flags | O_NONBLOCK);  /* and save the flags..  */
#endif

  res = connect(s, (struct sockaddr*) name, namelen);
  s_err = errno;			/* Save the error... */
  fcntl(s, F_SETFL, flags);
  if ((res != 0) && (s_err != EINPROGRESS))
  {
    errno = s_err;			/* Restore it */
    return(-1);
  }
  if (res == 0)				/* Connected quickly! */
    return(0);

  /* Otherwise, our connection is "in progress."  We can use
   * the select() call to wait up to a specified period of time
   * for the connection to suceed.  If select() returns 0
   * (after waiting howevermany seconds), our socket never became
   * writable (host is probably unreachable.)  Otherwise, if
   * select() returns 1, then one of two conditions exist:
   *
   * 1. An error occured.  We use getsockopt() to check for this.
   * 2. The connection was set up sucessfully: getsockopt() will
   * return 0 as an error.
   *
   * Thanks goes to Andrew Gierth <andrew@erlenstar.demon.co.uk>
   * who posted this method of timing out a connect() in
   * comp.unix.programmer on August 15th, 1997.
   */

  FD_ZERO(&sfds);
  FD_SET(s, &sfds);
  tv.tv_sec = (long) to;
  tv.tv_usec = 0;
#ifdef HPUX
  res = select(s+1, NULL, (int*) &sfds, NULL, &tv);
#else
  res = select(s+1, NULL, &sfds, NULL, &tv);
#endif
  if (res <= 0)					/* Never became writable */
    return(-1);

  /* select() returned something more interesting than zero, let's
   * see if we have any errors.  If the next two statements pass,
   * we've got an open socket!
   */

  s_err=0;
  if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0)
    return(-1);

  if (s_err)
  {						// getsockopt() could succeed
    errno = s_err;
    return(-1);					// but return an error...
  }
  return(0);					/* It's all good! */
#endif
}

/*****************************************************************************
** read a packet from server. Give error message if socket was down
** or packet is an error message
*****************************************************************************/

ulong STDCALL
mc_net_safe_read(MYSQL *mysql)
{
  NET *net= &mysql->net;
  ulong len=0;

  if (net->vio != 0)
    len=my_net_read(net);

  if (len == packet_error || len == 0)
  {
    DBUG_PRINT("error",("Wrong connection or packet. fd: %s  len: %d",
			vio_description(net->vio),len));
    if (socket_errno != SOCKET_EINTR)
    {
      mc_end_server(mysql);
      if(net->last_errno != ER_NET_PACKET_TOO_LARGE)
      {
	net->last_errno=CR_SERVER_LOST;
	strmov(net->last_error,ER(net->last_errno));
      }
      else
	strmov(net->last_error, "Packet too large - increase \
max_allowed_packet on this server");
    }	
    return(packet_error);
  }
  if (net->read_pos[0] == 255)
  {
    if (len > 3)
    {
      char *pos=(char*) net->read_pos+1;
      if (mysql->protocol_version > 9)
      {						/* New client protocol */
	net->last_errno=uint2korr(pos);
	pos+=2;
	len-=2;
	if(!net->last_errno)
	  net->last_errno = CR_UNKNOWN_ERROR;
      }
      else
      {
	net->last_errno=CR_UNKNOWN_ERROR;
	len--;
      }
      (void) strmake(net->last_error,(char*) pos,
		     min(len,sizeof(net->last_error)-1));
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


char * STDCALL mc_mysql_error(MYSQL *mysql)
{
  return (mysql)->net.last_error;
}

int STDCALL mc_mysql_errno(MYSQL *mysql)
{
  return (mysql)->net.last_errno;
}

my_bool STDCALL mc_mysql_reconnect(MYSQL *mysql)
{
  MYSQL tmp_mysql;
  DBUG_ENTER("mc_mysql_reconnect");

  if (!mysql->reconnect)
    DBUG_RETURN(1);

  mc_mysql_init(&tmp_mysql);
  tmp_mysql.options=mysql->options;
  if (!mc_mysql_connect(&tmp_mysql,mysql->host,mysql->user,mysql->passwd,
			  mysql->db, mysql->port, mysql->unix_socket,
			  mysql->client_flag))
    {
      tmp_mysql.reconnect=0;
      mc_mysql_close(&tmp_mysql); 
      DBUG_RETURN(1);
    }
  tmp_mysql.free_me=mysql->free_me;
  mysql->free_me=0;
  bzero((char*) &mysql->options,sizeof(&mysql->options));
  mc_mysql_close(mysql);
  *mysql=tmp_mysql;
  net_clear(&mysql->net);
  mysql->affected_rows= ~(my_ulonglong) 0;
  DBUG_RETURN(0);
}



int STDCALL
mc_simple_command(MYSQL *mysql,enum enum_server_command command,
		  const char *arg, uint length, my_bool skipp_check)
{
  NET *net= &mysql->net;
  int result= -1;

  if (mysql->net.vio == 0)
  {						/* Do reconnect if possible */
    if (mc_mysql_reconnect(mysql))
    {
      net->last_errno=CR_SERVER_GONE_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto end;
    }
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
			length ? length :(uint) strlen(arg)))
  {
    DBUG_PRINT("error",("Can't send command to server. Error: %d",socket_errno));
    mc_end_server(mysql);
    if (mc_mysql_reconnect(mysql) ||
	net_write_command(net,(uchar) command,arg,
			  length ? length :(uint) strlen(arg)))
    {
      net->last_errno=CR_SERVER_GONE_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto end;
    }
  }
  result=0;
  if (!skipp_check)
    result= ((mysql->packet_length=mc_net_safe_read(mysql)) == packet_error ?
	     -1 : 0);
 end:
  return result;
}


MYSQL * STDCALL
mc_mysql_connect(MYSQL *mysql,const char *host, const char *user,
		   const char *passwd, const char *db,
		   uint port, const char *unix_socket,uint client_flag)
{
  char		buff[100],*end,*host_info;
  my_socket	sock;
  ulong		ip_addr;
  struct	sockaddr_in sock_addr;
  ulong		pkt_length;
  NET		*net= &mysql->net;
  thr_alarm_t   alarmed;
  ALARM alarm_buff;

#ifdef __WIN__
  HANDLE	hPipe=INVALID_HANDLE_VALUE;
#endif
#ifdef HAVE_SYS_UN_H
  struct	sockaddr_un UNIXaddr;
#endif
  DBUG_ENTER("mc_mysql_connect");

  DBUG_PRINT("enter",("host: %s  db: %s  user: %s",
		      host ? host : "(Null)",
		      db ? db : "(Null)",
		      user ? user : "(Null)"));
  thr_alarm_init(&alarmed);
  thr_alarm(&alarmed,(uint) net_read_timeout,&alarm_buff);

  bzero((char*) &mysql->options,sizeof(mysql->options));
  net->vio = 0;				/* If something goes wrong */
  mysql->charset=default_charset_info;  /* Set character set */
  if (!port)
    port = MYSQL_PORT;			/* Should always be set by mysqld */
  if (!unix_socket)
    unix_socket=MYSQL_UNIX_ADDR;

  mysql->reconnect=1;			/* Reconnect as default */

  /*
  ** Grab a socket and connect it to the server
  */

#if defined(HAVE_SYS_UN_H)
  if (!host || !strcmp(host,LOCAL_HOST))
  {
    host=LOCAL_HOST;
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
    strmov(UNIXaddr.sun_path, unix_socket);
    if (mc_sock_connect(sock,(struct sockaddr *) &UNIXaddr, sizeof(UNIXaddr),
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
	  goto error;		/* User only requested named pipes */
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
    if (!host)
      host=LOCAL_HOST;
    sprintf(host_info=buff,ER(CR_TCP_CONNECTION),host);
    DBUG_PRINT("info",("Server name: '%s'.  TCP sock: %d", host,port));
    if ((sock = socket(AF_INET,SOCK_STREAM,0)) == SOCKET_ERROR)
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
#if defined(HAVE_GETHOSTBYNAME_R) && defined(_REENTRANT) && defined(THREAD)
    {
      int tmp_errno;
      struct hostent tmp_hostent,*hp;
      char buff2[GETHOSTBYNAME_BUFF_SIZE];
      hp = my_gethostbyname_r(host,&tmp_hostent,buff2,sizeof(buff2),
			      &tmp_errno);
      if (!hp)
      {
	net->last_errno=CR_UNKNOWN_HOST;
	sprintf(net->last_error, ER(CR_UNKNOWN_HOST), host, tmp_errno);
	goto error;
      }
      memcpy(&sock_addr.sin_addr,hp->h_addr, (size_t) hp->h_length);
    }
#else
    {
      struct hostent *hp;
      if (!(hp=gethostbyname(host)))
      {
	net->last_errno=CR_UNKNOWN_HOST;
	sprintf(net->last_error, ER(CR_UNKNOWN_HOST), host, socket_errno);
	goto error;
      }
      memcpy(&sock_addr.sin_addr,hp->h_addr, (size_t) hp->h_length);
    }
#endif
    sock_addr.sin_port = (ushort) htons((ushort) port);
    if (mc_sock_connect(sock,(struct sockaddr *) &sock_addr, sizeof(sock_addr),
			mysql->options.connect_timeout) <0)
    {
      DBUG_PRINT("error",("Got error %d on connect to '%s'",
			  socket_errno,host));
      net->last_errno= CR_CONN_HOST_ERROR;
      sprintf(net->last_error ,ER(CR_CONN_HOST_ERROR), host, socket_errno);
      if (thr_alarm_in_use(&alarmed))
	thr_end_alarm(&alarmed);
      goto error;
    }
    if (thr_alarm_in_use(&alarmed))
      thr_end_alarm(&alarmed);
  }

  if (!net->vio || my_net_init(net, net->vio))
  {
    vio_delete(net->vio);
    net->vio = 0; // safety
    net->last_errno=CR_OUT_OF_MEMORY;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }
  vio_keepalive(net->vio,TRUE);

  /* Get version info */
  mysql->protocol_version= PROTOCOL_VERSION;	/* Assume this */
  if ((pkt_length=mc_net_safe_read(mysql)) == packet_error)
    goto error;

  /* Check if version of protocoll matches current one */

  mysql->protocol_version= net->read_pos[0];
  DBUG_DUMP("packet",(char*) net->read_pos,10);
  DBUG_PRINT("info",("mysql protocol version %d, server=%d",
		     PROTOCOL_VERSION, mysql->protocol_version));
  if (mysql->protocol_version != PROTOCOL_VERSION &&
      mysql->protocol_version != PROTOCOL_VERSION-1)
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
  if (pkt_length > (uint) (end+9 - (char*) net->read_pos))
    mysql->server_capabilities=uint2korr(end+9);

  /* Save connection information */
  if (!user) user="";
  if (!passwd) passwd="";
  if (!my_multi_malloc(MYF(0),
		       &mysql->host_info, (uint) strlen(host_info)+1,
		       &mysql->host,      (uint) strlen(host)+1,
		       &mysql->unix_socket,
		       unix_socket ? (uint) strlen(unix_socket)+1 : (uint) 1,
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
  mysql->client_flag=client_flag | mysql->options.client_flag;
  DBUG_PRINT("info",("Server version = '%s'  capabilites: %ld",
		     mysql->server_version,mysql->server_capabilities));

  /* Send client information for access check */
  client_flag|=CLIENT_CAPABILITIES;

#ifdef HAVE_OPENSSL
  if (mysql->options.use_ssl)
    client_flag|=CLIENT_SSL;
#endif /* HAVE_OPENSSL */

  if (db)
    client_flag|=CLIENT_CONNECT_WITH_DB;
#ifdef HAVE_COMPRESS
  if (mysql->server_capabilities & CLIENT_COMPRESS &&
      (mysql->options.compress || client_flag & CLIENT_COMPRESS))
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
  /* Oops.. are we careful enough to not send ANY information */
  /* without encryption? */
  if (client_flag & CLIENT_SSL)
  {
    if (my_net_write(net,buff,(uint) (2)) || net_flush(net))
      goto error;
    /* Do the SSL layering. */
    DBUG_PRINT("info", ("IO layer change in progress..."));
    DBUG_PRINT("info", ("IO context %p",((struct st_VioSSLConnectorFd*)mysql->connector_fd)->ssl_context_));
    sslconnect((struct st_VioSSLConnectorFd*)(mysql->connector_fd),mysql->net.vio,60L);
    DBUG_PRINT("info", ("IO layer change done!"));
  }
#endif /* HAVE_OPENSSL */
  int3store(buff+2,max_allowed_packet);

  
  if (user && user[0])
    strmake(buff+5,user,32);
  else
    {
      user = getenv("USER");
      if(!user) user = "mysql";
       strmov((char*) buff+5, user );
    }

  DBUG_PRINT("info",("user: %s",buff+5));
  end=scramble(strend(buff+5)+1, mysql->scramble_buff, passwd,
	       (my_bool) (mysql->protocol_version == 9));
  if (db)
  {
    end=strmov(end+1,db);
    mysql->db=my_strdup(db,MYF(MY_WME));
  }
  if (my_net_write(net,buff,(uint) (end-buff)) || net_flush(net) ||
      mc_net_safe_read(mysql) == packet_error)
    goto error;
  if (client_flag & CLIENT_COMPRESS)		/* We will use compression */
    net->compress=1;
  DBUG_PRINT("exit",("Mysql handler: %lx",mysql));
  DBUG_RETURN(mysql);

error:
  DBUG_PRINT("error",("message: %u (%s)",net->last_errno,net->last_error));
  {
    /* Free alloced memory */
    my_bool free_me=mysql->free_me;
    mc_end_server(mysql);
    mysql->free_me=0;
    mc_mysql_close(mysql);
    mysql->free_me=free_me;
  }
  DBUG_RETURN(0);
}


#ifdef HAVE_OPENSSL
/*
**************************************************************************
** Free strings in the SSL structure and clear 'use_ssl' flag.
** NB! Errors are not reported until you do mysql_real_connect.
**************************************************************************
*/
int STDCALL
mysql_ssl_clear(MYSQL *mysql)
{
  my_free(mysql->options.ssl_key, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_cert, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_ca, MYF(MY_ALLOW_ZERO_PTR));
  my_free(mysql->options.ssl_capath, MYF(MY_ALLOW_ZERO_PTR));
  mysql->options.ssl_key = 0;
  mysql->options.ssl_cert = 0;
  mysql->options.ssl_ca = 0;
  mysql->options.ssl_capath = 0;
  mysql->options.use_ssl = FALSE;
  my_free(mysql->connector_fd,MYF(MY_ALLOW_ZERO_PTR));
  mysql->connector_fd = 0;
  return 0;
}
#endif /* HAVE_OPENSSL */

/*************************************************************************
** Send a QUIT to the server and close the connection
** If handle is alloced by mysql connect free it.
*************************************************************************/

void STDCALL
mc_mysql_close(MYSQL *mysql)
{
  DBUG_ENTER("mysql_close");
  if (mysql)					/* Some simple safety */
  {
    if (mysql->net.vio != 0)
    {
      mc_free_old_query(mysql);
      mysql->status=MYSQL_STATUS_READY; /* Force command */
      mysql->reconnect=0;
      mc_simple_command(mysql,COM_QUIT,NullS,0,1);
      mc_end_server(mysql);
    }
    my_free((gptr) mysql->host_info,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->user,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->passwd,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));
    /* Clear pointers for better safety */
    mysql->host_info=mysql->user=mysql->passwd=mysql->db=0;
    bzero((char*) &mysql->options,sizeof(mysql->options));
    mysql->net.vio = 0;
#ifdef HAVE_OPENSSL
    mysql_ssl_clear(mysql);
#endif /* HAVE_OPENSSL */
    if (mysql->free_me)
      my_free((gptr) mysql,MYF(0));
  }
  DBUG_VOID_RETURN;
}

void STDCALL mc_mysql_free_result(MYSQL_RES *result)
{
  DBUG_ENTER("mc_mysql_free_result");
  DBUG_PRINT("enter",("mysql_res: %lx",result));
  if (result)
  {
    if (result->handle && result->handle->status == MYSQL_STATUS_USE_RESULT)
    {
      DBUG_PRINT("warning",("Not all rows in set were read; Ignoring rows"));
      for (;;)
      {
	ulong pkt_len;
	if ((pkt_len=mc_net_safe_read(result->handle)) == packet_error)
	  break;
	if (pkt_len == 1 && result->handle->net.read_pos[0] == 254)
	  break;				/* End of data */
      }
      result->handle->status=MYSQL_STATUS_READY;
    }
    mc_free_rows(result->data);
    if (result->fields)
      free_root(&result->field_alloc,MYF(0));
    if (result->row)
      my_free((gptr) result->row,MYF(0));
    my_free((gptr) result,MYF(0));
  }
  DBUG_VOID_RETURN;
}

static void mc_free_rows(MYSQL_DATA *cur)
{
  if (cur)
  {
    free_root(&cur->alloc,MYF(0));
    my_free((gptr) cur,MYF(0));
  }
}

static MYSQL_FIELD *
mc_unpack_fields(MYSQL_DATA *data,MEM_ROOT *alloc,uint fields,
	      my_bool default_value, my_bool long_flag_protocol)
{
  MYSQL_ROWS	*row;
  MYSQL_FIELD	*field,*result;
  DBUG_ENTER("unpack_fields");

  field=result=(MYSQL_FIELD*) alloc_root(alloc,sizeof(MYSQL_FIELD)*fields);
  if (!result)
    DBUG_RETURN(0);

  for (row=data->data; row ; row = row->next,field++)
  {
    field->table=  strdup_root(alloc,(char*) row->data[0]);
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
  mc_free_rows(data);				/* Free old data */
  DBUG_RETURN(result);
}

int STDCALL
mc_mysql_send_query(MYSQL* mysql, const char* query, uint length)
{
  return mc_simple_command(mysql, COM_QUERY, query, length, 1);
}

int STDCALL mc_mysql_read_query_result(MYSQL *mysql)
{
  uchar *pos;
  ulong field_count;
  MYSQL_DATA *fields;
  ulong length;
  DBUG_ENTER("mc_mysql_read_query_result");

  if ((length = mc_net_safe_read(mysql)) == packet_error)
    DBUG_RETURN(-1);
  mc_free_old_query(mysql);			/* Free old result */
get_info:
  pos=(uchar*) mysql->net.read_pos;
  if ((field_count= mc_net_field_length(&pos)) == 0)
  {
    mysql->affected_rows= mc_net_field_length_ll(&pos);
    mysql->insert_id=	  mc_net_field_length_ll(&pos);
    if (mysql->server_capabilities & CLIENT_TRANSACTIONS)
    {
      mysql->server_status=uint2korr(pos); pos+=2;
    }
    if (pos < mysql->net.read_pos+length && mc_net_field_length(&pos))
      mysql->info=(char*) pos;
    DBUG_RETURN(0);
  }
  if (field_count == NULL_LENGTH)		/* LOAD DATA LOCAL INFILE */
  {
    int error=mc_send_file_to_server(mysql,(char*) pos);
    if ((length=mc_net_safe_read(mysql)) == packet_error || error)
      DBUG_RETURN(-1);
    goto get_info;				/* Get info packet */
  }
  if (!(mysql->server_status & SERVER_STATUS_AUTOCOMMIT))
    mysql->server_status|= SERVER_STATUS_IN_TRANS;

  mysql->extra_info= mc_net_field_length_ll(&pos); /* Maybe number of rec */
  if (!(fields=mc_read_rows(mysql,(MYSQL_FIELD*) 0,5)))
    DBUG_RETURN(-1);
  if (!(mysql->fields=mc_unpack_fields(fields,&mysql->field_alloc,
				    (uint) field_count,0,
				    (my_bool) test(mysql->server_capabilities &
						   CLIENT_LONG_FLAG))))
    DBUG_RETURN(-1);
  mysql->status=MYSQL_STATUS_GET_RESULT;
  mysql->field_count=field_count;
  DBUG_RETURN(0);
}

int STDCALL mc_mysql_query(MYSQL *mysql, const char *query, uint length)
{
  DBUG_ENTER("mysql_real_query");
  DBUG_PRINT("enter",("handle: %lx",mysql));
  DBUG_PRINT("query",("Query = \"%s\"",query));
  if(!length)
    length = strlen(query);
  if (mc_simple_command(mysql,COM_QUERY,query,length,1))
    DBUG_RETURN(-1);
  DBUG_RETURN(mc_mysql_read_query_result(mysql));
}

static int mc_send_file_to_server(MYSQL *mysql, const char *filename)
{
  int fd, readcount;
  char buf[IO_SIZE*15],*tmp_name;
  DBUG_ENTER("send_file_to_server");

  fn_format(buf,filename,"","",4);		/* Convert to client format */
  if (!(tmp_name=my_strdup(buf,MYF(0))))
  {
    strmov(mysql->net.last_error, ER(mysql->net.last_errno=CR_OUT_OF_MEMORY));
    DBUG_RETURN(-1);
  }
  if ((fd = my_open(tmp_name,O_RDONLY, MYF(0))) < 0)
  {
    mysql->net.last_errno=EE_FILENOTFOUND;
    sprintf(buf,EE(mysql->net.last_errno),tmp_name,errno);
    strmake(mysql->net.last_error,buf,sizeof(mysql->net.last_error)-1);
    my_net_write(&mysql->net,"",0); net_flush(&mysql->net);
    my_free(tmp_name,MYF(0));
    DBUG_RETURN(-1);
  }

  while ((readcount = (int) my_read(fd,(byte*) buf,sizeof(buf),MYF(0))) > 0)
  {
    if (my_net_write(&mysql->net,buf,readcount))
    {
      mysql->net.last_errno=CR_SERVER_LOST;
      strmov(mysql->net.last_error,ER(mysql->net.last_errno));
      DBUG_PRINT("error",("Lost connection to MySQL server during LOAD DATA of local file"));
      (void) my_close(fd,MYF(0));
      my_free(tmp_name,MYF(0));
      DBUG_RETURN(-1);
    }
  }
  (void) my_close(fd,MYF(0));
  /* Send empty packet to mark end of file */
  if (my_net_write(&mysql->net,"",0) || net_flush(&mysql->net))
  {
    mysql->net.last_errno=CR_SERVER_LOST;
    sprintf(mysql->net.last_error,ER(mysql->net.last_errno),errno);
    my_free(tmp_name,MYF(0));
    DBUG_RETURN(-1);
  }
  if (readcount < 0)
  {
    mysql->net.last_errno=EE_READ; /* the errmsg for not entire file read */
    sprintf(buf,EE(mysql->net.last_errno),tmp_name,errno);
    strmake(mysql->net.last_error,buf,sizeof(mysql->net.last_error)-1);
    my_free(tmp_name,MYF(0));
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/* Get the length of next field. Change parameter to point at fieldstart */
static ulong mc_net_field_length(uchar **packet)
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

static my_ulonglong mc_net_field_length_ll(uchar **packet)
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

/* Read all rows (fields or data) from server */

static MYSQL_DATA *mc_read_rows(MYSQL *mysql,MYSQL_FIELD *mysql_fields,
			     uint fields)
{
  uint	field;
  ulong pkt_len;
  ulong len;
  uchar *cp;
  char	*to;
  MYSQL_DATA *result;
  MYSQL_ROWS **prev_ptr,*cur;
  NET *net = &mysql->net;
  DBUG_ENTER("mc_read_rows");

  if ((pkt_len=mc_net_safe_read(mysql)) == packet_error)
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
      mc_free_rows(result);
      net->last_errno=CR_OUT_OF_MEMORY;
      strmov(net->last_error,ER(net->last_errno));
      DBUG_RETURN(0);
    }
    *prev_ptr=cur;
    prev_ptr= &cur->next;
    to= (char*) (cur->data+fields+1);
    for (field=0 ; field < fields ; field++)
    {
      if ((len=(ulong) mc_net_field_length(&cp)) == NULL_LENGTH)
      {						/* null field */
	cur->data[field] = 0;
      }
      else
      {
	cur->data[field] = to;
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
    if ((pkt_len=mc_net_safe_read(mysql)) == packet_error)
    {
      mc_free_rows(result);
      DBUG_RETURN(0);
    }
  }
  *prev_ptr=0;					/* last pointer is null */
  DBUG_PRINT("exit",("Got %d rows",result->rows));
  DBUG_RETURN(result);
}


/*
** Read one row. Uses packet buffer as storage for fields.
** When next packet is read, the previous field values are destroyed
*/


static int mc_read_one_row(MYSQL *mysql,uint fields,MYSQL_ROW row,
			   ulong *lengths)
{
  uint field;
  ulong pkt_len,len;
  uchar *pos,*prev_pos;

  if ((pkt_len=mc_net_safe_read(mysql)) == packet_error)
    return -1;
  if (pkt_len == 1 && mysql->net.read_pos[0] == 254)
    return 1;				/* End of data */
  prev_pos= 0;				/* allowed to write at packet[-1] */
  pos=mysql->net.read_pos;
  for (field=0 ; field < fields ; field++)
  {
    if ((len=(ulong) mc_net_field_length(&pos)) == NULL_LENGTH)
    {						/* null field */
      row[field] = 0;
      *lengths++=0;
    }
    else
    {
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

my_ulonglong STDCALL mc_mysql_num_rows(MYSQL_RES *res)
{
  return res->row_count;
}

unsigned int STDCALL mc_mysql_num_fields(MYSQL_RES *res)
{
  return res->field_count;
}

void STDCALL mc_mysql_data_seek(MYSQL_RES *result, my_ulonglong row)
{
  MYSQL_ROWS	*tmp=0;
  DBUG_PRINT("info",("mysql_data_seek(%ld)",(long) row));
  if (result->data)
    for (tmp=result->data->data; row-- && tmp ; tmp = tmp->next) ;
  result->current_row=0;
  result->data_cursor = tmp;
}

MYSQL_ROW STDCALL mc_mysql_fetch_row(MYSQL_RES *res)
{
  DBUG_ENTER("mc_mysql_fetch_row");
  if (!res->data)
  {						/* Unbufferred fetch */
    if (!res->eof)
    {
      if (!(mc_read_one_row(res->handle,res->field_count,res->row,
			    res->lengths)))
      {
	res->row_count++;
	DBUG_RETURN(res->current_row=res->row);
      }
      else
      {
	DBUG_PRINT("info",("end of data"));
	res->eof=1;
	res->handle->status=MYSQL_STATUS_READY;
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

int STDCALL mc_mysql_select_db(MYSQL *mysql, const char *db)
{
  int error;
  DBUG_ENTER("mysql_select_db");
  DBUG_PRINT("enter",("db: '%s'",db));

  if ((error=mc_simple_command(mysql,COM_INIT_DB,db,(uint) strlen(db),0)))
    DBUG_RETURN(error);
  my_free(mysql->db,MYF(MY_ALLOW_ZERO_PTR));
  mysql->db=my_strdup(db,MYF(MY_WME));
  DBUG_RETURN(0);
}


MYSQL_RES * STDCALL mc_mysql_store_result(MYSQL *mysql)
{
  MYSQL_RES *result;
  DBUG_ENTER("mysql_store_result");

  if (!mysql->fields)
    DBUG_RETURN(0);
  if (mysql->status != MYSQL_STATUS_GET_RESULT)
  {
    strmov(mysql->net.last_error,
	   ER(mysql->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    DBUG_RETURN(0);
  }
  mysql->status=MYSQL_STATUS_READY;		/* server is ready */
  if (!(result=(MYSQL_RES*) my_malloc(sizeof(MYSQL_RES)+
				      sizeof(ulong)*mysql->field_count,
				      MYF(MY_WME | MY_ZEROFILL))))
  {
    mysql->net.last_errno=CR_OUT_OF_MEMORY;
    strmov(mysql->net.last_error, ER(mysql->net.last_errno));
    DBUG_RETURN(0);
  }
  result->eof=1;				/* Marker for buffered */
  result->lengths=(ulong*) (result+1);
  if (!(result->data=mc_read_rows(mysql,mysql->fields,mysql->field_count)))
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








