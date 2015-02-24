#ifndef SQL_AUDIT_INCLUDED
#define SQL_AUDIT_INCLUDED

/* Copyright (c) 2007, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "mysql/plugin_audit.h"
#include "sql_security_ctx.h"       // Security_context


static const size_t MAX_USER_HOST_SIZE= 512;

bool is_any_audit_plugin_active(THD *thd);
static inline size_t make_user_name(Security_context *sctx, char *buf)
{
  LEX_CSTRING sctx_user= sctx->user();
  LEX_CSTRING sctx_host= sctx->host();
  LEX_CSTRING sctx_ip= sctx->ip();
  LEX_CSTRING sctx_priv_user= sctx->priv_user();
  return static_cast<size_t>(strxnmov(buf, MAX_USER_HOST_SIZE,
                                      sctx_priv_user.str[0] ?
                                        sctx_priv_user.str : "", "[",
                                      sctx_user.length ? sctx_user.str :
                                                         "", "] @ ",
                                      sctx_host.length ? sctx_host.str :
                                                         "", " [",
                                      sctx_ip.length ? sctx_ip.str : "", "]",
                                      NullS)
                             - buf);
}

#ifndef EMBEDDED_LIBRARY
struct st_plugin_int;

int initialize_audit_plugin(st_plugin_int *plugin);
int finalize_audit_plugin(st_plugin_int *plugin);

void mysql_audit_initialize();
void mysql_audit_finalize();

void mysql_audit_init_thd(THD *thd);
void mysql_audit_free_thd(THD *thd);
void mysql_audit_acquire_plugins(THD *thd, uint event_class);

void mysql_audit_notify(THD *thd, uint event_class,
                        uint event_subtype, ...);
void mysql_audit_release(THD *thd);

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
void mysql_audit_general_log(THD *thd, const char *cmd, size_t cmdlen);

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
void mysql_audit_general(THD *thd, uint event_subtype,
                         int error_code, const char *msg);

#define MYSQL_AUDIT_NOTIFY_CONNECTION_CONNECT(thd) mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_CONNECT,\
  (thd)->get_stmt_da()->is_error() ? (thd)->get_stmt_da()->mysql_errno() : 0,\
  (thd)->thread_id(), (thd)->security_context()->user().str,\
  (thd)->security_context()->user().length,\
  (thd)->security_context()->priv_user().str,\
  (thd)->security_context()->priv_user().length,\
  (thd)->security_context()->external_user().str,\
  (thd)->security_context()->external_user().length,\
  (thd)->security_context()->proxy_user().str,\
  (thd)->security_context()->proxy_user().length,\
  (thd)->security_context()->host().str,\
  (thd)->security_context()->host().length,\
  (thd)->security_context()->ip().str,\
  (thd)->security_context()->ip().length,\
  (thd)->db().str, (thd)->db().length)

#define MYSQL_AUDIT_NOTIFY_CONNECTION_DISCONNECT(thd, errcode)\
  mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_DISCONNECT,\
  (errcode), (thd)->thread_id(),\
  (thd)->security_context()->user().str,\
  (thd)->security_context()->user().length,\
  (thd)->security_context()->priv_user().str,\
  (thd)->security_context()->priv_user().length,\
  (thd)->security_context()->external_user().str,\
  (thd)->security_context()->external_user().length,\
  (thd)->security_context()->proxy_user().str,\
  (thd)->security_context()->proxy_user().length,\
  (thd)->security_context()->host().str,\
  (thd)->security_context()->host().length,\
  (thd)->security_context()->ip().str,\
  (thd)->security_context()->ip().length,\
  (thd)->db().str, (thd)->db().length)


#define MYSQL_AUDIT_NOTIFY_CONNECTION_CHANGE_USER(thd) mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_CHANGE_USER,\
  (thd)->get_stmt_da()->is_error() ? (thd)->get_stmt_da()->mysql_errno() : 0,\
  (thd)->thread_id(), (thd)->security_context()->user().str,\
  (thd)->security_context()->user().length,\
  (thd)->security_context()->priv_user().str,\
  (thd)->security_context()->priv_user().length,\
  (thd)->security_context()->external_user().str,\
  (thd)->security_context()->external_user().length,\
  (thd)->security_context()->proxy_user().str,\
  (thd)->security_context()->proxy_user().length,\
  (thd)->security_context()->host().str,\
  (thd)->security_context()->host().length,\
  (thd)->security_context()->ip().str,\
  (thd)->security_context()->ip().length,\
  (thd)->db().str, (thd)->db().length)

#endif // !EMBEDDED_LIBRARY

#endif /* SQL_AUDIT_INCLUDED */
