/*
   Copyright (c) 2007, 2012, Oracle and/or its affiliates. All rights reserved.

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
  Functions to autenticate and handle reqests for a connection
*/

#include "mysql_priv.h"

/** Size of the header fields of an authentication packet. */
#define AUTH_PACKET_HEADER_SIZE_PROTO_41    32
#define AUTH_PACKET_HEADER_SIZE_PROTO_40    5  
#define AUTH_PACKET_HEADER_SIZE_CONNJ_SSL   4

#ifdef __WIN__
extern void win_install_sigabrt_handler();
#endif

/*
  Get structure for logging connection data for the current user
*/

#ifndef NO_EMBEDDED_ACCESS_CHECKS
static HASH hash_user_connections;

static int get_or_create_user_conn(THD *thd, const char *user,
				   const char *host,
				   USER_RESOURCES *mqh)
{
  int return_val= 0;
  size_t temp_len, user_len;
  char temp_user[USER_HOST_BUFF_SIZE];
  struct  user_conn *uc;

  DBUG_ASSERT(user != 0);
  DBUG_ASSERT(host != 0);

  user_len= strlen(user);
  temp_len= (strmov(strmov(temp_user, user)+1, host) - temp_user)+1;
  (void) pthread_mutex_lock(&LOCK_user_conn);
  if (!(uc = (struct  user_conn *) hash_search(&hash_user_connections,
					       (uchar*) temp_user, temp_len)))
  {
    /* First connection for user; Create a user connection object */
    if (!(uc= ((struct user_conn*)
	       my_malloc(sizeof(struct user_conn) + temp_len+1,
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
      my_free((char*) uc,0);
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

static
int check_for_max_user_connections(THD *thd, USER_CONN *uc)
{
  int error=0;
  DBUG_ENTER("check_for_max_user_connections");

  (void) pthread_mutex_lock(&LOCK_user_conn);
  if (max_user_connections && !uc->user_resources.user_conn &&
      max_user_connections < (uint) uc->connections)
  {
    my_error(ER_TOO_MANY_USER_CONNECTIONS, MYF(0), uc->user);
    error=1;
    goto end;
  }
  time_out_user_resource_limits(thd, uc);
  if (uc->user_resources.user_conn &&
      uc->user_resources.user_conn < uc->connections)
  {
    my_error(ER_USER_LIMIT_REACHED, MYF(0), uc->user,
             "max_user_connections",
             (long) uc->user_resources.user_conn);
    error= 1;
    goto end;
  }
  if (uc->user_resources.conn_per_hour &&
      uc->user_resources.conn_per_hour <= uc->conn_per_hour)
  {
    my_error(ER_USER_LIMIT_REACHED, MYF(0), uc->user,
             "max_connections_per_hour",
             (long) uc->user_resources.conn_per_hour);
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

void decrease_user_connections(USER_CONN *uc)
{
  DBUG_ENTER("decrease_user_connections");
  (void) pthread_mutex_lock(&LOCK_user_conn);
  DBUG_ASSERT(uc->connections);
  if (!--uc->connections && !mqh_used)
  {
    /* Last connection for user; Delete it */
    (void) hash_delete(&hash_user_connections,(uchar*) uc);
  }
  (void) pthread_mutex_unlock(&LOCK_user_conn);
  DBUG_VOID_RETURN;
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

void time_out_user_resource_limits(THD *thd, USER_CONN *uc)
{
  ulonglong check_time= thd->start_utime;
  DBUG_ENTER("time_out_user_resource_limits");

  /* If more than a hour since last check, reset resource checking */
  if (check_time  - uc->reset_utime >= LL(3600000000))
  {
    uc->questions=1;
    uc->updates=0;
    uc->conn_per_hour=0;
    uc->reset_utime= check_time;
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
  USER_CONN *uc=thd->user_connect;
  DBUG_ENTER("check_mqh");
  DBUG_ASSERT(uc != 0);

  (void) pthread_mutex_lock(&LOCK_user_conn);

  time_out_user_resource_limits(thd, uc);

  /* Check that we have not done too many questions / hour */
  if (uc->user_resources.questions &&
      uc->questions++ >= uc->user_resources.questions)
  {
    my_error(ER_USER_LIMIT_REACHED, MYF(0), uc->user, "max_questions",
             (long) uc->user_resources.questions);
    error=1;
    goto end;
  }
  if (check_command < (uint) SQLCOM_END)
  {
    /* Check that we have not done too many updates / hour */
    if (uc->user_resources.updates &&
        (sql_command_flags[check_command] & CF_CHANGES_DATA) &&
	uc->updates++ >= uc->user_resources.updates)
    {
      my_error(ER_USER_LIMIT_REACHED, MYF(0), uc->user, "max_updates",
               (long) uc->user_resources.updates);
      error=1;
      goto end;
    }
  }
end:
  (void) pthread_mutex_unlock(&LOCK_user_conn);
  DBUG_RETURN(error);
}

#endif /* NO_EMBEDDED_ACCESS_CHECKS */


/**
  Check if user exist and password supplied is correct.

  @param  thd         thread handle, thd->security_ctx->{host,user,ip} are used
  @param  command     originator of the check: now check_user is called
                      during connect and change user procedures; used for
                      logging.
  @param  passwd      scrambled password received from client
  @param  passwd_len  length of scrambled password
  @param  db          database name to connect to, may be NULL
  @param  check_count TRUE if establishing a new connection. In this case
                      check that we have not exceeded the global
                      max_connections limist

  @note Host, user and passwd may point to communication buffer.
  Current implementation does not depend on that, but future changes
  should be done with this in mind; 'thd' is INOUT, all other params
  are 'IN'.

  @retval  0  OK; thd->security_ctx->user/master_access/priv_user/db_access and
              thd->db are updated; OK is sent to the client.
  @retval  1  error, e.g. access denied or handshake error, not sent to
              the client. A message is pushed into the error stack.
*/

int
check_user(THD *thd, enum enum_server_command command,
	       const char *passwd, uint passwd_len, const char *db,
	       bool check_count)
{
  DBUG_ENTER("check_user");
  LEX_STRING db_str= { (char *) db, db ? strlen(db) : 0 };

  /*
    Clear thd->db as it points to something, that will be freed when
    connection is closed. We don't want to accidentally free a wrong
    pointer if connect failed. Also in case of 'CHANGE USER' failure,
    current database will be switched to 'no database selected'.
  */
  thd->reset_db(NULL, 0);

#ifdef NO_EMBEDDED_ACCESS_CHECKS
  thd->main_security_ctx.master_access= GLOBAL_ACLS;       // Full rights
  /* Change database if necessary */
  if (db && db[0])
  {
    if (mysql_change_db(thd, &db_str, FALSE))
      DBUG_RETURN(1);
  }
  my_ok(thd);
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
    my_error(ER_NOT_SUPPORTED_AUTH_MODE, MYF(0));
    general_log_print(thd, COM_CONNECT, ER(ER_NOT_SUPPORTED_AUTH_MODE));
    DBUG_RETURN(1);
  }
  if (passwd_len != 0 &&
      passwd_len != SCRAMBLE_LENGTH &&
      passwd_len != SCRAMBLE_LENGTH_323)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0));
    DBUG_RETURN(1);
  }

  USER_RESOURCES ur;
  int res= acl_getroot(thd, &ur, passwd, passwd_len);
  DBUG_EXECUTE_IF("password_format_mismatch",{res= -1;};);
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
    DBUG_EXECUTE_IF("password_format_mismatch",
                    {
                      inc_host_errors(&thd->remote.sin_addr);
                      my_error(ER_HANDSHAKE_ERROR, MYF(0));
                      DBUG_RETURN(1);
                    };);
    if (opt_secure_auth_local)
    {
      my_error(ER_SERVER_IS_IN_SECURE_AUTH_MODE, MYF(0),
               thd->main_security_ctx.user,
               thd->main_security_ctx.host_or_ip);
      general_log_print(thd, COM_CONNECT, ER(ER_SERVER_IS_IN_SECURE_AUTH_MODE),
                        thd->main_security_ctx.user,
                        thd->main_security_ctx.host_or_ip);
      DBUG_RETURN(1);
    }
    /* We have to read very specific packet size */
    if (send_old_password_request(thd) ||
        my_net_read(net) != SCRAMBLE_LENGTH_323 + 1)
    {
      inc_host_errors(&thd->remote.sin_addr);
      my_error(ER_HANDSHAKE_ERROR, MYF(0));
      DBUG_RETURN(1);
    }
    /* Final attempt to check the user based on reply */
    /* So as passwd is short, errcode is always >= 0 */
    res= acl_getroot(thd, &ur, (char *) net->read_pos, SCRAMBLE_LENGTH_323);
  }
#endif /*EMBEDDED_LIBRARY*/
  /* here res is always >= 0 */
  if (res == 0)
  {
    if (!(thd->main_security_ctx.master_access &
          NO_ACCESS)) // authentication is OK
    {
      DBUG_PRINT("info",
                 ("Capabilities: %lu  packet_length: %ld  Host: '%s'  "
                  "Login user: '%s' Priv_user: '%s'  Using password: %s "
                  "Access: %lu  db: '%s'",
                  thd->client_capabilities,
                  thd->max_client_packet_length,
                  thd->main_security_ctx.host_or_ip,
                  thd->main_security_ctx.user,
                  thd->main_security_ctx.priv_user,
                  passwd_len ? "yes": "no",
                  thd->main_security_ctx.master_access,
                  (thd->db ? thd->db : "*none*")));

      if (check_count)
      {
        pthread_mutex_lock(&LOCK_connection_count);
        bool count_ok= connection_count <= max_connections ||
                       (thd->main_security_ctx.master_access & SUPER_ACL);
        VOID(pthread_mutex_unlock(&LOCK_connection_count));

        if (!count_ok)
        {                                         // too many connections
          my_error(ER_CON_COUNT_ERROR, MYF(0));
          DBUG_RETURN(1);
        }
      }

      /*
        Log the command before authentication checks, so that the user can
        check the log for the tried login tried and also to detect
        break-in attempts.
      */
      general_log_print(thd, command,
                        (thd->main_security_ctx.priv_user ==
                         thd->main_security_ctx.user ?
                         (char*) "%s@%s on %s" :
                         (char*) "%s@%s as anonymous on %s"),
                        thd->main_security_ctx.user,
                        thd->main_security_ctx.host_or_ip,
                        db ? db : (char*) "");

      /*
        This is the default access rights for the current database.  It's
        set to 0 here because we don't have an active database yet (and we
        may not have an active database to set.
      */
      thd->main_security_ctx.db_access=0;

      /* Don't allow user to connect if he has done too many queries */
      if ((ur.questions || ur.updates || ur.conn_per_hour || ur.user_conn ||
	   max_user_connections) &&
	  get_or_create_user_conn(thd,
            (opt_old_style_user_limits ? thd->main_security_ctx.user :
             thd->main_security_ctx.priv_user),
            (opt_old_style_user_limits ? thd->main_security_ctx.host_or_ip :
             thd->main_security_ctx.priv_host),
            &ur))
      {
        /* The error is set by get_or_create_user_conn(). */
	DBUG_RETURN(1);
      }
      if (thd->user_connect &&
	  (thd->user_connect->user_resources.conn_per_hour ||
	   thd->user_connect->user_resources.user_conn ||
	   max_user_connections) &&
	  check_for_max_user_connections(thd, thd->user_connect))
      {
        /* The error is set in check_for_max_user_connections(). */
        DBUG_RETURN(1);
      }

      /* Change database if necessary */
      if (db && db[0])
      {
        if (mysql_change_db(thd, &db_str, FALSE))
        {
          /* mysql_change_db() has pushed the error message. */
          if (thd->user_connect)
            decrease_user_connections(thd->user_connect);
          DBUG_RETURN(1);
        }
      }
      my_ok(thd);
      thd->password= test(passwd_len);          // remember for error messages 
#ifndef EMBEDDED_LIBRARY
      /*
        Allow the network layer to skip big packets. Although a malicious
        authenticated session might use this to trick the server to read
        big packets indefinitely, this is a previously established behavior
        that needs to be preserved as to not break backwards compatibility.
      */
      thd->net.skip_big_packet= TRUE;
#endif
      /* Ready to handle queries */
      DBUG_RETURN(0);
    }
  }
  else if (res == 2) // client gave short hash, server has long hash
  {
    my_error(ER_NOT_SUPPORTED_AUTH_MODE, MYF(0));
    general_log_print(thd, COM_CONNECT, ER(ER_NOT_SUPPORTED_AUTH_MODE));
    DBUG_RETURN(1);
  }
  my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
           thd->main_security_ctx.user,
           thd->main_security_ctx.host_or_ip,
           passwd_len ? ER(ER_YES) : ER(ER_NO));
  general_log_print(thd, COM_CONNECT, ER(ER_ACCESS_DENIED_ERROR),
                    thd->main_security_ctx.user,
                    thd->main_security_ctx.host_or_ip,
                    passwd_len ? ER(ER_YES) : ER(ER_NO));
  DBUG_RETURN(1);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}


/*
  Check for maximum allowable user connections, if the mysqld server is
  started with corresponding variable that is greater then 0.
*/

extern "C" uchar *get_key_conn(user_conn *buff, size_t *length,
			      my_bool not_used __attribute__((unused)))
{
  *length= buff->len;
  return (uchar*) buff->user;
}


extern "C" void free_user(struct user_conn *uc)
{
  my_free((char*) uc,MYF(0));
}


void init_max_user_conn(void)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  (void) hash_init(&hash_user_connections,system_charset_info,max_connections,
		   0,0,
		   (hash_get_key) get_key_conn, (hash_free_key) free_user,
		   0);
#endif
}


void free_max_user_conn(void)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  hash_free(&hash_user_connections);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
}


void reset_mqh(LEX_USER *lu, bool get_them= 0)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  (void) pthread_mutex_lock(&LOCK_user_conn);
  if (lu)  // for GRANT
  {
    USER_CONN *uc;
    uint temp_len=lu->user.length+lu->host.length+2;
    char temp_user[USER_HOST_BUFF_SIZE];

    memcpy(temp_user,lu->user.str,lu->user.length);
    memcpy(temp_user+lu->user.length+1,lu->host.str,lu->host.length);
    temp_user[lu->user.length]='\0'; temp_user[temp_len-1]=0;
    if ((uc = (struct  user_conn *) hash_search(&hash_user_connections,
						(uchar*) temp_user, temp_len)))
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


/*
  Initialize connection threads
*/

bool init_new_connection_handler_thread()
{
  pthread_detach_this_thread();
#if defined(__WIN__)
  win_install_sigabrt_handler();
#else
  /* Win32 calls this in pthread_create */
  if (my_thread_init())
    return 1;
#endif /* __WIN__ */
  return 0;
}

#ifndef EMBEDDED_LIBRARY

/** Get a string according to the protocol of the underlying buffer. */
typedef char * (*get_proto_string_func_t) (char **, size_t *, size_t *);

/**
  Get a string formatted according to the 4.1 version of the MySQL protocol.

  @param buffer[in, out]    Pointer to the user-supplied buffer to be scanned.
  @param max_bytes_available[in, out]  Limit the bytes to scan.
  @param string_length[out] The number of characters scanned not including
                            the null character.

  @remark Strings are always null character terminated in this version of the
          protocol.

  @remark The string_length does not include the terminating null character.
          However, after the call, the buffer is increased by string_length+1
          bytes, beyond the null character if there still available bytes to
          scan.

  @return pointer to beginning of the string scanned.
    @retval NULL The buffer content is malformed
*/

static
char *get_41_protocol_string(char **buffer,
                             size_t *max_bytes_available,
                             size_t *string_length)
{
  char *str= (char *)memchr(*buffer, '\0', *max_bytes_available);

  if (str == NULL)
    return NULL;

  *string_length= (size_t)(str - *buffer);
  *max_bytes_available-= *string_length + 1;
  str= *buffer;
  *buffer += *string_length + 1;

  return str;
}


/**
  Get a string formatted according to the 4.0 version of the MySQL protocol.

  @param buffer[in, out]    Pointer to the user-supplied buffer to be scanned.
  @param max_bytes_available[in, out]  Limit the bytes to scan.
  @param string_length[out] The number of characters scanned not including
                            the null character.

  @remark If there are not enough bytes left after the current position of
          the buffer to satisfy the current string, the string is considered
          to be empty and a pointer to empty_c_string is returned.

  @remark A string at the end of the packet is not null terminated.

  @return Pointer to beginning of the string scanned, or a pointer to a empty
          string.
*/
static
char *get_40_protocol_string(char **buffer,
                             size_t *max_bytes_available,
                             size_t *string_length)
{
  char *str;
  size_t len;

  /* No bytes to scan left, treat string as empty. */
  if ((*max_bytes_available) == 0)
  {
    *string_length= 0;
    return empty_c_string;
  }

  str= (char *) memchr(*buffer, '\0', *max_bytes_available);

  /*
    If the string was not null terminated by the client,
    the remainder of the packet is the string. Otherwise,
    advance the buffer past the end of the null terminated
    string.
  */
  if (str == NULL)
    len= *string_length= *max_bytes_available;
  else
    len= (*string_length= (size_t)(str - *buffer)) + 1;

  str= *buffer;
  *buffer+= len;
  *max_bytes_available-= len;

  return str;
}


/**
  Get a length encoded string from a user-supplied buffer.

  @param buffer[in, out] The buffer to scan; updates position after scan.
  @param max_bytes_available[in, out] Limit the number of bytes to scan
  @param string_length[out] Number of characters scanned

  @remark In case the length is zero, then the total size of the string is
    considered to be 1 byte; the size byte.

  @return pointer to first byte after the header in buffer.
    @retval NULL The buffer content is malformed
*/

static
char *get_length_encoded_string(char **buffer,
                                size_t *max_bytes_available,
                                size_t *string_length)
{
  if (*max_bytes_available == 0)
    return NULL;

  /* Do double cast to prevent overflow from signed / unsigned conversion */
  size_t str_len= (size_t)(unsigned char)**buffer;

  /*
    If the length encoded string has the length 0
    the total size of the string is only one byte long (the size byte)
  */
  if (str_len == 0)
  {
    ++*buffer;
    *string_length= 0;
    /*
      Return a pointer to the 0 character so the return value will be
      an empty string.
    */
    return *buffer-1;
  }

  if (str_len >= *max_bytes_available)
    return NULL;

  char *str= *buffer+1;
  *string_length= str_len;
  *max_bytes_available-= *string_length + 1;
  *buffer+= *string_length + 1;
  return str;
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

static int check_connection(THD *thd)
{
  uint connect_errors= 0;
  NET *net= &thd->net;
  ulong pkt_len= 0;
  char *end;

  bool packet_has_required_size= false;
  char *db;
  size_t db_len;
  char *passwd;
  size_t passwd_len;
  char *user;
  size_t user_len;
  uint charset_code= 0;
  size_t bytes_remaining_in_packet= 0;

  DBUG_PRINT("info",
             ("New connection received on %s", vio_description(net->vio)));
#ifdef SIGNAL_WITH_VIO_CLOSE
  thd->set_active_vio(net->vio);
#endif

  if (!thd->main_security_ctx.host)         // If TCP/IP connection
  {
    char ip[30];

    if (vio_peer_addr(net->vio, ip, &thd->peer_port))
    {
      my_error(ER_BAD_HOST_ERROR, MYF(0));
      return 1;
    }
    /* BEGIN : DEBUG */
    DBUG_EXECUTE_IF("addr_fake_ipv4",
                    {
                      struct sockaddr *sa= (sockaddr *) &net->vio->remote;
                      sa->sa_family= AF_INET;
                      struct in_addr *ip4= &((struct sockaddr_in *)sa)->sin_addr;
                      /* See RFC 5737, 192.0.2.0/23 is reserved */
                      const char* fake= "192.0.2.4";
                      ip4->s_addr= inet_addr(fake);
                      strcpy(ip, fake);
                    };);
    /* END   : DEBUG */

    if (!(thd->main_security_ctx.ip= my_strdup(ip,MYF(MY_WME))))
      return 1; /* The error is set by my_strdup(). */
    thd->main_security_ctx.host_or_ip= thd->main_security_ctx.ip;
    vio_in_addr(net->vio,&thd->remote.sin_addr);
    if (!(specialflag & SPECIAL_NO_RESOLVE))
    {
      vio_in_addr(net->vio,&thd->remote.sin_addr);
      thd->main_security_ctx.host=
        ip_to_hostname(&thd->remote.sin_addr, &connect_errors);
      /* Cut very long hostnames to avoid possible overflows */
      if (thd->main_security_ctx.host)
      {
        if (thd->main_security_ctx.host != my_localhost)
          thd->main_security_ctx.host[min(strlen(thd->main_security_ctx.host),
                                          HOSTNAME_LENGTH)]= 0;
        thd->main_security_ctx.host_or_ip= thd->main_security_ctx.host;
      }
      if (connect_errors > max_connect_errors)
      {
        my_error(ER_HOST_IS_BLOCKED, MYF(0), thd->main_security_ctx.host_or_ip);
        return 1;
      }
    }
    DBUG_PRINT("info",("Host: %s  ip: %s",
		       (thd->main_security_ctx.host ?
                        thd->main_security_ctx.host : "unknown host"),
		       (thd->main_security_ctx.ip ?
                        thd->main_security_ctx.ip : "unknown ip")));
    if (acl_check_host(thd->main_security_ctx.host, thd->main_security_ctx.ip))
    {
      my_error(ER_HOST_NOT_PRIVILEGED, MYF(0),
               thd->main_security_ctx.host_or_ip);
      return 1;
    }
  }
  else /* Hostname given means that the connection was on a socket */
  {
    DBUG_PRINT("info",("Host: %s", thd->main_security_ctx.host));
    thd->main_security_ctx.host_or_ip= thd->main_security_ctx.host;
    thd->main_security_ctx.ip= 0;
    /* Reset sin_addr */
    bzero((char*) &thd->remote, sizeof(thd->remote));
  }
  vio_keepalive(net->vio, TRUE);
  
  ulong server_capabilites;
  {
    /* buff[] needs to big enough to hold the server_version variable */
    char buff[SERVER_VERSION_LENGTH + 1 + SCRAMBLE_LENGTH + 1 + 64];
    server_capabilites= CLIENT_BASIC_FLAGS;

    if (opt_using_transactions)
      server_capabilites|= CLIENT_TRANSACTIONS;
#ifdef HAVE_COMPRESS
    server_capabilites|= CLIENT_COMPRESS;
#endif /* HAVE_COMPRESS */
#ifdef HAVE_OPENSSL
    if (ssl_acceptor_fd)
    {
      server_capabilites |= CLIENT_SSL;       /* Wow, SSL is available! */
      server_capabilites |= CLIENT_SSL_VERIFY_SERVER_CERT;
    }
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

    int2store(end, server_capabilites);
    /* write server characteristics: up to 16 bytes allowed */
    end[2]=(char) default_charset_info->number;
    int2store(end+3, thd->server_status);
    bzero(end+5, 13);
    end+= 18;
    /* write scramble tail */
    end= strmake(end, thd->scramble + SCRAMBLE_LENGTH_323, 
                 SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323) + 1;

    /* At this point we write connection message and read reply */
    if (net_write_command(net, (uchar) protocol_version, (uchar*) "", 0,
                          (uchar*) buff, (size_t) (end-buff)) ||
	(pkt_len= my_net_read(net)) == packet_error)
    {
      goto error;
    }
  }
#ifdef _CUSTOMCONFIG_
#include "_cust_sql_parse.h"
#endif
  if (thd->packet.alloc(thd->variables.net_buffer_length))
    return 1; /* The error is set by alloc(). */

  end= (char *)net->read_pos;
  /*
    In order to safely scan a head for '\0' string terminators
    we must keep track of how many bytes remain in the allocated
    buffer or we might read past the end of the buffer.
  */
  bytes_remaining_in_packet= pkt_len;
  
  /*
    Peek ahead on the client capability packet and determine which version of
    the protocol should be used.
  */
  DBUG_EXECUTE_IF("host_error_packet_length",
                  {
                    bytes_remaining_in_packet= 0;
                  };);
  if (bytes_remaining_in_packet < 2)
    goto error;
  
  thd->client_capabilities= uint2korr(end);

  /*
    Connector/J only sends client capabilities (4 bytes) before starting SSL
    negotiation so we don't have char_set and other information for client in
    packet read. In that case, skip reading those information. The below code 
    is patch for this.
  */
  if(bytes_remaining_in_packet == AUTH_PACKET_HEADER_SIZE_CONNJ_SSL &&
     (thd->client_capabilities & CLIENT_SSL))
  {
    thd->client_capabilities= uint4korr(end);
    thd->max_client_packet_length= global_system_variables.max_allowed_packet;
    charset_code= default_charset_info->number;
    end+= AUTH_PACKET_HEADER_SIZE_CONNJ_SSL;
    bytes_remaining_in_packet-= AUTH_PACKET_HEADER_SIZE_CONNJ_SSL;
    goto skip_to_ssl;
  }

  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
    packet_has_required_size= bytes_remaining_in_packet >= 
      AUTH_PACKET_HEADER_SIZE_PROTO_41;
  else
    packet_has_required_size= bytes_remaining_in_packet >=
      AUTH_PACKET_HEADER_SIZE_PROTO_40;
  
  if (!packet_has_required_size)
    goto error;
  
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    thd->client_capabilities= uint4korr(end);
    thd->max_client_packet_length= uint4korr(end + 4);
    charset_code= (uint)(uchar)*(end + 8);
    /*
      Skip 23 remaining filler bytes which have no particular meaning.
    */
    end+= AUTH_PACKET_HEADER_SIZE_PROTO_41;
    bytes_remaining_in_packet-= AUTH_PACKET_HEADER_SIZE_PROTO_41;
  }
  else
  {
    thd->client_capabilities= uint2korr(end);
    thd->max_client_packet_length= uint3korr(end + 2);
    end+= AUTH_PACKET_HEADER_SIZE_PROTO_40;
    bytes_remaining_in_packet-= AUTH_PACKET_HEADER_SIZE_PROTO_40;
    /**
      Old clients didn't have their own charset. Instead the assumption
      was that they used what ever the server used.
    */
    charset_code= default_charset_info->number;
  }

skip_to_ssl:

  DBUG_EXECUTE_IF("host_error_charset",
                  {
                    goto error;
                  };);
  DBUG_PRINT("info", ("client_character_set: %u", charset_code));
  if (thd_init_client_charset(thd, charset_code))
    goto error;
  thd->update_charset();

  /*
    Disable those bits which are not supported by the server.
    This is a precautionary measure, if the client lies. See Bug#27944.
  */
  thd->client_capabilities&= server_capabilites;

  if (thd->client_capabilities & CLIENT_IGNORE_SPACE)
    thd->variables.sql_mode|= MODE_IGNORE_SPACE;
#ifdef HAVE_OPENSSL
  DBUG_PRINT("info", ("client capabilities: %lu", thd->client_capabilities));
  
  /*
    If client requested SSL then we must stop parsing, try to switch to SSL,
    and wait for the client to send a new handshake packet.
    The client isn't expected to send any more bytes until SSL is initialized.
  */
  if (thd->client_capabilities & CLIENT_SSL)
  {
    /* Do the SSL layering. */
    if (!ssl_acceptor_fd)
      goto error;

    DBUG_PRINT("info", ("IO layer change in progress..."));
    if (sslaccept(ssl_acceptor_fd, net->vio, net->read_timeout))
    {
      DBUG_PRINT("error", ("Failed to accept new SSL connection"));
      goto error;
    }
    
    DBUG_PRINT("info", ("Reading user information over SSL layer"));
    if ((pkt_len= my_net_read(net)) == packet_error)
    {
      DBUG_PRINT("error", ("Failed to read user information (pkt_len= %lu)",
			   pkt_len));
      goto error;
    }
    /*
      A new packet was read and the statistics reflecting the remaining bytes
      in the packet must be updated.
    */
    bytes_remaining_in_packet= pkt_len;

    /*
      After the SSL handshake is performed the client resends the handshake
      packet but because of legacy reasons we chose not to parse the packet
      fields a second time and instead only assert the length of the packet.
    */
    if (thd->client_capabilities & CLIENT_PROTOCOL_41)
    {
      
      packet_has_required_size= bytes_remaining_in_packet >= 
        AUTH_PACKET_HEADER_SIZE_PROTO_41;
      end= (char *)net->read_pos + AUTH_PACKET_HEADER_SIZE_PROTO_41;
      bytes_remaining_in_packet -= AUTH_PACKET_HEADER_SIZE_PROTO_41;
    }
    else
    {
      packet_has_required_size= bytes_remaining_in_packet >= 
        AUTH_PACKET_HEADER_SIZE_PROTO_40;
      end= (char *)net->read_pos + AUTH_PACKET_HEADER_SIZE_PROTO_40;
      bytes_remaining_in_packet -= AUTH_PACKET_HEADER_SIZE_PROTO_40;
    }
  
    DBUG_EXECUTE_IF("host_error_SSL_layering",
                    {
                      packet_has_required_size= 0;
                    };);
    if (!packet_has_required_size)
      goto error;
  }
#endif /* HAVE_OPENSSL */

  if (thd->client_capabilities & CLIENT_INTERACTIVE)
    thd->variables.net_wait_timeout= thd->variables.net_interactive_timeout;
  if ((thd->client_capabilities & CLIENT_TRANSACTIONS) &&
      opt_using_transactions)
    net->return_status= &thd->server_status;

  /*
    The 4.0 and 4.1 versions of the protocol differ on how strings
    are terminated. In the 4.0 version, if a string is at the end
    of the packet, the string is not null terminated. Do not assume
    that the returned string is always null terminated.
  */
  get_proto_string_func_t get_string;

  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
    get_string= get_41_protocol_string;
  else
    get_string= get_40_protocol_string;

  user= get_string(&end, &bytes_remaining_in_packet, &user_len);
  DBUG_EXECUTE_IF("host_error_user",
                  {
                    user= NULL;
                  };);

  if (user == NULL)
    goto error;

  /*
    Old clients send a null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.
  */
  passwd_len= 0;
  passwd= NULL;

  if (thd->client_capabilities & CLIENT_SECURE_CONNECTION)
  {
    /*
      4.1+ password. First byte is password length.
    */
    passwd= get_length_encoded_string(&end, &bytes_remaining_in_packet,
                                      &passwd_len);
  }
  else
  {
    /*
      Old passwords are zero terminated strings.
    */
    passwd= get_string(&end, &bytes_remaining_in_packet, &passwd_len);
  }

  DBUG_EXECUTE_IF("host_error_password",
                  {
                    passwd= NULL;
                  };);

  if (passwd == NULL)
    goto error;

  db_len= 0;
  db= NULL;

  if (thd->client_capabilities & CLIENT_CONNECT_WITH_DB)
  {
    db= get_string(&end, &bytes_remaining_in_packet, &db_len);
    if (db == NULL)
      goto error;
  }

  char db_buff[NAME_LEN + 1];           // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];	// buffer to store user in utf8
  uint dummy_errors;

  /*
    Copy and convert the user and database names to the character set used
    by the server. Since 4.1 all database names are stored in UTF-8. Also,
    ensure that the names are properly null-terminated as this is relied
    upon later.
  */
  if (db)
  {
    db_len= copy_and_convert(db_buff, sizeof(db_buff)-1, system_charset_info,
                             db, db_len, thd->charset(), &dummy_errors);
    db_buff[db_len]= '\0';
    db= db_buff;
  }

  user_len= copy_and_convert(user_buff, sizeof(user_buff)-1,
                             system_charset_info, user, user_len,
                             thd->charset(), &dummy_errors);
  user_buff[user_len]= '\0';
  user= user_buff;

  /* If username starts and ends in "'", chop them off */
  if (user_len > 1 && user[0] == '\'' && user[user_len - 1] == '\'')
  {
    user[user_len-1]= 0;
    user++;
    user_len-= 2;
  }

  /*
    Clip username to allowed length in characters (not bytes).  This is
    mostly for backward compatibility.
  */
  {
    CHARSET_INFO *cs= system_charset_info;
    int           err;

    user_len= (uint) cs->cset->well_formed_len(cs, user, user + user_len,
                                               USERNAME_CHAR_LENGTH, &err);
    user[user_len]= '\0';
  }

  if (!(thd->main_security_ctx.user= my_strdup(user, MYF(MY_WME))))
    return 1; /* The error is set by my_strdup(). */

  if (!check_user(thd, COM_CONNECT, passwd, passwd_len, db, TRUE))
  {
    /*
      Call to reset_host_errors() should be made only when all sanity checks
      are done and connection is going to be a successful.
    */
    reset_host_errors(&thd->remote.sin_addr);
    return 0;
  }
  else
  {
    return 1;
  }
  
error:
  inc_host_errors(&thd->remote.sin_addr);
  my_error(ER_HANDSHAKE_ERROR, MYF(0));
  return 1;
}


/*
  Setup thread to be used with the current thread

  SYNOPSIS
    bool setup_connection_thread_globals()
    thd    Thread/connection handler

  RETURN
    0   ok
    1   Error (out of memory)
        In this case we will close the connection and increment status
*/

bool setup_connection_thread_globals(THD *thd)
{
  if (thd->store_globals())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES, 1);
    statistic_increment(aborted_connects,&LOCK_status);
    thread_scheduler.end_thread(thd, 0);
    return 1;                                   // Error
  }
  return 0;
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
  NET *net= &thd->net;
  int error;
  DBUG_ENTER("login_connection");
  DBUG_PRINT("info", ("login_connection called by thread %lu",
                      thd->thread_id));

  /* Use "connect_timeout" value during connection phase */
  my_net_set_read_timeout(net, connect_timeout);
  my_net_set_write_timeout(net, connect_timeout);

  error= check_connection(thd);
  net_end_statement(thd);

  if (error)
  {						// Wrong permissions
#ifdef __NT__
    if (vio_type(net->vio) == VIO_TYPE_NAMEDPIPE)
      my_sleep(1000);				/* must wait after eof() */
#endif
    statistic_increment(aborted_connects,&LOCK_status);
    DBUG_RETURN(1);
  }
  /* Connect completed, set read/write timeouts back to default */
  my_net_set_read_timeout(net, thd->variables.net_read_timeout);
  my_net_set_write_timeout(net, thd->variables.net_write_timeout);
  DBUG_RETURN(0);
}


/*
  Close an established connection

  NOTES
    This mainly updates status variables
*/

static void end_connection(THD *thd)
{
  NET *net= &thd->net;
  plugin_thdvar_cleanup(thd);
  if (thd->user_connect)
    decrease_user_connections(thd->user_connect);

  if (thd->killed || (net->error && net->vio != 0))
  {
    statistic_increment(aborted_threads,&LOCK_status);
  }

  if (net->error && net->vio != 0)
  {
    if (!thd->killed && thd->variables.log_warnings > 1)
    {
      Security_context *sctx= thd->security_ctx;

      sql_print_warning(ER(ER_NEW_ABORTING_CONNECTION),
                        thd->thread_id,(thd->db ? thd->db : "unconnected"),
                        sctx->user ? sctx->user : "unauthenticated",
                        sctx->host_or_ip,
                        (thd->main_da.is_error() ? thd->main_da.message() :
                         ER(ER_UNKNOWN_ERROR)));
    }
  }
}


/*
  Initialize THD to handle queries
*/

static void prepare_new_connection_state(THD* thd)
{
  Security_context *sctx= thd->security_ctx;

#ifdef __NETWARE__
  netware_reg_user(sctx->ip, sctx->user, "MySQL");
#endif

  if (thd->variables.max_join_size == HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;
  if (thd->client_capabilities & CLIENT_COMPRESS)
    thd->net.compress=1;				// Use compression

  /*
    Much of this is duplicated in create_embedded_thd() for the
    embedded server library.
    TODO: refactor this to avoid code duplication there
  */
  thd->version= refresh_version;
  thd->proc_info= 0;
  thd->command= COM_SLEEP;
  thd->set_time();
  thd->init_for_queries();

  if (sys_init_connect.value_length && !(sctx->master_access & SUPER_ACL))
  {
    execute_init_command(thd, &sys_init_connect, &LOCK_sys_init_connect);
    if (thd->is_error())
    {
      ulong packet_length;
      NET *net= &thd->net;

      sql_print_warning(ER(ER_NEW_ABORTING_CONNECTION),
                        thd->thread_id,
                        thd->db ? thd->db : "unconnected",
                        sctx->user ? sctx->user : "unauthenticated",
                        sctx->host_or_ip, "init_connect command failed");
      sql_print_warning("%s", thd->main_da.message());

      thd->lex->current_select= 0;
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
                 thd->thread_id,
                 thd->db ? thd->db : "unconnected",
                 sctx->user ? sctx->user : "unauthenticated",
                 sctx->host_or_ip, "init_connect command failed");

      thd->server_status&= ~SERVER_STATUS_CLEAR_SET;
      net_end_statement(thd);
      thd->killed = THD::KILL_CONNECTION;
      return;
    }

    thd->proc_info=0;
    thd->set_time();
    thd->init_for_queries();
  }
}


/*
  Thread handler for a connection

  SYNOPSIS
    handle_one_connection()
    arg		Connection object (THD)

  IMPLEMENTATION
    This function (normally) does the following:
    - Initialize thread
    - Initialize THD to be used with this thread
    - Authenticate user
    - Execute all queries sent on the connection
    - Take connection down
    - End thread  / Handle next connection using thread from thread cache
*/

pthread_handler_t handle_one_connection(void *arg)
{
  THD *thd= (THD*) arg;

  thd->thr_create_utime= my_micro_time();

  if (thread_scheduler.init_new_connection_thread())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES, 1);
    statistic_increment(aborted_connects,&LOCK_status);
    thread_scheduler.end_thread(thd,0);
    return 0;
  }

  /*
    If a thread was created to handle this connection:
    increment slow_launch_threads counter if it took more than
    slow_launch_time seconds to create the thread.
  */
  if (thd->prior_thr_create_utime)
  {
    ulong launch_time= (ulong) (thd->thr_create_utime -
                                thd->prior_thr_create_utime);
    if (launch_time >= slow_launch_time*1000000L)
      statistic_increment(slow_launch_threads, &LOCK_status);
    thd->prior_thr_create_utime= 0;
  }

  /*
    handle_one_connection() is normally the only way a thread would
    start and would always be on the very high end of the stack ,
    therefore, the thread stack always starts at the address of the
    first local variable of handle_one_connection, which is thd. We
    need to know the start of the stack so that we could check for
    stack overruns.
  */
  thd->thread_stack= (char*) &thd;
  if (setup_connection_thread_globals(thd))
    return 0;

  for (;;)
  {
    NET *net= &thd->net;

    lex_start(thd);
    if (login_connection(thd))
      goto end_thread;

    prepare_new_connection_state(thd);

    while (!net->error && net->vio != 0 &&
           !(thd->killed == THD::KILL_CONNECTION))
    {
      if (do_command(thd))
	break;
    }
    end_connection(thd);
   
end_thread:
    close_connection(thd, 0, 1);
    if (thread_scheduler.end_thread(thd,1))
      return 0;                                 // Probably no-threads

    /*
      If end_thread() returns, we are either running with
      thread-handler=no-threads or this thread has been schedule to
      handle the next connection.
    */
    thd= current_thd;
    thd->thread_stack= (char*) &thd;
  }
}
#endif /* EMBEDDED_LIBRARY */

