my_bool mysql_reconnect(MYSQL *mysql);
static my_bool send_file_to_server(MYSQL *mysql, const char *filename);
void end_server(MYSQL *mysql);

/****************************************************************************
  A modified version of connect().  my_connect() allows you to specify
  a timeout value, in seconds, that we should wait until we
  derermine we can't connect to a particular host.  If timeout is 0,
  my_connect() will behave exactly like connect().

  Base version coded by Steve Bernacki, Jr. <steve@navinet.net>
*****************************************************************************/

my_bool my_connect(my_socket s, const struct sockaddr *name,
		   uint namelen, uint timeout)
{
#if defined(__WIN__) || defined(OS2) || defined(__NETWARE__)
  return connect(s, (struct sockaddr*) name, namelen) != 0;
#else
  int flags, res, s_err;
  SOCKOPT_OPTLEN_TYPE s_err_size = sizeof(uint);
  fd_set sfds;
  struct timeval tv;
  time_t start_time, now_time;

  /*
    If they passed us a timeout of zero, we should behave
    exactly like the normal connect() call does.
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

  /*
    Otherwise, our connection is "in progress."  We can use
    the select() call to wait up to a specified period of time
    for the connection to succeed.  If select() returns 0
    (after waiting howevermany seconds), our socket never became
    writable (host is probably unreachable.)  Otherwise, if
    select() returns 1, then one of two conditions exist:

    1. An error occured.  We use getsockopt() to check for this.
    2. The connection was set up sucessfully: getsockopt() will
    return 0 as an error.

    Thanks goes to Andrew Gierth <andrew@erlenstar.demon.co.uk>
    who posted this method of timing out a connect() in
    comp.unix.programmer on August 15th, 1997.
  */

  FD_ZERO(&sfds);
  FD_SET(s, &sfds);
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
    if ((res = select(s+1, NULL, (int*) &sfds, NULL, &tv)) > 0)
      break;
#else
    if ((res = select(s+1, NULL, &sfds, NULL, &tv)) > 0)
      break;
#endif
    if (res == 0)					/* timeout */
      return -1;
    now_time=time(NULL);
    timeout-= (uint) (now_time - start_time);
    if (errno != EINTR || (int) timeout <= 0)
      return 1;
  }

  /*
    select() returned something more interesting than zero, let's
    see if we have any errors.  If the next two statements pass,
    we've got an open socket!
  */

  s_err=0;
  if (getsockopt(s, SOL_SOCKET, SO_ERROR, (char*) &s_err, &s_err_size) != 0)
    return(1);

  if (s_err)
  {						/* getsockopt could succeed */
    errno = s_err;
    return(1);					/* but return an error... */
  }
  return (0);					/* ok */

#endif
}

/*
  Create a named pipe connection
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

#ifdef _libmysql_c
  if ( ! unix_socket || (unix_socket)[0] == 0x00)
    unix_socket = mysql_unix_port;
#endif
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

/*****************************************************************************
  Read a packet from server. Give error message if socket was down
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

static void free_rows(MYSQL_DATA *cur)
{
  if (cur)
  {
    free_root(&cur->alloc,MYF(0));
    my_free((gptr) cur,MYF(0));
  }
}

static my_bool
advanced_command(MYSQL *mysql, enum enum_server_command command,
		 const char *header, ulong header_length,
		 const char *arg, ulong arg_length, my_bool skip_check)
{
  NET *net= &mysql->net;
  my_bool result= 1;
  init_sigpipe_variables

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(mysql);

  if (mysql->net.vio == 0)
  {						/* Do reconnect if possible */
    if (mysql_reconnect(mysql))
      return 1;
  }
  if (mysql->status != MYSQL_STATUS_READY)
  {
    strmov(net->last_error,ER(mysql->net.last_errno=CR_COMMANDS_OUT_OF_SYNC));
    return 1;
  }

  mysql->net.last_error[0]=0;
  mysql->net.last_errno=0;
  mysql->net.report_error=0;
  mysql->info=0;
  mysql->affected_rows= ~(my_ulonglong) 0;
  net_clear(&mysql->net);			/* Clear receive buffer */
  if (!arg)
    arg="";

  if (net_write_command(net,(uchar) command, header, header_length,
			arg, arg_length))
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
    if (net_write_command(net,(uchar) command, header, header_length,
			  arg, arg_length))
    {
      net->last_errno=CR_SERVER_GONE_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto end;
    }
  }
  result=0;
  if (!skip_check)
    result= ((mysql->packet_length=net_safe_read(mysql)) == packet_error ?
	     1 : 0);
 end:
  reset_sigpipe(mysql);
  return result;
}

my_bool
simple_command(MYSQL *mysql,enum enum_server_command command, const char *arg,
	       ulong length, my_bool skip_check)
{
  return advanced_command(mysql, command, NullS, 0, arg, length, skip_check);
}

static void free_old_query(MYSQL *mysql)
{
  DBUG_ENTER("free_old_query");
  if (mysql->fields)
    free_root(&mysql->field_alloc,MYF(0));
  else
    init_alloc_root(&mysql->field_alloc,8192,0); /* Assume rowlength < 8192 */
  mysql->fields=0;
  mysql->field_count=0;				/* For API */
  DBUG_VOID_RETURN;
}

#ifdef __WIN__
static my_bool is_NT(void)
{
  char *os=getenv("OS");
  return (os && !strcmp(os, "Windows_NT")) ? 1 : 0;
}
#endif

/**************************************************************************
  Shut down connection
**************************************************************************/

void end_server(MYSQL *mysql)
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
      DBUG_PRINT("warning",("Not all rows in set where read; Ignoring rows"));
      for (;;)
      {
	ulong pkt_len;
	if ((pkt_len=net_safe_read(result->handle)) == packet_error)
	  break;
	if (pkt_len <= 8 && result->handle->net.read_pos[0] == 254)
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

#ifdef _libmysql_c

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
  "protocol", "shared-memory-base-name",
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
	  options->client_flag|= CLIENT_COMPRESS;
	  break;
	case 4:				/* password */
	  if (opt_arg)
	  {
	    my_free(options->password,MYF(MY_ALLOW_ZERO_PTR));
	    options->password=my_strdup(opt_arg,MYF(MY_WME));
	  }
	  break;
        case 5:
          options->protocol = MYSQL_PROTOCOL_PIPE;
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
	  options->client_flag&= CLIENT_LOCAL_FILES;
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
	  options->max_allowed_packet= atoi(opt_arg);
	  break;
        case 28:		/* protocol */
          if ((options->protocol = find_type(opt_arg, &sql_protocol_typelib,0)) == ~(ulong) 0)
          {
            fprintf(stderr, "Unknown option to protocol: %s\n", opt_arg);
            exit(1);
          }
          break;
        case 29:		/* shared_memory_base_name */
#ifdef HAVE_SMEM
          if (options->shared_memory_base_name != def_shared_memory_base_name)
            my_free(options->shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
          options->shared_memory_base_name=my_strdup(opt_arg,MYF(MY_WME));
#endif
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

#endif /*_libmysql_c*/

/**************************************************************************
  Get column lengths of the current row
  If one uses mysql_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

static void fetch_lengths(ulong *to, MYSQL_ROW column, uint field_count)
{ 
  ulong *prev_length;
  byte *start=0;
  MYSQL_ROW end;

  prev_length=0;				/* Keep gcc happy */
  for (end=column + field_count + 1 ; column != end ; column++, to++)
  {
    if (!*column)
    {
      *to= 0;					/* Null */
      continue;
    }
    if (start)					/* Found end of prev string */
      *prev_length= (ulong) (*column-start-1);
    start= *column;
    prev_length= to;
  }
}


static inline void 
unpack_fields_40(MYSQL_ROWS *row, MYSQL_FIELD *field, MEM_ROOT *alloc,
		 ulong *lengths, uint n_lengths, 
		 my_bool default_value, my_bool long_flag_protocol)
{
  DBUG_ENTER("unpack_fields_40");

  for (; row ; row = row->next,field++)
  {
    fetch_lengths(lengths, row->data, n_lengths);
    field->org_table= field->table=  strdup_root(alloc,(char*) row->data[0]);
    field->name=   strdup_root(alloc,(char*) row->data[1]);
    field->length= (uint) uint3korr(row->data[2]);
    field->type=   (enum enum_field_types) (uchar) row->data[3][0];

    field->org_table_length=	field->table_length=	lengths[0];
    field->name_length=	lengths[1];

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
    {
      field->def=strdup_root(alloc,(char*) row->data[5]);
      field->def_length= lengths[5];
    }
    else
      field->def=0;
    field->max_length= 0;
  }
}

/***************************************************************************
  Change field rows to field structs
***************************************************************************/

static MYSQL_FIELD *
unpack_fields(MYSQL_DATA *data,MEM_ROOT *alloc,uint fields,
	      my_bool default_value, uint server_capabilities)
{
  MYSQL_ROWS	*row;
  MYSQL_FIELD	*field,*result;
  ulong lengths[8];				/* Max of fields */
  DBUG_ENTER("unpack_fields");

  field=result=(MYSQL_FIELD*) alloc_root(alloc,
					 (uint) sizeof(MYSQL_FIELD)*fields);
  if (!result)
  {
    free_rows(data);				/* Free old data */
    DBUG_RETURN(0);
  }
  bzero((char*) field, (uint) sizeof(MYSQL_FIELD)*fields);
#ifdef _mini_client_c
  unpack_fields_40(data->data, field, alloc, lengths, default_value ? 6 : 5,
		   default_value, server_capabilities & CLIENT_LONG_FLAG);
#else
  if (server_capabilities & CLIENT_PROTOCOL_41)
  {
    /* server is 4.1, and returns the new field result format */
    for (row=data->data; row ; row = row->next,field++)
    {
      uchar *pos;
      fetch_lengths(&lengths[0], row->data, default_value ? 7 : 6);
      field->db       = strdup_root(alloc,(char*) row->data[0]);
      field->table    = strdup_root(alloc,(char*) row->data[1]);
      field->org_table= strdup_root(alloc,(char*) row->data[2]);
      field->name     = strdup_root(alloc,(char*) row->data[3]);
      field->org_name = strdup_root(alloc,(char*) row->data[4]);

      field->db_length=		lengths[0];
      field->table_length=	lengths[1];
      field->org_table_length=	lengths[2];
      field->name_length=	lengths[3];
      field->org_name_length=	lengths[4];

      /* Unpack fixed length parts */
      pos= (uchar*) row->data[5];
      field->charsetnr= uint2korr(pos);
      field->length=	(uint) uint3korr(pos+2);
      field->type=	(enum enum_field_types) pos[5];
      field->flags=	uint2korr(pos+6);
      field->decimals=  (uint) pos[8];

      if (INTERNAL_NUM_FIELD(field))
        field->flags|= NUM_FLAG;
      if (default_value && row->data[6])
      {
        field->def=strdup_root(alloc,(char*) row->data[6]);
	field->def_length= lengths[6];
      }
      else
        field->def=0;
      field->max_length= 0;
    }
  }
#ifndef DELETE_SUPPORT_OF_4_0_PROTOCOL
  else
    unpack_fields_40(data->data, field, alloc, lengths, default_value ? 6 : 5,
		     default_value, server_capabilities & CLIENT_LONG_FLAG);
#endif /* DELETE_SUPPORT_OF_4_0_PROTOCOL */
#endif /*_mini_client_c*/
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

  /*
    The last EOF packet is either a single 254 character or (in MySQL 4.1)
    254 followed by 1-7 status bytes.

    This doesn't conflict with normal usage of 254 which stands for a
    string where the length of the string is 8 bytes. (see net_field_length())
  */

  while (*(cp=net->read_pos) != 254 || pkt_len >= 8)
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
#ifndef _mini_client_c
  if (pkt_len > 1)				/* MySQL 4.1 protocol */
  {
    mysql->warning_count= uint2korr(cp+1);
    DBUG_PRINT("info",("warning_count:  %ld", mysql->warning_count));
  }
#endif
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
  if (pkt_len <= 8 && mysql->net.read_pos[0] == 254)
  {
#ifndef _mini_client_c
    if (pkt_len > 1)				/* MySQL 4.1 protocol */
      mysql->warning_count= uint2korr(mysql->net.read_pos+1);
#endif
    return 1;				/* End of data */
  }
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


/****************************************************************************
  Init MySQL structure or allocate one
****************************************************************************/

MYSQL * STDCALL
mysql_init(MYSQL *mysql)
{
  mysql_once_init();
  if (!mysql)
  {
    if (!(mysql=(MYSQL*) my_malloc(sizeof(*mysql),MYF(MY_WME | MY_ZEROFILL))))
      return 0;
    mysql->free_me=1;
  }
  else
    bzero((char*) (mysql),sizeof(*(mysql)));
#ifndef _mini_client_c
  mysql->options.connect_timeout=CONNECT_TIMEOUT;
  mysql->last_used_con = mysql->next_slave = mysql->master = mysql;
  /*
    By default, we are a replication pivot. The caller must reset it
    after we return if this is not the case.
  */
  mysql->rpl_pivot = 1;
#if defined(SIGPIPE) && defined(THREAD) && !defined(__WIN__)
  if (!((mysql)->client_flag & CLIENT_IGNORE_SIGPIPE))
    (void) signal(SIGPIPE,pipe_sig_handler);
#endif

/*
  Only enable LOAD DATA INFILE by default if configured with
  --enable-local-infile
*/
#ifdef ENABLED_LOCAL_INFILE
  mysql->options.client_flag|= CLIENT_LOCAL_FILES;
#endif
#ifdef HAVE_SMEM
  mysql->options.shared_memory_base_name=(char*)def_shared_memory_base_name;
#endif

#else /*_mini_client_c*/

#ifdef __WIN__
  mysql->options.connect_timeout=20;
#endif
  mysql->net.read_timeout = slave_net_timeout;

#endif /*_mini_client_c*/
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
*/

void mysql_once_init(void)
{
#ifndef _mini_client_c

  if (!mysql_client_init)
  {
    mysql_client_init=1;
    org_my_init_done=my_init_done;
    my_init();					/* Will init threads */
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
#if defined(SIGPIPE) && !defined(THREAD) && !defined(__WIN__)
    (void) signal(SIGPIPE,SIG_IGN);
#endif
  }
#ifdef THREAD
  else
    my_thread_init();         /* Init if new thread */
#endif

#else /*_mini_client_c*/
  init_client_errs();
#endif /*_mini_client_c*/
}


/*
  Handle password authentication
*/

static my_bool mysql_autenticate(MYSQL *mysql, const char *passwd)
{
  ulong pkt_length;
  NET *net= &mysql->net;
  char buff[SCRAMBLE41_LENGTH];
  char password_hash[SCRAMBLE41_LENGTH]; /* Used for storage of stage1 hash */

  /* We shall only query server if it expect us to do so */
  if ((pkt_length=net_safe_read(mysql)) == packet_error)
    goto error;

  if (mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
  {
    /*
      This should always happen with new server unless empty password
      OK/Error packets have zero as the first char
    */
    if (pkt_length == 24 && net->read_pos[0])
    {
      /* Old passwords will have '*' at the first byte of hash */
      if (net->read_pos[0] != '*')
      {
        /* Build full password hash as it is required to decode scramble */
        password_hash_stage1(buff, passwd);
        /* Store copy as we'll need it later */
        memcpy(password_hash,buff,SCRAMBLE41_LENGTH);
        /* Finally hash complete password using hash we got from server */
        password_hash_stage2(password_hash,(const char*) net->read_pos);
        /* Decypt and store scramble 4 = hash for stage2 */
        password_crypt((const char*) net->read_pos+4,mysql->scramble_buff,
		       password_hash, SCRAMBLE41_LENGTH);
        mysql->scramble_buff[SCRAMBLE41_LENGTH]=0;
        /* Encode scramble with password. Recycle buffer */
        password_crypt(mysql->scramble_buff,buff,buff,SCRAMBLE41_LENGTH);
      }
      else
      {
	/* Create password to decode scramble */
	create_key_from_old_password(passwd,password_hash);
	/* Decypt and store scramble 4 = hash for stage2 */
	password_crypt((const char*) net->read_pos+4,mysql->scramble_buff,
		       password_hash, SCRAMBLE41_LENGTH);
	mysql->scramble_buff[SCRAMBLE41_LENGTH]=0;
	/* Finally scramble decoded scramble with password */
	scramble(buff, mysql->scramble_buff, passwd,0);
      }
      /* Write second package of authentication */
      if (my_net_write(net,buff,SCRAMBLE41_LENGTH) || net_flush(net))
      {
        net->last_errno= CR_SERVER_LOST;
        strmov(net->last_error,ER(net->last_errno));
        goto error;
      }
      /* Read what server thinks about out new auth message report */
      if (net_safe_read(mysql) == packet_error)
	goto error;
    }
  }
  return 0;

error:
  return 1;
}

#if defined(HAVE_GETPWUID) && defined(NO_GETPWUID_DECL)
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif


#if defined(__NETWARE__)
/* default to "root" on NetWare */
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

/*
  Note that the mysql argument must be initialized with mysql_init()
  before calling mysql_real_connect !
*/

MYSQL * STDCALL
mysql_real_connect(MYSQL *mysql,const char *host, const char *user,
		   const char *passwd, const char *db,
		   uint port, const char *unix_socket,ulong client_flag
#ifdef _mini_client_c
		   , uint net_read_timeout
#endif
)
{
  char		buff[NAME_LEN+USERNAME_LENGTH+100],charset_name_buff[16];
  char		*end,*host_info,*charset_name;
  my_socket	sock;
  uint32	ip_addr;
  struct	sockaddr_in sock_addr;
  ulong		pkt_length;
  NET		*net= &mysql->net;
#ifdef _mini_client_c
  thr_alarm_t   alarmed;
  ALARM		alarm_buff;
  ulong		max_allowed_packet;
#endif

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
#ifdef _mini_client_c
  mysql->charset=default_charset_info;  /* Set character set */
#endif

#ifdef _libmysql_c
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
      passwd=getenv("MYSQL_PWD");		/* get it from environment */
#endif
  }
  if (!db || !db[0])
    db=mysql->options.db;
  if (!port)
    port=mysql->options.port;
  if (!unix_socket)
    unix_socket=mysql->options.unix_socket;
#endif /*_libmysql_c*/
#ifdef _mini_client_c
  if (!port)
    port = MYSQL_PORT;			/* Should always be set by mysqld */
  if (!unix_socket)
    unix_socket=MYSQL_UNIX_ADDR;
  if (!mysql->options.connect_timeout)
    mysql->options.connect_timeout= net_read_timeout;
#endif /*mini_client_c*/

  mysql->reconnect=1;				/* Reconnect as default */
  mysql->server_status=SERVER_STATUS_AUTOCOMMIT;

  /*
    Grab a socket and connect it to the server
  */
#if defined(_libmysql_c) && defined(HAVE_SMEM)
  if ((!mysql->options.protocol ||
       mysql->options.protocol == MYSQL_PROTOCOL_MEMORY) &&
      (!host || !strcmp(host,LOCAL_HOST)))
  {
    if ((create_shared_memory(mysql,net, mysql->options.connect_timeout)) ==
	INVALID_HANDLE_VALUE)
    {
      DBUG_PRINT("error",
		 ("host: '%s'  socket: '%s'  shared memory: %s  have_tcpip: %d",
		  host ? host : "<null>",
		  unix_socket ? unix_socket : "<null>",
		  (int) mysql->options.shared_memory_base_name,
		  (int) have_tcpip));
      if (mysql->options.protocol == MYSQL_PROTOCOL_MEMORY)
	goto error;
      /* Try also with PIPE or TCP/IP */
    }
    else
    {
      mysql->options.protocol=MYSQL_PROTOCOL_MEMORY;
      sock=0;
      unix_socket = 0;
      host=mysql->options.shared_memory_base_name;
      host_info=(char*) ER(CR_SHARED_MEMORY_CONNECTION);
    }
  } else
#endif /* HAVE_SMEM */
#if defined(HAVE_SYS_UN_H)
    if (
#ifdef _libmysql_c
	(!mysql->options.protocol ||
	 mysql->options.protocol == MYSQL_PROTOCOL_SOCKET)&&
	(unix_socket || mysql_unix_port) &&
#endif
#ifdef _mini_client_c
	unix_socket &&
#endif
	(!host || !strcmp(host,LOCAL_HOST)))
    {
      host=LOCAL_HOST;
#ifdef _libmysql_c
      if (!unix_socket)
	unix_socket=mysql_unix_port;
#endif
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
      if (my_connect(sock,(struct sockaddr *) &UNIXaddr, sizeof(UNIXaddr),
		     mysql->options.connect_timeout))
      {
	DBUG_PRINT("error",("Got error %d on connect to local server",
			    socket_errno));
	net->last_errno=CR_CONNECTION_ERROR;
	sprintf(net->last_error,ER(net->last_errno),unix_socket,socket_errno);
	goto error;
      }
#ifdef _libmysql_c
      else
	mysql->options.protocol=MYSQL_PROTOCOL_SOCKET;
#endif
    }
    else
#elif defined(__WIN__)
    {
#ifdef _libmysql_c
      if ((!mysql->options.protocol ||
	   mysql->options.protocol == MYSQL_PROTOCOL_PIPE)&&
	  ((unix_socket || !host && is_NT() ||
	    host && !strcmp(host,LOCAL_HOST_NAMEDPIPE) ||! have_tcpip))&&
	  (!net->vio))
#elif _mini_client_c
      if ((unix_socket ||
	   !host && is_NT() ||
	   host && !strcmp(host,LOCAL_HOST_NAMEDPIPE) ||
	   mysql->options.named_pipe || !have_tcpip))
#endif
      {
	sock=0;
	if ((hPipe=create_named_pipe(net, mysql->options.connect_timeout,
				     (char**) &host, (char**) &unix_socket)) ==
	    INVALID_HANDLE_VALUE)
	{
	  DBUG_PRINT("error",
		     ("host: '%s'  socket: '%s'  have_tcpip: %d",
		      host ? host : "<null>",
		      unix_socket ? unix_socket : "<null>",
		      (int) have_tcpip));
	  if (mysql->options.protocol == MYSQL_PROTOCOL_PIPE ||
	      (host && !strcmp(host,LOCAL_HOST_NAMEDPIPE)) ||
	      (unix_socket && !strcmp(unix_socket,MYSQL_NAMEDPIPE)))
	    goto error;
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
#ifdef _mini_client_c
  if (hPipe == INVALID_HANDLE_VALUE)
#endif
#endif
#ifdef _libmysql_c
  if ((!mysql->options.protocol ||
       mysql->options.protocol == MYSQL_PROTOCOL_TCP)&&(!net->vio))
#endif
  {
    unix_socket=0;				/* This is not used */
#ifdef _libmysql_c
    if (!port)
      port=mysql_port;
#endif
    if (!host)
      host=LOCAL_HOST;
    sprintf(host_info=buff,ER(CR_TCP_CONNECTION),host);
    DBUG_PRINT("info",("Server name: '%s'.  TCP sock: %d", host,port));
#ifdef _mini_client_c
    thr_alarm_init(&alarmed);
    thr_alarm(&alarmed, net_read_timeout, &alarm_buff);
#endif
    sock = (my_socket) socket(AF_INET,SOCK_STREAM,0);
#ifdef _mini_client_c
    thr_end_alarm(&alarmed);
#endif
    /* _WIN64 ;  Assume that the (int) range is enough for socket() */
    if (sock == SOCKET_ERROR)
    {
      net->last_errno=CR_IPSOCK_ERROR;
      sprintf(net->last_error,ER(net->last_errno),socket_errno);
      goto error;
    }
    net->vio = vio_new(sock,VIO_TYPE_TCPIP,FALSE);
    bzero((char*) &sock_addr,sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;

    /*
      The server name may be a host name or IP address
    */

    if ((int) (ip_addr = inet_addr(host)) != (int) INADDR_NONE)
    {
      memcpy_fixed(&sock_addr.sin_addr,&ip_addr,sizeof(ip_addr));
    }
    else
    {
      int tmp_errno;
      struct hostent tmp_hostent,*hp;
      char buff2[GETHOSTBYNAME_BUFF_SIZE];
      hp = my_gethostbyname_r(host,&tmp_hostent,buff2,sizeof(buff2),
			      &tmp_errno);
      if (!hp)
      {
	my_gethostbyname_r_free();
	net->last_errno=CR_UNKNOWN_HOST;
	sprintf(net->last_error, ER(CR_UNKNOWN_HOST), host, tmp_errno);
	goto error;
      }
      memcpy(&sock_addr.sin_addr,hp->h_addr, (size_t) hp->h_length);
      my_gethostbyname_r_free();
    }
    sock_addr.sin_port = (ushort) htons((ushort) port);
    if (my_connect(sock,(struct sockaddr *) &sock_addr, sizeof(sock_addr),
		   mysql->options.connect_timeout))
    {
      DBUG_PRINT("error",("Got error %d on connect to '%s'",socket_errno,
			  host));
      net->last_errno= CR_CONN_HOST_ERROR;
      sprintf(net->last_error ,ER(CR_CONN_HOST_ERROR), host, socket_errno);
      goto error;
    }
  }
#ifdef _libmysql_c
  else if (!net->vio)
  {
    DBUG_PRINT("error",("Unknow protocol %d ",mysql->options.protocol));
    net->last_errno= CR_CONN_UNKNOW_PROTOCOL;
    sprintf(net->last_error ,ER(CR_CONN_UNKNOW_PROTOCOL));
    goto error;
  }
#endif /*_libmysql_c*/
  if (!net->vio || my_net_init(net, net->vio))
  {
    vio_delete(net->vio);
    net->vio = 0;
    net->last_errno=CR_OUT_OF_MEMORY;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }
  vio_keepalive(net->vio,TRUE);
#ifdef _mini_client_c
  net->read_timeout=slave_net_timeout;
#endif
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

  /* Check if version of protocol matches current one */

  mysql->protocol_version= net->read_pos[0];
  DBUG_DUMP("packet",(char*) net->read_pos,10);
  DBUG_PRINT("info",("mysql protocol version %d, server=%d",
		     PROTOCOL_VERSION, mysql->protocol_version));
  if (mysql->protocol_version != PROTOCOL_VERSION
#ifdef _mini_client_c
      && mysql->protocol_version != PROTOCOL_VERSION-1
#endif
      )
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

#ifdef _libmysql_c
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
#endif /*_libmysql_c*/

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

#ifdef _libmysql_c
  /* Remove options that server doesn't support */
  client_flag= ((client_flag &
		 ~(CLIENT_COMPRESS | CLIENT_SSL | CLIENT_PROTOCOL_41)) |
		(client_flag & mysql->server_capabilities));
#ifndef HAVE_COMPRESS
  client_flag&= ~CLIENT_COMPRESS;
#endif
  if (client_flag & CLIENT_PROTOCOL_41)
  {
    /* 4.1 server and 4.1 client has a 4 byte option flag */
    int4store(buff,client_flag);
    int4store(buff+4,max_allowed_packet);
    end= buff+8;
  }
  else
  {
    int2store(buff,client_flag);
    int3store(buff+2,max_allowed_packet);
    end= buff+5;
  }
  mysql->client_flag=client_flag;
#endif /*_libmysql_c*/

#ifdef _mini_client_c
#ifdef HAVE_COMPRESS
  if ((mysql->server_capabilities & CLIENT_COMPRESS) &&
      (mysql->options.compress || (client_flag & CLIENT_COMPRESS)))
    client_flag|=CLIENT_COMPRESS;		/* We will use compression */
  else
#endif
    client_flag&= ~CLIENT_COMPRESS;
#endif /*mini_client_c*/

#ifdef _mini_client_c
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

  max_allowed_packet=mysql->net.max_packet_size;
  int2store(buff,client_flag);
  int3store(buff+2,max_allowed_packet);
  end= buff+5;
  mysql->client_flag=client_flag;
#endif /*_mini_client_c*/

#ifdef HAVE_OPENSSL
  /*
    Oops.. are we careful enough to not send ANY information without
    encryption?
  */
  if (client_flag & CLIENT_SSL)
  {
    struct st_mysql_options *options= &mysql->options;
    if (my_net_write(net,buff,(uint) (end-buff)) || net_flush(net))
    {
      net->last_errno= CR_SERVER_LOST;
      strmov(net->last_error,ER(net->last_errno));
      goto error;
    }
    /* Do the SSL layering. */
#ifdef _libmysql_c
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
#endif /*libmysql_c*/
    DBUG_PRINT("info", ("IO layer change in progress..."));
    if (sslconnect((struct st_VioSSLConnectorFd*)(mysql->connector_fd),
		   mysql->net.vio, (long) (mysql->options.connect_timeout)))
    {
      net->last_errno= CR_SSL_CONNECTION_ERROR;
      strmov(net->last_error,ER(net->last_errno));
      goto error;
    }
    DBUG_PRINT("info", ("IO layer change done!"));
  }
#endif /* HAVE_OPENSSL */

  DBUG_PRINT("info",("Server version = '%s'  capabilites: %lu  status: %u  client_flag: %lu",
		     mysql->server_version,mysql->server_capabilities,
		     mysql->server_status, client_flag));
  /* This needs to be changed as it's not useful with big packets */
  if (user && user[0])
    strmake(end,user,32);			/* Max user name */
  else
#ifdef _mini_client_c
  {
    user = getenv("USER");
    if (!user) user = "mysql";
    strmov((char*) end, user );
  }
#else
    read_user_name((char*) end);
#endif /*_mini_client_c*/
  /* We have to handle different version of handshake here */
#if defined(_CUSTOMCONFIG_) && defined(_libmysql_c)
#include "_cust_libmysql.h";
#endif
  DBUG_PRINT("info",("user: %s",end));
  /*
    We always start with old type handshake the only difference is message sent
    If server handles secure connection type we'll not send the real scramble
  */
  if (mysql->server_capabilities & CLIENT_SECURE_CONNECTION)
  {
    if (passwd[0])
    {
      /* Prepare false scramble  */
      end=strend(end)+1;
      bfill(end, SCRAMBLE_LENGTH, 'x');
      end+=SCRAMBLE_LENGTH;
      *end=0;
    }
    else				/* For empty password*/
    {
      end=strend(end)+1;
      *end=0;				/* Store zero length scramble */
    }
  }
  else
  {
    /*
      Real scramble is only sent to old servers. This can be blocked 
      by calling mysql_options(MYSQL *, MYSQL_SECURE_CONNECT, (char*) &1);
    */
    end=scramble(strend(end)+1, mysql->scramble_buff, passwd,
                 (my_bool) (mysql->protocol_version == 9));
  }
  /* Add database if needed */
  if (db && (mysql->server_capabilities & CLIENT_CONNECT_WITH_DB))
  {
    end=strmake(end+1,db,NAME_LEN);
    mysql->db=my_strdup(db,MYF(MY_WME));
    db=0;
  }
  /* Write authentication package */
  if (my_net_write(net,buff,(ulong) (end-buff)) || net_flush(net))
  {
    net->last_errno= CR_SERVER_LOST;
    strmov(net->last_error,ER(net->last_errno));
    goto error;
  }

  if (mysql_autenticate(mysql, passwd))
    goto error;

  if (client_flag & CLIENT_COMPRESS)		/* We will use compression */
    net->compress=1;

#ifdef _libmysql_c
  if (mysql->options.max_allowed_packet)
    net->max_packet_size= mysql->options.max_allowed_packet;
  if (db && mysql_select_db(mysql,db))
    goto error;

  if (mysql->options.init_commands)
  {
    DYNAMIC_ARRAY *init_commands= mysql->options.init_commands;
    char **ptr= (char**)init_commands->buffer;
    char **end= ptr + init_commands->elements;

    my_bool reconnect=mysql->reconnect;
    mysql->reconnect=0;

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

    mysql->reconnect=reconnect;
  }

  if (mysql->options.rpl_probe && mysql_rpl_probe(mysql))
    goto error;
#endif /*_libmysql_c*/

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

my_bool mysql_reconnect(MYSQL *mysql)
{
  MYSQL tmp_mysql;
  DBUG_ENTER("mysql_reconnect");

  if (!mysql->reconnect
#ifdef _libmysql_c
      || (mysql->server_status & SERVER_STATUS_IN_TRANS) || !mysql->host_info
#endif
)
  {
   /* Allow reconnect next time */
#ifdef _libmysql_c
    mysql->server_status&= ~SERVER_STATUS_IN_TRANS;
#endif
    mysql->net.last_errno=CR_SERVER_GONE_ERROR;
    strmov(mysql->net.last_error,ER(mysql->net.last_errno));
    DBUG_RETURN(1);
  }
  mysql_init(&tmp_mysql);
  tmp_mysql.options=mysql->options;
  bzero((char*) &mysql->options,sizeof(mysql->options));
  tmp_mysql.rpl_pivot = mysql->rpl_pivot;
  if (!mysql_real_connect(&tmp_mysql,mysql->host,mysql->user,mysql->passwd,
			  mysql->db, mysql->port, mysql->unix_socket,
			  mysql->client_flag
#ifdef _mini_client_c
			  , mysql->net.read_timeout
#endif
))
  {
    mysql->net.last_errno= tmp_mysql.net.last_errno;
    strmov(mysql->net.last_error, tmp_mysql.net.last_error);
    DBUG_RETURN(1);
  }
  tmp_mysql.free_me=mysql->free_me;
  mysql->free_me=0;
  mysql_close(mysql);
  *mysql=tmp_mysql;
#ifdef _libmysql_c
  mysql_fix_pointers(mysql, &tmp_mysql); /* adjust connection pointers */
#endif
  net_clear(&mysql->net);
  mysql->affected_rows= ~(my_ulonglong) 0;
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

/*
  Free strings in the SSL structure and clear 'use_ssl' flag.
  NB! Errors are not reported until you do mysql_real_connect.
*/

#ifdef HAVE_OPENSSL
static void
mysql_ssl_free(MYSQL *mysql __attribute__((unused)))
{
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
}
#endif /* HAVE_OPENSSL */

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
#ifdef _libmysql_c
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
#ifdef HAVE_SMEM
    if (mysql->options.shared_memory_base_name != def_shared_memory_base_name)
      my_free(mysql->options.shared_memory_base_name,MYF(MY_ALLOW_ZERO_PTR));
#endif /* HAVE_SMEM */

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
    if (mysql->stmts)
    {
      /* Free any open prepared statements */
      LIST *element, *next_element;
      for (element= mysql->stmts; element; element= next_element)
      {
        next_element= element->next;
        stmt_close((MYSQL_STMT *)element->data, 0);
      }
    }
    if (mysql != mysql->master)
      mysql_close(mysql->master);
#endif /*_libmysql_c*/

#ifdef HAVE_OPENSSL
    mysql_ssl_free(mysql);
#endif /* HAVE_OPENSSL */
    /* Clear pointers for better safety */

    mysql->host_info=mysql->user=mysql->passwd=mysql->db=0;
    bzero((char*) &mysql->options,sizeof(mysql->options));
    if (mysql->free_me)
      my_free((gptr) mysql,MYF(0));
  }
  DBUG_VOID_RETURN;
}

my_bool STDCALL mysql_read_query_result(MYSQL *mysql)
{
  uchar *pos;
  ulong field_count;
  MYSQL_DATA *fields;
  ulong length;
  DBUG_ENTER("mysql_read_query_result");

#ifdef _libmysql_c
  /*
    Read from the connection which we actually used, which
    could differ from the original connection if we have slaves
  */
  mysql = mysql->last_used_con;
#endif

  if ((length = net_safe_read(mysql)) == packet_error)
    DBUG_RETURN(1);
  free_old_query(mysql);			/* Free old result */
get_info:
  pos=(uchar*) mysql->net.read_pos;
  if ((field_count= net_field_length(&pos)) == 0)
  {
    mysql->affected_rows= net_field_length_ll(&pos);
    mysql->insert_id=	  net_field_length_ll(&pos);
#ifdef _libmysql_c
    if (protocol_41(mysql))
    {
      mysql->server_status=uint2korr(pos); pos+=2;
      mysql->warning_count=uint2korr(pos); pos+=2;
    }
    else
#endif /*libmysql_c*/
    if (mysql->server_capabilities & CLIENT_TRANSACTIONS)
    {
      mysql->server_status=uint2korr(pos); pos+=2;
      mysql->warning_count= 0;
    }
    DBUG_PRINT("info",("status: %ld  warning_count:  %ld",
		       mysql->server_status, mysql->warning_count));
    if (pos < mysql->net.read_pos+length && net_field_length(&pos))
      mysql->info=(char*) pos;
    DBUG_RETURN(0);
  }
  if (field_count == NULL_LENGTH)		/* LOAD DATA LOCAL INFILE */
  {
    int error=send_file_to_server(mysql,(char*) pos);
    if ((length=net_safe_read(mysql)) == packet_error || error)
      DBUG_RETURN(1);
    goto get_info;				/* Get info packet */
  }
  if (!(mysql->server_status & SERVER_STATUS_AUTOCOMMIT))
    mysql->server_status|= SERVER_STATUS_IN_TRANS;

  mysql->extra_info= net_field_length_ll(&pos); /* Maybe number of rec */

  if (!(fields=read_rows(mysql,(MYSQL_FIELD*)0, 
#ifdef _libmysql_c
			 protocol_41(mysql) ? 6 : 
#endif
			 5)))
    DBUG_RETURN(1);
  if (!(mysql->fields=unpack_fields(fields,&mysql->field_alloc,
				    (uint) field_count,0,
				    mysql->server_capabilities)))
    DBUG_RETURN(1);
  mysql->status= MYSQL_STATUS_GET_RESULT;
  mysql->field_count= (uint) field_count;
  mysql->warning_count= 0;
  DBUG_RETURN(0);
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
#ifdef _libmysql_c
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
#endif /*libmysql_c*/

  DBUG_RETURN(simple_command(mysql, COM_QUERY, query, length, 1));
}


int STDCALL
mysql_real_query(MYSQL *mysql, const char *query, ulong length)
{
  DBUG_ENTER("mysql_real_query");
  DBUG_PRINT("enter",("handle: %lx",mysql));
  DBUG_PRINT("query",("Query = '%-.4096s'",query));

  if (mysql_send_query(mysql,query,length))
    DBUG_RETURN(1);
  DBUG_RETURN((int) mysql_read_query_result(mysql));
}

static my_bool
send_file_to_server(MYSQL *mysql, const char *filename)
{
  int fd, readcount;
  my_bool result= 1;
  uint packet_length=MY_ALIGN(mysql->net.max_packet-16,IO_SIZE);
  char *buf, tmp_name[FN_REFLEN];
  DBUG_ENTER("send_file_to_server");

  if (!(buf=my_malloc(packet_length,MYF(0))))
  {
    strmov(mysql->net.last_error, ER(mysql->net.last_errno=CR_OUT_OF_MEMORY));
    DBUG_RETURN(1);
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

#ifdef _libmysql_c
  /* read from the actually used connection */
  mysql = mysql->last_used_con;
#endif
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

uint STDCALL mysql_errno(MYSQL *mysql)
{
  return mysql->net.last_errno;
}

const char * STDCALL mysql_error(MYSQL *mysql)
{
  return mysql->net.last_error;
}
