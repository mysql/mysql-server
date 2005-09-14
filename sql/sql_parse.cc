/* Copyright (C) 2000-2003 MySQL AB

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
#include "sql_repl.h"
#include "repl_failsafe.h"
#include <m_ctype.h>
#include <myisam.h>
#include <my_dir.h>

#ifdef HAVE_INNOBASE_DB
#include "ha_innodb.h"
#endif

#ifdef HAVE_OPENSSL
/*
  Without SSL the handshake consists of one packet. This packet
  has both client capabilites and scrambled password.
  With SSL the handshake might consist of two packets. If the first
  packet (client capabilities) has CLIENT_SSL flag set, we have to
  switch to SSL and read the second packet. The scrambled password
  is in the second packet and client_capabilites field will be ignored.
  Maybe it is better to accept flags other than CLIENT_SSL from the
  second packet?
*/
#define SSL_HANDSHAKE_SIZE      2
#define NORMAL_HANDSHAKE_SIZE   6
#define MIN_HANDSHAKE_SIZE      2
#else
#define MIN_HANDSHAKE_SIZE      6
#endif /* HAVE_OPENSSL */

#ifdef SOLARIS
extern "C" int gethostname(char *name, int namelen);
#endif

static void time_out_user_resource_limits(THD *thd, USER_CONN *uc);
#ifndef NO_EMBEDDED_ACCESS_CHECKS
static int check_for_max_user_connections(THD *thd, USER_CONN *uc);
#endif
static void decrease_user_connections(USER_CONN *uc);
static bool check_db_used(THD *thd,TABLE_LIST *tables);
static bool check_multi_update_lock(THD *thd, TABLE_LIST *tables, 
				    List<Item> *fields, SELECT_LEX *select_lex);
static void remove_escape(char *name);
static void refresh_status(void);
static bool append_file_to_dir(THD *thd, const char **filename_ptr,
			       const char *table_name);
             
static TABLE_LIST* get_table_by_alias(TABLE_LIST* tl, const char* db,
  const char* alias);      

const char *any_db="*any*";	// Special symbol for check_access

const char *command_name[]={
  "Sleep", "Quit", "Init DB", "Query", "Field List", "Create DB",
  "Drop DB", "Refresh", "Shutdown", "Statistics", "Processlist",
  "Connect","Kill","Debug","Ping","Time","Delayed insert","Change user",
  "Binlog Dump","Table Dump",  "Connect Out", "Register Slave",
  "Prepare", "Execute", "Long Data", "Close stmt",
  "Reset stmt", "Set option",
  "Error"					// Last command number
};

static char empty_c_string[1]= {0};		// Used for not defined 'db'

#ifdef __WIN__
static void  test_signal(int sig_ptr)
{
#if !defined( DBUG_OFF)
  MessageBox(NULL,"Test signal","DBUG",MB_OK);
#endif
#if defined(OS2)
  fprintf(stderr, "Test signal %d\n", sig_ptr);
  fflush(stderr);
#endif
}
static void init_signals(void)
{
  int signals[7] = {SIGINT,SIGILL,SIGFPE,SIGSEGV,SIGTERM,SIGBREAK,SIGABRT } ;
  for (int i=0 ; i < 7 ; i++)
    signal( signals[i], test_signal) ;
}
#endif

static void unlock_locked_tables(THD *thd)
{
  if (thd->locked_tables)
  {
    thd->lock=thd->locked_tables;
    thd->locked_tables=0;			// Will be automaticly closed
    close_thread_tables(thd);			// Free tables
  }
}


static bool end_active_trans(THD *thd)
{
  int error=0;
  if (thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN |
		      OPTION_TABLE_LOCK))
  {
    thd->options&= ~(ulong) (OPTION_BEGIN | OPTION_STATUS_NO_TRANS_UPDATE);
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (ha_commit(thd))
      error=1;
  }
  return error;
}


#ifdef HAVE_REPLICATION
/*
  Returns true if all tables should be ignored
*/
inline bool all_tables_not_ok(THD *thd, TABLE_LIST *tables)
{
  return (table_rules_on && tables && !tables_ok(thd,tables));
}
#endif


static HASH hash_user_connections;

static int get_or_create_user_conn(THD *thd, const char *user,
				   const char *host,
				   USER_RESOURCES *mqh)
{
  int return_val= 0;
  uint temp_len, user_len;
  char temp_user[USERNAME_LENGTH+HOSTNAME_LENGTH+2];
  struct  user_conn *uc;

  DBUG_ASSERT(user != 0);
  DBUG_ASSERT(host != 0);

  user_len= strlen(user);
  temp_len= (strmov(strmov(temp_user, user)+1, host) - temp_user)+1;
  (void) pthread_mutex_lock(&LOCK_user_conn);
  if (!(uc = (struct  user_conn *) hash_search(&hash_user_connections,
					       (byte*) temp_user, temp_len)))
  {
    /* First connection for user; Create a user connection object */
    if (!(uc= ((struct user_conn*)
	       my_malloc(sizeof(struct user_conn) + temp_len+1,
			 MYF(MY_WME)))))
    {
      send_error(thd, 0, NullS);		// Out of memory
      return_val= 1;
      goto end;
    }
    uc->user=(char*) (uc+1);
    memcpy(uc->user,temp_user,temp_len+1);
    uc->user_len= user_len;
    uc->host= uc->user + uc->user_len +  1;
    uc->len= temp_len;
    uc->connections= 0;
    uc->questions= uc->updates= uc->conn_per_hour=0;
    uc->user_resources= *mqh;
    uc->intime= thd->thr_create_time;
    if (my_hash_insert(&hash_user_connections, (byte*) uc))
    {
      my_free((char*) uc,0);
      send_error(thd, 0, NullS);		// Out of memory
      return_val= 1;
      goto end;
    }
  }
  thd->user_connect=uc;
  uc->connections++;
end:
  (void) pthread_mutex_unlock(&LOCK_user_conn);
  return return_val;

}


/*
    Check if user exist and password supplied is correct. 
  SYNOPSIS
    check_user()
    thd          thread handle, thd->{host,user,ip} are used
    command      originator of the check: now check_user is called
                 during connect and change user procedures; used for 
                 logging.
    passwd       scrambled password recieved from client
    passwd_len   length of scrambled password
    db           database name to connect to, may be NULL
    check_count  dont know exactly

    Note, that host, user and passwd may point to communication buffer.
    Current implementation does not depened on that, but future changes
    should be done with this in mind; 'thd' is INOUT, all other params
    are 'IN'.

  RETURN VALUE
    0  OK; thd->user, thd->master_access, thd->priv_user, thd->db and
       thd->db_access are updated; OK is sent to client;
   -1  access denied or handshake error; error is sent to client;
   >0  error, not sent to client
*/

int check_user(THD *thd, enum enum_server_command command, 
	       const char *passwd, uint passwd_len, const char *db,
	       bool check_count)
{
  DBUG_ENTER("check_user");
  
#ifdef NO_EMBEDDED_ACCESS_CHECKS
  thd->master_access= GLOBAL_ACLS;			// Full rights
  /* Change database if necessary: OK or FAIL is sent in mysql_change_db */
  if (db && db[0])
  {
    thd->db= 0;
    thd->db_length= 0;
    if (mysql_change_db(thd, db))
    {
      if (thd->user_connect)
	decrease_user_connections(thd->user_connect);
      DBUG_RETURN(-1);
    }
  }
  else
    send_ok(thd);
  DBUG_RETURN(0);
#else

  my_bool opt_secure_auth_local;
  pthread_mutex_lock(&LOCK_global_system_variables);
  opt_secure_auth_local= opt_secure_auth;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  
  /*
    If the server is running in secure auth mode, short scrambles are 
    forbidden.
  */
  if (opt_secure_auth_local && passwd_len == SCRAMBLE_LENGTH_323)
  {
    net_printf(thd, ER_NOT_SUPPORTED_AUTH_MODE);
    mysql_log.write(thd, COM_CONNECT, ER(ER_NOT_SUPPORTED_AUTH_MODE));
    DBUG_RETURN(-1);
  }
  if (passwd_len != 0 &&
      passwd_len != SCRAMBLE_LENGTH &&
      passwd_len != SCRAMBLE_LENGTH_323)
    DBUG_RETURN(ER_HANDSHAKE_ERROR);

  /*
    Clear thd->db as it points to something, that will be freed when 
    connection is closed. We don't want to accidently free a wrong pointer
    if connect failed. Also in case of 'CHANGE USER' failure, current
    database will be switched to 'no database selected'.
  */
  thd->db= 0;
  thd->db_length= 0;
  
  USER_RESOURCES ur;
  int res= acl_getroot(thd, &ur, passwd, passwd_len);
#ifndef EMBEDDED_LIBRARY
  if (res == -1)
  {
    /*
      This happens when client (new) sends password scrambled with
      scramble(), but database holds old value (scrambled with
      scramble_323()). Here we please client to send scrambled_password
      in old format.
    */
    NET *net= &thd->net;
    if (opt_secure_auth_local)
    {
      net_printf(thd, ER_SERVER_IS_IN_SECURE_AUTH_MODE,
                 thd->user, thd->host_or_ip);
      mysql_log.write(thd, COM_CONNECT, ER(ER_SERVER_IS_IN_SECURE_AUTH_MODE),
                      thd->user, thd->host_or_ip);
      DBUG_RETURN(-1);
    }
    if (send_old_password_request(thd) ||
        my_net_read(net) != SCRAMBLE_LENGTH_323 + 1) // We have to read very
    {                                                // specific packet size
      inc_host_errors(&thd->remote.sin_addr);
      DBUG_RETURN(ER_HANDSHAKE_ERROR);
    }
    /* Final attempt to check the user based on reply */
    /* So as passwd is short, errcode is always >= 0 */
    res= acl_getroot(thd, &ur, (char *) net->read_pos, SCRAMBLE_LENGTH_323);
  }
#endif /*EMBEDDED_LIBRARY*/
  /* here res is always >= 0 */
  if (res == 0)
  {
    if (!(thd->master_access & NO_ACCESS)) // authentification is OK 
    {
      DBUG_PRINT("info",
                 ("Capabilities: %d  packet_length: %ld  Host: '%s'  "
                  "Login user: '%s' Priv_user: '%s'  Using password: %s "
                  "Access: %u  db: '%s'",
                  thd->client_capabilities, thd->max_client_packet_length,
                  thd->host_or_ip, thd->user, thd->priv_user,
                  passwd_len ? "yes": "no",
                  thd->master_access, thd->db ? thd->db : "*none*"));

      if (check_count)
      {
        VOID(pthread_mutex_lock(&LOCK_thread_count));
        bool count_ok= thread_count < max_connections + delayed_insert_threads
                       || (thd->master_access & SUPER_ACL);
        VOID(pthread_mutex_unlock(&LOCK_thread_count));
        if (!count_ok)
        {                                         // too many connections 
          send_error(thd, ER_CON_COUNT_ERROR);
          DBUG_RETURN(-1);
        }
      }

      /* Why logging is performed before all checks've passed? */
      mysql_log.write(thd,command,
                      (thd->priv_user == thd->user ?
                       (char*) "%s@%s on %s" :
                       (char*) "%s@%s as anonymous on %s"),
                      thd->user, thd->host_or_ip,
                      db ? db : (char*) "");

      /*
        This is the default access rights for the current database.  It's
        set to 0 here because we don't have an active database yet (and we
        may not have an active database to set.
      */
      thd->db_access=0;

      /* Don't allow user to connect if he has done too many queries */
      if ((ur.questions || ur.updates || ur.connections ||
	   max_user_connections) &&
	  get_or_create_user_conn(thd,thd->user,thd->host_or_ip,&ur))
	DBUG_RETURN(-1);
      if (thd->user_connect &&
	  (thd->user_connect->user_resources.connections ||
	   max_user_connections) &&
	  check_for_max_user_connections(thd, thd->user_connect))
	DBUG_RETURN(-1);

      /* Change database if necessary: OK or FAIL is sent in mysql_change_db */
      if (db && db[0])
      {
        if (mysql_change_db(thd, db))
        {
          if (thd->user_connect)
            decrease_user_connections(thd->user_connect);
          DBUG_RETURN(-1);
        }
      }
      else
	send_ok(thd);
      thd->password= test(passwd_len);          // remember for error messages 
      /* Ready to handle queries */
      DBUG_RETURN(0);
    }
  }
  else if (res == 2) // client gave short hash, server has long hash
  {
    net_printf(thd, ER_NOT_SUPPORTED_AUTH_MODE);
    mysql_log.write(thd,COM_CONNECT,ER(ER_NOT_SUPPORTED_AUTH_MODE));
    DBUG_RETURN(-1);
  }
  net_printf(thd, ER_ACCESS_DENIED_ERROR,
             thd->user,
             thd->host_or_ip,
             passwd_len ? ER(ER_YES) : ER(ER_NO));
  mysql_log.write(thd, COM_CONNECT, ER(ER_ACCESS_DENIED_ERROR),
                  thd->user,
                  thd->host_or_ip,
                  passwd_len ? ER(ER_YES) : ER(ER_NO));
  DBUG_RETURN(-1);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}

/*
  Check for maximum allowable user connections, if the mysqld server is
  started with corresponding variable that is greater then 0.
*/

extern "C" byte *get_key_conn(user_conn *buff, uint *length,
			      my_bool not_used __attribute__((unused)))
{
  *length=buff->len;
  return (byte*) buff->user;
}

extern "C" void free_user(struct user_conn *uc)
{
  my_free((char*) uc,MYF(0));
}

void init_max_user_conn(void)
{
  (void) hash_init(&hash_user_connections,system_charset_info,max_connections,
		   0,0,
		   (hash_get_key) get_key_conn, (hash_free_key) free_user,
		   0);
}


/*
  check if user has already too many connections
  
  SYNOPSIS
  check_for_max_user_connections()
  thd			Thread handle
  uc			User connect object

  NOTES
    If check fails, we decrease user connection count, which means one
    shouldn't call decrease_user_connections() after this function.

  RETURN
    0	ok
    1	error
*/

#ifndef NO_EMBEDDED_ACCESS_CHECKS

static int check_for_max_user_connections(THD *thd, USER_CONN *uc)
{
  int error=0;
  DBUG_ENTER("check_for_max_user_connections");

  (void) pthread_mutex_lock(&LOCK_user_conn);
  if (max_user_connections &&
      max_user_connections < (uint) uc->connections)
  {
    net_printf(thd,ER_TOO_MANY_USER_CONNECTIONS, uc->user);
    error=1;
    goto end;
  }
  time_out_user_resource_limits(thd, uc);
  if (uc->user_resources.connections &&
      uc->user_resources.connections <= uc->conn_per_hour)
  {
    net_printf(thd, ER_USER_LIMIT_REACHED, uc->user,
	       "max_connections_per_hour",
	       (long) uc->user_resources.connections);
    error=1;
    goto end;
  }
  uc->conn_per_hour++;

  end:
  if (error)
    uc->connections--; // no need for decrease_user_connections() here
  (void) pthread_mutex_unlock(&LOCK_user_conn);
  DBUG_RETURN(error);
}
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

/*
  Decrease user connection count

  SYNOPSIS
    decrease_user_connections()
    uc			User connection object

  NOTES
    If there is a n user connection object for a connection
    (which only happens if 'max_user_connections' is defined or
    if someone has created a resource grant for a user), then
    the connection count is always incremented on connect.

    The user connect object is not freed if some users has
    'max connections per hour' defined as we need to be able to hold
    count over the lifetime of the connection.
*/

static void decrease_user_connections(USER_CONN *uc)
{
  DBUG_ENTER("decrease_user_connections");
  (void) pthread_mutex_lock(&LOCK_user_conn);
  DBUG_ASSERT(uc->connections);
  if (!--uc->connections && !mqh_used)
  {
    /* Last connection for user; Delete it */
    (void) hash_delete(&hash_user_connections,(byte*) uc);
  }
  (void) pthread_mutex_unlock(&LOCK_user_conn);
  DBUG_VOID_RETURN;
}


void free_max_user_conn(void)
{
  hash_free(&hash_user_connections);
}


/*
  Mark all commands that somehow changes a table
  This is used to check number of updates / hour

  sql_command is actually set to SQLCOM_END sometimes
  so we need the +1 to include it in the array.
*/

char  uc_update_queries[SQLCOM_END+1];

void init_update_queries(void)
{
  bzero((gptr) &uc_update_queries, sizeof(uc_update_queries));

  uc_update_queries[SQLCOM_CREATE_TABLE]=1;
  uc_update_queries[SQLCOM_CREATE_INDEX]=1;
  uc_update_queries[SQLCOM_ALTER_TABLE]=1;
  uc_update_queries[SQLCOM_UPDATE]=1;
  uc_update_queries[SQLCOM_INSERT]=1;
  uc_update_queries[SQLCOM_INSERT_SELECT]=1;
  uc_update_queries[SQLCOM_DELETE]=1;
  uc_update_queries[SQLCOM_TRUNCATE]=1;
  uc_update_queries[SQLCOM_DROP_TABLE]=1;
  uc_update_queries[SQLCOM_LOAD]=1;
  uc_update_queries[SQLCOM_CREATE_DB]=1;
  uc_update_queries[SQLCOM_DROP_DB]=1;
  uc_update_queries[SQLCOM_REPLACE]=1;
  uc_update_queries[SQLCOM_REPLACE_SELECT]=1;
  uc_update_queries[SQLCOM_RENAME_TABLE]=1;
  uc_update_queries[SQLCOM_BACKUP_TABLE]=1;
  uc_update_queries[SQLCOM_RESTORE_TABLE]=1;
  uc_update_queries[SQLCOM_DELETE_MULTI]=1;
  uc_update_queries[SQLCOM_DROP_INDEX]=1;
  uc_update_queries[SQLCOM_UPDATE_MULTI]=1;
}

bool is_update_query(enum enum_sql_command command)
{
  DBUG_ASSERT(command >= 0 && command <= SQLCOM_END);
  return uc_update_queries[command];
}

/*
  Reset per-hour user resource limits when it has been more than
  an hour since they were last checked

  SYNOPSIS:
    time_out_user_resource_limits()
    thd			Thread handler
    uc			User connection details

  NOTE:
    This assumes that the LOCK_user_conn mutex has been acquired, so it is
    safe to test and modify members of the USER_CONN structure.
*/

static void time_out_user_resource_limits(THD *thd, USER_CONN *uc)
{
  time_t check_time = thd->start_time ?  thd->start_time : time(NULL);
  DBUG_ENTER("time_out_user_resource_limits");

  /* If more than a hour since last check, reset resource checking */
  if (check_time  - uc->intime >= 3600)
  {
    uc->questions=1;
    uc->updates=0;
    uc->conn_per_hour=0;
    uc->intime=check_time;
  }

  DBUG_VOID_RETURN;
}


/*
  Check if maximum queries per hour limit has been reached
  returns 0 if OK.
*/

static bool check_mqh(THD *thd, uint check_command)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  bool error= 0;
  USER_CONN *uc=thd->user_connect;
  DBUG_ENTER("check_mqh");
  DBUG_ASSERT(uc != 0);

  (void) pthread_mutex_lock(&LOCK_user_conn);

  time_out_user_resource_limits(thd, uc);

  /* Check that we have not done too many questions / hour */
  if (uc->user_resources.questions &&
      uc->questions++ >= uc->user_resources.questions)
  {
    net_printf(thd, ER_USER_LIMIT_REACHED, uc->user, "max_questions",
	       (long) uc->user_resources.questions);
    error=1;
    goto end;
  }
  if (check_command < (uint) SQLCOM_END)
  {
    /* Check that we have not done too many updates / hour */
    if (uc->user_resources.updates && uc_update_queries[check_command] &&
	uc->updates++ >= uc->user_resources.updates)
    {
      net_printf(thd, ER_USER_LIMIT_REACHED, uc->user, "max_updates",
		 (long) uc->user_resources.updates);
      error=1;
      goto end;
    }
  }
end:
  (void) pthread_mutex_unlock(&LOCK_user_conn);
  DBUG_RETURN(error);
#else
  return (0);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}


static void reset_mqh(THD *thd, LEX_USER *lu, bool get_them= 0)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  (void) pthread_mutex_lock(&LOCK_user_conn);
  if (lu)  // for GRANT
  {
    USER_CONN *uc;
    uint temp_len=lu->user.length+lu->host.length+2;
    char temp_user[USERNAME_LENGTH+HOSTNAME_LENGTH+2];

    memcpy(temp_user,lu->user.str,lu->user.length);
    memcpy(temp_user+lu->user.length+1,lu->host.str,lu->host.length);
    temp_user[lu->user.length]='\0'; temp_user[temp_len-1]=0;
    if ((uc = (struct  user_conn *) hash_search(&hash_user_connections,
						(byte*) temp_user, temp_len)))
    {
      uc->questions=0;
      get_mqh(temp_user,&temp_user[lu->user.length+1],uc);
      uc->updates=0;
      uc->conn_per_hour=0;
    }
  }
  else // for FLUSH PRIVILEGES and FLUSH USER_RESOURCES
  {
    for (uint idx=0;idx < hash_user_connections.records; idx++)
    {
      USER_CONN *uc=(struct user_conn *) hash_element(&hash_user_connections,
						      idx);
      if (get_them)
	get_mqh(uc->user,uc->host,uc);
      uc->questions=0;
      uc->updates=0;
      uc->conn_per_hour=0;
    }
  }
  (void) pthread_mutex_unlock(&LOCK_user_conn);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}

/*
    Perform handshake, authorize client and update thd ACL variables.
  SYNOPSIS
    check_connection()
    thd  thread handle

  RETURN
     0  success, OK is sent to user, thd is updated.
    -1  error, which is sent to user
   > 0  error code (not sent to user)
*/

#ifndef EMBEDDED_LIBRARY
static int check_connection(THD *thd)
{
  uint connect_errors= 0;
  NET *net= &thd->net;
  ulong pkt_len= 0;
  char *end;

  DBUG_PRINT("info",
             ("New connection received on %s", vio_description(net->vio)));

  if (!thd->host)                           // If TCP/IP connection
  {
    char ip[30];

    if (vio_peer_addr(net->vio, ip, &thd->peer_port))
      return (ER_BAD_HOST_ERROR);
    if (!(thd->ip= my_strdup(ip,MYF(0))))
      return (ER_OUT_OF_RESOURCES);
    thd->host_or_ip= thd->ip;
    vio_in_addr(net->vio,&thd->remote.sin_addr);
    if (!(specialflag & SPECIAL_NO_RESOLVE))
    {
      vio_in_addr(net->vio,&thd->remote.sin_addr);
      thd->host=ip_to_hostname(&thd->remote.sin_addr,&connect_errors);
      /* Cut very long hostnames to avoid possible overflows */
      if (thd->host)
      {
        if (thd->host != my_localhost)
          thd->host[min(strlen(thd->host), HOSTNAME_LENGTH)]= 0;
        thd->host_or_ip= thd->host;
      }
      if (connect_errors > max_connect_errors)
        return(ER_HOST_IS_BLOCKED);
    }
    DBUG_PRINT("info",("Host: %s  ip: %s",
		       thd->host ? thd->host : "unknown host",
		       thd->ip ? thd->ip : "unknown ip"));
    if (acl_check_host(thd->host,thd->ip))
      return(ER_HOST_NOT_PRIVILEGED);
  }
  else /* Hostname given means that the connection was on a socket */
  {
    DBUG_PRINT("info",("Host: %s",thd->host));
    thd->host_or_ip= thd->host;
    thd->ip= 0;
    /* Reset sin_addr */
    bzero((char*) &thd->remote, sizeof(thd->remote));
  }
  vio_keepalive(net->vio, TRUE);
  {
    /* buff[] needs to big enough to hold the server_version variable */
    char buff[SERVER_VERSION_LENGTH + SCRAMBLE_LENGTH + 64];
    ulong client_flags = (CLIENT_LONG_FLAG | CLIENT_CONNECT_WITH_DB |
			  CLIENT_PROTOCOL_41 | CLIENT_SECURE_CONNECTION);

    if (opt_using_transactions)
      client_flags|=CLIENT_TRANSACTIONS;
#ifdef HAVE_COMPRESS
    client_flags |= CLIENT_COMPRESS;
#endif /* HAVE_COMPRESS */
#ifdef HAVE_OPENSSL
    if (ssl_acceptor_fd)
      client_flags |= CLIENT_SSL;       /* Wow, SSL is avalaible! */
#endif /* HAVE_OPENSSL */

    end= strnmov(buff, server_version, SERVER_VERSION_LENGTH) + 1;
    int4store((uchar*) end, thd->thread_id);
    end+= 4;
    /*
      So as check_connection is the only entry point to authorization
      procedure, scramble is set here. This gives us new scramble for
      each handshake.
    */
    create_random_string(thd->scramble, SCRAMBLE_LENGTH, &thd->rand);
    /*
      Old clients does not understand long scrambles, but can ignore packet
      tail: that's why first part of the scramble is placed here, and second
      part at the end of packet.
    */
    end= strmake(end, thd->scramble, SCRAMBLE_LENGTH_323) + 1;
   
    int2store(end, client_flags);
    /* write server characteristics: up to 16 bytes allowed */
    end[2]=(char) default_charset_info->number;
    int2store(end+3, thd->server_status);
    bzero(end+5, 13);
    end+= 18;
    /* write scramble tail */
    end= strmake(end, thd->scramble + SCRAMBLE_LENGTH_323, 
                 SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323) + 1;

    /* At this point we write connection message and read reply */
    if (net_write_command(net, (uchar) protocol_version, "", 0, buff,
			  (uint) (end-buff)) ||
	(pkt_len= my_net_read(net)) == packet_error ||
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
  if (thd->packet.alloc(thd->variables.net_buffer_length))
    return(ER_OUT_OF_RESOURCES);

  thd->client_capabilities=uint2korr(net->read_pos);
#ifdef TO_BE_REMOVED_IN_4_1_RELEASE
  /*
    This is just a safety check against any client that would use the old
    CLIENT_CHANGE_USER flag
  */
  if ((thd->client_capabilities & CLIENT_PROTOCOL_41) &&
      !(thd->client_capabilities & (CLIENT_RESERVED |
				    CLIENT_SECURE_CONNECTION |
				    CLIENT_MULTI_RESULTS)))
    thd->client_capabilities&= ~CLIENT_PROTOCOL_41;
#endif
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    thd->client_capabilities|= ((ulong) uint2korr(net->read_pos+2)) << 16;
    thd->max_client_packet_length= uint4korr(net->read_pos+4);
    DBUG_PRINT("info", ("client_character_set: %d", (uint) net->read_pos[8]));
    /*
      Use server character set and collation if
      - opt_character_set_client_handshake is not set
      - client has not specified a character set
      - client character set is the same as the servers
      - client character set doesn't exists in server
    */
    if (!opt_character_set_client_handshake ||
        !(thd->variables.character_set_client=
	  get_charset((uint) net->read_pos[8], MYF(0))) ||
	!my_strcasecmp(&my_charset_latin1,
		       global_system_variables.character_set_client->name,
		       thd->variables.character_set_client->name))
    {
      thd->variables.character_set_client=
	global_system_variables.character_set_client;
      thd->variables.collation_connection=
	global_system_variables.collation_connection;
      thd->variables.character_set_results=
	global_system_variables.character_set_results;
    }
    else
    {
      thd->variables.character_set_results=
      thd->variables.collation_connection= 
	thd->variables.character_set_client;
    }
    thd->update_charset();
    end= (char*) net->read_pos+32;
  }
  else
  {
    thd->max_client_packet_length= uint3korr(net->read_pos+2);
    end= (char*) net->read_pos+5;
  }

  if (thd->client_capabilities & CLIENT_IGNORE_SPACE)
    thd->variables.sql_mode|= MODE_IGNORE_SPACE;
#ifdef HAVE_OPENSSL
  DBUG_PRINT("info", ("client capabilities: %d", thd->client_capabilities));
  if (thd->client_capabilities & CLIENT_SSL)
  {
    /* Do the SSL layering. */
    if (!ssl_acceptor_fd)
    {
      inc_host_errors(&thd->remote.sin_addr);
      return(ER_HANDSHAKE_ERROR);
    }
    DBUG_PRINT("info", ("IO layer change in progress..."));
    if (sslaccept(ssl_acceptor_fd, net->vio, thd->variables.net_wait_timeout))
    {
      DBUG_PRINT("error", ("Failed to read user information (pkt_len= %lu)",
			   pkt_len));
      inc_host_errors(&thd->remote.sin_addr);
      return(ER_HANDSHAKE_ERROR);
    }
    DBUG_PRINT("info", ("Reading user information over SSL layer"));
    if ((pkt_len= my_net_read(net)) == packet_error ||
	pkt_len < NORMAL_HANDSHAKE_SIZE)
    {
      DBUG_PRINT("error", ("Failed to read user information (pkt_len= %lu)",
			   pkt_len));
      inc_host_errors(&thd->remote.sin_addr);
      return(ER_HANDSHAKE_ERROR);
    }
  }
#endif

  if (end >= (char*) net->read_pos+ pkt_len +2)
  {
    inc_host_errors(&thd->remote.sin_addr);
    return(ER_HANDSHAKE_ERROR);
  }

  if (thd->client_capabilities & CLIENT_INTERACTIVE)
    thd->variables.net_wait_timeout= thd->variables.net_interactive_timeout;
  if ((thd->client_capabilities & CLIENT_TRANSACTIONS) &&
      opt_using_transactions)
    net->return_status= &thd->server_status;
  net->read_timeout=(uint) thd->variables.net_read_timeout;

  char *user= end;
  char *passwd= strend(user)+1;
  char *db= passwd;
  char db_buff[NAME_LEN+1];                     // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH+1];		// buffer to store user in utf8
  uint dummy_errors;

  /*
    Old clients send null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.
  */
  uint passwd_len= thd->client_capabilities & CLIENT_SECURE_CONNECTION ?
    *passwd++ : strlen(passwd);
  db= thd->client_capabilities & CLIENT_CONNECT_WITH_DB ?
    db + passwd_len + 1 : 0;

  /* Since 4.1 all database names are stored in utf8 */
  if (db)
  {
    db_buff[copy_and_convert(db_buff, sizeof(db_buff)-1,
                             system_charset_info,
                             db, strlen(db),
                             thd->charset(), &dummy_errors)]= 0;
    db= db_buff;
  }

  user_buff[copy_and_convert(user_buff, sizeof(user_buff)-1,
                             system_charset_info, user, strlen(user),
                             thd->charset(), &dummy_errors)]= '\0';
  user= user_buff;

  if (thd->user)
    x_free(thd->user);
  if (!(thd->user= my_strdup(user, MYF(0))))
    return (ER_OUT_OF_RESOURCES);
  return check_user(thd, COM_CONNECT, passwd, passwd_len, db, TRUE);
}


void execute_init_command(THD *thd, sys_var_str *init_command_var,
			  rw_lock_t *var_mutex)
{
  Vio* save_vio;
  ulong save_client_capabilities;

  thd->proc_info= "Execution of init_command";
  /*
    We need to lock init_command_var because
    during execution of init_command_var query
    values of init_command_var can't be changed
  */
  rw_rdlock(var_mutex);
  thd->query= init_command_var->value;
  thd->query_length= init_command_var->value_length;
  save_client_capabilities= thd->client_capabilities;
  thd->client_capabilities|= CLIENT_MULTI_QUERIES;
  /*
    We don't need return result of execution to client side.
    To forbid this we should set thd->net.vio to 0.
  */
  save_vio= thd->net.vio;
  thd->net.vio= 0;
  dispatch_command(COM_QUERY, thd, thd->query, thd->query_length+1);
  rw_unlock(var_mutex);
  thd->client_capabilities= save_client_capabilities;
  thd->net.vio= save_vio;
}


pthread_handler_decl(handle_one_connection,arg)
{
  THD *thd=(THD*) arg;
  uint launch_time  =
    (uint) ((thd->thr_create_time = time(NULL)) - thd->connect_time);
  if (launch_time >= slow_launch_time)
    statistic_increment(slow_launch_threads,&LOCK_status );

  pthread_detach_this_thread();

#if !defined( __WIN__) && !defined(OS2)	// Win32 calls this in pthread_create
  // The following calls needs to be done before we call DBUG_ macros
  if (!(test_flags & TEST_NO_THREADS) & my_thread_init())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES, 1);
    statistic_increment(aborted_connects,&LOCK_status);
    end_thread(thd,0);
    return 0;
  }
#endif

  /*
    handle_one_connection() is the only way a thread would start
    and would always be on top of the stack, therefore, the thread
    stack always starts at the address of the first local variable
    of handle_one_connection, which is thd. We need to know the
    start of the stack so that we could check for stack overruns.
  */
  DBUG_PRINT("info", ("handle_one_connection called by thread %d\n",
		      thd->thread_id));
  // now that we've called my_thread_init(), it is safe to call DBUG_*

#if defined(__WIN__)
  init_signals();				// IRENA; testing ?
#elif !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif
  if (thd->store_globals())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES, 1);
    statistic_increment(aborted_connects,&LOCK_status);
    end_thread(thd,0);
    return 0;
  }

  do
  {
    int error;
    NET *net= &thd->net;
    thd->thread_stack= (char*) &thd;

    if ((error=check_connection(thd)))
    {						// Wrong permissions
      if (error > 0)
	net_printf(thd,error,thd->host_or_ip);
#ifdef __NT__
      if (vio_type(net->vio) == VIO_TYPE_NAMEDPIPE)
	my_sleep(1000);				/* must wait after eof() */
#endif
      statistic_increment(aborted_connects,&LOCK_status);
      goto end_thread;
    }
#ifdef __NETWARE__
    netware_reg_user(thd->ip, thd->user, "MySQL");
#endif
    if (thd->variables.max_join_size == HA_POS_ERROR)
      thd->options |= OPTION_BIG_SELECTS;
    if (thd->client_capabilities & CLIENT_COMPRESS)
      net->compress=1;				// Use compression

    thd->version= refresh_version;
    thd->proc_info= 0;
    thd->set_time();
    thd->init_for_queries();
    if (sys_init_connect.value_length && !(thd->master_access & SUPER_ACL))
    {
      execute_init_command(thd, &sys_init_connect, &LOCK_sys_init_connect);
      if (thd->query_error)
	thd->killed= 1;
    }
    while (!net->error && net->vio != 0 && !thd->killed)
    {
      if (do_command(thd))
	break;
    }
    if (thd->user_connect)
      decrease_user_connections(thd->user_connect);
    free_root(thd->mem_root,MYF(0));
    if (net->error && net->vio != 0 && net->report_error)
    {
      if (!thd->killed && thd->variables.log_warnings > 1)
        sql_print_warning(ER(ER_NEW_ABORTING_CONNECTION),
                          thd->thread_id,(thd->db ? thd->db : "unconnected"),
                          thd->user ? thd->user : "unauthenticated",
                          thd->host_or_ip,
                          (net->last_errno ? ER(net->last_errno) :
                           ER(ER_UNKNOWN_ERROR)));
      send_error(thd,net->last_errno,NullS);
      statistic_increment(aborted_threads,&LOCK_status);
    }
    else if (thd->killed)
    {
      statistic_increment(aborted_threads,&LOCK_status);
    }
    
end_thread:
    close_connection(thd, 0, 1);
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

#endif /* EMBEDDED_LIBRARY */

/*
  Execute commands from bootstrap_file.
  Used when creating the initial grant tables
*/

extern "C" pthread_handler_decl(handle_bootstrap,arg)
{
  THD *thd=(THD*) arg;
  FILE *file=bootstrap_file;
  char *buff;

  /* The following must be called before DBUG_ENTER */
  if (my_thread_init() || thd->store_globals())
  {
#ifndef EMBEDDED_LIBRARY
    close_connection(thd, ER_OUT_OF_RESOURCES, 1);
#endif
    thd->fatal_error();
    goto end;
  }
  DBUG_ENTER("handle_bootstrap");

#ifndef EMBEDDED_LIBRARY
  pthread_detach_this_thread();
  thd->thread_stack= (char*) &thd;
#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif
#endif /* EMBEDDED_LIBRARY */

  if (thd->variables.max_join_size == HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;

  thd->proc_info=0;
  thd->version=refresh_version;
  thd->priv_user=thd->user=(char*) my_strdup("boot", MYF(MY_WME));

  buff= (char*) thd->net.buff;
  thd->init_for_queries();
  while (fgets(buff, thd->net.max_packet, file))
  {
    ulong length= (ulong) strlen(buff);
    while (buff[length-1] != '\n' && !feof(file))
    {
      /*
        We got only a part of the current string. Will try to increase
        net buffer then read the rest of the current string.
      */
      if (net_realloc(&(thd->net), 2 * thd->net.max_packet))
      {
        send_error(thd, thd->net.last_errno, NullS);
        thd->is_fatal_error= 1;
        break;
      }
      buff= (char*) thd->net.buff;
      fgets(buff + length, thd->net.max_packet - length, file);
      length+= (ulong) strlen(buff + length);
    }
    if (thd->is_fatal_error)
      break;
    while (length && (my_isspace(thd->charset(), buff[length-1]) ||
           buff[length-1] == ';'))
      length--;
    buff[length]=0;
    thd->query_length=length;
    thd->query= thd->memdup_w_gap(buff, length+1, thd->db_length+1);
    thd->query[length] = '\0';
    thd->query_id=query_id++;
    if (mqh_used && thd->user_connect && check_mqh(thd, SQLCOM_END))
    {
      thd->net.error = 0;
      close_thread_tables(thd);			// Free tables
      free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
      break;
    }
    mysql_parse(thd,thd->query,length);
    close_thread_tables(thd);			// Free tables
    if (thd->is_fatal_error)
      break;
    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
    free_root(&thd->transaction.mem_root,MYF(MY_KEEP_PREALLOC));
  }

  /* thd->fatal_error should be set in case something went wrong */
end:
#ifndef EMBEDDED_LIBRARY
  (void) pthread_mutex_lock(&LOCK_thread_count);
  thread_count--;
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  (void) pthread_cond_broadcast(&COND_thread_count);
  my_thread_end();
  pthread_exit(0);
#endif
  DBUG_RETURN(0);				// Never reached
}

    /* This works because items are allocated with sql_alloc() */

void free_items(Item *item)
{
  for (; item ; item=item->next)
    item->delete_self();
}

    /* This works because items are allocated with sql_alloc() */

void cleanup_items(Item *item)
{
  for (; item ; item=item->next)
    item->cleanup();
}

int mysql_table_dump(THD* thd, char* db, char* tbl_name, int fd)
{
  TABLE* table;
  TABLE_LIST* table_list;
  int error = 0;
  DBUG_ENTER("mysql_table_dump");
  db = (db && db[0]) ? db : thd->db;
  if (!(table_list = (TABLE_LIST*) thd->calloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(1); // out of memory
  table_list->db = db;
  table_list->real_name = table_list->alias = tbl_name;
  table_list->lock_type = TL_READ_NO_INSERT;
  table_list->next = 0;

  if (!db || check_db_name(db))
  {
    net_printf(thd,ER_WRONG_DB_NAME, db ? db : "NULL");
    goto err;
  }
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tbl_name);
  remove_escape(table_list->real_name);

  if (!(table=open_ltable(thd, table_list, TL_READ_NO_INSERT)))
    DBUG_RETURN(1);

  if (check_one_table_access(thd, SELECT_ACL, table_list))
    goto err;
  thd->free_list = 0;
  thd->query_length=(uint) strlen(tbl_name);
  thd->query = tbl_name;
  if ((error = mysqld_dump_create_info(thd, table, -1)))
  {
    my_error(ER_GET_ERRNO, MYF(0), my_errno);
    goto err;
  }
  net_flush(&thd->net);
  if ((error= table->file->dump(thd,fd)))
    my_error(ER_GET_ERRNO, MYF(0), error);

err:
  close_thread_tables(thd);
  DBUG_RETURN(error);
}


#ifndef EMBEDDED_LIBRARY

/*
  Read one command from socket and execute it (query or simple command).
  This function is called in loop from thread function.
  SYNOPSIS
    do_command()
  RETURN VALUE
    0  success
    1  request of thread shutdown (see dispatch_command() description)
*/

bool do_command(THD *thd)
{
  char *packet;
  uint old_timeout;
  ulong packet_length;
  NET *net;
  enum enum_server_command command;
  DBUG_ENTER("do_command");

  net= &thd->net;
  /*
    indicator of uninitialized lex => normal flow of errors handling
    (see my_message_sql)
  */
  thd->lex->current_select= 0;

  packet=0;
  old_timeout=net->read_timeout;
  // Wait max for 8 hours
  net->read_timeout=(uint) thd->variables.net_wait_timeout;
  thd->clear_error();				// Clear error message

  net_new_transaction(net);
  if ((packet_length=my_net_read(net)) == packet_error)
  {
    DBUG_PRINT("info",("Got error %d reading command from socket %s",
		       net->error,
		       vio_description(net->vio)));
    /* Check if we can continue without closing the connection */
    if (net->error != 3)
    {
      statistic_increment(aborted_threads,&LOCK_status);
      DBUG_RETURN(TRUE);			// We have to close it.
    }
    send_error(thd,net->last_errno,NullS);
    net->error= 0;
    DBUG_RETURN(FALSE);
  }
  else
  {
    packet=(char*) net->read_pos;
    command = (enum enum_server_command) (uchar) packet[0];
    if (command >= COM_END)
      command= COM_END;				// Wrong command
    DBUG_PRINT("info",("Command on %s = %d (%s)",
		       vio_description(net->vio), command,
		       command_name[command]));
  }
  net->read_timeout=old_timeout;		// restore it
  /*
    packet_length contains length of data, as it was stored in packet
    header. In case of malformed header, packet_length can be zero.
    If packet_length is not zero, my_net_read ensures that this number
    of bytes was actually read from network. Additionally my_net_read
    sets packet[packet_length]= 0 (thus if packet_length == 0,
    command == packet[0] == COM_SLEEP).
    In dispatch_command packet[packet_length] points beyond the end of packet.
  */
  DBUG_RETURN(dispatch_command(command,thd, packet+1, (uint) packet_length));
}
#endif  /* EMBEDDED_LIBRARY */

/*
   Perform one connection-level (COM_XXXX) command.
  SYNOPSIS
    dispatch_command()
    thd             connection handle
    command         type of command to perform 
    packet          data for the command, packet is always null-terminated
    packet_length   length of packet + 1 (to show that data is
                    null-terminated) except for COM_SLEEP, where it
                    can be zero.
  RETURN VALUE
    0   ok
    1   request of thread shutdown, i. e. if command is
        COM_QUIT/COM_SHUTDOWN
*/

bool dispatch_command(enum enum_server_command command, THD *thd,
		      char* packet, uint packet_length)
{
  NET *net= &thd->net;
  bool error= 0;
  DBUG_ENTER("dispatch_command");

  thd->command=command;
  /*
    Commands which always take a long time are logged into
    the slow log only if opt_log_slow_admin_statements is set.
  */
  thd->enable_slow_log= TRUE;
  thd->set_time();
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query_id=query_id;
  if (command != COM_STATISTICS && command != COM_PING)
    query_id++;
  thread_running++;
  /* TODO: set thd->lex->sql_command to SQLCOM_END here */
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  thd->server_status&=
           ~(SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED);
  switch (command) {
  case COM_INIT_DB:
  {
    LEX_STRING tmp;
    statistic_increment(com_stat[SQLCOM_CHANGE_DB],&LOCK_status);
    thd->convert_string(&tmp, system_charset_info,
			packet, strlen(packet), thd->charset());
    if (!mysql_change_db(thd, tmp.str))
      mysql_log.write(thd,command,"%s",thd->db);
    break;
  }
#ifdef HAVE_REPLICATION
  case COM_REGISTER_SLAVE:
  {
    if (!register_slave(thd, (uchar*)packet, packet_length))
      send_ok(thd);
    break;
  }
#endif
  case COM_TABLE_DUMP:
  {
    char *db, *tbl_name;
    uint db_len= *(uchar*) packet;
    uint tbl_len= *(uchar*) (packet + db_len + 1);

    statistic_increment(com_other, &LOCK_status);
    thd->enable_slow_log= opt_log_slow_admin_statements;
    db= thd->alloc(db_len + tbl_len + 2);
    tbl_name= strmake(db, packet + 1, db_len)+1;
    strmake(tbl_name, packet + db_len + 2, tbl_len);
    if (mysql_table_dump(thd, db, tbl_name, -1))
      send_error(thd); // dump to NET
    break;
  }
  case COM_CHANGE_USER:
  {
    thd->change_user();
    thd->clear_error();                         // if errors from rollback

    statistic_increment(com_other, &LOCK_status);
    char *user= (char*) packet;
    char *passwd= strend(user)+1;
    /* 
      Old clients send null-terminated string ('\0' for empty string) for
      password.  New clients send the size (1 byte) + string (not null
      terminated, so also '\0' for empty string).
    */
    char db_buff[NAME_LEN+1];                 // buffer to store db in utf8 
    char *db= passwd;
    uint passwd_len= thd->client_capabilities & CLIENT_SECURE_CONNECTION ? 
      *passwd++ : strlen(passwd);
    db+= passwd_len + 1;
#ifndef EMBEDDED_LIBRARY
    /* Small check for incomming packet */
    if ((uint) ((uchar*) db - net->read_pos) > packet_length)
    {
      send_error(thd, ER_UNKNOWN_COM_ERROR);
      break;
    }
#endif
    /* Convert database name to utf8 */
    uint dummy_errors;
    db_buff[copy_and_convert(db_buff, sizeof(db_buff)-1,
                             system_charset_info, db, strlen(db),
                             thd->charset(), &dummy_errors)]= 0;
    db= db_buff;

    /* Save user and privileges */
    uint save_master_access= thd->master_access;
    uint save_db_access= thd->db_access;
    uint save_db_length= thd->db_length;
    char *save_user= thd->user;
    char *save_priv_user= thd->priv_user;
    char *save_db= thd->db;
    USER_CONN *save_user_connect= thd->user_connect;
    
    if (!(thd->user= my_strdup(user, MYF(0))))
    {
      thd->user= save_user;
      send_error(thd, ER_OUT_OF_RESOURCES);
      break;
    }

    /* Clear variables that are allocated */
    thd->user_connect= 0;
    int res= check_user(thd, COM_CHANGE_USER, passwd, passwd_len, db, FALSE);

    if (res)
    {
      /* authentification failure, we shall restore old user */
      if (res > 0)
        send_error(thd, ER_UNKNOWN_COM_ERROR);
      x_free(thd->user);
      thd->user= save_user;
      thd->priv_user= save_priv_user;
      thd->user_connect= save_user_connect;
      thd->master_access= save_master_access;
      thd->db_access= save_db_access;
      thd->db= save_db;
      thd->db_length= save_db_length;
    }
    else
    {
      /* we've authenticated new user */
      if (save_user_connect)
	decrease_user_connections(save_user_connect);
      x_free((gptr) save_db);
      x_free((gptr) save_user);
    }
    break;
  }
  case COM_EXECUTE:
  {
    mysql_stmt_execute(thd, packet, packet_length);
    break;
  }
  case COM_LONG_DATA:
  {
    mysql_stmt_get_longdata(thd, packet, packet_length);
    break;
  }
  case COM_PREPARE:
  {
    mysql_stmt_prepare(thd, packet, packet_length);
    break;
  }
  case COM_CLOSE_STMT:
  {
    mysql_stmt_free(thd, packet);
    break;
  }
  case COM_RESET_STMT:
  {
    mysql_stmt_reset(thd, packet);
    break;
  }
  case COM_QUERY:
  {
    if (alloc_query(thd, packet, packet_length))
      break;					// fatal error is set
    char *packet_end= thd->query + thd->query_length;
    mysql_log.write(thd,command,"%s",thd->query);
    DBUG_PRINT("query",("%-.4096s",thd->query));
    mysql_parse(thd,thd->query, thd->query_length);

    while (!thd->killed && !thd->is_fatal_error && thd->lex->found_colon)
    {
      char *packet= thd->lex->found_colon;
      /*
        Multiple queries exits, execute them individually
	in embedded server - just store them to be executed later 
      */
#ifndef EMBEDDED_LIBRARY
      if (thd->lock || thd->open_tables || thd->derived_tables)
        close_thread_tables(thd);
#endif
      ulong length= (ulong)(packet_end-packet);

      log_slow_statement(thd);

      /* Remove garbage at start of query */
      while (my_isspace(thd->charset(), *packet) && length > 0)
      {
        packet++;
        length--;
      }
      VOID(pthread_mutex_lock(&LOCK_thread_count));
      thd->query_length= length;
      thd->query= packet;
      thd->query_id= query_id++;
      thd->set_time(); /* Reset the query start time. */
      /* TODO: set thd->lex->sql_command to SQLCOM_END here */
      VOID(pthread_mutex_unlock(&LOCK_thread_count));
#ifndef EMBEDDED_LIBRARY
      mysql_parse(thd, packet, length);
#else
      /*
	'packet' can point inside the query_rest's buffer
	so we have to do memmove here
       */
      if (thd->query_rest.length() > length)
      {
	memmove(thd->query_rest.c_ptr(), packet, length);
	thd->query_rest.length(length);
      }
      else
	thd->query_rest.copy(packet, length, thd->query_rest.charset());

      thd->server_status&= ~ (SERVER_QUERY_NO_INDEX_USED |
                              SERVER_QUERY_NO_GOOD_INDEX_USED);
      break;
#endif /*EMBEDDED_LIBRARY*/
    }

    if (!(specialflag & SPECIAL_NO_PRIOR))
      my_pthread_setprio(pthread_self(),WAIT_PRIOR);
    DBUG_PRINT("info",("query ready"));
    break;
  }
  case COM_FIELD_LIST:				// This isn't actually needed
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(thd,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    break;
#else
  {
    char *fields, *pend;
    TABLE_LIST table_list;
    LEX_STRING conv_name;

    statistic_increment(com_stat[SQLCOM_SHOW_FIELDS],&LOCK_status);
    bzero((char*) &table_list,sizeof(table_list));
    if (!(table_list.db=thd->db))
    {
      send_error(thd,ER_NO_DB_ERROR);
      break;
    }
    thd->free_list=0;
    pend= strend(packet);
    thd->convert_string(&conv_name, system_charset_info,
			packet, (uint) (pend-packet), thd->charset());
    table_list.alias= table_list.real_name= conv_name.str;
    packet= pend+1;
    thd->query_length= strlen(packet);       // for simplicity: don't optimize
    if (!(thd->query=fields=thd->memdup(packet,thd->query_length+1)))
      break;
    mysql_log.write(thd,command,"%s %s",table_list.real_name,fields);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, table_list.real_name);
    remove_escape(table_list.real_name);	// This can't have wildcards

    if (check_access(thd,SELECT_ACL,table_list.db,&table_list.grant.privilege,
		     0, 0))
      break;
    if (grant_option &&
	check_grant(thd, SELECT_ACL, &table_list, 2, UINT_MAX, 0))
      break;
    mysqld_list_fields(thd,&table_list,fields);
    free_items(thd->free_list);
    thd->free_list= 0;
    break;
  }
#endif
  case COM_QUIT:
    /* We don't calculate statistics for this command */
    mysql_log.write(thd,command,NullS);
    net->error=0;				// Don't give 'abort' message
    error=TRUE;					// End server
    break;

  case COM_CREATE_DB:				// QQ: To be removed
    {
      char *db=thd->strdup(packet), *alias;
      HA_CREATE_INFO create_info;

      statistic_increment(com_stat[SQLCOM_CREATE_DB],&LOCK_status);
      // null test to handle EOM
      if (!db || !(alias= thd->strdup(db)) || check_db_name(db))
      {
	net_printf(thd,ER_WRONG_DB_NAME, db ? db : "NULL");
	break;
      }
      if (check_access(thd,CREATE_ACL,db,0,1,0))
	break;
      mysql_log.write(thd,command,packet);
      bzero(&create_info, sizeof(create_info));
      if (mysql_create_db(thd, (lower_case_table_names == 2 ? alias : db),
                          &create_info, 0) < 0)
        send_error(thd, thd->killed ? ER_SERVER_SHUTDOWN : 0);
      break;
    }
  case COM_DROP_DB:				// QQ: To be removed
    {
      statistic_increment(com_stat[SQLCOM_DROP_DB],&LOCK_status);
      char *db=thd->strdup(packet), *alias;
      // null test to handle EOM
      if (!db || !(alias= thd->strdup(db)) || check_db_name(db))
      {
	net_printf(thd,ER_WRONG_DB_NAME, db ? db : "NULL");
	break;
      }
      if (check_access(thd,DROP_ACL,db,0,1,0))
	break;
      if (thd->locked_tables || thd->active_transaction())
      {
	send_error(thd,ER_LOCK_OR_ACTIVE_TRANSACTION);
	break;
      }
      mysql_log.write(thd,command,db);
      if (mysql_rm_db(thd, (lower_case_table_names == 2 ? alias : db),
                      0, 0) < 0)
        send_error(thd, thd->killed ? ER_SERVER_SHUTDOWN : 0);
      break;
    }
#ifndef EMBEDDED_LIBRARY
  case COM_BINLOG_DUMP:
    {
      ulong pos;
      ushort flags;
      uint32 slave_server_id;

      statistic_increment(com_other,&LOCK_status);
      thd->enable_slow_log= opt_log_slow_admin_statements;
      if (check_global_access(thd, REPL_SLAVE_ACL))
	break;

      /* TODO: The following has to be changed to an 8 byte integer */
      pos = uint4korr(packet);
      flags = uint2korr(packet + 4);
      thd->server_id=0; /* avoid suicide */
      if ((slave_server_id= uint4korr(packet+6))) // mysqlbinlog.server_id==0
	kill_zombie_dump_threads(slave_server_id);
      thd->server_id = slave_server_id;

      mysql_log.write(thd, command, "Log: '%s'  Pos: %ld", packet+10,
                      (long) pos);
      mysql_binlog_send(thd, thd->strdup(packet + 10), (my_off_t) pos, flags);
      unregister_slave(thd,1,1);
      // fake COM_QUIT -- if we get here, the thread needs to terminate
      error = TRUE;
      net->error = 0;
      break;
    }
#endif
  case COM_REFRESH:
    {
      statistic_increment(com_stat[SQLCOM_FLUSH],&LOCK_status);
      ulong options= (ulong) (uchar) packet[0];
      if (check_global_access(thd,RELOAD_ACL))
	break;
      mysql_log.write(thd,command,NullS);
      if (reload_acl_and_cache(thd, options, (TABLE_LIST*) 0, NULL))
        send_error(thd, 0);
      else
        send_ok(thd);
      break;
    }
#ifndef EMBEDDED_LIBRARY
  case COM_SHUTDOWN:
  {
    statistic_increment(com_other,&LOCK_status);
    if (check_global_access(thd,SHUTDOWN_ACL))
      break; /* purecov: inspected */
    /*
      If the client is < 4.1.3, it is going to send us no argument; then
      packet_length is 1, packet[0] is the end 0 of the packet. Note that
      SHUTDOWN_DEFAULT is 0. If client is >= 4.1.3, the shutdown level is in
      packet[0].
    */
    enum mysql_enum_shutdown_level level=
      (enum mysql_enum_shutdown_level) (uchar) packet[0];
    DBUG_PRINT("quit",("Got shutdown command for level %u", level));
    if (level == SHUTDOWN_DEFAULT)
      level= SHUTDOWN_WAIT_ALL_BUFFERS; // soon default will be configurable
    else if (level != SHUTDOWN_WAIT_ALL_BUFFERS)
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "this shutdown level");
      send_error(thd);
      break;
    }
    DBUG_PRINT("quit",("Got shutdown command for level %u", level));
    mysql_log.write(thd,command,NullS);
    send_eof(thd);
#ifdef __WIN__
    sleep(1);					// must wait after eof()
#endif
#ifndef OS2
    send_eof(thd);				// This is for 'quit request'
#endif
    close_connection(thd, 0, 1);
    close_thread_tables(thd);			// Free before kill
    free_root(thd->mem_root,MYF(0));
    free_root(&thd->transaction.mem_root,MYF(0));
    kill_mysql();
    error=TRUE;
    break;
  }
#endif
  case COM_STATISTICS:
  {
    mysql_log.write(thd,command,NullS);
    statistic_increment(com_stat[SQLCOM_SHOW_STATUS],&LOCK_status);
#ifndef EMBEDDED_LIBRARY
    char buff[200];
#else
    char *buff= thd->net.last_error;
#endif
    ulong uptime = (ulong) (thd->start_time - start_time);
    sprintf((char*) buff,
	    "Uptime: %ld  Threads: %d  Questions: %lu  Slow queries: %ld  Opens: %ld  Flush tables: %ld  Open tables: %u  Queries per second avg: %.3f",
	    uptime,
	    (int) thread_count,thd->query_id,long_query_count,
	    opened_tables,refresh_version, cached_tables(),
	    uptime ? (float)thd->query_id/(float)uptime : 0);
#ifdef SAFEMALLOC
    if (sf_malloc_cur_memory)				// Using SAFEMALLOC
      sprintf(strend(buff), "  Memory in use: %ldK  Max memory used: %ldK",
	      (sf_malloc_cur_memory+1023L)/1024L,
	      (sf_malloc_max_memory+1023L)/1024L);
#endif
#ifndef EMBEDDED_LIBRARY
    VOID(my_net_write(net, buff,(uint) strlen(buff)));
    VOID(net_flush(net));
#endif
    break;
  }
  case COM_PING:
    statistic_increment(com_other,&LOCK_status);
    send_ok(thd);				// Tell client we are alive
    break;
  case COM_PROCESS_INFO:
    statistic_increment(com_stat[SQLCOM_SHOW_PROCESSLIST],&LOCK_status);
    if (!thd->priv_user[0] && check_global_access(thd,PROCESS_ACL))
      break;
    mysql_log.write(thd,command,NullS);
    mysqld_list_processes(thd,
			  thd->master_access & PROCESS_ACL ? 
			  NullS : thd->priv_user, 0);
    break;
  case COM_PROCESS_KILL:
  {
    statistic_increment(com_stat[SQLCOM_KILL],&LOCK_status);
    ulong id=(ulong) uint4korr(packet);
    kill_one_thread(thd,id);
    break;
  }
  case COM_SET_OPTION:
  {
    statistic_increment(com_stat[SQLCOM_SET_OPTION], &LOCK_status);
    enum_mysql_set_option command= (enum_mysql_set_option) uint2korr(packet);
    switch (command) {
    case MYSQL_OPTION_MULTI_STATEMENTS_ON:
      thd->client_capabilities|= CLIENT_MULTI_STATEMENTS;
      send_eof(thd);
      break;
    case MYSQL_OPTION_MULTI_STATEMENTS_OFF:
      thd->client_capabilities&= ~CLIENT_MULTI_STATEMENTS;
      send_eof(thd);
      break;
    default:
      send_error(thd, ER_UNKNOWN_COM_ERROR);
      break;
    }
    break;
  }
  case COM_DEBUG:
    statistic_increment(com_other,&LOCK_status);
    if (check_global_access(thd, SUPER_ACL))
      break;					/* purecov: inspected */
    mysql_print_status(thd);
    mysql_log.write(thd,command,NullS);
    send_eof(thd);
    break;
  case COM_SLEEP:
  case COM_CONNECT:				// Impossible here
  case COM_TIME:				// Impossible from client
  case COM_DELAYED_INSERT:
  case COM_END:
  default:
    send_error(thd, ER_UNKNOWN_COM_ERROR);
    break;
  }
  if (thd->lock || thd->open_tables || thd->derived_tables)
  {
    thd->proc_info="closing tables";
    close_thread_tables(thd);			/* Free tables */
  }

  if (thd->is_fatal_error)
    send_error(thd,0);				// End of memory ?

  log_slow_statement(thd);

  thd->proc_info="cleaning up";
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For process list
  thd->proc_info=0;
  thd->command=COM_SLEEP;
  thd->query=0;
  thd->query_length=0;
  thread_running--;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->packet.shrink(thd->variables.net_buffer_length);	// Reclaim some memory
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
  DBUG_RETURN(error);
}


void log_slow_statement(THD *thd)
{
  time_t start_of_query=thd->start_time;
  thd->end_time();				// Set start time

  /*
    Do not log administrative statements unless the appropriate option is
    set; do not log into slow log if reading from backup.
  */
  if (thd->enable_slow_log && !thd->user_time)
  {
    thd->proc_info="logging slow query";

    if ((ulong) (thd->start_time - thd->time_after_lock) >
	thd->variables.long_query_time ||
	((thd->server_status &
	  (SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED)) &&
	 (specialflag & SPECIAL_LOG_QUERIES_NOT_USING_INDEXES)))
    {
      long_query_count++;
      mysql_slow_log.write(thd, thd->query, thd->query_length, start_of_query);
    }
  }
}


/*
  Read query from packet and store in thd->query
  Used in COM_QUERY and COM_PREPARE

  DESCRIPTION
    Sets the following THD variables:
      query
      query_length

  RETURN VALUES
    0	ok
    1	error;  In this case thd->fatal_error is set
*/

bool alloc_query(THD *thd, char *packet, ulong packet_length)
{
  packet_length--;				// Remove end null
  /* Remove garbage at start and end of query */
  while (my_isspace(thd->charset(),packet[0]) && packet_length > 0)
  {
    packet++;
    packet_length--;
  }
  char *pos=packet+packet_length;		// Point at end null
  while (packet_length > 0 &&
	 (pos[-1] == ';' || my_isspace(thd->charset() ,pos[-1])))
  {
    pos--;
    packet_length--;
  }
  /* We must allocate some extra memory for query cache */
  thd->query_length= 0;                        // Extra safety: Avoid races
  if (!(thd->query= (char*) thd->memdup_w_gap((gptr) (packet),
					      packet_length,
					      thd->db_length+ 1 +
					      QUERY_CACHE_FLAGS_SIZE)))
    return 1;
  thd->query[packet_length]=0;
  thd->query_length= packet_length;

  /* Reclaim some memory */
  thd->packet.shrink(thd->variables.net_buffer_length);
  thd->convert_buffer.shrink(thd->variables.net_buffer_length);

  if (!(specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),QUERY_PRIOR);
  return 0;
}

static void reset_one_shot_variables(THD *thd) 
{
  thd->variables.character_set_client=
    global_system_variables.character_set_client;
  thd->variables.collation_connection=
    global_system_variables.collation_connection;
  thd->variables.collation_database=
    global_system_variables.collation_database;
  thd->variables.collation_server=
    global_system_variables.collation_server;
  thd->update_charset();
  thd->variables.time_zone=
    global_system_variables.time_zone;
  thd->one_shot_set= 0;
}


/****************************************************************************
** mysql_execute_command
** Execute command saved in thd and current_lex->sql_command
****************************************************************************/

void
mysql_execute_command(THD *thd)
{
  int	res= 0;
  LEX	*lex= thd->lex;
  bool slave_fake_lock= 0;
  MYSQL_LOCK *fake_prev_lock= 0;
  SELECT_LEX *select_lex= &lex->select_lex;
  TABLE_LIST *tables= (TABLE_LIST*) select_lex->table_list.first;
  SELECT_LEX_UNIT *unit= &lex->unit;
  DBUG_ENTER("mysql_execute_command");

  /*
    Reset warning count for each query that uses tables
    A better approach would be to reset this for any commands
    that is not a SHOW command or a select that only access local
    variables, but for now this is probably good enough.
  */
  if (tables || &lex->select_lex != lex->all_selects_list ||
      lex->time_zone_tables_used)
    mysql_reset_errors(thd);

  /*
    When subselects or time_zone info is used in a query
    we create a new TABLE_LIST containing all referenced tables
    and set local variable 'tables' to point to this list.
  */
  if ((&lex->select_lex != lex->all_selects_list ||
       lex->time_zone_tables_used) &&
      lex->unit.create_total_list(thd, lex, &tables))
    DBUG_VOID_RETURN;

#ifdef HAVE_REPLICATION
  if (thd->slave_thread)
  {
    if (lex->sql_command == SQLCOM_UPDATE_MULTI)
    {
      DBUG_PRINT("info",("need faked locked tables"));
      
      if (check_multi_update_lock(thd, tables, &select_lex->item_list,
				  select_lex))
        goto error;

      /* Fix for replication, the tables are opened and locked,
         now we pretend that we have performed a LOCK TABLES action */
	 
      fake_prev_lock= thd->locked_tables;
      if (thd->lock)
        thd->locked_tables= thd->lock;
      thd->lock= 0;
      slave_fake_lock= 1;
    }
    /*
      Skip if we are in the slave thread, some table rules have been
      given and the table list says the query should not be replicated.

      Exceptions are:
      - SET: we always execute it (Not that many SET commands exists in
        the binary log anyway -- only 4.1 masters write SET statements,
	in 5.0 there are no SET statements in the binary log)
      - DROP TEMPORARY TABLE IF EXISTS: we always execute it (otherwise we
        have stale files on slave caused by exclusion of one tmp table).
    */
    if (!(lex->sql_command == SQLCOM_SET_OPTION) &&
	!(lex->sql_command == SQLCOM_DROP_TABLE &&
          lex->drop_temporary && lex->drop_if_exists) &&
        all_tables_not_ok(thd,tables))
    {
      /* we warn the slave SQL thread */
      my_error(ER_SLAVE_IGNORED_TABLE, MYF(0));
      reset_one_shot_variables(thd);
      DBUG_VOID_RETURN;
    }
#ifndef TO_BE_DELETED
    /*
      This is a workaround to deal with the shortcoming in 3.23.44-3.23.46
      masters in RELEASE_LOCK() logging. We re-write SELECT RELEASE_LOCK()
      as DO RELEASE_LOCK()
    */
    if (lex->sql_command == SQLCOM_SELECT)
    {
      lex->sql_command = SQLCOM_DO;
      lex->insert_list = &select_lex->item_list;
    }
#endif
  }
#endif /* !HAVE_REPLICATION */

  /*
    When option readonly is set deny operations which change tables.
    Except for the replication thread and the 'super' users.
  */
  if (opt_readonly &&
      !(thd->slave_thread || (thd->master_access & SUPER_ACL)) &&
      (uc_update_queries[lex->sql_command] > 0))
  {
    net_printf(thd, ER_OPTION_PREVENTS_STATEMENT, "--read-only");
    DBUG_VOID_RETURN;
  }

  statistic_increment(com_stat[lex->sql_command],&LOCK_status);
  switch (lex->sql_command) {
  case SQLCOM_SELECT:
  {
    /* assign global limit variable if limit is not given */
    {
      SELECT_LEX *param= lex->unit.global_parameters;
      if (!param->explicit_limit)
	param->select_limit= thd->variables.select_limit;
    }

    select_result *result=lex->result;
    if (tables)
    {
      res=check_table_access(thd,
			     lex->exchange ? SELECT_ACL | FILE_ACL :
			     SELECT_ACL,
			     tables,0);
    }
    else
      res=check_access(thd, lex->exchange ? SELECT_ACL | FILE_ACL : SELECT_ACL,
		       any_db,0,0,0);
    if (res)
    {
      res=0;
      break;					// Error message is given
    }
    /* 
       In case of single SELECT unit->global_parameters points on first SELECT
       TODO: move counters to SELECT_LEX
    */
    unit->offset_limit_cnt= (ha_rows) unit->global_parameters->offset_limit;
    unit->select_limit_cnt= (ha_rows) (unit->global_parameters->select_limit+
      unit->global_parameters->offset_limit);
    if (unit->select_limit_cnt <
	(ha_rows) unit->global_parameters->select_limit)
      unit->select_limit_cnt= HA_POS_ERROR;		// no limit
    if (unit->select_limit_cnt == HA_POS_ERROR && !select_lex->next_select())
      select_lex->options&= ~OPTION_FOUND_ROWS;

    if (!(res=open_and_lock_tables(thd,tables)))
    {
      if (lex->describe)
      {
	if (!(result= new select_send()))
	{
	  send_error(thd, ER_OUT_OF_RESOURCES);
	  DBUG_VOID_RETURN;
	}
	else
	  thd->send_explain_fields(result);
	res= mysql_explain_union(thd, &thd->lex->unit, result);
	if (lex->describe & DESCRIBE_EXTENDED)
	{
	  char buff[1024];
	  String str(buff,(uint32) sizeof(buff), system_charset_info);
	  str.length(0);
	  thd->lex->unit.print(&str);
	  str.append('\0');
	  push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
		       ER_YES, str.ptr());
	}
	result->send_eof();
        delete result;
      }
      else
      {
	if (!result && !(result= new select_send()))
	{
	  res= -1;
	  break;
	}
	query_cache_store_query(thd, tables);
	res= handle_select(thd, lex, result);
        if (result != lex->result)
          delete result;
      }
    }
    break;
  }
  case SQLCOM_PREPARE:
  {
    char *query_str;
    uint query_len;
    if (lex->prepared_stmt_code_is_varref)
    {
      /* This is PREPARE stmt FROM @var. */
      String str;
      CHARSET_INFO *to_cs= thd->variables.collation_connection;
      bool need_conversion;
      user_var_entry *entry;
      String *pstr= &str;
      uint32 unused;
      /*
        Convert @var contents to string in connection character set. Although
        it is known that int/real/NULL value cannot be a valid query we still
        convert it for error messages to uniform.
      */
      if ((entry=
             (user_var_entry*)hash_search(&thd->user_vars,
                                          (byte*)lex->prepared_stmt_code.str,
                                          lex->prepared_stmt_code.length))
          && entry->value)
      {
        my_bool is_var_null;
        pstr= entry->val_str(&is_var_null, &str, NOT_FIXED_DEC);
        /*
          NULL value of variable checked early as entry->value so here
          we can't get NULL in normal conditions
        */
        DBUG_ASSERT(!is_var_null);
        if (!pstr)
        {
          res= -1;
          break;      // EOM (error should be reported by allocator)
        }
      }
      else
      {
        /*
          variable absent or equal to NULL, so we need to set variable to
          something reasonable to get readable error message during parsing
        */
        str.set("NULL", 4, &my_charset_latin1);
      }

      need_conversion=
        String::needs_conversion(pstr->length(), pstr->charset(),
                                 to_cs, &unused);

      query_len= need_conversion? (pstr->length() * to_cs->mbmaxlen) :
                                  pstr->length();
      if (!(query_str= alloc_root(thd->mem_root, query_len+1)))
      {
        res= -1;
        break;        // EOM (error should be reported by allocator)
      }

      if (need_conversion)
      {
        uint dummy_errors;
        query_len= copy_and_convert(query_str, query_len, to_cs,
                                    pstr->ptr(), pstr->length(),
                                    pstr->charset(), &dummy_errors);
      }
      else
        memcpy(query_str, pstr->ptr(), pstr->length());
      query_str[query_len]= 0;
    }
    else
    {
      query_str= lex->prepared_stmt_code.str;
      query_len= lex->prepared_stmt_code.length;
      DBUG_PRINT("info", ("PREPARE: %.*s FROM '%.*s' \n",
                          lex->prepared_stmt_name.length,
                          lex->prepared_stmt_name.str,
                          query_len, query_str));
    }
    thd->command= COM_PREPARE;
    if (!mysql_stmt_prepare(thd, query_str, query_len + 1,
                            &lex->prepared_stmt_name))
      send_ok(thd, 0L, 0L, "Statement prepared");
    break;
  }
  case SQLCOM_EXECUTE:
  {
    DBUG_PRINT("info", ("EXECUTE: %.*s\n",
                        lex->prepared_stmt_name.length,
                        lex->prepared_stmt_name.str));
    mysql_sql_stmt_execute(thd, &lex->prepared_stmt_name);
    lex->prepared_stmt_params.empty();
    break;
  }
  case SQLCOM_DEALLOCATE_PREPARE:
  {
    Statement* stmt;
    DBUG_PRINT("info", ("DEALLOCATE PREPARE: %.*s\n", 
                        lex->prepared_stmt_name.length,
                        lex->prepared_stmt_name.str));
    /* We account deallocate in the same manner as mysql_stmt_close */
    statistic_increment(com_stmt_close, &LOCK_status);
    if ((stmt= thd->stmt_map.find_by_name(&lex->prepared_stmt_name)))
    {
      thd->stmt_map.erase(stmt);
      send_ok(thd);
    }
    else
    {
      res= -1;
      my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0),
               lex->prepared_stmt_name.length, lex->prepared_stmt_name.str,
               "DEALLOCATE PREPARE");
    }
    break;
  }
  case SQLCOM_DO:
    if (tables && ((res= check_table_access(thd, SELECT_ACL, tables,0)) ||
		   (res= open_and_lock_tables(thd,tables))))
	break;

    res= mysql_do(thd, *lex->insert_list);
    if (thd->net.report_error)
      res= -1;
    break;

  case SQLCOM_EMPTY_QUERY:
    send_ok(thd);
    break;

  case SQLCOM_HELP:
    res= mysqld_help(thd,lex->help_arg);
    break;

#ifndef EMBEDDED_LIBRARY
  case SQLCOM_PURGE:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    // PURGE MASTER LOGS TO 'file'
    res = purge_master_logs(thd, lex->to_log);
    break;
  }
  case SQLCOM_PURGE_BEFORE:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    // PURGE MASTER LOGS BEFORE 'data'
    res = purge_master_logs_before_date(thd, lex->purge_time);
    break;
  }
#endif
  case SQLCOM_SHOW_WARNS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      ((1L << (uint) MYSQL_ERROR::WARN_LEVEL_NOTE) |
			       (1L << (uint) MYSQL_ERROR::WARN_LEVEL_WARN) |
			       (1L << (uint) MYSQL_ERROR::WARN_LEVEL_ERROR)
			       ));
    break;
  }
  case SQLCOM_SHOW_ERRORS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      (1L << (uint) MYSQL_ERROR::WARN_LEVEL_ERROR));
    break;
  }
  case SQLCOM_SHOW_NEW_MASTER:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    /* This query don't work now. See comment in repl_failsafe.cc */
#ifndef WORKING_NEW_MASTER
    net_printf(thd, ER_NOT_SUPPORTED_YET, "SHOW NEW MASTER");
    res= 1;
#else
    res = show_new_master(thd);
#endif
    break;
  }

#ifdef HAVE_REPLICATION
  case SQLCOM_SHOW_SLAVE_HOSTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res = show_slave_hosts(thd);
    break;
  }
  case SQLCOM_SHOW_BINLOG_EVENTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res = show_binlog_events(thd);
    break;
  }
#endif

  case SQLCOM_BACKUP_TABLE:
  {
    if (check_db_used(thd,tables) ||
	check_table_access(thd,SELECT_ACL, tables,0) ||
	check_global_access(thd, FILE_ACL))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_backup_table(thd, tables);

    break;
  }
  case SQLCOM_RESTORE_TABLE:
  {
    if (check_db_used(thd,tables) ||
	check_table_access(thd, INSERT_ACL, tables,0) ||
	check_global_access(thd, FILE_ACL))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_restore_table(thd, tables);
    break;
  }
  case SQLCOM_ASSIGN_TO_KEYCACHE:
  {
    if (check_db_used(thd, tables) ||
        check_access(thd, INDEX_ACL, tables->db,
                     &tables->grant.privilege, 0, 0))
      goto error;
    res= mysql_assign_to_keycache(thd, tables, &lex->name_and_length);
    break;
  }
  case SQLCOM_PRELOAD_KEYS:
  {
    if (check_db_used(thd, tables) ||
	check_access(thd, INDEX_ACL, tables->db,
                     &tables->grant.privilege, 0, 0))
      goto error;
    res = mysql_preload_keys(thd, tables);
    break;
  }
#ifdef HAVE_REPLICATION
  case SQLCOM_CHANGE_MASTER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    pthread_mutex_lock(&LOCK_active_mi);
    res = change_master(thd,active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_SLAVE_STAT:
  {
    /* Accept one of two privileges */
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    pthread_mutex_lock(&LOCK_active_mi);
    res = show_master_info(thd,active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_MASTER_STAT:
  {
    /* Accept one of two privileges */
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    res = show_binlog_info(thd);
    break;
  }

  case SQLCOM_LOAD_MASTER_DATA: // sync with master
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    if (end_active_trans(thd))
      res= -1;
    else
      res = load_master_data(thd);
    break;
#endif /* HAVE_REPLICATION */
#ifdef HAVE_INNOBASE_DB
  case SQLCOM_SHOW_INNODB_STATUS:
    {
      if (check_global_access(thd, SUPER_ACL))
	goto error;
      res = innodb_show_status(thd);
      break;
    }
#endif
#ifdef HAVE_REPLICATION
  case SQLCOM_LOAD_MASTER_TABLE:
  {
    if (!tables->db)
      tables->db=thd->db;
    if (check_access(thd,CREATE_ACL,tables->db,&tables->grant.privilege,0,0))
      goto error;				/* purecov: inspected */
    if (grant_option)
    {
      /* Check that the first table has CREATE privilege */
      if (check_grant(thd, CREATE_ACL, tables, 0, 1, 0))
	goto error;
    }
    if (strlen(tables->real_name) > NAME_LEN)
    {
      net_printf(thd,ER_WRONG_TABLE_NAME, tables->real_name);
      break;
    }
    pthread_mutex_lock(&LOCK_active_mi);
    /*
      fetch_master_table will send the error to the client on failure.
      Give error if the table already exists.
    */
    if (!fetch_master_table(thd, tables->db, tables->real_name,
			    active_mi, 0, 0))
    {
      send_ok(thd);
    }
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
#endif /* HAVE_REPLICATION */

  case SQLCOM_CREATE_TABLE:
  {
    /* If CREATE TABLE of non-temporary table, do implicit commit */
    if (!(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE))
    {
      if (end_active_trans(thd))
      {
	res= -1;
	break;
      }
    }
    else 
    {
      /* So that CREATE TEMPORARY TABLE gets to binlog at commit/rollback */
      thd->options|= OPTION_STATUS_NO_TRANS_UPDATE;
    }
    /* Skip first table, which is the table we are creating */
    TABLE_LIST *create_table, *create_table_local;
    tables= lex->unlink_first_table(tables, &create_table,
				    &create_table_local);

    if ((res= create_table_precheck(thd, tables, create_table)))
      goto unsent_create_error;

#ifndef HAVE_READLINK
    lex->create_info.data_file_name=lex->create_info.index_file_name=0;
#else
    /* Fix names if symlinked tables */
    if (append_file_to_dir(thd, &lex->create_info.data_file_name,
			   create_table->real_name) ||
	append_file_to_dir(thd,&lex->create_info.index_file_name,
			   create_table->real_name))
    {
      res=-1;
      goto unsent_create_error;
    }
#endif
    /*
      If we are using SET CHARSET without DEFAULT, add an implicite
      DEFAULT to not confuse old users. (This may change).
    */
    if ((lex->create_info.used_fields & 
	 (HA_CREATE_USED_DEFAULT_CHARSET | HA_CREATE_USED_CHARSET)) ==
	HA_CREATE_USED_CHARSET)
    {
      lex->create_info.used_fields&= ~HA_CREATE_USED_CHARSET;
      lex->create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
      lex->create_info.default_table_charset= lex->create_info.table_charset;
      lex->create_info.table_charset= 0;
    }
    /*
      The create-select command will open and read-lock the select table
      and then create, open and write-lock the new table. If a global
      read lock steps in, we get a deadlock. The write lock waits for
      the global read lock, while the global read lock waits for the
      select table to be closed. So we wait until the global readlock is
      gone before starting both steps. Note that
      wait_if_global_read_lock() sets a protection against a new global
      read lock when it succeeds. This needs to be released by
      start_waiting_global_read_lock(). We protect the normal CREATE
      TABLE in the same way. That way we avoid that a new table is
      created during a gobal read lock.
    */
    if (wait_if_global_read_lock(thd, 0, 1))
    {
      res= -1;
      goto unsent_create_error;
    }
    if (select_lex->item_list.elements)		// With select
    {
      select_result *result;

      select_lex->options|= SELECT_NO_UNLOCK;
      unit->offset_limit_cnt= select_lex->offset_limit;
      unit->select_limit_cnt= select_lex->select_limit+
	select_lex->offset_limit;
      if (unit->select_limit_cnt < select_lex->select_limit)
	unit->select_limit_cnt= HA_POS_ERROR;	// No limit

      if (!(res=open_and_lock_tables(thd,tables)))
      {
	res= -1;				// If error
        if ((result=new select_create(create_table->db,
                                      create_table->real_name,
				      &lex->create_info,
                                      lex->create_list,
                                      lex->key_list,
                                      select_lex->item_list, lex->duplicates,
                                      lex->ignore)))
        {
          /*
            CREATE from SELECT give its SELECT_LEX for SELECT,
            and item_list belong to SELECT
          */
          select_lex->resolve_mode= SELECT_LEX::SELECT_MODE;
          res=handle_select(thd, lex, result);
          select_lex->resolve_mode= SELECT_LEX::NOMATTER_MODE;
        }
	//reset for PS
	lex->create_list.empty();
	lex->key_list.empty();
      }
    }
    else // regular create
    {
      if (lex->name)
        res= mysql_create_like_table(thd, create_table, &lex->create_info, 
                                     (Table_ident *)lex->name); 
      else
      {
        res= mysql_create_table(thd,create_table->db,
			         create_table->real_name, &lex->create_info,
			         lex->create_list,
			         lex->key_list,0,0);
      }
      if (!res)
	send_ok(thd);
    }
    /*
      Release the protection against the global read lock and wake
      everyone, who might want to set a global read lock.
    */
    start_waiting_global_read_lock(thd);

unsent_create_error:
    // put tables back for PS rexecuting
    tables= lex->link_first_table_back(tables, create_table,
				       create_table_local);
    break;
  }
  case SQLCOM_CREATE_INDEX:
    if (check_one_table_access(thd, INDEX_ACL, tables))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    if (end_active_trans(thd))
      res= -1;
    else
      res = mysql_create_index(thd, tables, lex->key_list);
    break;

#ifdef HAVE_REPLICATION
  case SQLCOM_SLAVE_START:
  {
    pthread_mutex_lock(&LOCK_active_mi);
    start_slave(thd,active_mi,1 /* net report*/);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SLAVE_STOP:
  /*
    If the client thread has locked tables, a deadlock is possible.
    Assume that
    - the client thread does LOCK TABLE t READ.
    - then the master updates t.
    - then the SQL slave thread wants to update t,
      so it waits for the client thread because t is locked by it.
    - then the client thread does SLAVE STOP.
      SLAVE STOP waits for the SQL slave thread to terminate its
      update t, which waits for the client thread because t is locked by it.
    To prevent that, refuse SLAVE STOP if the
    client thread has locked tables
  */
  if (thd->locked_tables || thd->active_transaction())
  {
    send_error(thd,ER_LOCK_OR_ACTIVE_TRANSACTION);
    break;
  }
  {
    pthread_mutex_lock(&LOCK_active_mi);
    stop_slave(thd,active_mi,1/* net report*/);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
#endif /* HAVE_REPLICATION */

  case SQLCOM_ALTER_TABLE:
#if defined(DONT_ALLOW_SHOW_COMMANDS)
    send_error(thd,ER_NOT_ALLOWED_COMMAND); /* purecov: inspected */
    break;
#else
    {
      ulong priv=0;
      if (lex->name && (!lex->name[0] || strlen(lex->name) > NAME_LEN))
      {
	net_printf(thd, ER_WRONG_TABLE_NAME, lex->name);
	res= 1;
	break;
      }
      if (!select_lex->db)
      {
        /*
          In the case of ALTER TABLE ... RENAME we should supply the
          default database if the new name is not explicitly qualified
          by a database. (Bug #11493)
        */
        if (lex->alter_info.flags & ALTER_RENAME)
        {
          if (! thd->db)
          {
            send_error(thd,ER_NO_DB_ERROR);
            goto error;
          }
          select_lex->db= thd->db;
        }
        else
          select_lex->db=tables->db;
      }
      if (check_access(thd,ALTER_ACL,tables->db,&tables->grant.privilege,0,0) ||
	  check_access(thd,INSERT_ACL | CREATE_ACL,select_lex->db,&priv,0,0)||
	  check_merge_table_access(thd, tables->db,
				   (TABLE_LIST *)
				   lex->create_info.merge_list.first))
	goto error;				/* purecov: inspected */
      if (grant_option)
      {
	if (check_grant(thd, ALTER_ACL, tables, 0, UINT_MAX, 0))
	  goto error;
	if (lex->name && !test_all_bits(priv,INSERT_ACL | CREATE_ACL))
	{					// Rename of table
	  TABLE_LIST tmp_table;
	  bzero((char*) &tmp_table,sizeof(tmp_table));
	  tmp_table.real_name=lex->name;
	  tmp_table.db=select_lex->db;
	  tmp_table.grant.privilege=priv;
	  if (check_grant(thd, INSERT_ACL | CREATE_ACL, &tmp_table, 0,
			  UINT_MAX, 0))
	    goto error;
	}
      }
      /* Don't yet allow changing of symlinks with ALTER TABLE */
      lex->create_info.data_file_name=lex->create_info.index_file_name=0;
      /* ALTER TABLE ends previous transaction */
      if (end_active_trans(thd))
	res= -1;
      else
      {
        thd->enable_slow_log= opt_log_slow_admin_statements;
	res= mysql_alter_table(thd, select_lex->db, lex->name,
			       &lex->create_info,
			       tables, lex->create_list,
			       lex->key_list,
			       select_lex->order_list.elements,
                               (ORDER *) select_lex->order_list.first,
			       lex->duplicates, lex->ignore, &lex->alter_info);
      }
      break;
    }
#endif /*DONT_ALLOW_SHOW_COMMANDS*/
  case SQLCOM_RENAME_TABLE:
  {
    TABLE_LIST *table;
    if (check_db_used(thd,tables))
      goto error;
    for (table=tables ; table ; table=table->next->next)
    {
      if (check_access(thd, ALTER_ACL | DROP_ACL, table->db,
		       &table->grant.privilege,0,0) ||
	  check_access(thd, INSERT_ACL | CREATE_ACL, table->next->db,
		       &table->next->grant.privilege,0,0))
	goto error;
      if (grant_option)
      {
	TABLE_LIST old_list,new_list;
	/*
	  we do not need initialize old_list and new_list because we will
	  come table[0] and table->next[0] there
	*/
	old_list=table[0];
	new_list=table->next[0];
	old_list.next=new_list.next=0;
	if (check_grant(thd, ALTER_ACL, &old_list, 0, UINT_MAX, 0) ||
	    (!test_all_bits(table->next->grant.privilege,
			    INSERT_ACL | CREATE_ACL) &&
	     check_grant(thd, INSERT_ACL | CREATE_ACL, &new_list, 0,
			 UINT_MAX, 0)))
	  goto error;
      }
    }
    query_cache_invalidate3(thd, tables, 0);
    if (end_active_trans(thd))
      res= -1;
    else if (mysql_rename_tables(thd,tables))
      res= -1;
    break;
  }
#ifndef EMBEDDED_LIBRARY
  case SQLCOM_SHOW_BINLOGS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(thd,ER_NOT_ALLOWED_COMMAND); /* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      if (check_global_access(thd, SUPER_ACL))
	goto error;
      res = show_binlogs(thd);
      break;
    }
#endif
#endif /* EMBEDDED_LIBRARY */
  case SQLCOM_SHOW_CREATE:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(thd,ER_NOT_ALLOWED_COMMAND); /* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      if (check_db_used(thd, tables) ||
	  check_access(thd, SELECT_ACL | EXTRA_ACL, tables->db,
		       &tables->grant.privilege,0,0))
	goto error;
      if (grant_option && check_grant(thd, SELECT_ACL, tables, 2, UINT_MAX, 0))
	goto error;
      res= mysqld_show_create(thd, tables);
      break;
    }
#endif
  case SQLCOM_CHECKSUM:
  {
    if (check_db_used(thd,tables) ||
	check_table_access(thd, SELECT_ACL | EXTRA_ACL , tables,0))
      goto error; /* purecov: inspected */
    res = mysql_checksum_table(thd, tables, &lex->check_opt);
    break;
  }
  case SQLCOM_REPAIR:
  {
    if (check_db_used(thd,tables) ||
	check_table_access(thd,SELECT_ACL | INSERT_ACL, tables,0))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_repair_table(thd, tables, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      mysql_update_log.write(thd, thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
	thd->clear_error(); // No binlog error generated
        Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
        mysql_bin_log.write(&qinfo);
      }
    }
    break;
  }
  case SQLCOM_CHECK:
  {
    if (check_db_used(thd,tables) ||
	check_table_access(thd, SELECT_ACL | EXTRA_ACL , tables,0))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_check_table(thd, tables, &lex->check_opt);
    break;
  }
  case SQLCOM_ANALYZE:
  {
    if (check_db_used(thd,tables) ||
	check_table_access(thd,SELECT_ACL | INSERT_ACL, tables,0))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_analyze_table(thd, tables, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      mysql_update_log.write(thd, thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
	thd->clear_error(); // No binlog error generated
        Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
        mysql_bin_log.write(&qinfo);
      }
    }
    break;
  }

  case SQLCOM_OPTIMIZE:
  {
    if (check_db_used(thd,tables) ||
	check_table_access(thd,SELECT_ACL | INSERT_ACL, tables,0))
      goto error; /* purecov: inspected */
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res= (specialflag & (SPECIAL_SAFE_MODE | SPECIAL_NO_NEW_FUNC)) ?
      mysql_recreate_table(thd, tables, 1) :
      mysql_optimize_table(thd, tables, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      mysql_update_log.write(thd, thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
	thd->clear_error(); // No binlog error generated
        Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
        mysql_bin_log.write(&qinfo);
      }
    }
    break;
  }
  case SQLCOM_UPDATE:
    if (update_precheck(thd, tables))
      break;
    res= mysql_update(thd,tables,
                      select_lex->item_list,
                      lex->value_list,
                      select_lex->where,
		      select_lex->order_list.elements,
                      (ORDER *) select_lex->order_list.first,
                      select_lex->select_limit,
                      lex->duplicates, lex->ignore);
    if (thd->net.report_error)
      res= -1;
    break;
  case SQLCOM_UPDATE_MULTI:
  {
    if ((res= multi_update_precheck(thd, tables)))
      break;
    res= mysql_multi_update(thd,tables,
			    &select_lex->item_list,
			    &lex->value_list,
			    select_lex->where,
			    select_lex->options,
			    lex->duplicates, lex->ignore, unit, select_lex);
    break;
  }
  case SQLCOM_REPLACE:
  case SQLCOM_INSERT:
  {
    if ((res= insert_precheck(thd, tables)))
      break;
    res= mysql_insert(thd,tables,lex->field_list,lex->many_values,
                      lex->update_list, lex->value_list,
                      lex->duplicates, lex->ignore);
    if (thd->net.report_error)
      res= -1;
    break;
  }
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  {
    TABLE_LIST *first_local_table= (TABLE_LIST *) select_lex->table_list.first;
    TABLE_LIST dup_tables;
    TABLE *insert_table;
    if ((res= insert_precheck(thd, tables)))
      break;

    /* Fix lock for first table */
    if (tables->lock_type == TL_WRITE_DELAYED)
      tables->lock_type= TL_WRITE;

    /* Don't unlock tables until command is written to binary log */
    select_lex->options|= SELECT_NO_UNLOCK;

    select_result *result;
    unit->offset_limit_cnt= select_lex->offset_limit;
    unit->select_limit_cnt= select_lex->select_limit+select_lex->offset_limit;
    if (unit->select_limit_cnt < select_lex->select_limit)
      unit->select_limit_cnt= HA_POS_ERROR;		// No limit

    if (find_real_table_in_list(tables->next, tables->db, tables->real_name))
    {
      /* Using same table for INSERT and SELECT */
      select_lex->options |= OPTION_BUFFER_RESULT;
    }

    if ((res= open_and_lock_tables(thd, tables)))
      break;
      
    insert_table= tables->table;
    /* Skip first table, which is the table we are inserting in */
    select_lex->table_list.first= (byte*) first_local_table->next;
    tables= (TABLE_LIST *) select_lex->table_list.first;
    dup_tables= *first_local_table;
    first_local_table->next= 0;
    if (select_lex->group_list.elements != 0)
    {
      /*
        When we are using GROUP BY we can't refere to other tables in the
        ON DUPLICATE KEY part
      */         
      dup_tables.next= 0;
    }

    if (!(res= mysql_prepare_insert(thd, tables, first_local_table,
				    &dup_tables, insert_table,
                                    lex->field_list, 0,
				    lex->update_list, lex->value_list,
				    lex->duplicates)) &&
        (result= new select_insert(insert_table, first_local_table,
				   &dup_tables, &lex->field_list,
				   &lex->update_list, &lex->value_list,
                                   lex->duplicates, lex->ignore)))
    {
      /*
        insert/replace from SELECT give its SELECT_LEX for SELECT,
        and item_list belong to SELECT
      */
      lex->select_lex.resolve_mode= SELECT_LEX::SELECT_MODE;
      res= handle_select(thd, lex, result);
      /* revert changes for SP */
      lex->select_lex.resolve_mode= SELECT_LEX::INSERT_MODE;
      delete result;
      if (thd->net.report_error)
        res= -1;
    }
    else
      res= -1;
    insert_table->insert_values= 0;        // Set by mysql_prepare_insert()
    first_local_table->next= tables;
    lex->select_lex.table_list.first= (byte*) first_local_table;
    break;
  }
  case SQLCOM_TRUNCATE:
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    if (check_one_table_access(thd, DELETE_ACL, tables))
      goto error;
    /*
      Don't allow this within a transaction because we want to use
      re-generate table
    */
    if (thd->locked_tables || thd->active_transaction())
    {
      send_error(thd,ER_LOCK_OR_ACTIVE_TRANSACTION,NullS);
      goto error;
    }
    res=mysql_truncate(thd, tables, 0);
    break;
  case SQLCOM_DELETE:
  {
    if ((res= delete_precheck(thd, tables)))
      break;
    res = mysql_delete(thd,tables, select_lex->where,
                       &select_lex->order_list,
                       select_lex->select_limit, select_lex->options);
    if (thd->net.report_error)
      res= -1;
    break;
  }
  case SQLCOM_DELETE_MULTI:
  {
    TABLE_LIST *aux_tables=
      (TABLE_LIST *)thd->lex->auxilliary_table_list.first;
    TABLE_LIST *target_tbl;
    uint table_count;
    multi_delete *result;

    if ((res= multi_delete_precheck(thd, tables, &table_count)))
      break;

    /* condition will be TRUE on SP re-excuting */
    if (select_lex->item_list.elements != 0)
      select_lex->item_list.empty();
    if (add_item_to_list(thd, new Item_null()))
    {
      res= -1;
      break;
    }

    thd->proc_info="init";
    if ((res=open_and_lock_tables(thd,tables)))
      break;
    /* Fix tables-to-be-deleted-from list to point at opened tables */
    for (target_tbl= (TABLE_LIST*) aux_tables;
	 target_tbl;
	 target_tbl= target_tbl->next)
    {
      TABLE_LIST *orig= target_tbl->table_list;
      target_tbl->table= orig->table;
      /*
	 Multi-delete can't be constructed over-union => we always have
	 single SELECT on top and have to check underlying SELECTs of it
      */
      if (lex->select_lex.check_updateable_in_subqueries(orig->db,
                                                         orig->real_name))
      {
        my_error(ER_UPDATE_TABLE_USED, MYF(0),
                 orig->real_name);
        res= -1;
        break;
      }
    }

    if (!thd->is_fatal_error && (result= new multi_delete(thd,aux_tables,
							  table_count)))
    {
      res= mysql_select(thd, &select_lex->ref_pointer_array,
			select_lex->get_table_list(),
			select_lex->with_wild,
			select_lex->item_list,
			select_lex->where,
			0, (ORDER *)NULL, (ORDER *)NULL, (Item *)NULL,
			(ORDER *)NULL,
			select_lex->options | thd->options |
			SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK,
			result, unit, select_lex);
      if (thd->net.report_error)
	res= -1;
      delete result;
    }
    else
      res= -1;					// Error is not sent
    close_thread_tables(thd);
    break;
  }
  case SQLCOM_DROP_TABLE:
  {
    if (!lex->drop_temporary)
    {
      if (check_table_access(thd,DROP_ACL,tables,0))
	goto error;				/* purecov: inspected */
      if (end_active_trans(thd))
      {
	res= -1;
	break;
      }
    }
    else
    {
      /*
	If this is a slave thread, we may sometimes execute some 
	DROP / * 40005 TEMPORARY * / TABLE
	that come from parts of binlogs (likely if we use RESET SLAVE or CHANGE
	MASTER TO), while the temporary table has already been dropped.
	To not generate such irrelevant "table does not exist errors",
	we silently add IF EXISTS if TEMPORARY was used.
      */
      if (thd->slave_thread)
	lex->drop_if_exists= 1;

      /* So that DROP TEMPORARY TABLE gets to binlog at commit/rollback */
      thd->options|= OPTION_STATUS_NO_TRANS_UPDATE;
    }
    res= mysql_rm_table(thd,tables,lex->drop_if_exists, lex->drop_temporary);
  }
  break;
  case SQLCOM_DROP_INDEX:
    if (check_one_table_access(thd, INDEX_ACL, tables))
      goto error;				/* purecov: inspected */
    if (end_active_trans(thd))
      res= -1;
    else
      res = mysql_drop_index(thd, tables, &lex->alter_info);
    break;
  case SQLCOM_SHOW_DATABASES:
#if defined(DONT_ALLOW_SHOW_COMMANDS)
    send_error(thd,ER_NOT_ALLOWED_COMMAND);   /* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    if ((specialflag & SPECIAL_SKIP_SHOW_DB) &&
	check_global_access(thd, SHOW_DB_ACL))
      goto error;
    res= mysqld_show_dbs(thd, (lex->wild ? lex->wild->ptr() : NullS));
    break;
#endif
  case SQLCOM_SHOW_PROCESSLIST:
    if (!thd->priv_user[0] && check_global_access(thd,PROCESS_ACL))
      break;
    mysqld_list_processes(thd,
			  thd->master_access & PROCESS_ACL ? NullS :
			  thd->priv_user,lex->verbose);
    break;
  case SQLCOM_SHOW_STORAGE_ENGINES:
    res= mysqld_show_storage_engines(thd);
    break;
  case SQLCOM_SHOW_PRIVILEGES:
    res= mysqld_show_privileges(thd);
    break;
  case SQLCOM_SHOW_COLUMN_TYPES:
    res= mysqld_show_column_types(thd);
    break;
  case SQLCOM_SHOW_STATUS:
    res= mysqld_show(thd,(lex->wild ? lex->wild->ptr() : NullS),status_vars,
		     OPT_GLOBAL, &LOCK_status);
    break;
  case SQLCOM_SHOW_VARIABLES:
    res= mysqld_show(thd, (lex->wild ? lex->wild->ptr() : NullS),
		     init_vars, lex->option_type,
		     &LOCK_global_system_variables);
    break;
  case SQLCOM_SHOW_LOGS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(thd,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      if (grant_option && check_access(thd, FILE_ACL, any_db,0,0,0))
	goto error;
      res= mysqld_show_logs(thd);
      break;
    }
#endif
  case SQLCOM_SHOW_TABLES:
    /* FALL THROUGH */
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(thd,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      char *db=select_lex->db ? select_lex->db : thd->db;
      if (!db)
      {
	send_error(thd,ER_NO_DB_ERROR);	/* purecov: inspected */
	goto error;				/* purecov: inspected */
      }
      remove_escape(db);				// Fix escaped '_'
      if (check_db_name(db))
      {
        net_printf(thd,ER_WRONG_DB_NAME, db);
        goto error;
      }
      if (check_access(thd,SELECT_ACL,db,&thd->col_access,0,0))
	goto error;				/* purecov: inspected */
      if (!thd->col_access && check_grant_db(thd,db))
      {
	net_printf(thd, ER_DBACCESS_DENIED_ERROR,
		   thd->priv_user,
		   thd->priv_host,
		   db);
	goto error;
      }
      /* grant is checked in mysqld_show_tables */
      if (lex->describe)
        res= mysqld_extend_show_tables(thd,db,
				       (lex->wild ? lex->wild->ptr() : NullS));
      else
	res= mysqld_show_tables(thd,db,
				(lex->wild ? lex->wild->ptr() : NullS));
      break;
    }
#endif
  case SQLCOM_SHOW_OPEN_TABLES:
    res= mysqld_show_open_tables(thd,(lex->wild ? lex->wild->ptr() : NullS));
    break;
  case SQLCOM_SHOW_CHARSETS:
    res= mysqld_show_charsets(thd,(lex->wild ? lex->wild->ptr() : NullS));
    break;
  case SQLCOM_SHOW_COLLATIONS:
    res= mysqld_show_collations(thd,(lex->wild ? lex->wild->ptr() : NullS));
    break;
  case SQLCOM_SHOW_FIELDS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(thd,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      char *db=tables->db;
      remove_escape(db);			// Fix escaped '_'
      remove_escape(tables->real_name);
      if (check_access(thd,SELECT_ACL | EXTRA_ACL,db,
		       &tables->grant.privilege, 0, 0))
	goto error;				/* purecov: inspected */
      if (grant_option && check_grant(thd, SELECT_ACL, tables, 2, UINT_MAX, 0))
	goto error;
      res= mysqld_show_fields(thd,tables,
			      (lex->wild ? lex->wild->ptr() : NullS),
			      lex->verbose);
      break;
    }
#endif
  case SQLCOM_SHOW_KEYS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(thd,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      char *db=tables->db;
      remove_escape(db);			// Fix escaped '_'
      remove_escape(tables->real_name);
      if (check_access(thd,SELECT_ACL | EXTRA_ACL,db,
		       &tables->grant.privilege, 0, 0))
	goto error;				/* purecov: inspected */
      if (grant_option && check_grant(thd, SELECT_ACL, tables, 2, UINT_MAX, 0))
	goto error;
      res= mysqld_show_keys(thd,tables);
      break;
    }
#endif
  case SQLCOM_CHANGE_DB:
    mysql_change_db(thd,select_lex->db);
    break;

  case SQLCOM_LOAD:
  {
    uint privilege= (lex->duplicates == DUP_REPLACE ?
		     INSERT_ACL | DELETE_ACL : INSERT_ACL);

    if (!lex->local_file)
    {
      if (check_access(thd,privilege | FILE_ACL,tables->db,0,0,0))
	goto error;
    }
    else
    {
      if (!(thd->client_capabilities & CLIENT_LOCAL_FILES) ||
	  ! opt_local_infile)
      {
	send_error(thd,ER_NOT_ALLOWED_COMMAND);
	goto error;
      }
      if (check_one_table_access(thd, privilege, tables))
	goto error;
    }
    res=mysql_load(thd, lex->exchange, tables, lex->field_list,
		   lex->duplicates, lex->ignore, (bool) lex->local_file, lex->lock_option);
    break;
  }

  case SQLCOM_SET_OPTION:
  {
    List<set_var_base> *lex_var_list= &lex->var_list;
    if (tables && ((res= check_table_access(thd, SELECT_ACL, tables,0)) ||
		   (res= open_and_lock_tables(thd,tables))))
      break;
    if (lex->one_shot_set && not_all_support_one_shot(lex_var_list))
    {
      my_printf_error(0, "The SET ONE_SHOT syntax is reserved for \
purposes internal to the MySQL server", MYF(0));
      res= -1;
      break;
    }
    if (!(res= sql_set_variables(thd, lex_var_list)))
    {
      /*
        If the previous command was a SET ONE_SHOT, we don't want to forget
        about the ONE_SHOT property of that SET. So we use a |= instead of = .
      */
      thd->one_shot_set|= lex->one_shot_set;
      send_ok(thd);
    }
    if (thd->net.report_error)
      res= -1;
    break;
  }

  case SQLCOM_UNLOCK_TABLES:
    /*
      It is critical for mysqldump --single-transaction --master-data that
      UNLOCK TABLES does not implicitely commit a connection which has only
      done FLUSH TABLES WITH READ LOCK + BEGIN. If this assumption becomes
      false, mysqldump will not work.
    */
    unlock_locked_tables(thd);
    if (thd->options & OPTION_TABLE_LOCK)
    {
      end_active_trans(thd);
      thd->options&= ~(ulong) (OPTION_TABLE_LOCK);
    }
    if (thd->global_read_lock)
      unlock_global_read_lock(thd);
    send_ok(thd);
    break;
  case SQLCOM_LOCK_TABLES:
    unlock_locked_tables(thd);
    if (check_db_used(thd,tables) || end_active_trans(thd))
      goto error;
    if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, tables,0))
      goto error;
    thd->in_lock_tables=1;
    thd->options|= OPTION_TABLE_LOCK;
    if (!(res= open_and_lock_tables(thd, tables)))
    {
#ifdef HAVE_QUERY_CACHE
      if (thd->variables.query_cache_wlock_invalidate)
	query_cache.invalidate_locked_for_write(tables);
#endif /*HAVE_QUERY_CACHE*/
      thd->locked_tables=thd->lock;
      thd->lock=0;
      send_ok(thd);
    }
    else
      thd->options&= ~(ulong) (OPTION_TABLE_LOCK);
    thd->in_lock_tables=0;
    break;
  case SQLCOM_CREATE_DB:
  {
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    char *alias;
    if (!(alias=thd->strdup(lex->name)) || check_db_name(lex->name))
    {
      net_printf(thd,ER_WRONG_DB_NAME, lex->name);
      break;
    }
    /*
      If in a slave thread :
      CREATE DATABASE DB was certainly not preceded by USE DB.
      For that reason, db_ok() in sql/slave.cc did not check the 
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (thd->slave_thread && 
	(!db_ok(lex->name, replicate_do_db, replicate_ignore_db) ||
	 !db_ok_with_wild_table(lex->name)))
    {
      my_error(ER_SLAVE_IGNORED_TABLE, MYF(0));
      reset_one_shot_variables(thd);
      break;
    }
#endif
    if (check_access(thd,CREATE_ACL,lex->name,0,1,0))
      break;
    res= mysql_create_db(thd,(lower_case_table_names == 2 ? alias : lex->name),
			 &lex->create_info, 0);
    break;
  }
  case SQLCOM_DROP_DB:
  {
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    char *alias;
    if (!(alias=thd->strdup(lex->name)) || check_db_name(lex->name))
    {
      net_printf(thd, ER_WRONG_DB_NAME, lex->name);
      break;
    }
    /*
      If in a slave thread :
      DROP DATABASE DB may not be preceded by USE DB.
      For that reason, maybe db_ok() in sql/slave.cc did not check the 
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (thd->slave_thread && 
	(!db_ok(lex->name, replicate_do_db, replicate_ignore_db) ||
	 !db_ok_with_wild_table(lex->name)))
    {
      my_error(ER_SLAVE_IGNORED_TABLE, MYF(0));
      reset_one_shot_variables(thd);
      break;
    }
#endif
    if (check_access(thd,DROP_ACL,lex->name,0,1,0))
      break;
    if (thd->locked_tables || thd->active_transaction())
    {
      send_error(thd,ER_LOCK_OR_ACTIVE_TRANSACTION);
      goto error;
    }
    res=mysql_rm_db(thd, (lower_case_table_names == 2 ? alias : lex->name),
                    lex->drop_if_exists, 0);
    break;
  }
  case SQLCOM_ALTER_DB:
  {
    char *db= lex->name ? lex->name : thd->db;
    if (!db)
    {
      send_error(thd, ER_NO_DB_ERROR);
      goto error;
    }
    if (!strip_sp(db) || check_db_name(db))
    {
      net_printf(thd, ER_WRONG_DB_NAME, db);
      break;
    }
    /*
      If in a slave thread :
      ALTER DATABASE DB may not be preceded by USE DB.
      For that reason, maybe db_ok() in sql/slave.cc did not check the
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
#ifdef HAVE_REPLICATION
    if (thd->slave_thread &&
	(!db_ok(db, replicate_do_db, replicate_ignore_db) ||
	 !db_ok_with_wild_table(db)))
    {
      my_error(ER_SLAVE_IGNORED_TABLE, MYF(0));
      reset_one_shot_variables(thd);
      break;
    }
#endif
    if (check_access(thd, ALTER_ACL, db, 0, 1, 0))
      break;
    if (thd->locked_tables || thd->active_transaction())
    {
      send_error(thd,ER_LOCK_OR_ACTIVE_TRANSACTION);
      goto error;
    }
    res= mysql_alter_db(thd, db, &lex->create_info);
    break;
  }
  case SQLCOM_SHOW_CREATE_DB:
  {
    if (!strip_sp(lex->name) || check_db_name(lex->name))
    {
      net_printf(thd,ER_WRONG_DB_NAME, lex->name);
      break;
    }
    if (check_access(thd,SELECT_ACL,lex->name,0,1,0))
      break;
    res=mysqld_show_create_db(thd,lex->name,&lex->create_info);
    break;
  }
  case SQLCOM_CREATE_FUNCTION:
    if (check_access(thd,INSERT_ACL,"mysql",0,1,0))
      break;
#ifdef HAVE_DLOPEN
    if (!(res = mysql_create_function(thd,&lex->udf)))
      send_ok(thd);
#else
    net_printf(thd, ER_CANT_OPEN_LIBRARY, lex->udf.dl, 0, "feature disabled");
    res= -1;
#endif
    break;
  case SQLCOM_DROP_FUNCTION:
    if (check_access(thd,DELETE_ACL,"mysql",0,1,0))
      break;
#ifdef HAVE_DLOPEN
    if (!(res = mysql_drop_function(thd,&lex->udf.name)))
      send_ok(thd);
#else
    res= -1;
#endif
    break;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  case SQLCOM_DROP_USER:
  {
    if (check_access(thd, GRANT_ACL,"mysql",0,1,0))
      break;
    if (!(res= mysql_drop_user(thd, lex->users_list)))
    {
      mysql_update_log.write(thd, thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
	Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
	mysql_bin_log.write(&qinfo);
      }
      send_ok(thd);
    }
    break;
  }
  case SQLCOM_REVOKE_ALL:
  {
    if (check_access(thd, GRANT_ACL ,"mysql",0,1,0))
      break;
    if (!(res = mysql_revoke_all(thd, lex->users_list)))
    {
      mysql_update_log.write(thd, thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
	Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
	mysql_bin_log.write(&qinfo);
      }
      send_ok(thd);
    }
    break;
  }
  case SQLCOM_REVOKE:
  case SQLCOM_GRANT:
  {
    if (check_access(thd, lex->grant | lex->grant_tot_col | GRANT_ACL,
		     tables ? tables->db : select_lex->db,
		     tables ? &tables->grant.privilege : 0,
		     tables ? 0 : 1, 0))
      goto error;

    /*
      Check that the user isn't trying to change a password for another
      user if he doesn't have UPDATE privilege to the MySQL database
    */

    if (thd->user)				// If not replication
    {
      LEX_USER *user;
      List_iterator <LEX_USER> user_list(lex->users_list);
      while ((user=user_list++))
      {
	if (user->password.str &&
	    (strcmp(thd->user,user->user.str) ||
	     user->host.str &&
	     my_strcasecmp(&my_charset_latin1,
                           user->host.str, thd->host_or_ip)))
	{
	  if (check_access(thd, UPDATE_ACL, "mysql", 0, 1, 1))
	  {
	    send_error(thd, ER_PASSWORD_NOT_ALLOWED);
	    goto error;
	  }
	  break;		  // We are allowed to do global changes
	}
      }
    }
    if (specialflag & SPECIAL_NO_RESOLVE)
    {
      LEX_USER *user;
      List_iterator <LEX_USER> user_list(lex->users_list);
      while ((user=user_list++))
      {
	if (hostname_requires_resolving(user->host.str))
	  push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			      ER_WARN_HOSTNAME_WONT_WORK,
			      ER(ER_WARN_HOSTNAME_WONT_WORK),
			      user->host.str);
      }
    }
    if (tables)
    {
      if (grant_option && check_grant(thd,
				      (lex->grant | lex->grant_tot_col |
				       GRANT_ACL),
				      tables, 0, UINT_MAX, 0))
	goto error;
      if (!(res = mysql_table_grant(thd,tables,lex->users_list, lex->columns,
				    lex->grant,
				    lex->sql_command == SQLCOM_REVOKE)))
      {
	mysql_update_log.write(thd, thd->query, thd->query_length);
	if (mysql_bin_log.is_open())
	{
          thd->clear_error();
	  Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
	  mysql_bin_log.write(&qinfo);
	}
      }
    }
    else
    {
      if (lex->columns.elements)
      {
	send_error(thd,ER_ILLEGAL_GRANT_FOR_TABLE);
	res=1;
      }
      else
	res = mysql_grant(thd, select_lex->db, lex->users_list, lex->grant,
			  lex->sql_command == SQLCOM_REVOKE);
      if (!res)
      {
	mysql_update_log.write(thd, thd->query, thd->query_length);
	if (mysql_bin_log.is_open())
	{
          thd->clear_error();
	  Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
	  mysql_bin_log.write(&qinfo);
	}
	if (mqh_used && lex->sql_command == SQLCOM_GRANT)
	{
	  List_iterator <LEX_USER> str_list(lex->users_list);
	  LEX_USER *user;
	  while ((user=str_list++))
	    reset_mqh(thd,user);
	}
      }
    }
    break;
  }
#endif /*!NO_EMBEDDED_ACCESS_CHECKS*/
  case SQLCOM_RESET:
    /* 
       RESET commands are never written to the binary log, so we have to
       initialize this variable because RESET shares the same code as FLUSH
    */
    lex->no_write_to_binlog= 1;
  case SQLCOM_FLUSH:
  {
    if (check_global_access(thd,RELOAD_ACL) || check_db_used(thd, tables))
      goto error;
    /*
      reload_acl_and_cache() will tell us if we are allowed to write to the
      binlog or not.
    */
    bool write_to_binlog;
    if (reload_acl_and_cache(thd, lex->type, tables, &write_to_binlog))
      send_error(thd, 0);
    else
    {
      /*
        We WANT to write and we CAN write.
        ! we write after unlocking the table.
      */
      if (!lex->no_write_to_binlog && write_to_binlog)
      {
        mysql_update_log.write(thd, thd->query, thd->query_length);
        if (mysql_bin_log.is_open())
        {
          Query_log_event qinfo(thd, thd->query, thd->query_length, 0, FALSE);
          mysql_bin_log.write(&qinfo);
        }
      }
      send_ok(thd);
    }
    break;
  }
  case SQLCOM_KILL:
    kill_one_thread(thd,lex->thread_id);
    break;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  case SQLCOM_SHOW_GRANTS:
    res=0;
    if ((thd->priv_user &&
	 !strcmp(thd->priv_user,lex->grant_user->user.str)) ||
	!check_access(thd, SELECT_ACL, "mysql",0,1,0))
    {
      res = mysql_show_grants(thd,lex->grant_user);
    }
    break;
#endif
  case SQLCOM_HA_OPEN:
    if (check_db_used(thd,tables) ||
	check_table_access(thd,SELECT_ACL, tables,0))
      goto error;
    res = mysql_ha_open(thd, tables);
    break;
  case SQLCOM_HA_CLOSE:
    if (check_db_used(thd,tables))
      goto error;
    res = mysql_ha_close(thd, tables);
    break;
  case SQLCOM_HA_READ:
    /*
      There is no need to check for table permissions here, because
      if a user has no permissions to read a table, he won't be
      able to open it (with SQLCOM_HA_OPEN) in the first place.
    */
    if (check_db_used(thd,tables))
      goto error;
    res = mysql_ha_read(thd, tables, lex->ha_read_mode, lex->backup_dir,
			lex->insert_list, lex->ha_rkey_mode, select_lex->where,
			select_lex->select_limit, select_lex->offset_limit);
    break;

  case SQLCOM_BEGIN:
    if (thd->locked_tables)
    {
      thd->lock=thd->locked_tables;
      thd->locked_tables=0;			// Will be automaticly closed
      close_thread_tables(thd);			// Free tables
    }
    if (end_active_trans(thd))
    {
      res= -1;
    }
    else
    {
      thd->options= ((thd->options & (ulong) ~(OPTION_STATUS_NO_TRANS_UPDATE)) |
		     OPTION_BEGIN);
      thd->server_status|= SERVER_STATUS_IN_TRANS;
      if (!(lex->start_transaction_opt & MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT) ||
          !(res= ha_start_consistent_snapshot(thd)))
        send_ok(thd);
    }
    break;
  case SQLCOM_COMMIT:
    /*
      We don't use end_active_trans() here to ensure that this works
      even if there is a problem with the OPTION_AUTO_COMMIT flag
      (Which of course should never happen...)
    */
  {
    thd->options&= ~(ulong) (OPTION_BEGIN | OPTION_STATUS_NO_TRANS_UPDATE);
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (!ha_commit(thd))
    {
      send_ok(thd);
    }
    else
      res= -1;
    break;
  }
  case SQLCOM_ROLLBACK:
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (!ha_rollback(thd))
    {
      /*
        If a non-transactional table was updated, warn; don't warn if this is a
        slave thread (because when a slave thread executes a ROLLBACK, it has
        been read from the binary log, so it's 100% sure and normal to produce
        error ER_WARNING_NOT_COMPLETE_ROLLBACK. If we sent the warning to the
        slave SQL thread, it would not stop the thread but just be printed in
        the error log; but we don't want users to wonder why they have this
        message in the error log, so we don't send it.
      */
      if ((thd->options & OPTION_STATUS_NO_TRANS_UPDATE) && !thd->slave_thread)
	send_warning(thd,ER_WARNING_NOT_COMPLETE_ROLLBACK,0);
      else
	send_ok(thd);
    }
    else
      res= -1;
    thd->options&= ~(ulong) (OPTION_BEGIN | OPTION_STATUS_NO_TRANS_UPDATE);
    break;
  case SQLCOM_ROLLBACK_TO_SAVEPOINT:
    if (!ha_rollback_to_savepoint(thd, lex->savepoint_name))
    {
      if ((thd->options & OPTION_STATUS_NO_TRANS_UPDATE) && !thd->slave_thread)
	send_warning(thd, ER_WARNING_NOT_COMPLETE_ROLLBACK, 0);
      else
	send_ok(thd);
    }
    else
      res= -1;
    break;
  case SQLCOM_SAVEPOINT:
    if (!ha_savepoint(thd, lex->savepoint_name))
      send_ok(thd);
    else
      res= -1;
    break;
  default:					/* Impossible */
    send_ok(thd);
    break;
  }
  thd->proc_info="query end";			// QQ

  /*
    Reset system variables temporarily modified by SET ONE SHOT.

    Exception: If this is a SET, do nothing. This is to allow
    mysqlbinlog to print many SET commands (in this case we want the
    charset temp setting to live until the real query). This is also
    needed so that SET CHARACTER_SET_CLIENT... does not cancel itself
    immediately.
  */
  if (thd->one_shot_set && lex->sql_command != SQLCOM_SET_OPTION)
    reset_one_shot_variables(thd);

  if (res < 0)
    send_error(thd,thd->killed ? ER_SERVER_SHUTDOWN : 0);

error:
  if (unlikely(slave_fake_lock))
  {
    DBUG_PRINT("info",("undoing faked lock"));
    thd->lock= thd->locked_tables;
    thd->locked_tables= fake_prev_lock;
    if (thd->lock == thd->locked_tables)
      thd->lock= 0;
  }
  DBUG_VOID_RETURN;
}


/*
  Check grants for commands which work only with one table and all other
  tables belonging to subselects or implicitly opened tables.

  SYNOPSIS
    check_one_table_access()
    thd			Thread handler
    privilege		requested privelage
    tables		table list of command

  RETURN
    0 - OK
    1 - access denied, error is sent to client
*/

int check_one_table_access(THD *thd, ulong privilege, TABLE_LIST *tables)
{
  if (check_access(thd, privilege, tables->db, &tables->grant.privilege,0,0))
    return 1;

  /* Show only 1 table for check_grant */
  if (grant_option && check_grant(thd, privilege, tables, 0, 1, 0))
    return 1;

  /* Check rights on tables of subselects and implictly opened tables */
  TABLE_LIST *subselects_tables;
  if ((subselects_tables= tables->next))
  {
    if ((check_table_access(thd, SELECT_ACL, subselects_tables,0)))
      return 1;
  }
  return 0;
}


/****************************************************************************
  Get the user (global) and database privileges for all used tables

  NOTES
    The idea of EXTRA_ACL is that one will be granted access to the table if
    one has the asked privilege on any column combination of the table; For
    example to be able to check a table one needs to have SELECT privilege on
    any column of the table.

  RETURN
    0  ok
    1  If we can't get the privileges and we don't use table/column grants.

    save_priv	In this we store global and db level grants for the table
		Note that we don't store db level grants if the global grants
                is enough to satisfy the request and the global grants contains
                a SELECT grant.
****************************************************************************/

bool
check_access(THD *thd, ulong want_access, const char *db, ulong *save_priv,
	     bool dont_check_global_grants, bool no_errors)
{
  DBUG_ENTER("check_access");
  DBUG_PRINT("enter",("db: '%s'  want_access: %lu  master_access: %lu",
                      db ? db : "", want_access, thd->master_access));
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  ulong db_access;
  bool  db_is_pattern= test(want_access & GRANT_ACL);
#endif
  ulong dummy;
  if (save_priv)
    *save_priv=0;
  else
    save_priv= &dummy;

  if ((!db || !db[0]) && !thd->db && !dont_check_global_grants)
  {
    if (!no_errors)
      send_error(thd,ER_NO_DB_ERROR);	/* purecov: tested */
    DBUG_RETURN(TRUE);				/* purecov: tested */
  }

#ifdef NO_EMBEDDED_ACCESS_CHECKS
  DBUG_RETURN(0);
#else
  if ((thd->master_access & want_access) == want_access)
  {
    /*
      If we don't have a global SELECT privilege, we have to get the database
      specific access rights to be able to handle queries of type
      UPDATE t1 SET a=1 WHERE b > 0
    */
    db_access= thd->db_access;
    if (!(thd->master_access & SELECT_ACL) &&
	(db && (!thd->db || db_is_pattern || strcmp(db,thd->db))))
      db_access=acl_get(thd->host, thd->ip, thd->priv_user, db, db_is_pattern);
    *save_priv=thd->master_access | db_access;
    DBUG_RETURN(FALSE);
  }
  if (((want_access & ~thd->master_access) & ~(DB_ACLS | EXTRA_ACL)) ||
      ! db && dont_check_global_grants)
  {						// We can never grant this
    if (!no_errors)
      net_printf(thd,ER_ACCESS_DENIED_ERROR,
		 thd->priv_user,
		 thd->priv_host,
		 thd->password ? ER(ER_YES) : ER(ER_NO));/* purecov: tested */
    DBUG_RETURN(TRUE);				/* purecov: tested */
  }

  if (db == any_db)
    DBUG_RETURN(FALSE);				// Allow select on anything

  if (db && (!thd->db || db_is_pattern || strcmp(db,thd->db)))
    db_access=acl_get(thd->host, thd->ip, thd->priv_user, db, db_is_pattern);
  else
    db_access=thd->db_access;
  DBUG_PRINT("info",("db_access: %lu", db_access));
  /* Remove SHOW attribute and access rights we already have */
  want_access &= ~(thd->master_access | EXTRA_ACL);
  db_access= ((*save_priv=(db_access | thd->master_access)) & want_access);

  /* grant_option is set if there exists a single table or column grant */
  if (db_access == want_access ||
      ((grant_option && !dont_check_global_grants) &&
       !(want_access & ~(db_access | TABLE_ACLS))))
    DBUG_RETURN(FALSE);				/* Ok */
  if (!no_errors)
    net_printf(thd,ER_DBACCESS_DENIED_ERROR,
	       thd->priv_user,
	       thd->priv_host,
	       db ? db : thd->db ? thd->db : "unknown"); /* purecov: tested */
  DBUG_RETURN(TRUE);				/* purecov: tested */
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}


/*
  check for global access and give descriptive error message if it fails

  SYNOPSIS
    check_global_access()
    thd			Thread handler
    want_access		Use should have any of these global rights

  WARNING
    One gets access rigth if one has ANY of the rights in want_access
    This is useful as one in most cases only need one global right,
    but in some case we want to check if the user has SUPER or
    REPL_CLIENT_ACL rights.

  RETURN
    0	ok
    1	Access denied.  In this case an error is sent to the client
*/

bool check_global_access(THD *thd, ulong want_access)
{
#ifdef NO_EMBEDDED_ACCESS_CHECKS
  return 0;
#else
  char command[128];
  if ((thd->master_access & want_access))
    return 0;
  get_privilege_desc(command, sizeof(command), want_access);
  net_printf(thd,ER_SPECIFIC_ACCESS_DENIED_ERROR,
	     command);
  return 1;
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}


/*
  Check the privilege for all used tables.  Table privileges are cached
  in the table list for GRANT checking
*/

bool
check_table_access(THD *thd, ulong want_access,TABLE_LIST *tables,
		   bool no_errors)
{
  uint found=0;
  ulong found_access=0;
  TABLE_LIST *org_tables=tables;
  for (; tables ; tables=tables->next)
  {
    if (tables->derived ||
        (tables->table && (int)tables->table->tmp_table) ||
        my_tz_check_n_skip_implicit_tables(&tables,
                                           thd->lex->time_zone_tables_used))
      continue;
    if ((thd->master_access & want_access) == (want_access & ~EXTRA_ACL) &&
	thd->db)
      tables->grant.privilege= want_access;
    else if (tables->db && tables->db == thd->db)
    {
      if (found && !grant_option)		// db already checked
	tables->grant.privilege=found_access;
      else
      {
	if (check_access(thd,want_access,tables->db,&tables->grant.privilege,
			 0, no_errors))
	  return TRUE;				// Access denied
	found_access=tables->grant.privilege;
	found=1;
      }
    }
    else if (check_access(thd,want_access,tables->db,&tables->grant.privilege,
			  0, no_errors))
      return TRUE;
  }
  if (grant_option)
    return check_grant(thd,want_access & ~EXTRA_ACL,org_tables,
		       test(want_access & EXTRA_ACL), UINT_MAX, no_errors);
  return FALSE;
}

bool check_merge_table_access(THD *thd, char *db,
			      TABLE_LIST *table_list)
{
  int error=0;
  if (table_list)
  {
    /* Check that all tables use the current database */
    TABLE_LIST *tmp;
    for (tmp=table_list; tmp ; tmp=tmp->next)
    {
      if (!tmp->db || !tmp->db[0])
	tmp->db=db;
    }
    error=check_table_access(thd, SELECT_ACL | UPDATE_ACL | DELETE_ACL,
			     table_list,0);
  }
  return error;
}


static bool check_db_used(THD *thd,TABLE_LIST *tables)
{
  for (; tables ; tables=tables->next)
  {
    if (!tables->db)
    {
      if (!(tables->db=thd->db))
      {
	send_error(thd,ER_NO_DB_ERROR);	/* purecov: tested */
	return TRUE;				/* purecov: tested */
      }
    }
  }
  return FALSE;
}

/****************************************************************************
	Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/

#if STACK_DIRECTION < 0
#define used_stack(A,B) (long) (A - B)
#else
#define used_stack(A,B) (long) (B - A)
#endif

#ifndef DBUG_OFF
long max_stack_used;
#endif

#ifndef EMBEDDED_LIBRARY
bool check_stack_overrun(THD *thd,char *buf __attribute__((unused)))
{
  long stack_used;
  if ((stack_used=used_stack(thd->thread_stack,(char*) &stack_used)) >=
      (long) thread_stack_min)
  {
    sprintf(errbuff[0],ER(ER_STACK_OVERRUN),stack_used,thread_stack);
    my_message(ER_STACK_OVERRUN,errbuff[0],MYF(0));
    thd->fatal_error();
    return 1;
  }
#ifndef DBUG_OFF
  max_stack_used= max(max_stack_used, stack_used);
#endif
  return 0;
}
#endif /* EMBEDDED_LIBRARY */

#define MY_YACC_INIT 1000			// Start with big alloc
#define MY_YACC_MAX  32000			// Because of 'short'

bool my_yyoverflow(short **yyss, YYSTYPE **yyvs, ulong *yystacksize)
{
  LEX	*lex=current_lex;
  ulong old_info=0;
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
  Initialize global thd variables needed for query
****************************************************************************/

void
mysql_init_query(THD *thd, uchar *buf, uint length)
{
  DBUG_ENTER("mysql_init_query");
  lex_start(thd, buf, length);
  mysql_reset_thd_for_next_command(thd);
  DBUG_VOID_RETURN;
}


/*
 Reset THD part responsible for command processing state.

 DESCRIPTION
   This needs to be called before execution of every statement
   (prepared or conventional).

 TODO
   Make it a method of THD and align its name with the rest of
   reset/end/start/init methods.
   Call it after we use THD for queries, not before.
*/

void mysql_reset_thd_for_next_command(THD *thd)
{
  DBUG_ENTER("mysql_reset_thd_for_next_command");
  thd->free_list= 0;
  thd->select_number= 1;
  thd->total_warn_count= 0;                     // Warnings for this query
  thd->last_insert_id_used= thd->query_start_used= thd->insert_id_used=0;
  thd->sent_row_count= thd->examined_row_count= 0;
  thd->is_fatal_error= thd->rand_used= thd->time_zone_used= 0;
  thd->server_status&= ~ (SERVER_MORE_RESULTS_EXISTS | 
			  SERVER_QUERY_NO_INDEX_USED |
			  SERVER_QUERY_NO_GOOD_INDEX_USED);
  thd->tmp_table_used= 0;
  if (opt_bin_log)
    reset_dynamic(&thd->user_var_events);
  thd->clear_error();
  DBUG_VOID_RETURN;
}


void
mysql_init_select(LEX *lex)
{
  SELECT_LEX *select_lex= lex->current_select;
  select_lex->init_select();
  select_lex->select_limit= HA_POS_ERROR;
  if (select_lex == &lex->select_lex)
  {
    DBUG_ASSERT(lex->result == 0);
    lex->exchange= 0;
  }
}


bool
mysql_new_select(LEX *lex, bool move_down)
{
  SELECT_LEX *select_lex;
  if (!(select_lex= new(lex->thd->mem_root) SELECT_LEX()))
    return 1;
  select_lex->select_number= ++lex->thd->select_number;
  select_lex->init_query();
  select_lex->init_select();
  /*
    Don't evaluate this subquery during statement prepare even if
    it's a constant one. The flag is switched off in the end of
    mysql_stmt_prepare.
  */
  if (lex->thd->current_arena->is_stmt_prepare())
    select_lex->uncacheable|= UNCACHEABLE_PREPARE;

  if (move_down)
  {
    lex->subqueries= TRUE;
    /* first select_lex of subselect or derived table */
    SELECT_LEX_UNIT *unit;
    if (!(unit= new(lex->thd->mem_root) SELECT_LEX_UNIT()))
      return 1;

    unit->init_query();
    unit->init_select();
    unit->thd= lex->thd;
    unit->include_down(lex->current_select);
    unit->link_next= 0;
    unit->link_prev= 0;
    unit->return_to= lex->current_select;
    select_lex->include_down(unit);
    // TODO: assign resolve_mode for fake subquery after merging with new tree
  }
  else
  {
    select_lex->include_neighbour(lex->current_select);
    SELECT_LEX_UNIT *unit= select_lex->master_unit();
    SELECT_LEX *fake= unit->fake_select_lex;
    if (!fake)
    {
      /*
	as far as we included SELECT_LEX for UNION unit should have
	fake SELECT_LEX for UNION processing
      */
      if (!(fake= unit->fake_select_lex= new(lex->thd->mem_root) SELECT_LEX()))
        return 1;
      fake->include_standalone(unit,
			       (SELECT_LEX_NODE**)&unit->fake_select_lex);
      fake->select_number= INT_MAX;
      fake->make_empty_select();
      fake->linkage= GLOBAL_OPTIONS_TYPE;
      fake->select_limit= HA_POS_ERROR;
    }
  }

  select_lex->master_unit()->global_parameters= select_lex;
  select_lex->include_global((st_select_lex_node**)&lex->all_selects_list);
  lex->current_select= select_lex;
  select_lex->resolve_mode= SELECT_LEX::SELECT_MODE;
  return 0;
}

/*
  Create a select to return the same output as 'SELECT @@var_name'.

  SYNOPSIS
    create_select_for_variable()
    var_name		Variable name

  DESCRIPTION
    Used for SHOW COUNT(*) [ WARNINGS | ERROR]

    This will crash with a core dump if the variable doesn't exists
*/

void create_select_for_variable(const char *var_name)
{
  THD *thd;
  LEX *lex;
  LEX_STRING tmp, null_lex_string;
  Item *var;
  char buff[MAX_SYS_VAR_LENGTH*2+4+8], *end;
  DBUG_ENTER("create_select_for_variable");

  thd= current_thd;
  lex= thd->lex;
  mysql_init_select(lex);
  lex->sql_command= SQLCOM_SELECT;
  tmp.str= (char*) var_name;
  tmp.length=strlen(var_name);
  bzero((char*) &null_lex_string.str, sizeof(null_lex_string));
  /*
    We set the name of Item to @@session.var_name because that then is used
    as the column name in the output.
  */
  if ((var= get_system_var(thd, OPT_SESSION, tmp, null_lex_string)))
  {
    end= strxmov(buff, "@@session.", var_name, NullS);
    var->set_name(buff, end-buff, system_charset_info);
    add_item_to_list(thd, var);
  }
  DBUG_VOID_RETURN;
}

static TABLE_LIST* get_table_by_alias(TABLE_LIST* tl, const char* db,
  const char* alias)
{
  for (;tl;tl= tl->next)
  {
    if (!strcmp(db,tl->db) &&
        tl->alias && !my_strcasecmp(table_alias_charset,tl->alias,alias))
      return tl;
  }
  
  return 0;
}     

/* Sets up lex->auxilliary_table_list */
void fix_multi_delete_lex(LEX* lex)
{
  TABLE_LIST *tl;
  TABLE_LIST *good_list= (TABLE_LIST*)lex->select_lex.table_list.first;
  
  for (tl= (TABLE_LIST*)lex->auxilliary_table_list.first; tl; tl= tl->next)
  {
    TABLE_LIST* good_table= get_table_by_alias(good_list,tl->db,tl->alias);
    if (good_table && !good_table->derived)
    {
      /* 
          real_name points to a member of Table_ident which is
          allocated via thd->strmake() from THD memroot 
       */
      tl->real_name= good_table->real_name;
      tl->real_name_length= good_table->real_name_length;
      good_table->updating= tl->updating;
    }
  }
}  

void mysql_init_multi_delete(LEX *lex)
{
  lex->sql_command=  SQLCOM_DELETE_MULTI;
  mysql_init_select(lex);
  lex->select_lex.select_limit= lex->unit.select_limit_cnt=
    HA_POS_ERROR;
  lex->select_lex.table_list.save_and_clear(&lex->auxilliary_table_list);
  lex->lock_option= using_update_log ? TL_READ_NO_INSERT : TL_READ;
}


/*
  When you modify mysql_parse(), you may need to mofify
  mysql_test_parse_for_slave() in this same file.
*/

void mysql_parse(THD *thd, char *inBuf, uint length)
{
  DBUG_ENTER("mysql_parse");

  mysql_init_query(thd, (uchar*) inBuf, length);
  if (query_cache_send_result_to_client(thd, inBuf, length) <= 0)
  {
    LEX *lex= thd->lex;
    if (!yyparse((void *)thd) && ! thd->is_fatal_error)
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      if (mqh_used && thd->user_connect &&
	  check_mqh(thd, lex->sql_command))
      {
	thd->net.error = 0;
      }
      else
#endif
      {
	if (thd->net.report_error)
	  send_error(thd, 0, NullS);
	else
	{
          /*
            Binlog logs a string starting from thd->query and having length
            thd->query_length; so we set thd->query_length correctly (to not
            log several statements in one event, when we executed only first).
            We set it to not see the ';' (otherwise it would get into binlog
            and Query_log_event::print() would give ';;' output).
            This also helps display only the current query in SHOW
            PROCESSLIST.
            Note that we don't need LOCK_thread_count to modify query_length.
          */
          if (lex->found_colon &&
              (thd->query_length= (ulong)(lex->found_colon - thd->query)))
            thd->query_length--;
          /* Actually execute the query */
	  mysql_execute_command(thd);
	  query_cache_end_of_result(thd);
	}
      }
    }
    else
    {
      DBUG_PRINT("info",("Command aborted. Fatal_error: %d",
			 thd->is_fatal_error));
      query_cache_abort(&thd->net);
    }
    thd->proc_info="freeing items";
    thd->end_statement();
    DBUG_ASSERT(thd->change_list.is_empty());
  }
  DBUG_VOID_RETURN;
}


#ifdef HAVE_REPLICATION
/*
  Usable by the replication SQL thread only: just parse a query to know if it
  can be ignored because of replicate-*-table rules.

  RETURN VALUES
    0	cannot be ignored
    1	can be ignored
*/

bool mysql_test_parse_for_slave(THD *thd, char *inBuf, uint length)
{
  LEX *lex= thd->lex;
  bool error= 0;

  mysql_init_query(thd, (uchar*) inBuf, length);
  if (!yyparse((void*) thd) && ! thd->is_fatal_error &&
      all_tables_not_ok(thd,(TABLE_LIST*) lex->select_lex.table_list.first))
    error= 1;                /* Ignore question */
  thd->end_statement();
  return error;
}
#endif


/*****************************************************************************
** Store field definition for create
** Return 0 if ok
******************************************************************************/

bool add_field_to_list(THD *thd, char *field_name, enum_field_types type,
		       char *length, char *decimals,
		       uint type_modifier,
		       Item *default_value, Item *on_update_value,
                       LEX_STRING *comment,
		       char *change,
                       List<String> *interval_list, CHARSET_INFO *cs,
		       uint uint_geom_type)
{
  register create_field *new_field;
  LEX  *lex= thd->lex;
  uint allowed_type_modifier=0;
  char warn_buff[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("add_field_to_list");

  if (strlen(field_name) > NAME_LEN)
  {
    net_printf(thd, ER_TOO_LONG_IDENT, field_name); /* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if (type_modifier & PRI_KEY_FLAG)
  {
    lex->col_list.push_back(new key_part_spec(field_name,0));
    lex->key_list.push_back(new Key(Key::PRIMARY, NullS, HA_KEY_ALG_UNDEF,
				    0, lex->col_list));
    lex->col_list.empty();
  }
  if (type_modifier & (UNIQUE_FLAG | UNIQUE_KEY_FLAG))
  {
    lex->col_list.push_back(new key_part_spec(field_name,0));
    lex->key_list.push_back(new Key(Key::UNIQUE, NullS, HA_KEY_ALG_UNDEF, 0,
				    lex->col_list));
    lex->col_list.empty();
  }

  if (default_value)
  {
    /* 
      Default value should be literal => basic constants =>
      no need fix_fields()
      
      We allow only one function as part of default value - 
      NOW() as default for TIMESTAMP type.
    */
    if (default_value->type() == Item::FUNC_ITEM && 
        !(((Item_func*)default_value)->functype() == Item_func::NOW_FUNC &&
         type == FIELD_TYPE_TIMESTAMP))
    {
      net_printf(thd, ER_INVALID_DEFAULT, field_name);
      DBUG_RETURN(1);
    }
    else if (default_value->type() == Item::NULL_ITEM)
    {
      default_value= 0;
      if ((type_modifier & (NOT_NULL_FLAG | AUTO_INCREMENT_FLAG)) ==
	  NOT_NULL_FLAG)
      {
	net_printf(thd,ER_INVALID_DEFAULT,field_name);
	DBUG_RETURN(1);
      }
    }
    else if (type_modifier & AUTO_INCREMENT_FLAG)
    {
      net_printf(thd, ER_INVALID_DEFAULT, field_name);
      DBUG_RETURN(1);
    }
  }

  if (on_update_value && type != FIELD_TYPE_TIMESTAMP)
  {
    net_printf(thd, ER_INVALID_ON_UPDATE, field_name);
    DBUG_RETURN(1);
  }
    
  if (!(new_field=new create_field()))
    DBUG_RETURN(1);
  new_field->field=0;
  new_field->field_name=field_name;
  new_field->def= default_value;
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
  new_field->charset=cs;
  new_field->geom_type= (Field::geometry_type) uint_geom_type;

  if (!comment)
  {
    new_field->comment.str=0;
    new_field->comment.length=0;
  }
  else
  {
    /* In this case comment is always of type Item_string */
    new_field->comment.str=   (char*) comment->str;
    new_field->comment.length=comment->length;
  }
  if (length && !(new_field->length= (uint) atoi(length)))
    length=0; /* purecov: inspected */
  uint sign_len=type_modifier & UNSIGNED_FLAG ? 0 : 1;

  if (new_field->length && new_field->decimals &&
      new_field->length < new_field->decimals+1 &&
      new_field->decimals != NOT_FIXED_DEC)
    new_field->length=new_field->decimals+1; /* purecov: inspected */

  switch (type) {
  case FIELD_TYPE_TINY:
    if (!length) new_field->length=MAX_TINYINT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_SHORT:
    if (!length) new_field->length=MAX_SMALLINT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_INT24:
    if (!length) new_field->length=MAX_MEDIUMINT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_LONG:
    if (!length) new_field->length=MAX_INT_WIDTH+sign_len;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_LONGLONG:
    if (!length) new_field->length=MAX_BIGINT_WIDTH;
    allowed_type_modifier= AUTO_INCREMENT_FLAG;
    break;
  case FIELD_TYPE_NULL:
    break;
  case FIELD_TYPE_DECIMAL:
    if (!length)
    {
      if ((new_field->length= new_field->decimals))
        new_field->length++;
      else
        new_field->length= 10;                  // Default length for DECIMAL
    }
    if (new_field->length < MAX_FIELD_WIDTH)	// Skip wrong argument
    {
      new_field->length+=sign_len;
      if (new_field->decimals)
	new_field->length++;
    }
    break;
  case FIELD_TYPE_STRING:
  case FIELD_TYPE_VAR_STRING:
    if (new_field->length <= MAX_FIELD_CHARLENGTH || default_value)
      break;
    /* Convert long CHAR() and VARCHAR columns to TEXT or BLOB */
    new_field->sql_type= FIELD_TYPE_BLOB;
    sprintf(warn_buff, ER(ER_AUTO_CONVERT), field_name, "CHAR",
	    (cs == &my_charset_bin) ? "BLOB" : "TEXT");
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, ER_AUTO_CONVERT,
		 warn_buff);
    /* fall through */
  case FIELD_TYPE_BLOB:
  case FIELD_TYPE_TINY_BLOB:
  case FIELD_TYPE_LONG_BLOB:
  case FIELD_TYPE_MEDIUM_BLOB:
  case FIELD_TYPE_GEOMETRY:
    if (new_field->length)
    {
      /* The user has given a length to the blob column */
      if (new_field->length < 256)
	type= FIELD_TYPE_TINY_BLOB;
      else if (new_field->length < 65536)
	type= FIELD_TYPE_BLOB;
      else if (new_field->length < 256L*256L*256L)
	type= FIELD_TYPE_MEDIUM_BLOB;
      else
	type= FIELD_TYPE_LONG_BLOB;
      new_field->length= 0;
    }
    new_field->sql_type= type;
    if (default_value)				// Allow empty as default value
    {
      String str,*res;
      res=default_value->val_str(&str);
      if (res->length())
      {
	net_printf(thd,ER_BLOB_CANT_HAVE_DEFAULT,field_name); /* purecov: inspected */
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
	net_printf(thd,ER_WRONG_FIELD_SPEC,field_name);
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
    else if (new_field->length != 19)
    {
      /*
        We support only even TIMESTAMP lengths less or equal than 14
        and 19 as length of 4.1 compatible representation.
      */
      new_field->length=((new_field->length+1)/2)*2; /* purecov: inspected */
      new_field->length= min(new_field->length,14); /* purecov: inspected */
    }
    new_field->flags|= ZEROFILL_FLAG | UNSIGNED_FLAG;
    if (default_value)
    {
      /* Grammar allows only NOW() value for ON UPDATE clause */
      if (default_value->type() == Item::FUNC_ITEM && 
          ((Item_func*)default_value)->functype() == Item_func::NOW_FUNC)
      {
        new_field->unireg_check= (on_update_value?Field::TIMESTAMP_DNUN_FIELD:
                                                  Field::TIMESTAMP_DN_FIELD);
        /*
          We don't need default value any longer moreover it is dangerous.
          Everything handled by unireg_check further.
        */
        new_field->def= 0;
      }
      else
        new_field->unireg_check= (on_update_value?Field::TIMESTAMP_UN_FIELD:
                                                  Field::NONE);
    }
    else
    {
      /*
        If we have default TIMESTAMP NOT NULL column without explicit DEFAULT
        or ON UPDATE values then for the sake of compatiblity we should treat
        this column as having DEFAULT NOW() ON UPDATE NOW() (when we don't
        have another TIMESTAMP column with auto-set option before this one)
        or DEFAULT 0 (in other cases).
        So here we are setting TIMESTAMP_OLD_FIELD only temporary, and will
        replace this value by TIMESTAMP_DNUN_FIELD or NONE later when
        information about all TIMESTAMP fields in table will be availiable.

        If we have TIMESTAMP NULL column without explicit DEFAULT value
        we treat it as having DEFAULT NULL attribute.
      */
      new_field->unireg_check= on_update_value ?
                               Field::TIMESTAMP_UN_FIELD :
                               (new_field->flags & NOT_NULL_FLAG ?
                                Field::TIMESTAMP_OLD_FIELD:
                                Field::NONE);
    }
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
      if (interval_list->elements > sizeof(longlong)*8)
      {
        net_printf(thd,ER_TOO_BIG_SET,field_name); /* purecov: inspected */
        DBUG_RETURN(1);				/* purecov: inspected */
      }
      new_field->pack_length= get_set_pack_length(interval_list->elements);

      List_iterator<String> it(*interval_list);
      String *tmp;
      while ((tmp= it++))
        new_field->interval_list.push_back(tmp);
      /*
        Set fake length to 1 to pass the below conditions.
        Real length will be set in mysql_prepare_table()
        when we know the character set of the column
      */
      new_field->length= 1;
    }
    break;
  case FIELD_TYPE_ENUM:
    {
      // Should be safe
      new_field->pack_length= get_enum_pack_length(interval_list->elements);

      List_iterator<String> it(*interval_list);
      String *tmp;
      while ((tmp= it++))
        new_field->interval_list.push_back(tmp);
      new_field->length= 1; // See comment for FIELD_TYPE_SET above.
    }
    break;
  }

  if ((new_field->length > MAX_FIELD_CHARLENGTH && type != FIELD_TYPE_SET && 
       type != FIELD_TYPE_ENUM) ||
      (!new_field->length && !(new_field->flags & BLOB_FLAG) &&
       type != FIELD_TYPE_STRING &&
       type != FIELD_TYPE_VAR_STRING && type != FIELD_TYPE_GEOMETRY))
  {
    net_printf(thd,ER_TOO_BIG_FIELDLENGTH,field_name,
	       MAX_FIELD_CHARLENGTH);		/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  type_modifier&= AUTO_INCREMENT_FLAG;
  if ((~allowed_type_modifier) & type_modifier)
  {
    net_printf(thd,ER_WRONG_FIELD_SPEC,field_name);
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
add_proc_to_list(THD* thd, Item *item)
{
  ORDER *order;
  Item	**item_ptr;

  if (!(order = (ORDER *) thd->alloc(sizeof(ORDER)+sizeof(Item*))))
    return 1;
  item_ptr = (Item**) (order+1);
  *item_ptr= item;
  order->item=item_ptr;
  order->free_me=0;
  thd->lex->proc_list.link_in_list((byte*) order,(byte**) &order->next);
  return 0;
}


/* Fix escaping of _, % and \ in database and table names (for ODBC) */

static void remove_escape(char *name)
{
  if (!*name)					// For empty DB names
    return;
  char *to;
#ifdef USE_MB
  char *strend=name+(uint) strlen(name);
#endif
  for (to=name; *name ; name++)
  {
#ifdef USE_MB
    int l;
/*    if ((l = ismbchar(name, name+MBMAXLEN))) { Wei He: I think it's wrong */
    if (use_mb(system_charset_info) &&
        (l = my_ismbchar(system_charset_info, name, strend)))
    {
	while (l--)
	    *to++ = *name++;
	name--;
	continue;
    }
#endif
    if (*name == '\\' && name[1])
      name++;					// Skip '\\'
    *to++= *name;
  }
  *to=0;
}

/****************************************************************************
** save order by and tables in own lists
****************************************************************************/


bool add_to_list(THD *thd, SQL_LIST &list,Item *item,bool asc)
{
  ORDER *order;
  DBUG_ENTER("add_to_list");
  if (!(order = (ORDER *) thd->alloc(sizeof(ORDER))))
    DBUG_RETURN(1);
  order->item_ptr= item;
  order->item= &order->item_ptr;
  order->asc = asc;
  order->free_me=0;
  order->used=0;
  list.link_in_list((byte*) order,(byte**) &order->next);
  DBUG_RETURN(0);
}


/*
  Add a table to list of used tables

  SYNOPSIS
    add_table_to_list()
    table		Table to add
    alias		alias for table (or null if no alias)
    table_options	A set of the following bits:
			TL_OPTION_UPDATING	Table will be updated
			TL_OPTION_FORCE_INDEX	Force usage of index
    lock_type		How table should be locked
    use_index		List of indexed used in USE INDEX
    ignore_index	List of indexed used in IGNORE INDEX

    RETURN
      0		Error
      #		Pointer to TABLE_LIST element added to the total table list
*/

TABLE_LIST *st_select_lex::add_table_to_list(THD *thd,
					     Table_ident *table,
					     LEX_STRING *alias,
					     ulong table_options,
					     thr_lock_type lock_type,
					     List<String> *use_index_arg,
					     List<String> *ignore_index_arg,
                                             LEX_STRING *option)
{
  register TABLE_LIST *ptr;
  char *alias_str;
  DBUG_ENTER("add_table_to_list");

  if (!table)
    DBUG_RETURN(0);				// End of memory
  alias_str= alias ? alias->str : table->table.str;
  if (check_table_name(table->table.str,table->table.length) ||
      table->db.str && check_db_name(table->db.str))
  {
    net_printf(thd, ER_WRONG_TABLE_NAME, table->table.str);
    DBUG_RETURN(0);
  }

  if (!alias)					/* Alias is case sensitive */
  {
    if (table->sel)
    {
      net_printf(thd,ER_DERIVED_MUST_HAVE_ALIAS);
      DBUG_RETURN(0);
    }
    if (!(alias_str=thd->memdup(alias_str,table->table.length+1)))
      DBUG_RETURN(0);
  }
  if (!(ptr = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(0);				/* purecov: inspected */
  if (table->db.str)
  {
    ptr->db= table->db.str;
    ptr->db_length= table->db.length;
  }
  else if (thd->db)
  {
    ptr->db= thd->db;
    ptr->db_length= thd->db_length;
  }
  else
  {
    /* The following can't be "" as we may do 'casedn_str()' on it */
    ptr->db= empty_c_string;
    ptr->db_length= 0;
  }
  if (thd->current_arena->is_stmt_prepare())
    ptr->db= thd->strdup(ptr->db);

  ptr->alias= alias_str;
  if (lower_case_table_names && table->table.length)
    my_casedn_str(files_charset_info, table->table.str);
  ptr->real_name=table->table.str;
  ptr->real_name_length=table->table.length;
  ptr->lock_type=   lock_type;
  ptr->updating=    test(table_options & TL_OPTION_UPDATING);
  ptr->force_index= test(table_options & TL_OPTION_FORCE_INDEX);
  ptr->ignore_leaves= test(table_options & TL_OPTION_IGNORE_LEAVES);
  ptr->derived=	    table->sel;
  ptr->cacheable_table= 1;
  if (use_index_arg)
    ptr->use_index=(List<String> *) thd->memdup((gptr) use_index_arg,
						sizeof(*use_index_arg));
  if (ignore_index_arg)
    ptr->ignore_index=(List<String> *) thd->memdup((gptr) ignore_index_arg,
						   sizeof(*ignore_index_arg));
  ptr->option= option ? option->str : 0;
  /* check that used name is unique */
  if (lock_type != TL_IGNORE)
  {
    for (TABLE_LIST *tables=(TABLE_LIST*) table_list.first ;
	 tables ;
	 tables=tables->next)
    {
      if (!my_strcasecmp(table_alias_charset, alias_str, tables->alias) &&
	  !strcmp(ptr->db, tables->db))
      {
	net_printf(thd,ER_NONUNIQ_TABLE,alias_str); /* purecov: tested */
	DBUG_RETURN(0);				/* purecov: tested */
      }
    }
  }
  table_list.link_in_list((byte*) ptr, (byte**) &ptr->next);
  DBUG_RETURN(ptr);
}


/*
  Set lock for all tables in current select level

  SYNOPSIS:
    set_lock_for_tables()
    lock_type			Lock to set for tables

  NOTE:
    If lock is a write lock, then tables->updating is set 1
    This is to get tables_ok to know that the table is updated by the
    query
*/

void st_select_lex::set_lock_for_tables(thr_lock_type lock_type)
{
  bool for_update= lock_type >= TL_READ_NO_INSERT;
  DBUG_ENTER("set_lock_for_tables");
  DBUG_PRINT("enter", ("lock_type: %d  for_update: %d", lock_type,
		       for_update));

  for (TABLE_LIST *tables= (TABLE_LIST*) table_list.first ;
       tables ;
       tables=tables->next)
  {
    tables->lock_type= lock_type;
    tables->updating=  for_update;
  }
  DBUG_VOID_RETURN;
}


void add_join_on(TABLE_LIST *b,Item *expr)
{
  if (expr)
  {
    if (!b->on_expr)
      b->on_expr=expr;
    else
    {
      // This only happens if you have both a right and left join
      b->on_expr=new Item_cond_and(b->on_expr,expr);
    }
    b->on_expr->top_level_item();
  }
}


/*
  Mark that we have a NATURAL JOIN between two tables

  SYNOPSIS
    add_join_natural()
    a			Table to do normal join with
    b			Do normal join with this table
  
  IMPLEMENTATION
    This function just marks that table b should be joined with a.
    The function setup_cond() will create in b->on_expr a list
    of equal condition between all fields of the same name.

    SELECT * FROM t1 NATURAL LEFT JOIN t2
     <=>
    SELECT * FROM t1 LEFT JOIN t2 ON (t1.i=t2.i and t1.j=t2.j ... )
*/

void add_join_natural(TABLE_LIST *a,TABLE_LIST *b)
{
  b->natural_join=a;
}

/*
  Reload/resets privileges and the different caches.

  SYNOPSIS
    reload_acl_and_cache()
    thd			Thread handler
    options             What should be reset/reloaded (tables, privileges,
    slave...)
    tables              Tables to flush (if any)
    write_to_binlog     Depending on 'options', it may be very bad to write the
                        query to the binlog (e.g. FLUSH SLAVE); this is a
                        pointer where, if it is not NULL, reload_acl_and_cache()
                        will put 0 if it thinks we really should not write to
                        the binlog. Otherwise it will put 1.

  RETURN
    0	 ok
    !=0  error
*/

bool reload_acl_and_cache(THD *thd, ulong options, TABLE_LIST *tables,
                          bool *write_to_binlog)
{
  bool result=0;
  select_errors=0;				/* Write if more errors */
  bool tmp_write_to_binlog= 1;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (options & REFRESH_GRANT)
  {
    THD *tmp_thd= 0;
    /*
      If reload_acl_and_cache() is called from SIGHUP handler we have to
      allocate temporary THD for execution of acl_reload()/grant_reload().
    */
    if (!thd && (thd= (tmp_thd= new THD)))
      thd->store_globals();
    if (thd)
    {
      (void)acl_reload(thd);
      (void)grant_reload(thd);
      if (mqh_used)
        reset_mqh(thd, (LEX_USER *) NULL, TRUE);
    }
    if (tmp_thd)
    {
      delete tmp_thd;
      /* Remember that we don't have a THD */
      my_pthread_setspecific_ptr(THR_THD,  0);
      thd= 0;
    }
  }
#endif
  if (options & REFRESH_LOG)
  {
    /*
      Flush the normal query log, the update log, the binary log,
      the slow query log, and the relay log (if it exists).
    */

    /*
      Writing this command to the binlog may result in infinite loops when
      doing mysqlbinlog|mysql, and anyway it does not really make sense to
      log it automatically (would cause more trouble to users than it would
      help them)
    */
    tmp_write_to_binlog= 0;
    mysql_log.new_file(1);
    mysql_update_log.new_file(1);
    mysql_bin_log.new_file(1);
    mysql_slow_log.new_file(1);
#ifdef HAVE_REPLICATION
    if (mysql_bin_log.is_open() && expire_logs_days)
    {
      long purge_time= time(0) - expire_logs_days*24*60*60;
      if (purge_time >= 0)
	mysql_bin_log.purge_logs_before_date(purge_time);
    }
    pthread_mutex_lock(&LOCK_active_mi);
    rotate_relay_log(active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);
#endif
    if (ha_flush_logs())
      result=1;
    if (flush_error_log())
      result=1;
  }
#ifdef HAVE_QUERY_CACHE
  if (options & REFRESH_QUERY_CACHE_FREE)
  {
    query_cache.pack();				// FLUSH QUERY CACHE
    options &= ~REFRESH_QUERY_CACHE; //don't flush all cache, just free memory
  }
  if (options & (REFRESH_TABLES | REFRESH_QUERY_CACHE))
  {
    query_cache.flush();			// RESET QUERY CACHE
  }
#endif /*HAVE_QUERY_CACHE*/
  /*
    Note that if REFRESH_READ_LOCK bit is set then REFRESH_TABLES is set too
    (see sql_yacc.yy)
  */
  if (options & (REFRESH_TABLES | REFRESH_READ_LOCK)) 
  {
    if ((options & REFRESH_READ_LOCK) && thd)
    {
      /*
        We must not try to aspire a global read lock if we have a write
        locked table. This would lead to a deadlock when trying to
        reopen (and re-lock) the table after the flush.
      */
      if (thd->locked_tables)
      {
        THR_LOCK_DATA **lock_p= thd->locked_tables->locks;
        THR_LOCK_DATA **end_p= lock_p + thd->locked_tables->lock_count;

        for (; lock_p < end_p; lock_p++)
        {
          if ((*lock_p)->type == TL_WRITE)
          {
            my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
            return 1;
          }
        }
      }
      /*
	Writing to the binlog could cause deadlocks, as we don't log
	UNLOCK TABLES
      */
      tmp_write_to_binlog= 0;
      if (lock_global_read_lock(thd))
	return 1;
      result=close_cached_tables(thd,(options & REFRESH_FAST) ? 0 : 1,
                                 tables);
      make_global_read_lock_block_commit(thd);
    }
    else
      result=close_cached_tables(thd,(options & REFRESH_FAST) ? 0 : 1, tables);
    my_dbopt_cleanup();
  }
  if (options & REFRESH_HOSTS)
    hostname_cache_refresh();
  if (options & REFRESH_STATUS)
    refresh_status();
  if (options & REFRESH_THREADS)
    flush_thread_cache();
#ifdef HAVE_REPLICATION
  if (options & REFRESH_MASTER)
  {
    tmp_write_to_binlog= 0;
    if (reset_master(thd))
      result=1;
  }
#endif
#ifdef OPENSSL
   if (options & REFRESH_DES_KEY_FILE)
   {
     if (des_key_file)
       result=load_des_key_file(des_key_file);
   }
#endif
#ifdef HAVE_REPLICATION
 if (options & REFRESH_SLAVE)
 {
   tmp_write_to_binlog= 0;
   pthread_mutex_lock(&LOCK_active_mi);
   if (reset_slave(thd, active_mi))
     result=1;
   pthread_mutex_unlock(&LOCK_active_mi);
 }
#endif
 if (options & REFRESH_USER_RESOURCES)
   reset_mqh(thd,(LEX_USER *) NULL);
 if (write_to_binlog)
   *write_to_binlog= tmp_write_to_binlog;
 return result;
}

/*
  kill on thread

  SYNOPSIS
    kill_one_thread()
    thd			Thread class
    id			Thread id

  NOTES
    This is written such that we have a short lock on LOCK_thread_count
*/

void kill_one_thread(THD *thd, ulong id)
{
  THD *tmp;
  uint error=ER_NO_SUCH_THREAD;
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    if (tmp->thread_id == id)
    {
      pthread_mutex_lock(&tmp->LOCK_delete);	// Lock from delete
      break;
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  if (tmp)
  {
    if ((thd->master_access & SUPER_ACL) ||
	!strcmp(thd->user,tmp->user))
    {
      tmp->awake(1 /*prepare to die*/);
      error=0;
    }
    else
      error=ER_KILL_DENIED_ERROR;
    pthread_mutex_unlock(&tmp->LOCK_delete);
  }

  if (!error)
    send_ok(thd);
  else
    net_printf(thd,error,id);
}


/* Clear most status variables */

static void refresh_status(void)
{
  pthread_mutex_lock(&LOCK_status);
  for (struct show_var_st *ptr=status_vars; ptr->name; ptr++)
  {
    if (ptr->type == SHOW_LONG)
      *(ulong*) ptr->value= 0;
  }
  /* Reset the counters of all key caches (default and named). */
  process_key_caches(reset_key_cache_counters);
  pthread_mutex_unlock(&LOCK_status);
}


	/* If pointer is not a null pointer, append filename to it */

static bool append_file_to_dir(THD *thd, const char **filename_ptr,
			       const char *table_name)
{
  char buff[FN_REFLEN],*ptr, *end;
  if (!*filename_ptr)
    return 0;					// nothing to do

  /* Check that the filename is not too long and it's a hard path */
  if (strlen(*filename_ptr)+strlen(table_name) >= FN_REFLEN-1 ||
      !test_if_hard_path(*filename_ptr))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), *filename_ptr);
    return 1;
  }
  /* Fix is using unix filename format on dos */
  strmov(buff,*filename_ptr);
  end=convert_dirname(buff, *filename_ptr, NullS);
  if (!(ptr=thd->alloc((uint) (end-buff)+(uint) strlen(table_name)+1)))
    return 1;					// End of memory
  *filename_ptr=ptr;
  strxmov(ptr,buff,table_name,NullS);
  return 0;
}


/*
  Check if the select is a simple select (not an union)

  SYNOPSIS
    check_simple_select()

  RETURN VALUES
    0	ok
    1	error	; In this case the error messege is sent to the client
*/

bool check_simple_select()
{
  THD *thd= current_thd;
  if (thd->lex->current_select != &thd->lex->select_lex)
  {
    char command[80];
    strmake(command, thd->lex->yylval->symbol.str,
	    min(thd->lex->yylval->symbol.length, sizeof(command)-1));
    net_printf(thd, ER_CANT_USE_OPTION_HERE, command);
    return 1;
  }
  return 0;
}

/*
  Setup locking for multi-table updates. Used by the replication slave.
  Replication slave SQL thread examines (all_tables_not_ok()) the
  locking state of referenced tables to determine if the query has to
  be executed or ignored. Since in multi-table update, the 
  'default' lock is read-only, this lock is corrected early enough by
  calling this function, before the slave decides to execute/ignore.

  SYNOPSIS
    check_multi_update_lock()
    thd		Current thread
    tables	List of user-supplied tables
    fields	List of fields requiring update

  RETURN VALUES
    0	ok
    1	error
*/
static bool check_multi_update_lock(THD *thd, TABLE_LIST *tables, 
				    List<Item> *fields, SELECT_LEX *select_lex)
{
  bool res= 1;
  TABLE_LIST *table;
  DBUG_ENTER("check_multi_update_lock");
  
  if (check_db_used(thd, tables))
    goto error;

  /*
    Ensure that we have UPDATE or SELECT privilege for each table
    The exact privilege is checked in mysql_multi_update()
  */
  for (table= tables ; table ; table= table->next)
  {
    TABLE_LIST *save= table->next;
    table->next= 0;
    if ((check_access(thd, UPDATE_ACL, table->db, &table->grant.privilege,0,1) ||
        (grant_option && check_grant(thd, UPDATE_ACL, table,0,1,1))) &&
	check_one_table_access(thd, SELECT_ACL, table))
	goto error;
    table->next= save;
  }
    
  if (mysql_multi_update_lock(thd, tables, fields, select_lex))
    goto error;
  
  res= 0;
  
error:
  DBUG_RETURN(res);
}


Comp_creator *comp_eq_creator(bool invert)
{
  return invert?(Comp_creator *)&ne_creator:(Comp_creator *)&eq_creator;
}


Comp_creator *comp_ge_creator(bool invert)
{
  return invert?(Comp_creator *)&lt_creator:(Comp_creator *)&ge_creator;
}


Comp_creator *comp_gt_creator(bool invert)
{
  return invert?(Comp_creator *)&le_creator:(Comp_creator *)&gt_creator;
}


Comp_creator *comp_le_creator(bool invert)
{
  return invert?(Comp_creator *)&gt_creator:(Comp_creator *)&le_creator;
}


Comp_creator *comp_lt_creator(bool invert)
{
  return invert?(Comp_creator *)&ge_creator:(Comp_creator *)&lt_creator;
}


Comp_creator *comp_ne_creator(bool invert)
{
  return invert?(Comp_creator *)&eq_creator:(Comp_creator *)&ne_creator;
}


/*
  Construct ALL/ANY/SOME subquery Item

  SYNOPSIS
    all_any_subquery_creator()
    left_expr - pointer to left expression
    cmp - compare function creator
    all - true if we create ALL subquery
    select_lex - pointer on parsed subquery structure

  RETURN VALUE
    constructed Item (or 0 if out of memory)
*/
Item * all_any_subquery_creator(Item *left_expr,
				chooser_compare_func_creator cmp,
				bool all,
				SELECT_LEX *select_lex)
{
  if ((cmp == &comp_eq_creator) && !all)       //  = ANY <=> IN
    return new Item_in_subselect(left_expr, select_lex);

  if ((cmp == &comp_ne_creator) && all)        // <> ALL <=> NOT IN
    return new Item_func_not(new Item_in_subselect(left_expr, select_lex));

  Item_allany_subselect *it=
    new Item_allany_subselect(left_expr, (*cmp)(all), select_lex, all);
  if (all)
    return it->upper_item= new Item_func_not_all(it);	/* ALL */

  return it->upper_item= new Item_func_nop_all(it);      /* ANY/SOME */
}


/*
  CREATE INDEX and DROP INDEX are implemented by calling ALTER TABLE with
  the proper arguments.  This isn't very fast but it should work for most
  cases.

  In the future ALTER TABLE will notice that only added indexes
  and create these one by one for the existing table without having to do
  a full rebuild.

  One should normally create all indexes with CREATE TABLE or ALTER TABLE.
*/

int mysql_create_index(THD *thd, TABLE_LIST *table_list, List<Key> &keys)
{
  List<create_field> fields;
  ALTER_INFO alter_info;
  alter_info.flags= ALTER_ADD_INDEX;
  alter_info.is_simple= 0;
  HA_CREATE_INFO create_info;
  DBUG_ENTER("mysql_create_index");
  bzero((char*) &create_info,sizeof(create_info));
  create_info.db_type=DB_TYPE_DEFAULT;
  create_info.default_table_charset= thd->variables.collation_database;
  DBUG_RETURN(mysql_alter_table(thd,table_list->db,table_list->real_name,
				&create_info, table_list,
				fields, keys, 0, (ORDER*)0,
				DUP_ERROR, 0, &alter_info));
}


int mysql_drop_index(THD *thd, TABLE_LIST *table_list, ALTER_INFO *alter_info)
{
  List<create_field> fields;
  List<Key> keys;
  HA_CREATE_INFO create_info;
  DBUG_ENTER("mysql_drop_index");
  bzero((char*) &create_info,sizeof(create_info));
  create_info.db_type=DB_TYPE_DEFAULT;
  create_info.default_table_charset= thd->variables.collation_database;
  alter_info->clear();
  alter_info->flags= ALTER_DROP_INDEX;
  alter_info->is_simple= 0;
  DBUG_RETURN(mysql_alter_table(thd,table_list->db,table_list->real_name,
				&create_info, table_list,
				fields, keys, 0, (ORDER*)0,
				DUP_ERROR, 0, alter_info));
}


/*
  Multi update query pre-check

  SYNOPSIS
    multi_update_precheck()
    thd		Thread handler
    tables	Global table list

  RETURN VALUE
    0   OK
    1   Error (message is sent to user)
    -1  Error (message is not sent to user)
*/

int multi_update_precheck(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("multi_update_precheck");
  const char *msg= 0;
  TABLE_LIST *table;
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  TABLE_LIST *update_list= (TABLE_LIST*)select_lex->table_list.first;

  if (select_lex->item_list.elements != lex->value_list.elements)
  {
    my_error(ER_WRONG_VALUE_COUNT, MYF(0));
    DBUG_RETURN(-1);
  }
  /*
    Ensure that we have UPDATE or SELECT privilege for each table
    The exact privilege is checked in mysql_multi_update()
  */
  for (table= update_list; table; table= table->next)
  {
    if (table->derived)
      table->grant.privilege= SELECT_ACL;
    else if ((check_access(thd, UPDATE_ACL, table->db,
                           &table->grant.privilege, 0, 1) ||
              grant_option &&
              check_grant(thd, UPDATE_ACL, table, 0, 1, 1)) &&
	(check_access(thd, SELECT_ACL, table->db,
		      &table->grant.privilege, 0, 0) ||
	 grant_option && check_grant(thd, SELECT_ACL, table, 0, 1, 0)))
      DBUG_RETURN(1);

    /*
      We assign following flag only to copy of table, because it will
      be checked only if query contains subqueries i.e. only if copy exists
    */
    if (table->table_list)
      table->table_list->table_in_update_from_clause= 1;
  }
  /*
    Is there tables of subqueries?
  */
  if (&lex->select_lex != lex->all_selects_list || lex->time_zone_tables_used)
  {
    DBUG_PRINT("info",("Checking sub query list"));
    for (table= tables; table; table= table->next)
    {
      if (my_tz_check_n_skip_implicit_tables(&table,
                                             lex->time_zone_tables_used))
        continue;
      else if (table->table_in_update_from_clause)
      {
	/*
	  If we check table by local TABLE_LIST copy then we should copy
	  grants to global table list, because it will be used for table
	  opening.
	*/
	if (table->table_list)
	  table->grant= table->table_list->grant;
      }
      else if (!table->derived)
      {
	if (check_access(thd, SELECT_ACL, table->db,
			 &table->grant.privilege, 0, 0) ||
	    grant_option && check_grant(thd, SELECT_ACL, table, 0, 1, 0))
	  DBUG_RETURN(1);
      }
    }
  }

  if (select_lex->order_list.elements)
    msg= "ORDER BY";
  else if (select_lex->select_limit && select_lex->select_limit !=
	   HA_POS_ERROR)
    msg= "LIMIT";
  if (msg)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", msg);
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/*
  Multi delete query pre-check

  SYNOPSIS
    multi_delete_precheck()
    thd			Thread handler
    tables		Global table list
    table_count		Pointer to table counter

  RETURN VALUE
    0   OK
    1   error (message is sent to user)
    -1  error (message is not sent to user)
*/

int multi_delete_precheck(THD *thd, TABLE_LIST *tables, uint *table_count)
{
  DBUG_ENTER("multi_delete_precheck");
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  TABLE_LIST *aux_tables=
    (TABLE_LIST *)thd->lex->auxilliary_table_list.first;
  TABLE_LIST *delete_tables= (TABLE_LIST *)select_lex->table_list.first;
  TABLE_LIST *target_tbl;

  *table_count= 0;

  /* sql_yacc guarantees that tables and aux_tables are not zero */
  DBUG_ASSERT(aux_tables != 0);
  if (check_db_used(thd, tables) || check_db_used(thd,aux_tables) ||
      check_table_access(thd,SELECT_ACL, tables,0) ||
      check_table_access(thd,DELETE_ACL, aux_tables,0))
    DBUG_RETURN(1);
  if ((thd->options & OPTION_SAFE_UPDATES) && !select_lex->where)
  {
    my_error(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE, MYF(0));
    DBUG_RETURN(-1);
  }
  for (target_tbl= aux_tables; target_tbl; target_tbl= target_tbl->next)
  {
    (*table_count)++;
    /* All tables in aux_tables must be found in FROM PART */
    TABLE_LIST *walk;
    walk= get_table_by_alias(delete_tables,target_tbl->db,target_tbl->alias);
    if (!walk)
    {
      my_error(ER_UNKNOWN_TABLE, MYF(0), target_tbl->real_name,
	       "MULTI DELETE");
      DBUG_RETURN(-1);
    }
    if (walk->derived)
    {
      my_error(ER_NON_UPDATABLE_TABLE, MYF(0), target_tbl->real_name,
	       "DELETE");
      DBUG_RETURN(-1);
    }
    walk->lock_type= target_tbl->lock_type;
    target_tbl->table_list= walk;	// Remember corresponding table
    
    /* in case of subselects, we need to set lock_type in
     * corresponding table in list of all tables */
    if (walk->table_list)
    {
      target_tbl->table_list= walk->table_list;
      walk->table_list->lock_type= walk->lock_type;
    }
  }
  DBUG_RETURN(0);
}


/*
  simple UPDATE query pre-check

  SYNOPSIS
    update_precheck()
    thd		Thread handler
    tables	Global table list

  RETURN VALUE
    0   OK
    1   Error (message is sent to user)
    -1  Error (message is not sent to user)
*/

int update_precheck(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("update_precheck");
  if (thd->lex->select_lex.item_list.elements != thd->lex->value_list.elements)
  {
    my_error(ER_WRONG_VALUE_COUNT, MYF(0));
    DBUG_RETURN(-1);
  }
  DBUG_RETURN((check_db_used(thd, tables) ||
	       check_one_table_access(thd, UPDATE_ACL, tables)) ? 1 : 0);
}


/*
  simple DELETE query pre-check

  SYNOPSIS
    delete_precheck()
    thd		Thread handler
    tables	Global table list

  RETURN VALUE
    0   OK
    1   error (message is sent to user)
    -1  error (message is not sent to user)
*/

int delete_precheck(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("delete_precheck");
  if (check_one_table_access(thd, DELETE_ACL, tables))
    DBUG_RETURN(1);
  /* Set privilege for the WHERE clause */
  tables->grant.want_privilege=(SELECT_ACL & ~tables->grant.privilege);
  DBUG_RETURN(0);
}


/*
  simple INSERT query pre-check

  SYNOPSIS
    insert_precheck()
    thd		Thread handler
    tables	Global table list

  RETURN VALUE
    0   OK
    1   error (message is sent to user)
    -1  error (message is not sent to user)
*/

int insert_precheck(THD *thd, TABLE_LIST *tables)
{
  LEX *lex= thd->lex;
  DBUG_ENTER("insert_precheck");

  /*
    Check that we have modify privileges for the first table and
    select privileges for the rest
  */
  ulong privilege= INSERT_ACL |
                   (lex->duplicates == DUP_REPLACE ? DELETE_ACL : 0) |
                   (lex->duplicates == DUP_UPDATE ? UPDATE_ACL : 0);

  if (check_one_table_access(thd, privilege, tables))
    DBUG_RETURN(1);

  if (lex->update_list.elements != lex->value_list.elements)
  {
    my_error(ER_WRONG_VALUE_COUNT, MYF(0));
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}


/*
  CREATE TABLE query pre-check

  SYNOPSIS
    create_table_precheck()
    thd			Thread handler
    tables		Global table list
    create_table	Table which will be created

  RETURN VALUE
    0   OK
    1   Error (message is sent to user)
*/

int create_table_precheck(THD *thd, TABLE_LIST *tables,
			  TABLE_LIST *create_table)
{
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  ulong want_priv;
  int error= 1;                                 // Error message is given
  DBUG_ENTER("create_table_precheck");

  want_priv= ((lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) ?
              CREATE_TMP_ACL : CREATE_ACL);
  lex->create_info.alias= create_table->alias;
  if (check_access(thd, want_priv, create_table->db,
		   &create_table->grant.privilege, 0, 0) ||
      check_merge_table_access(thd, create_table->db,
			       (TABLE_LIST *)
			       lex->create_info.merge_list.first))
    goto err;
  if (grant_option && want_priv != CREATE_TMP_ACL &&
      check_grant(thd, want_priv, create_table, 0, UINT_MAX, 0))
    goto err;

  if (select_lex->item_list.elements)
  {
    /* Check permissions for used tables in CREATE TABLE ... SELECT */

    /*
      For temporary tables or PREPARED STATEMETNS we don't have to check
      if the created table exists
    */
    if (!(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
        ! thd->current_arena->is_stmt_prepare() &&
        find_real_table_in_list(tables, create_table->db,
                                create_table->real_name))
    {
      net_printf(thd,ER_UPDATE_TABLE_USED, create_table->real_name);

      goto err;
    }
    if (lex->create_info.used_fields & HA_CREATE_USED_UNION)
    {
      TABLE_LIST *tab;
      for (tab= tables; tab; tab= tab->next)
      {
        if (find_real_table_in_list((TABLE_LIST*) lex->create_info.
                                    merge_list.first,
                                    tables->db, tab->real_name))
        {
          net_printf(thd, ER_UPDATE_TABLE_USED, tab->real_name);
          goto err;
        }
      }  
    }    

    if (tables && check_table_access(thd, SELECT_ACL, tables,0))
      goto err;
  }
  error= 0;

err:
  DBUG_RETURN(error);
}


/*
  negate given expression

  SYNOPSIS
    negate_expression()
    thd  therad handler
    expr expression for negation

  RETURN
    negated expression
*/

Item *negate_expression(THD *thd, Item *expr)
{
  Item *negated;
  if (expr->type() == Item::FUNC_ITEM &&
      ((Item_func *) expr)->functype() == Item_func::NOT_FUNC)
  {
    /* it is NOT(NOT( ... )) */
    Item *arg= ((Item_func *) expr)->arguments()[0];
    enum_parsing_place place= thd->lex->current_select->parsing_place;
    if (arg->is_bool_func() || place == IN_WHERE || place == IN_HAVING)
      return arg;
    /*
      if it is not boolean function then we have to emulate value of
      not(not(a)), it will be a != 0
    */
    return new Item_func_ne(arg, new Item_int((char*) "0", 0, 1));
  }

  if ((negated= expr->neg_transformer(thd)) != 0)
    return negated;
  return new Item_func_not(expr);
}
