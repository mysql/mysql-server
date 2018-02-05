/*
   Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  Functions to authenticate and handle requests for a connection
*/

#include "sql_connect.h"

#include "hash.h"                       // HASH
#include "m_string.h"                   // my_stpcpy
#include "probes_mysql.h"               // MYSQL_CONNECTION_START
#include "auth_common.h"                // SUPER_ACL
#include "hostname.h"                   // Host_errors
#include "log.h"                        // sql_print_information
#include "mysqld.h"                     // LOCK_user_conn
#include "sql_audit.h"                  // MYSQL_AUDIT_NOTIFY_CONNECTION_CONNECT
#include "sql_class.h"                  // THD
#include "sql_parse.h"                  // sql_command_flags
#include "sql_plugin.h"                 // plugin_thdvar_cleanup

#include <algorithm>
#include <string.h>

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

using std::min;
using std::max;

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
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
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */

/*
  Get structure for logging connection data for the current user
*/

#ifndef NO_EMBEDDED_ACCESS_CHECKS
static HASH hash_user_connections;

int get_or_create_user_conn(THD *thd, const char *user,
                            const char *host,
                            const USER_RESOURCES *mqh)
{
  int return_val= 0;
  size_t temp_len, user_len;
  char temp_user[USER_HOST_BUFF_SIZE];
  struct  user_conn *uc;

  DBUG_ASSERT(user != 0);
  DBUG_ASSERT(host != 0);

  user_len= strlen(user);
  temp_len= (my_stpcpy(my_stpcpy(temp_user, user)+1, host) - temp_user)+1;
  mysql_mutex_lock(&LOCK_user_conn);
  if (!(uc = (struct  user_conn *) my_hash_search(&hash_user_connections,
                 (uchar*) temp_user, temp_len)))
  {
    /* First connection for user; Create a user connection object */
    if (!(uc= ((struct user_conn*)
         my_malloc(key_memory_user_conn,
                   sizeof(struct user_conn) + temp_len+1,
       MYF(MY_WME)))))
    {
      /* MY_WME ensures an error is set in THD. */
      return_val= 1;
      goto end;
    }
    uc->user=(char*) (uc+1);
    memcpy(uc->user,temp_user,temp_len+1);
    uc->host= uc->user + user_len +  1;
    uc->len= temp_len;
    uc->connections= uc->questions= uc->updates= uc->conn_per_hour= 0;
    uc->user_resources= *mqh;
    uc->reset_utime= thd->thr_create_utime;
    if (my_hash_insert(&hash_user_connections, (uchar*) uc))
    {
      /* The only possible error is out of memory, MY_WME sets an error. */
      my_free(uc);
      return_val= 1;
      goto end;
    }
  }
  thd->set_user_connect(uc);
  thd->increment_user_connections_counter();
end:
  mysql_mutex_unlock(&LOCK_user_conn);
  return return_val;

}


/*
  check if user has already too many connections

  SYNOPSIS
  check_for_max_user_connections()
  thd     Thread handle
  uc      User connect object

  NOTES
    If check fails, we decrease user connection count, which means one
    shouldn't call decrease_user_connections() after this function.

  RETURN
    0 ok
    1 error
*/

int check_for_max_user_connections(THD *thd, const USER_CONN *uc)
{
  int error=0;
  Host_errors errors;
  DBUG_ENTER("check_for_max_user_connections");

  mysql_mutex_lock(&LOCK_user_conn);
  if (global_system_variables.max_user_connections &&
      !uc->user_resources.user_conn &&
      global_system_variables.max_user_connections < (uint) uc->connections)
  {
    my_error(ER_TOO_MANY_USER_CONNECTIONS, MYF(0), uc->user);
    error=1;
    errors.m_max_user_connection= 1;
    goto end;
  }
  thd->time_out_user_resource_limits();
  if (uc->user_resources.user_conn &&
      uc->user_resources.user_conn < uc->connections)
  {
    my_error(ER_USER_LIMIT_REACHED, MYF(0), uc->user,
             "max_user_connections",
             (long) uc->user_resources.user_conn);
    error= 1;
    errors.m_max_user_connection= 1;
    goto end;
  }
  if (uc->user_resources.conn_per_hour &&
      uc->user_resources.conn_per_hour <= uc->conn_per_hour)
  {
    my_error(ER_USER_LIMIT_REACHED, MYF(0), uc->user,
             "max_connections_per_hour",
             (long) uc->user_resources.conn_per_hour);
    error=1;
    errors.m_max_user_connection_per_hour= 1;
    goto end;
  }
  thd->increment_con_per_hour_counter();

end:
  if (error)
  {
    thd->decrement_user_connections_counter();
    /*
      The thread may returned back to the pool and assigned to a user
      that doesn't have a limit. Ensure the user is not using resources
      of someone else.
    */
    thd->set_user_connect(NULL);
  }
  mysql_mutex_unlock(&LOCK_user_conn);
  if (error)
  {
    inc_host_errors(thd->m_main_security_ctx.ip().str, &errors);
  }
  DBUG_RETURN(error);
}


/*
  Decrease user connection count

  SYNOPSIS
    decrease_user_connections()
    uc      User connection object

  NOTES
    If there is a n user connection object for a connection
    (which only happens if 'max_user_connections' is defined or
    if someone has created a resource grant for a user), then
    the connection count is always incremented on connect.

    The user connect object is not freed if some users has
    'max connections per hour' defined as we need to be able to hold
    count over the lifetime of the connection.
*/

void decrease_user_connections(USER_CONN *uc)
{
  DBUG_ENTER("decrease_user_connections");
  mysql_mutex_lock(&LOCK_user_conn);
  DBUG_ASSERT(uc->connections);
  if (!--uc->connections && !mqh_used)
  {
    /* Last connection for user; Delete it */
    (void) my_hash_delete(&hash_user_connections,(uchar*) uc);
  }
  mysql_mutex_unlock(&LOCK_user_conn);
  DBUG_VOID_RETURN;
}

/*
   Decrements user connections count from the USER_CONN held by THD
   And removes USER_CONN from the hash if no body else is using it.

   SYNOPSIS
     release_user_connection()
     THD  Thread context object.
 */
void release_user_connection(THD *thd)
{
  const USER_CONN *uc= thd->get_user_connect();
  DBUG_ENTER("release_user_connection");

  if (uc)
  {
    mysql_mutex_lock(&LOCK_user_conn);
    DBUG_ASSERT(uc->connections > 0);
    thd->decrement_user_connections_counter();
    if (!uc->connections && !mqh_used)
    {
      /* Last connection for user; Delete it */
      (void) my_hash_delete(&hash_user_connections,(uchar*) uc);
    }
    mysql_mutex_unlock(&LOCK_user_conn);
    thd->set_user_connect(NULL);
  }

  DBUG_VOID_RETURN;
}



/*
  Check if maximum queries per hour limit has been reached
  returns 0 if OK.
*/

bool check_mqh(THD *thd, uint check_command)
{
  bool error= 0;
  const USER_CONN *uc=thd->get_user_connect();
  DBUG_ENTER("check_mqh");
  DBUG_ASSERT(uc != 0);

  mysql_mutex_lock(&LOCK_user_conn);

  thd->time_out_user_resource_limits();

  /* Check that we have not done too many questions / hour */
  if (uc->user_resources.questions)
  {
    thd->increment_questions_counter();
    if ((uc->questions - 1) >= uc->user_resources.questions)
    {
      my_error(ER_USER_LIMIT_REACHED, MYF(0), uc->user, "max_questions",
               (long) uc->user_resources.questions);
      error=1;
      goto end;
    }
  }
  if (check_command < (uint) SQLCOM_END)
  {
    /* Check that we have not done too many updates / hour */
    if (uc->user_resources.updates &&
        (sql_command_flags[check_command] & CF_CHANGES_DATA))
    {
      thd->increment_updates_counter();
      if ((uc->updates - 1) >= uc->user_resources.updates)
      {
        my_error(ER_USER_LIMIT_REACHED, MYF(0), uc->user, "max_updates",
                 (long) uc->user_resources.updates);
        error=1;
        goto end;
      }
    }
  }
end:
  mysql_mutex_unlock(&LOCK_user_conn);
  DBUG_RETURN(error);
}
#else

int check_for_max_user_connections(THD *thd, const USER_CONN *uc)
{
  return 0;
}

void decrease_user_connections(USER_CONN *uc)
{
  return;
}

void release_user_connection(THD *thd)
{
  const USER_CONN *uc= thd->get_user_connect();
  DBUG_ENTER("release_user_connection");

  if (uc)
  {
    thd->set_user_connect(NULL);
  }

  DBUG_VOID_RETURN;
}

#endif /* NO_EMBEDDED_ACCESS_CHECKS */

/*
  Check for maximum allowable user connections, if the mysqld server is
  started with corresponding variable that is greater then 0.
*/

extern "C" uchar *get_key_conn(user_conn *buff, size_t *length,
            my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length= buff->len;
  return (uchar*) buff->user;
}


extern "C" void free_user(struct user_conn *uc)
{
  my_free(uc);
}


void init_max_user_conn(void)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  (void)
    my_hash_init(&hash_user_connections,system_charset_info,max_connections,
                 0,0, (my_hash_get_key) get_key_conn,
                 (my_hash_free_key) free_user, 0,
                 key_memory_user_conn);
#endif
}


void free_max_user_conn(void)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  my_hash_free(&hash_user_connections);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}


void reset_mqh(LEX_USER *lu, bool get_them= 0)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  mysql_mutex_lock(&LOCK_user_conn);
  if (lu)  // for GRANT
  {
    USER_CONN *uc;
    size_t temp_len=lu->user.length+lu->host.length+2;
    char temp_user[USER_HOST_BUFF_SIZE];

    memcpy(temp_user,lu->user.str,lu->user.length);
    memcpy(temp_user+lu->user.length+1,lu->host.str,lu->host.length);
    temp_user[lu->user.length]='\0'; temp_user[temp_len-1]=0;
    if ((uc = (struct  user_conn *) my_hash_search(&hash_user_connections,
                                                   (uchar*) temp_user,
                                                   temp_len)))
    {
      uc->questions=0;
      get_mqh(temp_user,&temp_user[lu->user.length+1],uc);
      uc->updates=0;
      uc->conn_per_hour=0;
    }
  }
  else
  {
    /* for FLUSH PRIVILEGES and FLUSH USER_RESOURCES */
    for (uint idx=0;idx < hash_user_connections.records; idx++)
    {
      USER_CONN *uc=(struct user_conn *)
        my_hash_element(&hash_user_connections, idx);
      if (get_them)
  get_mqh(uc->user,uc->host,uc);
      uc->questions=0;
      uc->updates=0;
      uc->conn_per_hour=0;
    }
  }
  mysql_mutex_unlock(&LOCK_user_conn);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}


/**
  Set thread character set variables from the given ID

  @param  thd         thread handle
  @param  cs_number   character set and collation ID

  @retval  0  OK; character_set_client, collation_connection and
              character_set_results are set to the new value,
              or to the default global values.

  @retval  1  error, e.g. the given ID is not supported by parser.
              Corresponding SQL error is sent.
*/

bool thd_init_client_charset(THD *thd, uint cs_number)
{
  CHARSET_INFO *cs;
  /*
   Use server character set and collation if
   - opt_character_set_client_handshake is not set
   - client has not specified a character set
   - client character set is the same as the servers
   - client character set doesn't exists in server
  */
  if (!opt_character_set_client_handshake ||
      !(cs= get_charset(cs_number, MYF(0))) ||
      !my_strcasecmp(&my_charset_latin1,
                     global_system_variables.character_set_client->name,
                     cs->name))
  {
    if (!is_supported_parser_charset(
      global_system_variables.character_set_client))
    {
      /* Disallow non-supported parser character sets: UCS2, UTF16, UTF32 */
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "character_set_client",
               global_system_variables.character_set_client->csname);
      return true;
    }    
    thd->variables.character_set_client=
      global_system_variables.character_set_client;
    thd->variables.collation_connection=
      global_system_variables.collation_connection;
    thd->variables.character_set_results=
      global_system_variables.character_set_results;
  }
  else
  {
    if (!is_supported_parser_charset(cs))
    {
      /* Disallow non-supported parser character sets: UCS2, UTF16, UTF32 */
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "character_set_client",
               cs->csname);
      return true;
    }
    thd->variables.character_set_results=
      thd->variables.collation_connection=
      thd->variables.character_set_client= cs;
  }
  return false;
}


#ifndef EMBEDDED_LIBRARY
/*
  Perform handshake, authorize client and update thd ACL variables.

  SYNOPSIS
    check_connection()
    thd  thread handle

  RETURN
     0  success, thd is updated.
     1  error
*/

static int check_connection(THD *thd)
{
  uint connect_errors= 0;
  int auth_rc;
  NET *net= thd->get_protocol_classic()->get_net();
  DBUG_PRINT("info",
             ("New connection received on %s", vio_description(net->vio)));

  thd->set_active_vio(net->vio);

  if (!thd->m_main_security_ctx.host().length)     // If TCP/IP connection
  {
    my_bool peer_rc;
    char ip[NI_MAXHOST];
    LEX_CSTRING main_sctx_ip;

    peer_rc= vio_peer_addr(net->vio, ip, &thd->peer_port, NI_MAXHOST);

    /*
    ===========================================================================
    DEBUG code only (begin)
    Simulate various output from vio_peer_addr().
    ===========================================================================
    */

    DBUG_EXECUTE_IF("vio_peer_addr_error",
                    {
                      peer_rc= 1;
                    }
                    );
    DBUG_EXECUTE_IF("vio_peer_addr_fake_ipv4",
                    {
                      struct sockaddr *sa= (sockaddr *) &net->vio->remote;
                      sa->sa_family= AF_INET;
                      struct in_addr *ip4= &((struct sockaddr_in *) sa)->sin_addr;
                      /* See RFC 5737, 192.0.2.0/24 is reserved. */
                      const char* fake= "192.0.2.4";
                      ip4->s_addr= inet_addr(fake);
                      strcpy(ip, fake);
                      peer_rc= 0;
                    }
                    );

#ifdef HAVE_IPV6
    DBUG_EXECUTE_IF("vio_peer_addr_fake_ipv6",
                    {
                      struct sockaddr_in6 *sa= (sockaddr_in6 *) &net->vio->remote;
                      sa->sin6_family= AF_INET6;
                      struct in6_addr *ip6= & sa->sin6_addr;
                      /* See RFC 3849, ipv6 2001:DB8::/32 is reserved. */
                      const char* fake= "2001:db8::6:6";
                      /* inet_pton(AF_INET6, fake, ip6); not available on Windows XP. */
                      ip6->s6_addr[ 0] = 0x20;
                      ip6->s6_addr[ 1] = 0x01;
                      ip6->s6_addr[ 2] = 0x0d;
                      ip6->s6_addr[ 3] = 0xb8;
                      ip6->s6_addr[ 4] = 0x00;
                      ip6->s6_addr[ 5] = 0x00;
                      ip6->s6_addr[ 6] = 0x00;
                      ip6->s6_addr[ 7] = 0x00;
                      ip6->s6_addr[ 8] = 0x00;
                      ip6->s6_addr[ 9] = 0x00;
                      ip6->s6_addr[10] = 0x00;
                      ip6->s6_addr[11] = 0x00;
                      ip6->s6_addr[12] = 0x00;
                      ip6->s6_addr[13] = 0x06;
                      ip6->s6_addr[14] = 0x00;
                      ip6->s6_addr[15] = 0x06;
                      strcpy(ip, fake);
                      peer_rc= 0;
                    }
                    );
#endif /* HAVE_IPV6 */

    /*
    ===========================================================================
    DEBUG code only (end)
    ===========================================================================
    */

    if (peer_rc)
    {
      /*
        Since we can not even get the peer IP address,
        there is nothing to show in the host_cache,
        so increment the global status variable for peer address errors.
      */
      connection_errors_peer_addr++;
      my_error(ER_BAD_HOST_ERROR, MYF(0));
      return 1;
    }
    thd->m_main_security_ctx.assign_ip(ip, strlen(ip));
    main_sctx_ip= thd->m_main_security_ctx.ip();
    if (!(main_sctx_ip.length))
    {
      /*
        No error accounting per IP in host_cache,
        this is treated as a global server OOM error.
        TODO: remove the need for my_strdup.
      */
      connection_errors_internal++;
      return 1; /* The error is set by my_strdup(). */
    }
    thd->m_main_security_ctx.set_host_or_ip_ptr(main_sctx_ip.str,
                                                main_sctx_ip.length);
    if (!(specialflag & SPECIAL_NO_RESOLVE))
    {
      int rc;
      char *host;
      LEX_CSTRING main_sctx_host;

      rc= ip_to_hostname(&net->vio->remote,
                         main_sctx_ip.str,
                         &host, &connect_errors);

      thd->m_main_security_ctx.assign_host(host, host? strlen(host) : 0);
      main_sctx_host= thd->m_main_security_ctx.host();
      if (host && host != my_localhost)
      {
        my_free(host);
        host= (char *) main_sctx_host.str;
      }

      /* Cut very long hostnames to avoid possible overflows */
      if (main_sctx_host.length)
      {
        if (main_sctx_host.str != my_localhost)
          thd->m_main_security_ctx.set_host_ptr(
            main_sctx_host.str,
            min<size_t>(main_sctx_host.length, HOSTNAME_LENGTH));
        thd->m_main_security_ctx.set_host_or_ip_ptr(main_sctx_host.str,
                                                    main_sctx_host.length);
      }

      if (rc == RC_BLOCKED_HOST)
      {
        /* HOST_CACHE stats updated by ip_to_hostname(). */
        my_error(ER_HOST_IS_BLOCKED, MYF(0),
                 thd->m_main_security_ctx.host_or_ip().str);
        return 1;
      }
    }
    DBUG_PRINT("info",("Host: %s  ip: %s",
           (thd->m_main_security_ctx.host().length ?
              thd->m_main_security_ctx.host().str : "unknown host"),
           (main_sctx_ip.length ? main_sctx_ip.str : "unknown ip")));
    if (acl_check_host(thd->m_main_security_ctx.host().str, main_sctx_ip.str))
    {
      /* HOST_CACHE stats updated by acl_check_host(). */
      my_error(ER_HOST_NOT_PRIVILEGED, MYF(0),
               thd->m_main_security_ctx.host_or_ip().str);
      return 1;
    }
  }
  else /* Hostname given means that the connection was on a socket */
  {
    LEX_CSTRING main_sctx_host= thd->m_main_security_ctx.host();
    DBUG_PRINT("info",("Host: %s", main_sctx_host.str));
    thd->m_main_security_ctx.set_host_or_ip_ptr(main_sctx_host.str,
                                                main_sctx_host.length);
    thd->m_main_security_ctx.set_ip_ptr(STRING_WITH_LEN(""));
    /* Reset sin_addr */
    memset(&net->vio->remote, 0, sizeof(net->vio->remote));
  }
  vio_keepalive(net->vio, TRUE);

  if (thd->get_protocol_classic()->get_packet()->alloc(
      thd->variables.net_buffer_length))
  {
    /*
      Important note:
      net_buffer_length is a SESSION variable,
      so it may be tempting to account OOM conditions per IP in the HOST_CACHE,
      in case some clients are more demanding than others ...
      However, this session variable is *not* initialized with a per client
      value during the initial connection, it is initialized from the
      GLOBAL net_buffer_length variable from the server.
      Hence, there is no reason to account on OOM conditions per client IP,
      we count failures in the global server status instead.
    */
    connection_errors_internal++;
    return 1; /* The error is set by alloc(). */
  }

  if (mysql_audit_notify(thd,
                        AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE)))
  {
    return 1;
  }

  auth_rc= acl_authenticate(thd, COM_CONNECT);

  if (mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_CONNECT)))
  {
    return 1;
  }

  if (auth_rc == 0 && connect_errors != 0)
  {
    /*
      A client connection from this IP was successful,
      after some previous failures.
      Reset the connection error counter.
    */
    reset_host_connect_errors(thd->m_main_security_ctx.ip().str);
  }

  /*
    Now that acl_authenticate() is executed,
    the SSL info is available.
    Advertise it to THD, so SSL status variables
    can be inspected.
  */
  thd->set_ssl(net->vio);

  return auth_rc;
}


/*
  Autenticate user, with error reporting

  SYNOPSIS
   login_connection()
   thd        Thread handler

  NOTES
    Connection is not closed in case of errors

  RETURN
    0    ok
    1    error
*/


static bool login_connection(THD *thd)
{
  int error;
  DBUG_ENTER("login_connection");
  DBUG_PRINT("info", ("login_connection called by thread %u",
                      thd->thread_id()));

  /* Use "connect_timeout" value during connection phase */
  thd->get_protocol_classic()->set_read_timeout(connect_timeout);
  thd->get_protocol_classic()->set_write_timeout(connect_timeout);

  error= check_connection(thd);
  thd->send_statement_status();

  if (error)
  {           // Wrong permissions
#ifdef _WIN32
    if (vio_type(thd->get_protocol_classic()->get_vio()) ==
        VIO_TYPE_NAMEDPIPE)
      my_sleep(1000);       /* must wait after eof() */
#endif
    DBUG_RETURN(1);
  }
  /* Connect completed, set read/write timeouts back to default */
  thd->get_protocol_classic()->set_read_timeout(
    thd->variables.net_read_timeout);
  thd->get_protocol_classic()->set_write_timeout(
    thd->variables.net_write_timeout);
  DBUG_RETURN(0);
}


/*
  Close an established connection

  NOTES
    This mainly updates status variables
*/

void end_connection(THD *thd)
{
  NET *net= thd->get_protocol_classic()->get_net();

  mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_DISCONNECT), 0);

  plugin_thdvar_cleanup(thd, thd->m_enable_plugins);

  /*
    The thread may returned back to the pool and assigned to a user
    that doesn't have a limit. Ensure the user is not using resources
    of someone else.
  */
  release_user_connection(thd);

  if (thd->killed || (net->error && net->vio != 0))
  {
    aborted_threads++;
  }

  if (net->error && net->vio != 0)
  {
    if (!thd->killed)
    {
      Security_context *sctx= thd->security_context();
      LEX_CSTRING sctx_user= sctx->user();
      sql_print_information(ER(ER_NEW_ABORTING_CONNECTION),
                            thd->thread_id(),
                            (thd->db().str ? thd->db().str : "unconnected"),
                            sctx_user.str ? sctx_user.str : "unauthenticated",
                            sctx->host_or_ip().str,
                            (thd->get_stmt_da()->is_error() ?
                             thd->get_stmt_da()->message_text() :
                             ER(ER_UNKNOWN_ERROR)));
    }
  }
}


/*
  Initialize THD to handle queries
*/

static void prepare_new_connection_state(THD* thd)
{
  NET *net= thd->get_protocol_classic()->get_net();
  Security_context *sctx= thd->security_context();

  if (thd->get_protocol()->has_client_capability(CLIENT_COMPRESS))
    net->compress=1;        // Use compression

  // Initializing session system variables.
  alloc_and_copy_thd_dynamic_variables(thd, true);

  /*
    Much of this is duplicated in create_embedded_thd() for the
    embedded server library.
    TODO: refactor this to avoid code duplication there
  */
  thd->proc_info= 0;
  thd->set_command(COM_SLEEP);
  thd->set_time();
  thd->init_for_queries();

  if (opt_init_connect.length && !(sctx->check_access(SUPER_ACL)))
  {
    if (sctx->password_expired())
    {
      sql_print_warning("init_connect variable is ignored for user: %s "
                        "host: %s due to expired password.", sctx->priv_user().str,
                        sctx->priv_host().str);
      return;
    }

    execute_init_command(thd, &opt_init_connect, &LOCK_sys_init_connect);

    if (thd->is_error())
    {
      Host_errors errors;
      ulong packet_length;
      LEX_CSTRING sctx_user= sctx->user();

      sql_print_warning(ER(ER_NEW_ABORTING_CONNECTION),
                        thd->thread_id(),
                        thd->db().str ? thd->db().str : "unconnected",
                        sctx_user.str ? sctx_user.str : "unauthenticated",
                        sctx->host_or_ip().str, "init_connect command failed");
      sql_print_warning("%s", thd->get_stmt_da()->message_text());

      thd->lex->set_current_select(0);
      my_net_set_read_timeout(net, thd->variables.net_wait_timeout);
      thd->clear_error();
      net_new_transaction(net);
      packet_length= my_net_read(net);
      /*
        If my_net_read() failed, my_error() has been already called,
        and the main Diagnostics Area contains an error condition.
      */
      if (packet_length != packet_error)
        my_error(ER_NEW_ABORTING_CONNECTION, MYF(0),
                 thd->thread_id(),
                 thd->db().str ? thd->db().str : "unconnected",
                 sctx_user.str ? sctx_user.str : "unauthenticated",
                 sctx->host_or_ip().str, "init_connect command failed");

      thd->server_status&= ~SERVER_STATUS_CLEAR_SET;
      thd->send_statement_status();
      thd->killed = THD::KILL_CONNECTION;
      errors.m_init_connect= 1;
      inc_host_errors(thd->m_main_security_ctx.ip().str, &errors);
      return;
    }

    thd->proc_info=0;
    thd->set_time();
    thd->init_for_queries();
  }
}


bool thd_prepare_connection(THD *thd)
{
  bool rc;
  lex_start(thd);
  rc= login_connection(thd);

  if (rc)
    return rc;

  MYSQL_CONNECTION_START(thd->thread_id(),
                         (char *) &thd->security_context()->priv_user().str[0],
                         (char *) thd->security_context()->host_or_ip().str);

  prepare_new_connection_state(thd);
  return FALSE;
}


/**
  Close a connection.

  @param thd        Thread handle.
  @param sql_errno  The error code to send before disconnect.
  @param server_shutdown Argument passed to the THD's disconnect method.
  @param generate_event  Generate Audit API disconnect event.

  @note
    For the connection that is doing shutdown, this is called twice
*/

void close_connection(THD *thd, uint sql_errno,
                      bool server_shutdown, bool generate_event)
{
  DBUG_ENTER("close_connection");

  if (sql_errno)
    net_send_error(thd, sql_errno, ER_DEFAULT(sql_errno));

  thd->disconnect(server_shutdown);

  MYSQL_CONNECTION_DONE((int) sql_errno, thd->thread_id());

  if (MYSQL_CONNECTION_DONE_ENABLED())
  {
    sleep(0); /* Workaround to avoid tailcall optimisation */
  }

  if (generate_event)
    mysql_audit_notify(thd,
                       AUDIT_EVENT(MYSQL_AUDIT_CONNECTION_DISCONNECT),
                       sql_errno);

  DBUG_VOID_RETURN;
}


bool thd_connection_alive(THD *thd)
{
  NET *net= thd->get_protocol_classic()->get_net();
  if (!net->error &&
      net->vio != 0 &&
      !(thd->killed == THD::KILL_CONNECTION))
    return true;
  return false;
}

#endif /* EMBEDDED_LIBRARY */
