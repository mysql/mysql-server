#ifndef SQL_AUDIT_INCLUDED
#define SQL_AUDIT_INCLUDED

/* Copyright (c) 2007, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#include <my_global.h>

#include <mysql/plugin_audit.h>
#include "sql_class.h"
#include "sql_rewrite.h"

extern unsigned long mysql_global_audit_mask[];


extern void mysql_audit_initialize();
extern void mysql_audit_finalize();


extern void mysql_audit_init_thd(THD *thd);
extern void mysql_audit_free_thd(THD *thd);
extern void mysql_audit_acquire_plugins(THD *thd, uint event_class);


#ifndef EMBEDDED_LIBRARY
extern void mysql_audit_notify(THD *thd, uint event_class,
                               uint event_subtype, ...);
#else
#define mysql_audit_notify(...)
#endif
extern void mysql_audit_release(THD *thd);

#define MAX_USER_HOST_SIZE 512
static inline uint make_user_name(THD *thd, char *buf)
{
  Security_context *sctx= thd->security_ctx;
  return strxnmov(buf, MAX_USER_HOST_SIZE,
                  sctx->priv_user[0] ? sctx->priv_user : "", "[",
                  sctx->user ? sctx->user : "", "] @ ",
                  sctx->get_host()->length() ? sctx->get_host()->ptr() :
                  "", " [", sctx->get_ip()->length() ? sctx->get_ip()->ptr() :
                  "", "]", NullS) - buf;
}

/**
  Call audit plugins of GENERAL audit class, MYSQL_AUDIT_GENERAL_LOG subtype.
  
  @param[in] thd
  @param[in] time             time that event occurred
  @param[in] user             User name
  @param[in] userlen          User name length
  @param[in] cmd              Command name
  @param[in] cmdlen           Command name length
  @param[in] query            Query string
  @param[in] querylen         Query string length
*/
 
static inline
void mysql_audit_general_log(THD *thd, time_t time,
                             const char *user, uint userlen,
                             const char *cmd, uint cmdlen,
                             const char *query, uint querylen)
{
#ifndef EMBEDDED_LIBRARY
  if (mysql_global_audit_mask[0] & MYSQL_AUDIT_GENERAL_CLASSMASK)
  {
    MYSQL_LEX_STRING sql_command, ip, host, external_user;
    static MYSQL_LEX_STRING empty= { C_STRING_WITH_LEN("") };

    if (thd)
    {
      ip.str= (char *) thd->security_ctx->get_ip()->ptr();
      ip.length= thd->security_ctx->get_ip()->length();
      host.str= (char *) thd->security_ctx->get_host()->ptr();
      host.length= thd->security_ctx->get_host()->length();
      external_user.str= (char *) thd->security_ctx->get_external_user()->ptr();
      external_user.length= thd->security_ctx->get_external_user()->length();
      sql_command.str= (char *) sql_statement_names[thd->lex->sql_command].str;
      sql_command.length= sql_statement_names[thd->lex->sql_command].length;
    }
    else
    {
      ip= empty;
      host= empty;
      external_user= empty;
      sql_command= empty;
    }
    const CHARSET_INFO *clientcs= thd ? thd->variables.character_set_client
      : global_system_variables.character_set_client;

    mysql_audit_notify(thd, MYSQL_AUDIT_GENERAL_CLASS, MYSQL_AUDIT_GENERAL_LOG,
                       0, time, user, userlen, cmd, cmdlen, query, querylen,
                       clientcs, 0, sql_command, host, external_user, ip);
  }
#endif
}


/**
  Call audit plugins of GENERAL audit class.
  event_subtype should be set to one of:
    MYSQL_AUDIT_GENERAL_ERROR
    MYSQL_AUDIT_GENERAL_RESULT
    MYSQL_AUDIT_GENERAL_STATUS
  
  @param[in] thd
  @param[in] event_subtype    Type of general audit event.
  @param[in] error_code       Error code
  @param[in] msg              Message
*/
static inline
void mysql_audit_general(THD *thd, uint event_subtype,
                         int error_code, const char *msg)
{
#ifndef EMBEDDED_LIBRARY
  if (mysql_global_audit_mask[0] & MYSQL_AUDIT_GENERAL_CLASSMASK)
  {
    time_t time= my_time(0);
    uint msglen= msg ? strlen(msg) : 0;
    uint userlen;
    const char *user;
    char user_buff[MAX_USER_HOST_SIZE];
    CSET_STRING query;
    MYSQL_LEX_STRING ip, host, external_user, sql_command;
    ha_rows rows;
    static MYSQL_LEX_STRING empty= { C_STRING_WITH_LEN("") };

    if (thd)
    {
      if (!thd->rewritten_query.length())
        mysql_rewrite_query(thd);
      if (thd->rewritten_query.length())
        query= CSET_STRING((char *) thd->rewritten_query.ptr(),
                           thd->rewritten_query.length(),
                           thd->rewritten_query.charset());
      else
        query= thd->query_string;
      user= user_buff;
      userlen= make_user_name(thd, user_buff);
      rows= thd->get_stmt_da()->current_row_for_warning();
      ip.str= (char *) thd->security_ctx->get_ip()->ptr();
      ip.length= thd->security_ctx->get_ip()->length();
      host.str= (char *) thd->security_ctx->get_host()->ptr();
      host.length= thd->security_ctx->get_host()->length();
      external_user.str= (char *) thd->security_ctx->get_external_user()->ptr();
      external_user.length= thd->security_ctx->get_external_user()->length();
      sql_command.str= (char *) sql_statement_names[thd->lex->sql_command].str;
      sql_command.length= sql_statement_names[thd->lex->sql_command].length;
    }
    else
    {
      user= 0;
      userlen= 0;
      ip= empty;
      host= empty;
      external_user= empty;
      sql_command= empty;
      rows= 0;
    }

    mysql_audit_notify(thd, MYSQL_AUDIT_GENERAL_CLASS, event_subtype,
                       error_code, time, user, userlen, msg, msglen,
                       query.str(), query.length(), query.charset(), rows,
                       sql_command, host, external_user, ip);
  }
#endif
}

#define MYSQL_AUDIT_NOTIFY_CONNECTION_CONNECT(thd) mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_CONNECT,\
  (thd)->get_stmt_da()->is_error() ? (thd)->get_stmt_da()->sql_errno() : 0,\
  (thd)->thread_id, (thd)->security_ctx->user,\
  (thd)->security_ctx->user ? strlen((thd)->security_ctx->user) : 0,\
  (thd)->security_ctx->priv_user, strlen((thd)->security_ctx->priv_user),\
  (thd)->security_ctx->get_external_user()->ptr(),\
  (thd)->security_ctx->get_external_user()->length(),\
  (thd)->security_ctx->proxy_user, strlen((thd)->security_ctx->proxy_user),\
  (thd)->security_ctx->get_host()->ptr(),\
  (thd)->security_ctx->get_host()->length(),\
  (thd)->security_ctx->get_ip()->ptr(),\
  (thd)->security_ctx->get_ip()->length(),\
  (thd)->db, (thd)->db ? strlen((thd)->db) : 0)

#define MYSQL_AUDIT_NOTIFY_CONNECTION_DISCONNECT(thd, errcode)\
  mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_DISCONNECT,\
  (errcode), (thd)->thread_id, "", 0, "", 0, "", 0, "", 0, "", 0, "", 0, "", 0)

#define MYSQL_AUDIT_NOTIFY_CONNECTION_CHANGE_USER(thd) mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_CHANGE_USER,\
  (thd)->get_stmt_da()->is_error() ? (thd)->get_stmt_da()->sql_errno() : 0,\
  (thd)->thread_id, (thd)->security_ctx->user,\
  (thd)->security_ctx->user ? strlen((thd)->security_ctx->user) : 0,\
  (thd)->security_ctx->priv_user, strlen((thd)->security_ctx->priv_user),\
  (thd)->security_ctx->get_external_user()->ptr(),\
  (thd)->security_ctx->get_external_user()->length(),\
  (thd)->security_ctx->proxy_user, strlen((thd)->security_ctx->proxy_user),\
  (thd)->security_ctx->get_host()->ptr(),\
  (thd)->security_ctx->get_host()->length(),\
  (thd)->security_ctx->get_ip()->ptr(),\
  (thd)->security_ctx->get_ip()->length(),\
  (thd)->db, (thd)->db ? strlen((thd)->db) : 0)

#endif /* SQL_AUDIT_INCLUDED */
