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

#include "embedded_priv.h"
#include <my_sys.h>
#include <mysys_err.h>
#include <m_string.h>
#include <m_ctype.h>
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
#  include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif

static my_bool	mysql_client_init=0;
uint		mysql_port=0;
my_string	mysql_unix_port=0;
const char *sql_protocol_names_lib[] =
{ "TCP", "SOCKET", "PIPE", "MEMORY",NullS };
TYPELIB sql_protocol_typelib = {array_elements(sql_protocol_names_lib)-1,"",
			   sql_protocol_names_lib};

#define CLIENT_CAPABILITIES	(CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG | CLIENT_TRANSACTIONS | CLIENT_PROTOCOL_41)

#if defined(MSDOS) || defined(__WIN__)
#define ERRNO WSAGetLastError()
#define perror(A)
#else
#include <errno.h>
#define ERRNO errno
#define SOCKET_ERROR -1
#define closesocket(A) close(A)
#endif

void mysqld_once_init(void);
static void end_server(MYSQL *mysql);
static void append_wild(char *to,char *end,const char *wild);
static ulong mysql_sub_escape_string(CHARSET_INFO *charset_info, char *to,
				     const char *from, ulong length);

#define init_sigpipe_variables
#define set_sigpipe(mysql)
#define reset_sigpipe(mysql)

static void free_rows(MYSQL_DATA *cur)
{
  if (cur)
  {
    free_root(&cur->alloc,MYF(0));
    my_free((gptr) cur,MYF(0));
  }
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

#ifdef HAVE_GETPWUID
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif

#ifdef __WIN__
static my_bool is_NT(void)
{
  char *os=getenv("OS");
  return (os && !strcmp(os, "Windows_NT")) ? 1 : 0;
}
#endif

/*
** Expand wildcard to a sql string
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
** Init debugging if MYSQL_DEBUG environment variable is found
**************************************************************************/

void STDCALL
mysql_debug(const char *debug)
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
      strmov(strmov(buff,"libmysql: "),env);
      MessageBox((HWND) 0,"Debugging variable MYSQL_DEBUG used",buff,MB_OK);
    }
#endif
  }
#endif
}

/**************************************************************************
** Shut down connection
**************************************************************************/

static void
end_server(MYSQL *mysql)
{
  DBUG_ENTER("end_server");
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
** Get options from my.cnf
****************************************************************************/

static const char *default_options[]=
{
  "port","socket","compress","password","pipe", "timeout", "user",
  "init-command", "host", "database", "debug", "return-found-rows",
  "ssl-key" ,"ssl-cert" ,"ssl-ca" ,"ssl-capath",
  "character-sets-dir", "default-character-set", "interactive-timeout",
  "connect-timeout", "local-infile", "disable-local-infile",
  "replication-probe", "enable-reads-from-master", "repl-parse-query",
  "ssl-cipher","protocol", "shared_memory_base_name",
  NullS
};

static TYPELIB option_types={array_elements(default_options)-1,
			     "options",default_options};

static int add_init_command(struct st_mysql_options *options, const char *cmd)
{
  char *tmp;

  if (!options->init_commands)
  {
    options->init_commands= (DYNAMIC_ARRAY*)my_malloc(sizeof(DYNAMIC_ARRAY),
						      MYF(MY_WME));
    init_dynamic_array(options->init_commands,sizeof(char*),0,5 CALLER_INFO);
  }

  if (!(tmp= my_strdup(cmd,MYF(MY_WME))) ||
      insert_dynamic(options->init_commands, (gptr)&tmp))
  {
    my_free(tmp, MYF(MY_ALLOW_ZERO_PTR));
    return 1;
  }

  return 0;
}

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
          options->protocol = MYSQL_PROTOCOL_PIPE;
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
	  add_init_command(options,opt_arg);
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
	case 13:				/* Ignore SSL options */
	case 14:
	case 15:
	case 16:
	case 26:
	  break;
	case 17:			/* charset-lib */
	  my_free(options->charset_dir,MYF(MY_ALLOW_ZERO_PTR));
          options->charset_dir = my_strdup(opt_arg, MYF(MY_WME));
	  break;
	case 18:
	  my_free(options->charset_name,MYF(MY_ALLOW_ZERO_PTR));
          options->charset_name = my_strdup(opt_arg, MYF(MY_WME));
	  break;
	case 19:				/* Interactive-timeout */
	case 21:				/* client_local_files */
	case 22:
	case 23:				/* Replication options */
	case 24:
	case 25:
	case 27:				/* Protocol */
	case 28:				/* Shared memory */
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



/****************************************************************************
** Init MySQL structure or allocate one
****************************************************************************/

MYSQL * STDCALL
mysql_init(MYSQL *mysql)
{
  mysqld_once_init();
  if (!mysql)
  {
    if (!(mysql=(MYSQL*) my_malloc(sizeof(*mysql),MYF(MY_WME | MY_ZEROFILL))))
      return 0;
    mysql->free_me=1;
  }
  else
    bzero((char*) (mysql),sizeof(*(mysql)));
  return mysql;
}


void mysqld_once_init()
{
  if (!mysql_client_init)
  {
    mysql_client_init=1;

    my_init();					/* Will init threads */
    init_client_errs();
    mysql_port = MYSQL_PORT;
    mysql_debug(NullS);
  }
#ifdef THREAD
  else
    my_thread_init();         /* Init if new thread */
#endif
}

/**************************************************************************
** Connect to sql server
** If host == 0 then use localhost
**************************************************************************/

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

static inline int mysql_init_charset(MYSQL *mysql)
{
  char charset_name_buff[16], *charset_name;

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
    mysql->charset=get_charset((uint8) mysql->server_language, MYF(MY_WME));
  }
  else
    mysql->charset=default_charset_info;

  if (!mysql->charset)
  {
    mysql->last_errno=CR_CANT_READ_CHARSET;
    if (mysql->options.charset_dir)
      sprintf(mysql->last_error,ER(mysql->last_errno),
              charset_name ? charset_name : "unknown",
              mysql->options.charset_dir);
    else
    {
      char cs_dir_name[FN_REFLEN];
      get_charsets_dir(cs_dir_name);
      sprintf(mysql->last_error,ER(mysql->last_errno),
              charset_name ? charset_name : "unknown",
              cs_dir_name);
    }
    return mysql->last_errno;
  }
  return 0;
}

/*
** Note that the mysql argument must be initialized with mysql_init()
** before calling mysql_real_connect !
*/

MYSQL * STDCALL
mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		   const char *passwd __attribute__((unused)), const char *db,
		   uint port, const char *unix_socket,ulong client_flag)
{
  char          *db_name;
  DBUG_ENTER("mysql_real_connect");
  DBUG_PRINT("enter",("host: %s  db: %s  user: %s",
		      host ? host : "(Null)",
		      db ? db : "(Null)",
		      user ? user : "(Null)"));

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

  if (!db || !db[0])
    db=mysql->options.db;

  port=0;
  unix_socket=0;
  db_name = db ? my_strdup(db,MYF(MY_WME)) : NULL;

  mysql->thd= create_embedded_thd(client_flag, db_name);

  init_embedded_mysql(mysql, client_flag, db_name);

  if (mysql_init_charset(mysql))
    goto error;

  /* Send client information for access check */
  client_flag|=CLIENT_CAPABILITIES;
  client_flag&= ~CLIENT_COMPRESS;
  if (db)
    client_flag|=CLIENT_CONNECT_WITH_DB;

  if (mysql->options.init_commands)
  {
    DYNAMIC_ARRAY *init_commands= mysql->options.init_commands;
    char **ptr= (char**)init_commands->buffer;
    char **end= ptr + init_commands->elements;

    for (; ptr<end; ptr++)
    {
      MYSQL_RES *res;
      if (mysql_query(mysql,*ptr))
	goto error;
      if (mysql->fields)
      {
	if (!(res= mysql_use_result(mysql)))
	  goto error;
	mysql_free_result(res);
      }
    }
  }

  DBUG_PRINT("exit",("Mysql handler: %lx",mysql));
  reset_sigpipe(mysql);
  DBUG_RETURN(mysql);

error:
  reset_sigpipe(mysql);
  DBUG_PRINT("error",("message: %u (%s)",mysql->last_errno,mysql->last_error));
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


/**************************************************************************
** Change user and database 
**************************************************************************/

my_bool	STDCALL mysql_change_user(MYSQL *mysql __attribute__((unused)), const char *user __attribute__((unused)), 
				  const char *passwd __attribute__((unused)), const char *db __attribute__((unused)))
{
#ifdef DUMMY
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
#endif
  return 0;
}


/**************************************************************************
** Set current database
**************************************************************************/

int STDCALL
mysql_select_db(MYSQL *mysql, const char *db)
{
  int error;
  DBUG_ENTER("mysql_select_db");
  DBUG_PRINT("enter",("db: '%s'",db));

  if ((error=simple_command(mysql,COM_INIT_DB,db,(ulong) strlen(db),0)))
    DBUG_RETURN(error);
  DBUG_RETURN(0);
}


/*************************************************************************
** Send a QUIT to the server and close the connection
** If handle is alloced by mysql connect free it.
*************************************************************************/

void STDCALL
mysql_close(MYSQL *mysql)
{
  DBUG_ENTER("mysql_close");
  if (mysql)					/* Some simple safety */
  {
    my_free(mysql->options.user,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.host,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.password,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.unix_socket,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.db,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.charset_dir,MYF(MY_ALLOW_ZERO_PTR));
    my_free(mysql->options.charset_name,MYF(MY_ALLOW_ZERO_PTR));
    if (mysql->options.init_commands)
    {
      DYNAMIC_ARRAY *init_commands= mysql->options.init_commands;
      char **ptr= (char**)init_commands->buffer;
      char **end= ptr + init_commands->elements;
      for (; ptr<end; ptr++)
	my_free(*ptr,MYF(MY_WME));
      delete_dynamic(init_commands);
      my_free((char*)init_commands,MYF(MY_WME));
    }
    /* Clear pointers for better safety */
    bzero((char*) &mysql->options,sizeof(mysql->options));
#ifdef HAVE_OPENSSL
    ((VioConnectorFd*)(mysql->connector_fd))->delete();
    mysql->connector_fd = 0;
#endif /* HAVE_OPENSSL */
    if (mysql->free_me)
      my_free((gptr) mysql,MYF(0));
  }
  DBUG_VOID_RETURN;
}


/**************************************************************************
** Do a query. If query returned rows, free old rows.
** Read data by mysql_store_result or by repeat call of mysql_fetch_row
**************************************************************************/

int STDCALL
mysql_query(MYSQL *mysql, const char *query)
{
  return mysql_real_query(mysql,query, (ulong) strlen(query));
}

my_bool STDCALL
mysql_read_query_result(MYSQL *mysql)
{
  if (mysql->last_errno)
    return -1;

  if (mysql->field_count)
  {
    mysql->status=MYSQL_STATUS_GET_RESULT;
    mysql->affected_rows= mysql->result->row_count= mysql->result->data->rows;
    mysql->result->data_cursor= mysql->result->data->data;
  }

  return 0;
}

/****************************************************************************
* A modified version of connect().  my_connect() allows you to specify
* a timeout value, in seconds, that we should wait until we
* derermine we can't connect to a particular host.  If timeout is 0,
* my_connect() will behave exactly like connect().
*
* Base version coded by Steve Bernacki, Jr. <steve@navinet.net>
*****************************************************************************/

my_bool my_connect(my_socket s, const struct sockaddr *name, uint namelen,
		   uint timeout)
{
#if defined(__WIN__) || defined(OS2)
  return connect(s, (struct sockaddr*) name, namelen) != 0;
#else
  int flags, res, s_err;
  SOCKOPT_OPTLEN_TYPE s_err_size = sizeof(uint);
  fd_set sfds;
  struct timeval tv;
  time_t start_time, now_time;

  /* If they passed us a timeout of zero, we should behave
   * exactly like the normal connect() call does.
   */

  if (timeout == 0)
    return connect(s, (struct sockaddr*) name, namelen) != 0;

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
    return(1);
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
  /*
   * select could be interrupted by a signal, and if it is, 
   * the timeout should be adjusted and the select restarted
   * to work around OSes that don't restart select and 
   * implementations of select that don't adjust tv upon
   * failure to reflect the time remaining
   */
  start_time = time(NULL);
  for (;;)
  {
    tv.tv_sec = (long) timeout;
    tv.tv_usec = 0;
    if ((res = select(s+1, NULL, &sfds, NULL, &tv)) >= 0)
      break;
    now_time=time(NULL);
    timeout-= (uint) (now_time - start_time);
    if (errno != EINTR || (int) timeout <= 0)
      return 1;
  }

  /* select() returned something more interesting than zero, let's
   * see if we have any errors.  If the next two statements pass,
   * we've got an open socket!
   */

  s_err=0;
  if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0)
    return(1);

  if (s_err)
  {						/* getsockopt could succeed */
    errno = s_err;
    return(1);					/* but return an error... */
  }
  return(0);					/* It's all good! */
#endif
}

int STDCALL
mysql_send_query(MYSQL* mysql, const char* query, ulong length)
{
  DBUG_ENTER("mysql_send_query");

  if (mysql->options.separate_thread)
  {
    return -1;
  }

  mysql->result= NULL;

  free_old_query(mysql);			/* Free old result */

  DBUG_RETURN(simple_command(mysql, COM_QUERY, query, length, 1));
}

int STDCALL
mysql_real_query(MYSQL *mysql, const char *query, ulong length)
{
  DBUG_ENTER("mysql_real_query");
  DBUG_PRINT("enter",("handle: %lx",mysql));
  DBUG_PRINT("query",("Query = \"%s\"",query));

  if (mysql->options.separate_thread)
  {
    return -1;
  }

  mysql->result= NULL;

  free_old_query(mysql);			/* Free old result */

  if (simple_command(mysql, COM_QUERY, query, length, 1))
    DBUG_RETURN(-1);

  DBUG_RETURN(mysql_read_query_result(mysql));
}

/**************************************************************************
** Alloc result struct for buffered results. All rows are read to buffer.
** mysql_data_seek may be used.
**************************************************************************/
MYSQL_RES * STDCALL
mysql_store_result(MYSQL *mysql)
{
  MYSQL_RES *result= mysql->result;
  if (!result)
    return 0;

  mysql->result= NULL;
  *result->data->prev_ptr= 0;
  result->eof= 1;
  result->lengths= (ulong*)(result + 1);
  mysql->affected_rows= result->row_count= result->data->rows;
  result->data_cursor=  result->data->data;

  mysql->status=MYSQL_STATUS_READY;		/* server is ready */
  return result;
}

/**************************************************************************
** Alloc struct for use with unbuffered reads. Data is fetched by domand
** when calling to mysql_fetch_row.
** mysql_data_seek is a noop.
**
** No other queries may be specified with the same MYSQL handle.
** There shouldn't be much processing per row because mysql server shouldn't
** have to wait for the client (and will not wait more than 30 sec/packet).
**************************************************************************/

MYSQL_RES * STDCALL
mysql_use_result(MYSQL *mysql)
{
  DBUG_ENTER("mysql_use_result");
  if (mysql->options.separate_thread)
    DBUG_RETURN(0);

  DBUG_RETURN(mysql_store_result(mysql));
}

/**************************************************************************
** Return next field of the query results
**************************************************************************/

MYSQL_FIELD * STDCALL
mysql_fetch_field(MYSQL_RES *result)
{
  if (result->current_field >= result->field_count)
    return(NULL);
  return &result->fields[result->current_field++];
}


/**************************************************************************
**  Return next row of the query results
**************************************************************************/

MYSQL_ROW STDCALL
mysql_fetch_row(MYSQL_RES *res)
{
  DBUG_ENTER("mysql_fetch_row");
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
** Get column lengths of the current row
** If one uses mysql_use_result, res->lengths contains the length information,
** else the lengths are calculated from the offset between pointers.
**************************************************************************/

ulong * STDCALL
mysql_fetch_lengths(MYSQL_RES *res)
{
  ulong *lengths;
  MYSQL_ROW column,end;

  if (!(column=res->current_row))
    return 0;					/* Something is wrong */
  if (res->data)
  {
    lengths=res->lengths;
    for (end=column+res->field_count; column != end ; column++,lengths++)
    {
      *lengths= *column ? strlen(*column) : 0;
    }
  }
  return res->lengths;
}

/**************************************************************************
** Move to a specific row and column
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
** put the row or field cursor one a position one got from mysql_row_tell()
** This doesn't restore any data. The next mysql_fetch_row or
** mysql_fetch_field will return the next row or field after the last used
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
** List all databases
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
** List all tables in a database
** If wild is given then only the tables matching wild is returned
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
** List all fields in a table
** If wild is given then only the fields matching wild is returned
** Instead of this use query:
** show fields in 'table' like "wild"
**************************************************************************/

MYSQL_RES * STDCALL
mysql_list_fields(MYSQL *mysql __attribute__((unused)), const char *table __attribute__((unused)), const char *wild __attribute__((unused)))
{
#ifdef DUMMY
  MYSQL_RES *result;
  MYSQL_DATA *query;
  char	     buff[257],*end;
  DBUG_ENTER("mysql_list_fields");
  DBUG_PRINT("enter",("table: '%s'  wild: '%s'",table,wild ? wild : ""));

  LINT_INIT(query);

  end=strmake(strmake(buff, table,128)+1,wild ? wild : "",128);
  if (simple_command(mysql,COM_FIELD_LIST,buff,(uint) (end-buff),1) ||
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
#endif
  return 0;
}

/* List all running processes (threads) in server */
MYSQL_RES * STDCALL
mysql_list_processes(MYSQL *mysql)
{
#ifdef DUMMY
  MYSQL_DATA *fields;
  uint field_count;
  uchar *pos;
  DBUG_ENTER("mysql_list_processes");

  LINT_INIT(fields);
  if (simple_command(mysql,COM_PROCESS_INFO,"",0,0))
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
#endif /*DUMMY*/
  return 0;
}


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


int STDCALL
mysql_shutdown(MYSQL *mysql)
{
  DBUG_ENTER("mysql_shutdown");
  DBUG_RETURN(simple_command(mysql,COM_SHUTDOWN,"",0,0));
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
  DBUG_RETURN(simple_command(mysql,COM_DEBUG,"",0,0));
}

const char * STDCALL
mysql_stat(MYSQL *mysql)
{
#ifdef DUMMY
  DBUG_ENTER("mysql_stat");
  if (simple_command(mysql,COM_STATISTICS,"",0,0))
    return mysql->last_error;
  mysql->net.read_pos[mysql->packet_length]=0;	/* End of stat string */
  if (!mysql->net.read_pos[0])
  {
    mysql->net.last_errno=CR_WRONG_HOST_INFO;
    strmov(mysql->net.last_error, ER(mysql->net.last_errno));
    return mysql->net.last_error;
  }
  DBUG_RETURN((char*) mysql->net.read_pos);
#endif
  return (char *)mysql;
}


int STDCALL
mysql_ping(MYSQL *mysql)
{
  DBUG_ENTER("mysql_ping");
  DBUG_RETURN(simple_command(mysql,COM_PING,"",0,0));
}


const char * STDCALL
mysql_get_server_info(MYSQL *mysql __attribute__((unused)))
{
  return MYSQL_SERVER_VERSION;
}


ulong STDCALL
mysql_get_server_version(MYSQL *mysql __attribute__((unused)))
{
  return MYSQL_VERSION_ID;
}

const char * STDCALL
mysql_get_host_info(MYSQL *mysql __attribute__((unused)))
{
  return "localhost";
}


uint STDCALL
mysql_get_proto_info(MYSQL *mysql __attribute__((unused)))
{
  return PROTOCOL_VERSION;
}

const char * STDCALL
mysql_get_client_info(void)
{
  return MYSQL_SERVER_VERSION;
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
    mysql->options.compress=1;			/* Remember for connect */
    break;
  case MYSQL_OPT_USE_RESULT:
    mysql->options.separate_thread=1;		/* Use separate thread for query execution*/
    break;
  case MYSQL_OPT_NAMED_PIPE:
    mysql->options.protocol=MYSQL_PROTOCOL_PIPE; /* Force named pipe */
    break;
  case MYSQL_OPT_LOCAL_INFILE:			/* Allow LOAD DATA LOCAL ?*/
    if (!arg || test(*(uint*) arg))
      mysql->options.client_flag|= CLIENT_LOCAL_FILES;
    else
      mysql->options.client_flag&= ~CLIENT_LOCAL_FILES;
    break;
  case MYSQL_INIT_COMMAND:
    add_init_command(&mysql->options,arg);
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
  case MYSQL_OPT_PROTOCOL:
    mysql->options.protocol= *(uint*) arg;
    break;
  case MYSQL_SHARED_MEMORY_BASE_NAME:
    break;
  default:
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/****************************************************************************
** Functions to get information from the MySQL structure
** These are functions to make shared libraries more usable.
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

MYSQL_ROWS * STDCALL mysql_row_tell(MYSQL_RES *res)
{
  return res->data_cursor;
}

uint STDCALL mysql_field_tell(MYSQL_RES *res)
{
  return (res)->current_field;
}

/* MYSQL */

unsigned int STDCALL mysql_field_count(MYSQL *mysql)
{
  return mysql->field_count;
}

my_ulonglong STDCALL mysql_affected_rows(MYSQL *mysql)
{
  return (mysql)->affected_rows;
}

my_ulonglong STDCALL mysql_insert_id(MYSQL *mysql)
{
  return (mysql)->insert_id;
}

uint STDCALL mysql_errno(MYSQL *mysql)
{
  return mysql->last_errno;
}

const char * STDCALL mysql_error(MYSQL *mysql)
{
  return mysql->last_error;
}

const char *STDCALL mysql_info(MYSQL *mysql __attribute__((unused)))
{
#ifdef DUMMY
  return (mysql)->info;
#endif
  return 0;
}

ulong STDCALL mysql_thread_id(MYSQL *mysql __attribute__((unused)))
{
#ifdef DUMMY
  return (mysql)->thread_id;
#endif
  return 0;
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

MYSQL_RES *STDCALL mysql_warnings(MYSQL *mysql)
{
  uint warning_count;
  DBUG_ENTER("mysql_warnings");
  /* Save warning count as mysql_real_query may change this */
  warning_count= mysql_warning_count(mysql);
  if (mysql_real_query(mysql, "SHOW WARNINGS", 13))
    DBUG_RETURN(0);
  DBUG_RETURN(mysql_store_result(mysql));
}

/****************************************************************************
** Some support functions
****************************************************************************/

/*
** Add escape characters to a string (blob?) to make it suitable for a insert
** to should at least have place for length*2+1 chars
** Returns the length of the to string
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
