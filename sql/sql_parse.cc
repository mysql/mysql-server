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


#include "mysql_priv.h"
#include "sql_acl.h"
#include "sql_repl.h"
#include <m_ctype.h>
#include <thr_alarm.h>
#include <myisam.h>
#include <my_dir.h>

#define SCRAMBLE_LENGTH 8


extern int yyparse(void);
extern "C" pthread_mutex_t THR_LOCK_keycache;
#ifdef SOLARIS
extern "C" int gethostname(char *name, int namelen);
#endif

static bool check_table_access(THD *thd,uint want_access, TABLE_LIST *tables);
static bool check_db_used(THD *thd,TABLE_LIST *tables);
static bool check_merge_table_access(THD *thd, char *db, TABLE_LIST *tables);
static bool check_dup(THD *thd,const char *db,const char *name,
		      TABLE_LIST *tables);
static void mysql_init_query(THD *thd);
static void remove_escape(char *name);
static void refresh_status(void);

const char *any_db="*any*";	// Special symbol for check_access

const char *command_name[]={
  "Sleep", "Quit", "Init DB", "Query", "Field List", "Create DB",
  "Drop DB", "Refresh", "Shutdown", "Statistics", "Processlist",
  "Connect","Kill","Debug","Ping","Time","Delayed_insert","Change user",
  "Binlog Dump","Table Dump",  "Connect Out"
};

bool volatile abort_slave = 0;

#ifdef HAVE_OPENSSL
extern VioSSLAcceptorFd* ssl_acceptor_fd;
#endif /* HAVE_OPENSSL */

#ifdef __WIN__
static void  test_signal(int sig_ptr)
{
#ifndef DBUG_OFF
  MessageBox(NULL,"Test signal","DBUG",MB_OK);
#endif
}
static void init_signals(void)
{
  int signals[7] = {SIGINT,SIGILL,SIGFPE,SIGSEGV,SIGTERM,SIGBREAK,SIGABRT } ;
  for(int i=0 ; i < 7 ; i++)
    signal( signals[i], test_signal) ;
}
#endif

static inline bool end_active_trans(THD *thd)
{
  if (!(thd->options & OPTION_AUTO_COMMIT) ||
      (thd->options & OPTION_BEGIN))
  {
    if (ha_commit(thd))
      return 1;
    thd->options&= ~(ulong) (OPTION_BEGIN);
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
  }
  return 0;
}


/*
** Check if user is ok
** Updates:
** thd->user, thd->master_access, thd->priv_user, thd->db, thd->db_access
*/

static bool check_user(THD *thd,enum_server_command command, const char *user,
		       const char *passwd, const char *db, bool check_count)
{
  NET *net= &thd->net;
  thd->db=0;

  if (!(thd->user = my_strdup(user, MYF(0))))
  {
    send_error(net,ER_OUT_OF_RESOURCES);
    return 1;
  }
  thd->master_access=acl_getroot(thd->host, thd->ip, thd->user,
				 passwd, thd->scramble, &thd->priv_user,
				 protocol_version == 9 ||
				 !(thd->client_capabilities &
				   CLIENT_LONG_PASSWORD));
  DBUG_PRINT("general",
	     ("Capabilities: %d  packet_length: %d  Host: '%s'  User: '%s'  Using password: %s  Access: %u  db: '%s'",
	      thd->client_capabilities, thd->max_packet_length,
	      thd->host ? thd->host : thd->ip, thd->priv_user,
	      passwd[0] ? "yes": "no",
	      thd->master_access, thd->db ? thd->db : "*none*"));
  if (thd->master_access & NO_ACCESS)
  {
    net_printf(net, ER_ACCESS_DENIED_ERROR,
	       thd->user,
	       thd->host ? thd->host : thd->ip,
	       passwd[0] ? ER(ER_YES) : ER(ER_NO));
    mysql_log.write(thd,COM_CONNECT,ER(ER_ACCESS_DENIED_ERROR),
		    thd->user,
		    thd->host ? thd->host : thd->ip ? thd->ip : "unknown ip",
		    passwd[0] ? ER(ER_YES) : ER(ER_NO));
    return(1);					// Error already given
  }
  if (check_count)
  {
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    bool tmp=(thread_count - delayed_insert_threads >= max_connections &&
	      !(thd->master_access & PROCESS_ACL));
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    if (tmp)
    {						// Too many connections
      send_error(net, ER_CON_COUNT_ERROR);
      return(1);
    }
  }
  mysql_log.write(thd,command,
		  (thd->priv_user == thd->user ?
		   (char*) "%s@%s on %s" :
		   (char*) "%s@%s as anonymous on %s"),
		  user,
		  thd->host ? thd->host : thd->ip ? thd->ip : "unknown ip",
		  db ? db : (char*) "");
  thd->db_access=0;
  if (db && db[0])
    return test(mysql_change_db(thd,db));
  else
    send_ok(net);				// Ready to handle questions
  return 0;					// ok
}


/*
** check connnetion and get priviliges
** returns 0 on ok, -1 < if error is given > 0 on error.
*/


static int
check_connections(THD *thd)
{
  uint connect_errors=0;
  NET *net= &thd->net;
  /*
  ** store the connection details
  */
  DBUG_PRINT("info", (("check_connections called by thread %d"),
	     thd->thread_id));
  DBUG_PRINT("general",("New connection received on %s",
			vio_description(net->vio)));
  if (!thd->host)                           // If TCP/IP connection
  {
    char ip[17];

    if (vio_peer_addr(net->vio,ip))
      return (ER_BAD_HOST_ERROR);
    if (!(thd->ip = my_strdup(ip,MYF(0))))
      return (ER_OUT_OF_RESOURCES);
#if !defined(HAVE_SYS_UN_H) || defined(HAVE_mit_thread)
    /* Fast local hostname resolve for Win32 */
    if (!strcmp(thd->ip,"127.0.0.1"))
      thd->host=(char*) localhost;
    else
#endif
    if (!(specialflag & SPECIAL_NO_RESOLVE))
    {
      vio_in_addr(net->vio,&thd->remote.sin_addr);
      thd->host=ip_to_hostname(&thd->remote.sin_addr,&connect_errors);
      if (connect_errors > max_connect_errors)
	return(ER_HOST_IS_BLOCKED);
    }
    DBUG_PRINT("general",("Host: %s  ip: %s",
			  thd->host ? thd->host : "unknown host",
			  thd->ip ? thd->ip : "unknown ip"));
    if (acl_check_host(thd->host,thd->ip))
      return(ER_HOST_NOT_PRIVILEGED);
  }
  else /* Hostname given means that the connection was on a socket */
  {
    DBUG_PRINT("general",("Host: %s",thd->host));
    thd->ip=0;
    bzero((char*) &thd->remote,sizeof(struct sockaddr));
  }
  vio_keepalive(net->vio, TRUE);

  /* nasty, but any other way? */
  uint pkt_len = 0;
  {
    char buff[60],*end;
    int client_flags = CLIENT_LONG_FLAG | CLIENT_CONNECT_WITH_DB |
	               CLIENT_TRANSACTIONS;
    LINT_INIT(pkt_len);

    end=strmov(buff,server_version)+1;
    int4store((uchar*) end,thd->thread_id);
    end+=4;
    memcpy(end,thd->scramble,SCRAMBLE_LENGTH+1);
    end+=SCRAMBLE_LENGTH +1;
#ifdef HAVE_COMPRESS
    client_flags |= CLIENT_COMPRESS;
#endif /* HAVE_COMPRESS */
#ifdef HAVE_OPENSSL
    if (ssl_acceptor_fd!=0)
      client_flags |= CLIENT_SSL;       /* Wow, SSL is avalaible! */
    /*
     * Without SSL the handshake consists of one packet. This packet
     * has both client capabilites and scrambled password.
     * With SSL the handshake might consist of two packets. If the first
     * packet (client capabilities) has CLIENT_SSL flag set, we have to
     * switch to SSL and read the second packet. The scrambled password
     * is in the second packet and client_capabilites field will be ignored.
     * Maybe it is better to accept flags other than CLIENT_SSL from the
     * second packet?
  */
#define  SSL_HANDSHAKE_SIZE      2
#define  NORMAL_HANDSHAKE_SIZE   6
#define  MIN_HANDSHAKE_SIZE      2

#else
#define  MIN_HANDSHAKE_SIZE      6
#endif /* HAVE_OPENSSL */
    int2store(end,client_flags);
    end[2]=MY_CHARSET_CURRENT;
    int2store(end+3,thd->server_status);
    bzero(end+5,13);
    end+=18;
    if (net_write_command(net,protocol_version, buff,
			  (uint) (end-buff)) ||
       (pkt_len=my_net_read(net)) == packet_error ||
	pkt_len < MIN_HANDSHAKE_SIZE)
    {
      inc_host_errors(&thd->remote.sin_addr);
      return(ER_HANDSHAKE_ERROR);
    }
  }
#ifdef _CUSTOMCONFIG_
#include "_cust_sql_parse.h"
#endif
  if (connect_errors)
    reset_host_errors(&thd->remote.sin_addr);
  if (thd->packet.alloc(net_buffer_length))
    return(ER_OUT_OF_RESOURCES);

  thd->client_capabilities=uint2korr(net->read_pos);
#ifdef HAVE_OPENSSL
  DBUG_PRINT("info",
	     ("pkt_len:%d, client capabilities: %d",
	      pkt_len, thd->client_capabilities) );
  if (thd->client_capabilities & CLIENT_SSL)
  {
    DBUG_PRINT("info", ("Agreed to change IO layer to SSL") );
    /* Do the SSL layering. */
    DBUG_PRINT("info", ("IO layer change in progress..."));
    VioSocket*	vio_socket = my_reinterpret_cast(VioSocket*)(net->vio);
    VioSSL*	vio_ssl =    ssl_acceptor_fd->accept(vio_socket);
    net->vio =               my_reinterpret_cast(NetVio*) (vio_ssl);
    DBUG_PRINT("info", ("Reading user information over SSL layer"));
    if ((pkt_len=my_net_read(net)) == packet_error ||
	pkt_len < NORMAL_HANDSHAKE_SIZE)
    {
      DBUG_PRINT("info", ("pkt_len:%d", pkt_len));
      DBUG_PRINT("error", ("Failed to read user information"));
      inc_host_errors(&thd->remote.sin_addr);
      return(ER_HANDSHAKE_ERROR);
    }
  }
  else
  {
    DBUG_PRINT("info", ("Leaving IO layer intact"));
    if (pkt_len < NORMAL_HANDSHAKE_SIZE)
    {
      inc_host_errors(&thd->remote.sin_addr);
      return ER_HANDSHAKE_ERROR;
    }
  }
#endif

  thd->max_packet_length=uint3korr(net->read_pos+2);
  char *user=   (char*) net->read_pos+5;
  char *passwd= strend(user)+1;
  char *db=0;
  if (passwd[0] && strlen(passwd) != SCRAMBLE_LENGTH)
    return ER_HANDSHAKE_ERROR;
  if (thd->client_capabilities & CLIENT_CONNECT_WITH_DB)
    db=strend(passwd)+1;
  if (thd->client_capabilities & CLIENT_INTERACTIVE)
    thd->inactive_timeout=net_interactive_timeout;
  if (thd->client_capabilities & CLIENT_TRANSACTIONS)
    thd->net.return_status= &thd->server_status;
  net->timeout=net_read_timeout;
  if (check_user(thd,COM_CONNECT, user, passwd, db, 1))
    return (-1);
  thd->password=test(passwd[0]);
  return 0;
}


pthread_handler_decl(handle_one_connection,arg)
{
  THD *thd=(THD*) arg;
  uint launch_time  =
    (uint) ((thd->thr_create_time = time(NULL)) - thd->connect_time);
  if (launch_time >= slow_launch_time)
    statistic_increment(slow_launch_threads,&LOCK_status );

  pthread_detach_this_thread();

#ifndef __WIN__	/* Win32 calls this in pthread_create */
  if (my_thread_init()) // needed to be called first before we call
    // DBUG_ macros
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES);
    statistic_increment(aborted_connects,&LOCK_thread_count);
    end_thread(thd,0);
    return 0;
  }
#endif

  // handle_one_connection() is the only way a thread would start
  // and would always be on top of the stack
  // therefore, the thread stack always starts at the address of the first
  // local variable of handle_one_connection, which is thd
  // we need to know the start of the stack so that we could check for
  // stack overruns

  DBUG_PRINT("info", ("handle_one_connection called by thread %d\n",
		      thd->thread_id));
  // now that we've called my_thread_init(), it is safe to call DBUG_*

#ifdef __WIN__
  init_signals();				// IRENA; testing ?
#else
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif
  if (thd->store_globals())
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES);
    statistic_increment(aborted_connects,&LOCK_thread_count);
    end_thread(thd,0);
    return 0;
  }

  do
  {
    int error;
    NET *net= &thd->net;

    thd->mysys_var=my_thread_var;
    thd->dbug_thread_id=my_thread_id();
    thd->thread_stack= (char*) &thd;

    if ((error=check_connections(thd)))
    {						// Wrong permissions
      if (error > 0)
	net_printf(net,error,thd->host ? thd->host : thd->ip);
#ifdef __NT__
      if (vio_type(net->vio) == VIO_TYPE_NAMEDPIPE)
	sleep(1);				/* must wait after eof() */
#endif
      statistic_increment(aborted_connects,&LOCK_thread_count);
      goto end_thread;
    }

    if (thd->max_join_size == HA_POS_ERROR)
      thd->options |= OPTION_BIG_SELECTS;
    if (thd->client_capabilities & CLIENT_COMPRESS)
      net->compress=1;				// Use compression
    if (thd->options & OPTION_ANSI_MODE)
      thd->client_capabilities|=CLIENT_IGNORE_SPACE;

    thd->proc_info=0;				// Remove 'login'
    thd->command=COM_SLEEP;
    thd->version=refresh_version;
    thd->set_time();
    init_sql_alloc(&thd->mem_root,8192,8192);
    while (!net->error && net->vio != 0 && !thd->killed)
    {
      if (do_command(thd))
	break;
    }
    free_root(&thd->mem_root,MYF(0));
    if (net->error && net->vio != 0)
    {
      sql_print_error(ER(ER_NEW_ABORTING_CONNECTION),
		      thd->thread_id,(thd->db ? thd->db : "unconnected"),
		      thd->user,
		      (thd->host ? thd->host : thd->ip ? thd->ip : "unknown"),
		      (net->last_errno ? ER(net->last_errno) :
		       ER(ER_UNKNOWN_ERROR)));
      send_error(net,net->last_errno,NullS);
      thread_safe_increment(aborted_threads,&LOCK_thread_count);
    }

end_thread:
    close_connection(net);
    end_thread(thd,1);
    /*
      If end_thread returns, we are either running with --one-thread
      or this thread has been schedule to handle the next query
    */
    thd= current_thd;
  } while (!(test_flags & TEST_NO_THREADS));
  /* The following is only executed if we are not using --one-thread */
  return(0);					/* purecov: deadcode */
}


int handle_bootstrap(THD *thd,FILE *file)
{
  DBUG_ENTER("handle_bootstrap");
  thd->thread_stack= (char*) &thd;

  if (init_thr_lock() ||
      my_pthread_setspecific_ptr(THR_THD,  thd) ||
      my_pthread_setspecific_ptr(THR_MALLOC, &thd->mem_root) ||
      my_pthread_setspecific_ptr(THR_NET,  &thd->net))
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES);
    DBUG_RETURN(-1);
  }
  thd->mysys_var=my_thread_var;
  thd->dbug_thread_id=my_thread_id();
#ifndef __WIN__
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  if (thd->max_join_size == (ulong) ~0L)
    thd->options |= OPTION_BIG_SELECTS;

  thd->proc_info=0;
  thd->version=refresh_version;

  char *buff= (char*) thd->net.buff;
  init_sql_alloc(&thd->mem_root,8192,8192);
  while (fgets(buff, thd->net.max_packet, file))
  {
    uint length=(uint) strlen(buff);
    while (length && (isspace(buff[length-1]) || buff[length-1] == ';'))
      length--;
    buff[length]=0;
    thd->current_tablenr=0;
    thd->query= thd->memdup(buff,length+1);
    thd->query_id=query_id++;
    mysql_parse(thd,thd->query,length);
    close_thread_tables(thd);			// Free tables
    if (thd->fatal_error)
    {
      DBUG_RETURN(-1);
    }
    free_root(&thd->mem_root,MYF(MY_KEEP_PREALLOC));
  }
  free_root(&thd->mem_root,MYF(0));
  DBUG_RETURN(0);
}


static inline void free_items(THD *thd)
{
    /* This works because items are allocated with sql_alloc() */
  for (Item *item=thd->free_list ; item ; item=item->next)
    delete item;
}

int mysql_table_dump(THD* thd, char* db, char* tbl_name, int fd)
{
  TABLE* table;
  TABLE_LIST* table_list;
  int error = 0;
  DBUG_ENTER("mysql_table_dump");
  db = (db && db[0]) ? db : thd->db;
  if(!(table_list = (TABLE_LIST*) sql_calloc(sizeof(TABLE_LIST))))
     DBUG_RETURN(1); // out of memory
  table_list->db = db;
  table_list->real_name = table_list->name = tbl_name;
  table_list->lock_type = TL_READ_NO_INSERT;
  table_list->next = 0;
  remove_escape(table_list->real_name);

  if(!(table=open_ltable(thd, table_list, TL_READ_NO_INSERT)))
    DBUG_RETURN(1);

  if (check_access(thd, SELECT_ACL, db, &table_list->grant.privilege))
    goto err;
  if (grant_option && check_grant(thd, SELECT_ACL, table_list))
    goto err;

  thd->free_list = 0;
  thd->query = tbl_name;
  if((error = mysqld_dump_create_info(thd, table, -1)))
    {
      my_error(ER_GET_ERRNO, MYF(0));
      goto err;
    }
  net_flush(&thd->net);
  error = table->file->dump(thd,fd);
  if(error)
      my_error(ER_GET_ERRNO, MYF(0));

err:

  close_thread_tables(thd);

  DBUG_RETURN(error);
}



	/* Execute one command from socket (query or simple command) */

bool do_command(THD *thd)
{
  char *packet;
  uint old_timeout,packet_length;
  bool	error=0;
  NET *net;
  enum enum_server_command command;
  DBUG_ENTER("do_command");

  net= &thd->net;
  thd->current_tablenr=0;

  packet=0;
  old_timeout=net->timeout;
  net->timeout=thd->inactive_timeout;		/* Wait max for 8 hours */
  net->last_error[0]=0;				// Clear error message
  net->last_errno=0;

  net_new_transaction(net);
  if ((packet_length=my_net_read(net)) == packet_error)
  {
     DBUG_PRINT("general",("Got error reading command from socket %s",
				vio_description(net->vio) ));
    return TRUE;
  }
  else
  {
    packet=(char*) net->read_pos;
    command = (enum enum_server_command) (uchar) packet[0];
    DBUG_PRINT("general",("Command on %s = %d (%s)",
			  vio_description(net->vio), command,
			  command_name[command]));
  }
  net->timeout=old_timeout;			/* Timeout */
  thd->command=command;
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query_id=query_id;
  if (command != COM_STATISTICS && command != COM_PING)
    query_id++;
  thread_running++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->set_time();
  switch(command) {
  case COM_INIT_DB:
    if (!mysql_change_db(thd,packet+1))
      mysql_log.write(thd,command,"%s",thd->db);
    break;
  case COM_TABLE_DUMP:
    {
      char* data = packet + 1;
      uint db_len = *data;
      uint tbl_len = *(data + db_len + 1);
      char* db = sql_alloc(db_len + tbl_len + 2);
      memcpy(db, data + 1, db_len);
      char* tbl_name = db + db_len;
      *tbl_name++ = 0;
      memcpy(tbl_name, data + db_len + 2, tbl_len);
      tbl_name[tbl_len] = 0;
      if(mysql_table_dump(thd, db, tbl_name, -1))
	send_error(&thd->net); // dump to NET

      break;
    }
  case COM_CHANGE_USER:
  {
    char *user=   (char*) packet+1;
    char *passwd= strend(user)+1;
    char *db=     strend(passwd)+1;

    /* Save user and privileges */
    uint save_master_access=thd->master_access;
    uint save_db_access=    thd->db_access;
    char *save_user=	    thd->user;
    char *save_priv_user=   thd->priv_user;
    char *save_db=	    thd->db;

    if ((uint) ((uchar*) db - net->read_pos) > packet_length)
    {						// Check if protocol is ok
      send_error(net, ER_UNKNOWN_COM_ERROR);
      break;
    }
    if (check_user(thd, COM_CHANGE_USER, user, passwd, db,0))
    {						// Restore old user
      x_free(thd->user);
      x_free(thd->db);
      thd->master_access=save_master_access;
      thd->db_access=save_db_access;
      thd->db=save_db;
      thd->user=save_user;
      thd->priv_user=save_priv_user;
      break;
    }
    x_free((gptr) save_db);
    x_free((gptr) save_user);
    thd->password=test(passwd[0]);
    break;
  }

  case COM_QUERY:
  {
    char *pos=packet+packet_length;		// Point at end null
    /* Remove garage at end of query */
    while (packet_length > 0 && pos[-1] == ';')
    {
      pos--;
      packet_length--;
    }
    *pos=0;
    if (!(thd->query= (char*) thd->memdup((gptr) (packet+1),packet_length)))
      break;
    thd->packet.shrink(net_buffer_length);	// Reclaim some memory
    if (!(specialflag & SPECIAL_NO_PRIOR))
      my_pthread_setprio(pthread_self(),QUERY_PRIOR);
    mysql_log.write(thd,command,"%s",thd->query);
    DBUG_PRINT("query",("%s",thd->query));
    mysql_parse(thd,thd->query,packet_length-1);
    if (!(specialflag & SPECIAL_NO_PRIOR))
      my_pthread_setprio(pthread_self(),WAIT_PRIOR);
    DBUG_PRINT("info",("query ready"));
    break;
  }
  case COM_FIELD_LIST:				// This isn't actually neaded
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    break;
#else
  {
    char *fields;
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    if (!(table_list.db=thd->db))
    {
      send_error(net,ER_NO_DB_ERROR);
      break;
    }
    thd->free_list=0;
    table_list.name=table_list.real_name=thd->strdup(packet+1);
    thd->query=fields=thd->strdup(strend(packet+1)+1);
    mysql_log.write(thd,command,"%s %s",table_list.real_name,fields);
    remove_escape(table_list.real_name);	// This can't have wildcards

    if (check_access(thd,SELECT_ACL,table_list.db,&thd->col_access))
      break;
    table_list.grant.privilege=thd->col_access;
    if (grant_option && check_grant(thd,SELECT_ACL,&table_list,2))
      break;
    mysqld_list_fields(thd,&table_list,fields);
    free_items(thd);
    break;
  }
#endif
  case COM_QUIT:
    mysql_log.write(thd,command,NullS);
    net->error=0;				// Don't give 'abort' message
    error=TRUE;					// End server
    break;

  case COM_CREATE_DB:
    {
      char *db=thd->strdup(packet+1);
      if (check_access(thd,CREATE_ACL,db,0,1))
	break;
      mysql_log.write(thd,command,packet+1);
      mysql_create_db(thd,db,0);
      break;
    }
  case COM_DROP_DB:
    {
      char *db=thd->strdup(packet+1);
      if (check_access(thd,DROP_ACL,db,0,1))
	break;
      mysql_log.write(thd,command,db);
      mysql_rm_db(thd,db,0);
      break;
    }
  case COM_BINLOG_DUMP:
    {
      if(check_access(thd, FILE_ACL, any_db))
	break;
      mysql_log.write(thd,command, 0);

      ulong pos;
      ushort flags;
      uint32 slave_server_id;
      pos = uint4korr(packet + 1);
      flags = uint2korr(packet + 5);
      pthread_mutex_lock(&LOCK_server_id);
      kill_zombie_dump_threads(slave_server_id = uint4korr(packet+7));
      thd->server_id = slave_server_id;
      pthread_mutex_unlock(&LOCK_server_id);
      mysql_binlog_send(thd, strdup(packet + 11), pos, flags);
      // fake COM_QUIT -- if we get here, the thread needs to terminate
      error = TRUE;
      net->error = 0;
      break;
    }
  case COM_REFRESH:
    {
      uint options=(uchar) packet[1];
      if (check_access(thd,RELOAD_ACL,any_db))
	break;
      mysql_log.write(thd,command,NullS);
      if (reload_acl_and_cache(thd, options, (TABLE_LIST*) 0))
	send_error(net,0);
      else
	send_eof(net);
      break;
    }
  case COM_SHUTDOWN:
    if (check_access(thd,SHUTDOWN_ACL,any_db))
      break; /* purecov: inspected */
    DBUG_PRINT("quit",("Got shutdown command"));
    mysql_log.write(thd,command,NullS);
    send_eof(net);
#ifdef __WIN__
    sleep(1);					// must wait after eof()
#endif
    send_eof(net);				// This is for 'quit request'
    close_connection(net);
    close_thread_tables(thd);			// Free before kill
    free_root(&thd->mem_root,MYF(0));
    kill_mysql();
    error=TRUE;
    break;

  case COM_STATISTICS:
  {
    mysql_log.write(thd,command,NullS);
    char buff[200];
    ulong uptime = (ulong) (thd->start_time - start_time);
    sprintf((char*) buff,
	    "Uptime: %ld  Threads: %d  Questions: %lu  Slow queries: %ld  Opens: %ld  Flush tables: %ld  Open tables: %d Queries per second avg: %.3f",
	    uptime,
	    (int) thread_count,thd->query_id,long_query_count,
	    opened_tables,refresh_version, cached_tables(),
	    uptime ? (float)thd->query_id/(float)uptime : 0);
#ifdef SAFEMALLOC
    if (lCurMemory)				// Using SAFEMALLOC
      sprintf(strend(buff), "  Memory in use: %ldK  Max memory used: %ldK",
	      (lCurMemory+1023L)/1024L,(lMaxMemory+1023L)/1024L);
 #endif
    VOID(my_net_write(net, buff,(uint) strlen(buff)));
    VOID(net_flush(net));
    break;
  }
  case COM_PING:
    send_ok(net);				// Tell client we are alive
    break;
  case COM_PROCESS_INFO:
    if (!thd->priv_user[0] && check_access(thd,PROCESS_ACL,any_db))
      break;
    mysql_log.write(thd,command,NullS);
    mysqld_list_processes(thd,thd->master_access & PROCESS_ACL ? NullS :
			  thd->priv_user,0);
    break;
  case COM_PROCESS_KILL:
  {
    ulong id=(ulong) uint4korr(packet+1);
    kill_one_thread(thd,id);
    break;
  }
  case COM_DEBUG:
    if (check_access(thd,PROCESS_ACL,any_db))
      break;					/* purecov: inspected */
    mysql_print_status(thd);
    mysql_log.write(thd,command,NullS);
    send_eof(net);
    break;
  case COM_SLEEP:
  case COM_CONNECT:				// Impossible here
  case COM_TIME:				// Impossible from client
  case COM_DELAYED_INSERT:
  default:
    send_error(net, ER_UNKNOWN_COM_ERROR);
    break;
  }
  if (thd->lock || thd->open_tables)
  {
    thd->proc_info="closing tables";
    close_thread_tables(thd);			/* Free tables */
  }
  thd->proc_info="cleaning up";

  if (thd->fatal_error)
    send_error(net,0);				// End of memory ?

  time_t start_of_query=thd->start_time;
  thd->end_time();				// Set start time
  /* If not reading from backup and if the query took too long */
  if (!thd->user_time)
  {
    if ((ulong) (thd->start_time - thd->time_after_lock) > long_query_time ||
	((thd->options & (OPTION_NO_INDEX_USED | OPTION_NO_GOOD_INDEX_USED)) &&
	 (specialflag & SPECIAL_LONG_LOG_FORMAT)))
    {
      long_query_count++;
      mysql_slow_log.write(thd, thd->query, thd->query_length, start_of_query);
    }
  }
  thd->proc_info="cleaning up2";
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For process list
  thd->proc_info=0;
  thd->command=COM_SLEEP;
  thd->query=0;
  thread_running--;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->packet.shrink(net_buffer_length);	// Reclaim some memory
  free_root(&thd->mem_root,MYF(MY_KEEP_PREALLOC));
  DBUG_RETURN(error);
}

/****************************************************************************
** mysql_execute_command
** Execute command saved in thd and current_lex->sql_command
****************************************************************************/

void
mysql_execute_command(void)
{
  int	res=0;
  THD	*thd=current_thd;
  LEX	*lex=current_lex;
  TABLE_LIST *tables=(TABLE_LIST*) lex->table_list.first;
  DBUG_ENTER("mysql_execute_command");

  if(thd->slave_thread && table_rules_on && tables && !tables_ok(thd,tables))
    DBUG_VOID_RETURN; // skip if we are in the slave thread, some table
  // rules have been given and the table list says the query should not be
  // replicated
  
  switch (lex->sql_command) {
  case SQLCOM_SELECT:
  {
    select_result *result;
    if (lex->options & SELECT_DESCRIBE)
      lex->exchange=0;
    if (tables)
    {
      res=check_table_access(thd,
			     lex->exchange ? SELECT_ACL | FILE_ACL :
			     SELECT_ACL,
			     tables);
    }
    else
      res=check_access(thd, lex->exchange ? SELECT_ACL | FILE_ACL : SELECT_ACL,
		       any_db);
    if (res)
    {
      res=0;
      break;					// Error message is given
    }

    thd->offset_limit=lex->offset_limit;
    thd->select_limit=lex->select_limit+lex->offset_limit;
    if (thd->select_limit < lex->select_limit)
      thd->select_limit= HA_POS_ERROR;		// no limit

    if (lex->exchange)
    {
      if (lex->exchange->dumpfile)
      {
	if (!(result=new select_dump(lex->exchange)))
	{
	  res= -1;
	  break;
	}
      }
      else
      {
	if (!(result=new select_export(lex->exchange)))
	{
	  res= -1;
	  break;
	}
      }
    }
    else if (!(result=new select_send()))
    {
      res= -1;
#ifdef DELETE_ITEMS
      delete lex->having;
      delete lex->where;
#endif
      break;
    }

    if (lex->options & SELECT_HIGH_PRIORITY)
    {
      TABLE_LIST *table;
      for (table = tables ; table ; table=table->next)
	table->lock_type=TL_READ_HIGH_PRIORITY;
    }

    if (!(res=open_and_lock_tables(thd,tables)))
    {
      res=mysql_select(thd,tables,lex->item_list,
		       lex->where,
                       lex->ftfunc_list,
		       (ORDER*) lex->order_list.first,
		       (ORDER*) lex->group_list.first,
		       lex->having,
		       (ORDER*) lex->proc_list.first,
		       lex->options | thd->options,
		       result);
      if (res)
	result->abort();
    }
    delete result;
#ifdef DELETE_ITEMS
    delete lex->having;
    delete lex->where;
#endif
    break;
  }
  case SQLCOM_PURGE:
    {
      if(check_access(thd, PROCESS_ACL, any_db))
	goto error;
      res = purge_master_logs(thd, lex->to_log);
      break;
    }
  case SQLCOM_BACKUP_TABLE:
    {
      if (check_db_used(thd,tables) ||
	  check_table_access(thd,SELECT_ACL, tables) ||
	  check_access(thd, FILE_ACL, any_db))
	goto error; /* purecov: inspected */
      res = mysql_backup_table(thd, tables);

      break;
    }
  case SQLCOM_RESTORE_TABLE:
    {
      if (check_db_used(thd,tables) ||
	  check_table_access(thd,INSERT_ACL, tables) ||
	  check_access(thd, FILE_ACL, any_db))
	goto error; /* purecov: inspected */
      res = mysql_restore_table(thd, tables);
      break;
    }
  case SQLCOM_CHANGE_MASTER:
    {
      if(check_access(thd, PROCESS_ACL, any_db))
	goto error;
      res = change_master(thd);
      break;
    }
  case SQLCOM_SHOW_SLAVE_STAT:
    {
      if(check_access(thd, PROCESS_ACL, any_db))
	goto error;
      res = show_master_info(thd);
      break;
    }
  case SQLCOM_SHOW_MASTER_STAT:
    {
      if (check_access(thd, PROCESS_ACL, any_db))
	goto error;
      res = show_binlog_info(thd);
      break;
    }
  case SQLCOM_LOAD_MASTER_TABLE:

    if (!tables->db)
      tables->db=thd->db;
    if (check_access(thd,CREATE_ACL,tables->db,&tables->grant.privilege))
      goto error;				/* purecov: inspected */
    if (grant_option)
    {
      /* Check that the first table has CREATE privilege */
      TABLE_LIST *tmp_table_list=tables->next;
      tables->next=0;
      bool error=check_grant(thd,CREATE_ACL,tables);
      tables->next=tmp_table_list;
      if (error)
	  goto error;
    }
    if (strlen(tables->name) > NAME_LEN)
    {
      net_printf(&thd->net,ER_WRONG_TABLE_NAME,tables->name);
      res=0;
      break;
    }

    thd->last_nx_table = tables->real_name;
    thd->last_nx_db = tables->db;
    if(fetch_nx_table(thd, &glob_mi))
      // fetch_nx_table is responsible for sending
      // the error
      {
	res = 0;
	thd->net.no_send_ok = 0; // easier to do it here
	// this way we make sure that when we are done, we are clean
        break;
      }

    res = 0;
    send_ok(&thd->net);
    break;

  case SQLCOM_CREATE_TABLE:
    if (!tables->db)
      tables->db=thd->db;
    if (check_access(thd,CREATE_ACL,tables->db,&tables->grant.privilege) ||
	check_merge_table_access(thd, tables->db,
				 (TABLE_LIST *)
				 lex->create_info.merge_list.first))
      goto error;				/* purecov: inspected */
    if (grant_option)
    {
      /* Check that the first table has CREATE privilege */
      TABLE_LIST *tmp_table_list=tables->next;
      tables->next=0;
      bool error=check_grant(thd,CREATE_ACL,tables);
      tables->next=tmp_table_list;
      if (error)
	goto error;
    }
    if (strlen(tables->name) > NAME_LEN)
    {
      net_printf(&thd->net,ER_WRONG_TABLE_NAME,tables->name);
      res=0;
      break;
    }
    if (lex->item_list.elements)		// With select
    {
      select_result *result;

      if (!(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
	  check_dup(thd,tables->db,tables->real_name,tables->next))
      {
	net_printf(&thd->net,ER_INSERT_TABLE_USED,tables->real_name);
	DBUG_VOID_RETURN;
      }
      if (tables->next)
      {
	if (check_table_access(thd, SELECT_ACL, tables->next))
	  goto error;				// Error message is given
      }
      thd->offset_limit=lex->offset_limit;
      thd->select_limit=lex->select_limit+lex->offset_limit;
      if (thd->select_limit < lex->select_limit)
	thd->select_limit= HA_POS_ERROR;		// No limit

      if (!(res=open_and_lock_tables(thd,tables->next)))
      {
	if ((result=new select_create(tables->db ? tables->db : thd->db,
				      tables->real_name, &lex->create_info,
				      lex->create_list,
				      lex->key_list,
				      lex->item_list,lex->duplicates)))
	{
	  res=mysql_select(thd,tables->next,lex->item_list,
			   lex->where,
                           lex->ftfunc_list,
			   (ORDER*) lex->order_list.first,
			   (ORDER*) lex->group_list.first,
			   lex->having,
			   (ORDER*) lex->proc_list.first,
			   lex->options | thd->options,
			   result);
	  if (res)
	    result->abort();
	  delete result;
	}
	else
	  res= -1;
      }
    }
    else // regular create
    {
      res = mysql_create_table(thd,tables->db ? tables->db : thd->db,
			       tables->real_name, &lex->create_info,
			       lex->create_list,
			       lex->key_list,0, 0); // do logging
      if (!res)
	send_ok(&thd->net);
    }
    break;
  case SQLCOM_CREATE_INDEX:
    if (!tables->db)
      tables->db=thd->db;
    if (check_access(thd,INDEX_ACL,tables->db,&tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,INDEX_ACL,tables))
      goto error;
    res = mysql_create_index(thd, tables, lex->key_list);
    break;

  case SQLCOM_SLAVE_START:
    start_slave(thd);
    break;
  case SQLCOM_SLAVE_STOP:
    stop_slave(thd);
    break;

  case SQLCOM_ALTER_TABLE:
#if defined(DONT_ALLOW_SHOW_COMMANDS)
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND); /* purecov: inspected */
    break;
#else
    {
      uint priv=0;
      if (lex->name && strlen(lex->name) > NAME_LEN)
      {
	net_printf(&thd->net,ER_WRONG_TABLE_NAME,lex->name);
	res=0;
	break;
      }
      if (!tables->db)
	tables->db=thd->db;
      if (!lex->db)
	lex->db=tables->db;
      if (check_access(thd,ALTER_ACL,tables->db,&tables->grant.privilege) ||
	  check_access(thd,INSERT_ACL | CREATE_ACL,lex->db,&priv) ||
	  check_merge_table_access(thd, tables->db, 
				   (TABLE_LIST *)
				   lex->create_info.merge_list.first))
	goto error;				/* purecov: inspected */
      if (!tables->db)
	tables->db=thd->db;
      if (grant_option)
      {
	if (check_grant(thd,ALTER_ACL,tables))
	  goto error;
	if (lex->name && !test_all_bits(priv,INSERT_ACL | CREATE_ACL))
	{					// Rename of table
	  TABLE_LIST tmp_table;
	  bzero((char*) &tmp_table,sizeof(tmp_table));
	  tmp_table.real_name=lex->name;
	  tmp_table.db=lex->db;
	  tmp_table.grant.privilege=priv;
	  if (check_grant(thd,INSERT_ACL | CREATE_ACL,tables))
	    goto error;
	}
      }
      /* ALTER TABLE ends previous transaction */
      if (end_active_trans(thd))
	res= -1;
      else
	res= mysql_alter_table(thd, lex->db, lex->name,
			       &lex->create_info,
			       tables, lex->create_list,
			       lex->key_list, lex->drop_list, lex->alter_list,
                               (ORDER *) lex->order_list.first,
			       lex->drop_primary, lex->duplicates);
      break;
    }
#endif
  case SQLCOM_RENAME_TABLE:
  {
    TABLE_LIST *table;
    if (check_db_used(thd,tables))
      goto error;
    for (table=tables ; table ; table=table->next->next)
    {
      if (check_access(thd, ALTER_ACL | DROP_ACL, table->db,
		       &table->grant.privilege) ||
	  check_access(thd, INSERT_ACL | CREATE_ACL, table->next->db,
		       &table->next->grant.privilege))
	goto error;
      if (grant_option)
      {
	TABLE_LIST old_list,new_list;
	old_list=table[0];
	new_list=table->next[0];
	old_list.next=new_list.next=0;
	if (check_grant(thd,ALTER_ACL,&old_list) ||
	    (!test_all_bits(table->next->grant.privilege,
			   INSERT_ACL | CREATE_ACL) &&
	     check_grant(thd,INSERT_ACL | CREATE_ACL, &new_list)))
	  goto error;
      }
    }
    if (mysql_rename_tables(thd,tables))
      res= -1;
    break;
  }
  case SQLCOM_SHOW_BINLOGS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND); /* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      if(check_access(thd, PROCESS_ACL, any_db))
	goto error;
      res = show_binlogs(thd);
      break;
    }
#endif    
  case SQLCOM_SHOW_CREATE:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND); /* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      if (check_db_used(thd, tables) ||
	  check_access(thd, SELECT_ACL | EXTRA_ACL, tables->db,
		       &tables->grant.privilege))
	goto error;
      res = mysqld_show_create(thd, tables);
      break;
    }
#endif
  case SQLCOM_REPAIR:
    {
      if (check_db_used(thd,tables) ||
	  check_table_access(thd,SELECT_ACL | INSERT_ACL, tables))
	goto error; /* purecov: inspected */
      res = mysql_repair_table(thd, tables, &lex->check_opt);
      break;
    }
  case SQLCOM_CHECK:
    {
      if (check_db_used(thd,tables) ||
	  check_table_access(thd, SELECT_ACL | EXTRA_ACL , tables))
	goto error; /* purecov: inspected */
      res = mysql_check_table(thd, tables, &lex->check_opt);
      break;
    }
  case SQLCOM_ANALYZE:
  {
    if (check_db_used(thd,tables) ||
	check_table_access(thd,SELECT_ACL | INSERT_ACL, tables))
      goto error; /* purecov: inspected */
    res = mysql_analyze_table(thd, tables, &lex->check_opt);
    break;
  }

  case SQLCOM_OPTIMIZE:
  {
    HA_CREATE_INFO create_info;
    if (check_db_used(thd,tables) ||
	check_table_access(thd,SELECT_ACL | INSERT_ACL, tables))
      goto error; /* purecov: inspected */
    if (specialflag & (SPECIAL_SAFE_MODE | SPECIAL_NO_NEW_FUNC))
    {
      /* Use ALTER TABLE */
      lex->create_list.empty();
      lex->key_list.empty();
      lex->col_list.empty();
      lex->drop_list.empty();
      lex->alter_list.empty();
      bzero((char*) &create_info,sizeof(create_info));
      create_info.db_type=DB_TYPE_DEFAULT;
      create_info.row_type=ROW_TYPE_DEFAULT;
      res= mysql_alter_table(thd, NullS, NullS, &create_info,
			     tables, lex->create_list,
			     lex->key_list, lex->drop_list, lex->alter_list,
                             (ORDER *) 0,
			     0,DUP_ERROR);
    }
    else
      res = mysql_optimize_table(thd, tables, &lex->check_opt);
    break;
  }
  case SQLCOM_UPDATE:
    if (check_access(thd,UPDATE_ACL,tables->db,&tables->grant.privilege))
      goto error;
    if (grant_option && check_grant(thd,UPDATE_ACL,tables))
      goto error;
    if (lex->item_list.elements != lex->value_list.elements)
    {
      send_error(&thd->net,ER_WRONG_VALUE_COUNT);
      DBUG_VOID_RETURN;
    }
    res = mysql_update(thd,tables,
		       lex->item_list,
		       lex->value_list,
		       lex->where,
		       lex->select_limit,
		       lex->duplicates,
		       lex->lock_option);

#ifdef DELETE_ITEMS
    delete lex->where;
#endif
    break;
  case SQLCOM_INSERT:
    if (check_access(thd,INSERT_ACL,tables->db,&tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,INSERT_ACL,tables))
      goto error;
    res = mysql_insert(thd,tables,lex->field_list,lex->many_values,
		       lex->duplicates,
		       lex->lock_option);
    break;
  case SQLCOM_REPLACE:
    if (check_access(thd,INSERT_ACL | UPDATE_ACL | DELETE_ACL,
		     tables->db,&tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,INSERT_ACL | UPDATE_ACL | DELETE_ACL,
				    tables))

      goto error;
    res = mysql_insert(thd,tables,lex->field_list,lex->many_values,
		       DUP_REPLACE,
		       lex->lock_option);
    break;
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  {
    // Check that we have modify privileges for the first table and
    // select privileges for the rest
    uint privilege= (lex->sql_command == SQLCOM_INSERT_SELECT ?
		     INSERT_ACL : INSERT_ACL | UPDATE_ACL | DELETE_ACL);
    TABLE_LIST *save_next=tables->next;
    tables->next=0;
    if (check_access(thd, privilege,
		     tables->db,&tables->grant.privilege) ||
	(grant_option && check_grant(thd, privilege, tables)))
      goto error;
    tables->next=save_next;
    if ((res=check_table_access(thd, SELECT_ACL, save_next)))
      goto error;

    select_result *result;
    thd->offset_limit=lex->offset_limit;
    thd->select_limit=lex->select_limit+lex->offset_limit;
    if (thd->select_limit < lex->select_limit)
      thd->select_limit= HA_POS_ERROR;		// No limit

    if (check_dup(thd,tables->db,tables->real_name,tables->next))
    {
      net_printf(&thd->net,ER_INSERT_TABLE_USED,tables->real_name);
      DBUG_VOID_RETURN;
    }
    tables->lock_type=TL_WRITE;				// update first table
    if (!(res=open_and_lock_tables(thd,tables)))
    {
      if ((result=new select_insert(tables->table,&lex->field_list,
				    lex->sql_command == SQLCOM_REPLACE_SELECT ?
				    DUP_REPLACE : DUP_IGNORE)))
      {
	res=mysql_select(thd,tables->next,lex->item_list,
			 lex->where,
                         lex->ftfunc_list,
			 (ORDER*) lex->order_list.first,
			 (ORDER*) lex->group_list.first,
			 lex->having,
			 (ORDER*) lex->proc_list.first,
			 lex->options | thd->options,
			 result);
	delete result;
      }
      else
	res= -1;
    }
#ifdef DELETE_ITEMS
    delete lex->having;
    delete lex->where;
#endif
    break;
  }
  case SQLCOM_DELETE:
  case SQLCOM_TRUNCATE:
  {
    if (check_access(thd,DELETE_ACL,tables->db,&tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,DELETE_ACL,tables))
      goto error;
    // Set privilege for the WHERE clause
    tables->grant.want_privilege=(SELECT_ACL & ~tables->grant.privilege);
    /* TRUNCATE ends previous transaction */
    if (lex->sql_command == SQLCOM_TRUNCATE && end_active_trans(thd))
      res= -1;
    else
      res = mysql_delete(thd,tables,lex->where,lex->select_limit,
			 lex->lock_option, lex->options);
    break;
  }
  case SQLCOM_DROP_TABLE:
    {
      if (check_table_access(thd,DROP_ACL,tables))
	goto error;				/* purecov: inspected */
      res = mysql_rm_table(thd,tables,lex->drop_if_exists);
    }
    break;
  case SQLCOM_DROP_INDEX:
    if (!tables->db)
      tables->db=thd->db;
    if (check_access(thd,INDEX_ACL,tables->db,&tables->grant.privilege))
      goto error;				/* purecov: inspected */
    if (grant_option && check_grant(thd,INDEX_ACL,tables))
      goto error;
    res = mysql_drop_index(thd, tables, lex->drop_list);
    break;
  case SQLCOM_SHOW_DATABASES:
#if defined(DONT_ALLOW_SHOW_COMMANDS)
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    if ((specialflag & SPECIAL_SKIP_SHOW_DB) &&
	check_access(thd,PROCESS_ACL,any_db))
      goto error;
    res= mysqld_show_dbs(thd, (lex->wild ? lex->wild->ptr() : NullS));
    break;
#endif
  case SQLCOM_SHOW_PROCESSLIST:
    if (!thd->priv_user[0] && check_access(thd,PROCESS_ACL,any_db))
      break;
    mysqld_list_processes(thd,thd->master_access & PROCESS_ACL ? NullS :
			  thd->priv_user,lex->verbose);
    break;
  case SQLCOM_SHOW_STATUS:
    res= mysqld_show(thd,(lex->wild ? lex->wild->ptr() : NullS),status_vars);
    break;
  case SQLCOM_SHOW_VARIABLES:
    res= mysqld_show(thd, (lex->wild ? lex->wild->ptr() : NullS),
		     init_vars);
    break;
  case SQLCOM_SHOW_TABLES:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      char *db=lex->db ? lex->db : thd->db;
      if (!db)
      {
	send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
	goto error;				/* purecov: inspected */
      }
      remove_escape(db);				// Fix escaped '_'
      if (strlen(db) > NAME_LEN)
      {
	net_printf(&thd->net,ER_WRONG_DB_NAME, db);
	goto error;
      }
      if (check_access(thd,SELECT_ACL,db,&thd->col_access))
	goto error;				/* purecov: inspected */
      /* grant is checked in mysqld_show_tables */
      if (lex->options & SELECT_DESCRIBE)
      res= mysqld_extend_show_tables(thd,db,
				     (lex->wild ? lex->wild->ptr() : NullS));
      else
	res= mysqld_show_tables(thd,db,
				(lex->wild ? lex->wild->ptr() : NullS));
      break;
    }
#endif
  case SQLCOM_SHOW_FIELDS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      char *db=tables->db ? tables->db : thd->db;
      if (!db)
      {
	send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
	goto error;				/* purecov: inspected */
      }
      remove_escape(db);			// Fix escaped '_'
      remove_escape(tables->name);
      if (!tables->db)
	tables->db=thd->db;
      if (check_access(thd,SELECT_ACL | EXTRA_ACL,db,&thd->col_access))
	goto error;				/* purecov: inspected */
      tables->grant.privilege=thd->col_access;
      if (grant_option && check_grant(thd,SELECT_ACL,tables,2))
	goto error;
      res= mysqld_show_fields(thd,tables,
			      (lex->wild ? lex->wild->ptr() : NullS));
      break;
    }
#endif
  case SQLCOM_SHOW_KEYS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      char *db=tables->db ? tables->db : thd->db;
      if (!db)
      {
	send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
	goto error;				/* purecov: inspected */
      }
      remove_escape(db);			// Fix escaped '_'
      remove_escape(tables->name);
      if (!tables->db)
	tables->db=thd->db;
      if (check_access(thd,SELECT_ACL,db,&thd->col_access))
	goto error; /* purecov: inspected */
      tables->grant.privilege=thd->col_access;
      if (grant_option && check_grant(thd,SELECT_ACL,tables,2))
	goto error;
      res= mysqld_show_keys(thd,tables);
      break;
    }
#endif
  case SQLCOM_CHANGE_DB:
    mysql_change_db(thd,lex->db);
    break;
  case SQLCOM_LOAD:
  {
    uint privilege= (lex->duplicates == DUP_REPLACE ?
		     INSERT_ACL | UPDATE_ACL | DELETE_ACL : INSERT_ACL);
    if (!(lex->local_file && (thd->client_capabilities & CLIENT_LOCAL_FILES)))
    {
      if (check_access(thd,privilege | FILE_ACL,tables->db))
	goto error;
    }
    else
    {
      if (check_access(thd,privilege,tables->db,&tables->grant.privilege) ||
	 grant_option && check_grant(thd,privilege,tables))
	goto error;
    }
    res=mysql_load(thd, lex->exchange, tables, lex->field_list,
		   lex->duplicates, (bool) lex->local_file, lex->lock_option);
    break;
  }
  case SQLCOM_SET_OPTION:
  {
    uint org_options=thd->options;
    thd->options=lex->options;
    thd->update_lock_default= ((thd->options & OPTION_LOW_PRIORITY_UPDATES) ?
			       TL_WRITE_LOW_PRIORITY : TL_WRITE);
    thd->default_select_limit=lex->select_limit;
    DBUG_PRINT("info",("options: %ld  limit: %ld",
		       thd->options,(long) thd->default_select_limit));

    /* Check if auto_commit mode changed */
    if ((org_options ^ lex->options) & OPTION_AUTO_COMMIT)
    {
      if (!(org_options & OPTION_AUTO_COMMIT))
      {
	/* We changed to auto_commit mode */
	thd->options&= ~(ulong) (OPTION_BEGIN);
	thd->server_status|= SERVER_STATUS_AUTOCOMMIT;
	if (ha_commit(thd))
	{
	  res= -1;
	  break;
	}
      }
      else
	thd->server_status&= ~SERVER_STATUS_AUTOCOMMIT;
    }
    send_ok(&thd->net);
    break;
  }
  case SQLCOM_UNLOCK_TABLES:
    if (thd->locked_tables)
    {
      thd->lock=thd->locked_tables;
      thd->locked_tables=0;			// Will be automaticly closed
    }
    if (thd->global_read_lock)
    {
      thd->global_read_lock=0;
      pthread_mutex_lock(&LOCK_open);
      global_read_lock--;
      pthread_cond_broadcast(&COND_refresh);
      pthread_mutex_unlock(&LOCK_open);
    }
    send_ok(&thd->net);
    break;
  case SQLCOM_LOCK_TABLES:
    if (thd->locked_tables)
    {
      thd->lock=thd->locked_tables;
      thd->locked_tables=0;			// Will be automaticly closed
      close_thread_tables(thd);
    }
    if (check_db_used(thd,tables))
      goto error;
    thd->in_lock_tables=1;
    if (!(res=open_and_lock_tables(thd,tables)))
    {
      thd->locked_tables=thd->lock;
      thd->lock=0;
      send_ok(&thd->net);
    }
    thd->in_lock_tables=0;
    break;
  case SQLCOM_CREATE_DB:
    {
      if (check_access(thd,CREATE_ACL,lex->name,0,1))
	break;
      mysql_create_db(thd,lex->name,lex->create_info.options);
      break;
    }
  case SQLCOM_DROP_DB:
    {
      if (check_access(thd,DROP_ACL,lex->name,0,1))
	break;
      mysql_rm_db(thd,lex->name,lex->drop_if_exists);
      break;
    }
  case SQLCOM_CREATE_FUNCTION:
    if (check_access(thd,INSERT_ACL,"mysql",0,1))
      break;
#ifdef HAVE_DLOPEN
    if (!(res = mysql_create_function(thd,&lex->udf)))
      send_ok(&thd->net);
#else
    res= -1;
#endif
    break;
  case SQLCOM_DROP_FUNCTION:
    if (check_access(thd,DELETE_ACL,"mysql",0,1))
      break;
#ifdef HAVE_DLOPEN
    if (!(res = mysql_drop_function(thd,lex->udf.name)))
      send_ok(&thd->net);
#else
    res= -1;
#endif
    break;
 case SQLCOM_REVOKE:
 case SQLCOM_GRANT:
   {
     if (tables && !tables->db)
       tables->db=thd->db;
     if (check_access(thd, lex->grant | lex->grant_tot_col | GRANT_ACL,
		      tables && tables->db ? tables->db : lex->db,
		      tables ? &tables->grant.privilege : 0,
		      tables ? 0 : 1))
       goto error;

     /* Check that the user isn't trying to change a password for another
	user if he doesn't have UPDATE privilege to the MySQL database */

     if (thd->user)				// If not replication
     {
       LEX_USER *user;
       List_iterator <LEX_USER> user_list(lex->users_list);
       while ((user=user_list++))
       {
	 if (user->password.str &&
	     (strcmp(thd->user,user->user.str) ||
	      user->host.str &&
	      my_strcasecmp(user->host.str, thd->host ? thd->host : thd->ip)))
	 {
	   if (check_access(thd, UPDATE_ACL, "mysql",0,1))
	     goto error;
	   break;			// We are allowed to do changes
	 }
       }
     }
     if (tables)
     {
       if (grant_option && check_grant(thd,
				       (lex->grant | lex->grant_tot_col |
					GRANT_ACL),
				       tables))
	 goto error;
       res = mysql_table_grant(thd,tables,lex->users_list, lex->columns,
			       lex->grant, lex->sql_command == SQLCOM_REVOKE);
       if(!res)
       {
	 mysql_update_log.write(thd, thd->query,thd->query_length);
	 if (mysql_bin_log.is_open())
	 {
	   Query_log_event qinfo(thd, thd->query);
	   mysql_bin_log.write(&qinfo);
	 }
       }
     }
     else
     {
       if (lex->columns.elements)
       {
	 net_printf(&thd->net,ER_ILLEGAL_GRANT_FOR_TABLE);
	 res=1;
       }
       else
	 res = mysql_grant(thd, lex->db, lex->users_list, lex->grant,
			   lex->sql_command == SQLCOM_REVOKE);
       if(!res)
       {
	 mysql_update_log.write(thd, thd->query,thd->query_length);
	 if (mysql_bin_log.is_open())
	 {
	   Query_log_event qinfo(thd, thd->query);
	   mysql_bin_log.write(&qinfo);
	 }
       }
     }
     break;
   }
  case SQLCOM_FLUSH:
  case SQLCOM_RESET:
    if (check_access(thd,RELOAD_ACL,any_db) || check_db_used(thd, tables))
      goto error;
    if (reload_acl_and_cache(thd, lex->type, tables))
      send_error(&thd->net,0);
    else
      send_ok(&thd->net);
    break;
  case SQLCOM_KILL:
    kill_one_thread(thd,lex->thread_id);
    break;
  case SQLCOM_SHOW_GRANTS:
    res=0;
    if ((thd->user && !strcmp(thd->user,lex->grant_user->user.str)) ||
	!(check_access(thd, SELECT_ACL, "mysql")))
    {
      res = mysql_show_grants(thd,lex->grant_user);
    }
    break;
  case SQLCOM_BEGIN:
    thd->options|= OPTION_BEGIN;
    thd->server_status|= SERVER_STATUS_IN_TRANS;
    send_ok(&thd->net);
    break;
  case SQLCOM_COMMIT:
    /*
      We don't use end_active_trans() here to ensure that this works
      even if there is a problem with the OPTION_AUTO_COMMIT flag
      (Which of course should never happen...)
    */
    thd->options&= ~(ulong) (OPTION_BEGIN);
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (!ha_commit(thd))
      send_ok(&thd->net);
    else
      res= -1;
    break;
  case SQLCOM_ROLLBACK:
    thd->options&= ~(ulong) (OPTION_BEGIN);
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (!ha_rollback(thd))
      send_ok(&thd->net);
    else
      res= -1;
    break;
  default:					/* Impossible */
    send_ok(&thd->net);
    break;
  }
  thd->proc_info="query end";			// QQ
  if (res < 0)
    send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN : 0);

error:
  DBUG_VOID_RETURN;
}


/****************************************************************************
** Get the user (global) and database privileges for all used tables
** Returns true (error) if we can't get the privileges and we don't use
** table/column grants.
** The idea of EXTRA_ACL is that one will be granted access to the table if
** one has the asked privilege on any column combination of the table; For
** example to be able to check a table one needs to have SELECT privilege on
** any column of the table.
****************************************************************************/

bool
check_access(THD *thd,uint want_access,const char *db, uint *save_priv,
	     bool no_grant)
{
  uint db_access,dummy;
  if (save_priv)
    *save_priv=0;
  else
    save_priv= &dummy;

  if (!db && !thd->db && !no_grant)
  {
    send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: tested */
    return TRUE;				/* purecov: tested */
  }

  if ((thd->master_access & want_access) == want_access)
  {
    *save_priv=thd->master_access;
    return FALSE;
  }
  if ((want_access & ~thd->master_access) & ~(DB_ACLS | EXTRA_ACL) ||
      ! db && no_grant)
  {						// We can never grant this
    net_printf(&thd->net,ER_ACCESS_DENIED_ERROR,
	       thd->priv_user,
	       thd->host ? thd->host : (thd->ip ? thd->ip : "unknown"),
	       thd->password ? ER(ER_YES) : ER(ER_NO));/* purecov: tested */
    return TRUE;				/* purecov: tested */
  }

  if (db == any_db)
    return FALSE;				// Allow select on anything
  if (db && (!thd->db || strcmp(db,thd->db)))
    db_access=acl_get(thd->host, thd->ip, (char*) &thd->remote.sin_addr,
		      thd->priv_user, db); /* purecov: inspected */
  else
    db_access=thd->db_access;
  want_access &= ~EXTRA_ACL;			// Remove SHOW attribute
  db_access= ((*save_priv=(db_access | thd->master_access)) & want_access);
  if (db_access == want_access ||
      ((grant_option && !no_grant) && !(want_access & ~TABLE_ACLS)))
    return FALSE;				/* Ok */
  net_printf(&thd->net,ER_DBACCESS_DENIED_ERROR,
	     thd->priv_user,
	     thd->host ? thd->host : (thd->ip ? thd->ip : "unknown"),
	     db ? db : thd->db ? thd->db : "unknown"); /* purecov: tested */
  return TRUE;					/* purecov: tested */
}


/*
** Check the privilege for all used tables.  Table privileges are cached
** in the table list for GRANT checking
*/

static bool
check_table_access(THD *thd,uint want_access,TABLE_LIST *tables)
{
  uint found=0,found_access=0;
  TABLE_LIST *org_tables=tables;
  for (; tables ; tables=tables->next)
  {
    if ((thd->master_access & want_access) == (want_access & ~EXTRA_ACL) &&
	thd->db)
      tables->grant.privilege= want_access;
    else if (tables->db && tables->db == thd->db)
    {
      if (found && !grant_option)		// db already checked
	tables->grant.privilege=found_access;
      else
      {
	if (check_access(thd,want_access,tables->db,&tables->grant.privilege))
	  return TRUE;				// Access denied
	found_access=tables->grant.privilege;
	found=1;
      }
    }
    else if (check_access(thd,want_access,tables->db,&tables->grant.privilege))
      return TRUE;				// Access denied
  }
  if (grant_option)
  {
    want_access &= ~EXTRA_ACL;			// Remove SHOW attribute
    return check_grant(thd,want_access,org_tables);
  }
  return FALSE;
}


static bool check_db_used(THD *thd,TABLE_LIST *tables)
{
  for (; tables ; tables=tables->next)
  {
    if (!tables->db)
    {
      if (!(tables->db=thd->db))
      {
	send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: tested */
	return TRUE;				/* purecov: tested */
      }
    }
  }
  return FALSE;
}


static bool check_merge_table_access(THD *thd, char *db, TABLE_LIST *table_list)
{
  int error=0;
  if (table_list)
  {
    /* Force all tables to use the current database */
    TABLE_LIST *tmp;
    for (tmp=table_list; tmp ; tmp=tmp->next)
      tmp->db=db;
    error=check_table_access(thd, SELECT_ACL | UPDATE_ACL | DELETE_ACL,
			     table_list);
  }
  return error;
}


/****************************************************************************
	Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/

#if STACK_DIRECTION < 0
#define used_stack(A,B) (long) (A - B)
#else
#define used_stack(A,B) (long) (B - A)
#endif

bool check_stack_overrun(THD *thd,char *buf __attribute__((unused)))
{
  long stack_used;
  if ((stack_used=used_stack(thd->thread_stack,(char*) &stack_used)) >=
      (long) thread_stack_min)
  {
    sprintf(errbuff[0],ER(ER_STACK_OVERRUN),stack_used,thread_stack);
    my_message(ER_STACK_OVERRUN,errbuff[0],MYF(0));
    thd->fatal_error=1;
    return 1;
  }
  return 0;
}

#define MY_YACC_INIT 1000			// Start with big alloc
#define MY_YACC_MAX  32000			// Because of 'short'

bool my_yyoverflow(short **yyss, YYSTYPE **yyvs, int *yystacksize)
{
  LEX	*lex=current_lex;
  int  old_info=0;
  if ((uint) *yystacksize >= MY_YACC_MAX)
    return 1;
  if (!lex->yacc_yyvs)
    old_info= *yystacksize;
  *yystacksize= set_zone((*yystacksize)*2,MY_YACC_INIT,MY_YACC_MAX);
  if (!(lex->yacc_yyvs= (char*)
	my_realloc((gptr) lex->yacc_yyvs,
		   *yystacksize*sizeof(**yyvs),
		   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))) ||
      !(lex->yacc_yyss= (char*)
	my_realloc((gptr) lex->yacc_yyss,
		   *yystacksize*sizeof(**yyss),
		   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))))
    return 1;
  if (old_info)
  {						// Copy old info from stack
    memcpy(lex->yacc_yyss, (gptr) *yyss, old_info*sizeof(**yyss));
    memcpy(lex->yacc_yyvs, (gptr) *yyvs, old_info*sizeof(**yyvs));
  }
  *yyss=(short*) lex->yacc_yyss;
  *yyvs=(YYSTYPE*) lex->yacc_yyvs;
  return 0;
}


/****************************************************************************
	Initialize global thd variables neaded for query
****************************************************************************/

static void
mysql_init_query(THD *thd)
{
  DBUG_ENTER("mysql_init_query");
  thd->lex.item_list.empty();
  thd->lex.value_list.empty();
  thd->lex.table_list.elements=0;
  thd->free_list=0;

  thd->lex.table_list.first=0;
  thd->lex.table_list.next= (byte**) &thd->lex.table_list.first;
  thd->fatal_error=0;				// Safety
  thd->last_insert_id_used=thd->query_start_used=thd->insert_id_used=0;
  thd->sent_row_count=0;
  DBUG_VOID_RETURN;
}

void
mysql_init_select(LEX *lex)
{
  lex->where=lex->having=0;
  lex->select_limit=current_thd->default_select_limit;
  lex->offset_limit=0L;
  lex->options=0;
  lex->exchange = 0;
  lex->proc_list.first=0;
  lex->order_list.elements=lex->group_list.elements=0;
  lex->order_list.first=0;
  lex->order_list.next= (byte**) &lex->order_list.first;
  lex->group_list.first=0;
  lex->group_list.next= (byte**) &lex->group_list.first;
}


void
mysql_parse(THD *thd,char *inBuf,uint length)
{
  DBUG_ENTER("mysql_parse");

  mysql_init_query(thd);
  thd->query_length = length;
  LEX *lex=lex_start(thd, (uchar*) inBuf, length);
  if (!yyparse() && ! thd->fatal_error)
    mysql_execute_command();
  thd->proc_info="freeing items";
  free_items(thd);  /* Free strings used by items */
  lex_end(lex);
  DBUG_VOID_RETURN;
}


inline static void
link_in_list(SQL_LIST *list,byte *element,byte **next)
{
  list->elements++;
  (*list->next)=element;
  list->next=next;
  *next=0;
}


/*****************************************************************************
** Store field definition for create
** Return 0 if ok
******************************************************************************/

bool add_field_to_list(char *field_name, enum_field_types type,
		       char *length, char *decimals,
		       uint type_modifier, Item *default_value,char *change,
		       TYPELIB *interval)
{
  register create_field *new_field;
  THD	*thd=current_thd;
  LEX  *lex= &thd->lex;
  uint allowed_type_modifier=0;
  DBUG_ENTER("add_field_to_list");

  if (strlen(field_name) > NAME_LEN)
  {
    net_printf(&thd->net, ER_TOO_LONG_IDENT, field_name); /* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if (type_modifier & PRI_KEY_FLAG)
  {
    lex->col_list.push_back(new key_part_spec(field_name,0));
    lex->key_list.push_back(new Key(Key::PRIMARY,NullS,
				    lex->col_list));
    lex->col_list.empty();
  }
  if (type_modifier & (UNIQUE_FLAG | UNIQUE_KEY_FLAG))
  {
    lex->col_list.push_back(new key_part_spec(field_name,0));
    lex->key_list.push_back(new Key(Key::UNIQUE,NullS,
				    lex->col_list));
    lex->col_list.empty();
  }

  if (default_value && default_value->type() == Item::NULL_ITEM)
  {
    if ((type_modifier & (NOT_NULL_FLAG | AUTO_INCREMENT_FLAG)) ==
	NOT_NULL_FLAG)
    {
      net_printf(&thd->net,ER_INVALID_DEFAULT,field_name);
      DBUG_RETURN(1);
    }
    default_value=0;
  }
  if (!(new_field=new create_field()))
    DBUG_RETURN(1);
  new_field->field=0;
  new_field->field_name=field_name;
  new_field->def= (type_modifier & AUTO_INCREMENT_FLAG ? 0 : default_value);
  new_field->flags= type_modifier;
  new_field->unireg_check= (type_modifier & AUTO_INCREMENT_FLAG ?
			    Field::NEXT_NUMBER : Field::NONE);
  new_field->decimals= decimals ? (uint) set_zone(atoi(decimals),0,
						  NOT_FIXED_DEC-1) : 0;
  new_field->sql_type=type;
  new_field->length=0;
  new_field->change=change;
  new_field->interval=0;
  new_field->pack_length=0;
  if (length)
    if (!(new_field->length= (uint) atoi(length)))
      length=0; /* purecov: inspected */
  uint sign_len=type_modifier & UNSIGNED_FLAG ? 0 : 1;

  if (new_field->length && new_field->decimals &&
      new_field->length < new_field->decimals+2 &&
      new_field->decimals != NOT_FIXED_DEC)
    new_field->length=new_field->decimals+2; /* purecov: inspected */

  switch (type) {
  case FIELD_TYPE_TINY:
    if (!length) new_field->length=3+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_SHORT:
    if (!length) new_field->length=5+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_INT24:
    if (!length) new_field->length=8+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_LONG:
    if (!length) new_field->length=10+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_LONGLONG:
    if (!length) new_field->length=20;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_STRING:
  case FIELD_TYPE_VAR_STRING:
  case FIELD_TYPE_NULL:
    break;
  case FIELD_TYPE_DECIMAL:
    if (!length)
      new_field->length = 10;			// Default length for DECIMAL
    new_field->length+=sign_len;
    if (new_field->decimals)
      new_field->length++;
    break;
  case FIELD_TYPE_BLOB:
  case FIELD_TYPE_TINY_BLOB:
  case FIELD_TYPE_LONG_BLOB:
  case FIELD_TYPE_MEDIUM_BLOB:
    if (default_value)				// Allow empty as default value
    {
      String str,*res;
      res=default_value->val_str(&str);
      if (res->length())
      {
	net_printf(&thd->net,ER_BLOB_CANT_HAVE_DEFAULT,field_name); /* purecov: inspected */
	DBUG_RETURN(1); /* purecov: inspected */
      }
      new_field->def=0;
    }
    new_field->flags|=BLOB_FLAG;
    break;
  case FIELD_TYPE_YEAR:
    if (!length || new_field->length != 2)
      new_field->length=4;			// Default length
    new_field->flags|= ZEROFILL_FLAG | UNSIGNED_FLAG;
    break;
  case FIELD_TYPE_FLOAT:
    /* change FLOAT(precision) to FLOAT or DOUBLE */
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    if (length && !decimals)
    {
      uint tmp_length=new_field->length;
      if (tmp_length > PRECISION_FOR_DOUBLE)
      {
	net_printf(&thd->net,ER_WRONG_FIELD_SPEC,field_name);
	DBUG_RETURN(1);
      }
      else if (tmp_length > PRECISION_FOR_FLOAT)
      {
	new_field->sql_type=FIELD_TYPE_DOUBLE;
	new_field->length=DBL_DIG+7;			// -[digits].E+###
      }
      else
	new_field->length=FLT_DIG+6;			// -[digits].E+##
      new_field->decimals= NOT_FIXED_DEC;
      break;
    }
    if (!length)
    {
      new_field->length =  FLT_DIG+6;
      new_field->decimals= NOT_FIXED_DEC;
    }
    break;
  case FIELD_TYPE_DOUBLE:
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    if (!length)
    {
      new_field->length = DBL_DIG+7;
      new_field->decimals=NOT_FIXED_DEC;
    }
    break;
  case FIELD_TYPE_TIMESTAMP:
    if (!length)
      new_field->length= 14;			// Full date YYYYMMDDHHMMSS
    else
    {
      new_field->length=((new_field->length+1)/2)*2; /* purecov: inspected */
      new_field->length= min(new_field->length,14); /* purecov: inspected */
    }
    new_field->flags|= ZEROFILL_FLAG | UNSIGNED_FLAG | NOT_NULL_FLAG;
    break;
  case FIELD_TYPE_DATE:				// Old date type
    if (protocol_version != PROTOCOL_VERSION-1)
      new_field->sql_type=FIELD_TYPE_NEWDATE;
    /* fall trough */
  case FIELD_TYPE_NEWDATE:
    new_field->length=10;
    break;
  case FIELD_TYPE_TIME:
    new_field->length=10;
    break;
  case FIELD_TYPE_DATETIME:
    new_field->length=19;
    break;
  case FIELD_TYPE_SET:
    {
      if (interval->count > sizeof(longlong)*8)
      {
	net_printf(&thd->net,ER_TOO_BIG_SET,field_name); /* purecov: inspected */
	DBUG_RETURN(1);				/* purecov: inspected */
      }
      new_field->pack_length=(interval->count+7)/8;
      if (new_field->pack_length > 4)
	new_field->pack_length=8;
      new_field->interval=interval;
      new_field->length=0;
      for (const char **pos=interval->type_names; *pos ; pos++)
	new_field->length+=(uint) strlen(*pos)+1;
      new_field->length--;
      set_if_smaller(new_field->length,MAX_FIELD_WIDTH-1);
      if (default_value)
      {
	thd->cuted_fields=0;
	String str,*res;
	res=default_value->val_str(&str);
	(void) find_set(interval,res->ptr(),res->length());
	if (thd->cuted_fields)
	{
	  net_printf(&thd->net,ER_INVALID_DEFAULT,field_name);
	  DBUG_RETURN(1);
	}
      }
    }
    break;
  case FIELD_TYPE_ENUM:
    {
      new_field->interval=interval;
      new_field->pack_length=interval->count < 256 ? 1 : 2; // Should be safe
      new_field->length=(uint) strlen(interval->type_names[0]);
      for (const char **pos=interval->type_names+1; *pos ; pos++)
      {
	uint length=(uint) strlen(*pos);
	set_if_bigger(new_field->length,length);
      }
      set_if_smaller(new_field->length,MAX_FIELD_WIDTH-1);
      if (default_value)
      {
	String str,*res;
	res=default_value->val_str(&str);
	if (!find_enum(interval,res->ptr(),res->length()))
	{
	  net_printf(&thd->net,ER_INVALID_DEFAULT,field_name);
	  DBUG_RETURN(1);
	}
      }
      break;
    }
  }

  if (new_field->length >= MAX_FIELD_WIDTH ||
      (!new_field->length && !(new_field->flags & BLOB_FLAG) &&
       type != FIELD_TYPE_STRING))
  {
    net_printf(&thd->net,ER_TOO_BIG_FIELDLENGTH,field_name,
	       MAX_FIELD_WIDTH-1);		/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  type_modifier&= AUTO_INCREMENT_FLAG;
  if ((~allowed_type_modifier) & type_modifier)
  {
    net_printf(&thd->net,ER_WRONG_FIELD_SPEC,field_name);
    DBUG_RETURN(1);
  }
  if (!new_field->pack_length)
    new_field->pack_length=calc_pack_length(new_field->sql_type ==
					    FIELD_TYPE_VAR_STRING ?
					    FIELD_TYPE_STRING :
					    new_field->sql_type,
					    new_field->length);
  lex->create_list.push_back(new_field);
  lex->last_field=new_field;
  DBUG_RETURN(0);
}

/* Store position for column in ALTER TABLE .. ADD column */

void store_position_for_column(const char *name)
{
  current_lex->last_field->after=my_const_cast(char*) (name);
}

bool
add_proc_to_list(Item *item)
{
  ORDER *order;
  Item	**item_ptr;

  if (!(order = (ORDER *) sql_alloc(sizeof(ORDER)+sizeof(Item*))))
    return 1;
  item_ptr = (Item**) (order+1);
  *item_ptr= item;
  order->item=item_ptr;
  order->free_me=0;
  link_in_list(&current_lex->proc_list,(byte*) order,(byte**) &order->next);
  return 0;
}


/* Fix escaping of _, % and \ in database and table names (for ODBC) */

static void remove_escape(char *name)
{
  char *to;
#ifdef USE_MB
  char *strend=name+(uint) strlen(name);
#endif
  for (to=name; *name ; name++)
  {
#ifdef USE_MB
    int l;
/*    if ((l = ismbchar(name, name+MBMAXLEN))) { Wei He: I think it's wrong */
    if (use_mb(default_charset_info) &&
        (l = my_ismbchar(default_charset_info, name, strend)))
    {
	while (l--)
	    *to++ = *name++;
	name--;
	continue;
    }
#endif
    if (*name == '\\' && name[1])
      name++;					// Skipp '\\'
    *to++= *name;
  }
  *to=0;
}

/****************************************************************************
** save order by and tables in own lists
****************************************************************************/


bool add_to_list(SQL_LIST &list,Item *item,bool asc)
{
  ORDER *order;
  Item	**item_ptr;
  DBUG_ENTER("add_to_list");
  if (!(order = (ORDER *) sql_alloc(sizeof(ORDER)+sizeof(Item*))))
    DBUG_RETURN(1);
  item_ptr = (Item**) (order+1);
  *item_ptr=item;
  order->item= item_ptr;
  order->asc = asc;
  order->free_me=0;
  order->used=0;
  link_in_list(&list,(byte*) order,(byte**) &order->next);
  DBUG_RETURN(0);
}


TABLE_LIST *add_table_to_list(Table_ident *table, LEX_STRING *alias,
			      bool updating,
			      thr_lock_type flags,
			      List<String> *use_index,
			      List<String> *ignore_index
			      )
{
  register TABLE_LIST *ptr;
  THD	*thd=current_thd;
  char *alias_str;
  const char *current_db;
  DBUG_ENTER("add_table_to_list");

  if (!table)
    DBUG_RETURN(0);				// End of memory
  alias_str= alias ? alias->str : table->table.str;
  if (table->table.length > NAME_LEN ||
      table->db.str && table->db.length > NAME_LEN ||
      check_table_name(table->table.str,table->table.length))
  {
    net_printf(&thd->net,ER_WRONG_TABLE_NAME,table->table.str);
    DBUG_RETURN(0);
  }

#ifdef FN_LOWER_CASE
  if (!alias)					/* Alias is case sensitive */
    if (!(alias_str=sql_strmake(alias_str,table->table.length)))
      DBUG_RETURN(0);
  if (lower_case_table_names)
    casedn_str(table->table.str);
#endif
  if (!(ptr = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(0);				/* purecov: inspected */
  ptr->db= table->db.str;
  ptr->real_name=table->table.str;
  ptr->name=alias_str;
  ptr->lock_type=flags;
  ptr->updating=updating;
  if (use_index)
    ptr->use_index=(List<String> *) thd->memdup((gptr) use_index,
					       sizeof(*use_index));
  if (ignore_index)
    ptr->ignore_index=(List<String> *) thd->memdup((gptr) ignore_index,
						   sizeof(*ignore_index));

  /* check that used name is unique */
  current_db=thd->db ? thd->db : "";

  if (flags != TL_IGNORE)
  {
    for (TABLE_LIST *tables=(TABLE_LIST*) thd->lex.table_list.first ; tables ;
	 tables=tables->next)
    {
      if (!strcmp(alias_str,tables->name) &&
	  !strcmp(ptr->db ? ptr->db : current_db,
		  tables->db ? tables->db : current_db))
      {
	net_printf(&thd->net,ER_NONUNIQ_TABLE,alias_str); /* purecov: tested */
	DBUG_RETURN(0);				/* purecov: tested */
      }
    }
  }
  link_in_list(&thd->lex.table_list,(byte*) ptr,(byte**) &ptr->next);
  DBUG_RETURN(ptr);
}

void add_join_on(TABLE_LIST *b,Item *expr)
{
  if (!b->on_expr)
    b->on_expr=expr;
  else
  {
    // This only happens if you have both a right and left join
    b->on_expr=new Item_cond_and(b->on_expr,expr);
  }
}


void add_join_natural(TABLE_LIST *a,TABLE_LIST *b)
{
  b->natural_join=a;
}

	/* Check if name is used in table list */

static bool check_dup(THD *thd,const char *db,const char *name,
		      TABLE_LIST *tables)
{
  const char *thd_db=thd->db ? thd->db : any_db;
  for (; tables ; tables=tables->next)
    if (!strcmp(name,tables->real_name) &&
	!strcmp(db ? db : thd_db, tables->db ? tables->db : thd_db))
      return 1;
  return 0;
}

bool reload_acl_and_cache(THD *thd, uint options, TABLE_LIST *tables)
{
  bool result=0;

  select_errors=0;				/* Write if more errors */
  // mysql_log.flush();				// Flush log
  if (options & REFRESH_GRANT)
  {
    acl_reload();
    grant_reload();
  }
  if (options & REFRESH_LOG)
  {
    mysql_log.new_file();
    mysql_update_log.new_file();
    mysql_bin_log.new_file();
    mysql_slow_log.new_file();
    if (ha_flush_logs())
      result=1;
  }
  if (options & (REFRESH_TABLES | REFRESH_READ_LOCK))
  {
    if ((options & REFRESH_READ_LOCK) && thd && ! thd->global_read_lock)
    {
      thd->global_read_lock=1;
      thread_safe_increment(global_read_lock,&LOCK_open);
    }
    result=close_cached_tables(thd,(options & REFRESH_FAST) ? 0 : 1, tables);
  }
  if (options & REFRESH_HOSTS)
    hostname_cache_refresh();
  if (options & REFRESH_STATUS)
    refresh_status();
  if (options & REFRESH_THREADS)
    flush_thread_cache();
  if (options & REFRESH_MASTER)
    reset_master();
  if (options & REFRESH_SLAVE)
    reset_slave();

  return result;
}


void kill_one_thread(THD *thd, ulong id)
{
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  I_List_iterator<THD> it(threads);
  THD *tmp;
  uint error=ER_NO_SUCH_THREAD;
  while ((tmp=it++))
  {
    if (tmp->thread_id == id)
    {
      if ((thd->master_access & PROCESS_ACL) ||
	  !strcmp(thd->user,tmp->user))
      {
	thr_alarm_kill(tmp->real_id);
	tmp->killed=1;
	error=0;
	if (tmp->mysys_var)
	{
	  pthread_mutex_lock(&tmp->mysys_var->mutex);
	  if (!tmp->system_thread)		// Don't abort locks
	    tmp->mysys_var->abort=1;
	  if (tmp->mysys_var->current_mutex)
	  {
	    pthread_mutex_lock(tmp->mysys_var->current_mutex);
	    pthread_cond_broadcast(tmp->mysys_var->current_cond);
	    pthread_mutex_unlock(tmp->mysys_var->current_mutex);
	  }
	  pthread_mutex_unlock(&tmp->mysys_var->mutex);
	}
      }
      else
	error=ER_KILL_DENIED_ERROR;
      break;					// Found thread
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  if (!error)
    send_ok(&thd->net);
  else
    net_printf(&thd->net,error,id);
}

/* Clear most status variables */

static void refresh_status(void)
{
  pthread_mutex_lock(&THR_LOCK_keycache);
  pthread_mutex_lock(&LOCK_status);
  for (struct show_var_st *ptr=status_vars; ptr->name; ptr++)
  {
    if (ptr->type == SHOW_LONG)
      *(ulong*) ptr->value=0;
  }
  pthread_mutex_unlock(&LOCK_status);
  pthread_mutex_unlock(&THR_LOCK_keycache);
}

