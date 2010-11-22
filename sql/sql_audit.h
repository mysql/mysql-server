#ifndef SQL_AUDIT_INCLUDED
#define SQL_AUDIT_INCLUDED

/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <mysql/plugin_audit.h>
#include "sql_class.h"

extern unsigned long mysql_global_audit_mask[];


extern void mysql_audit_initialize();
extern void mysql_audit_finalize();


extern void mysql_audit_init_thd(THD *thd);
extern void mysql_audit_free_thd(THD *thd);
extern void mysql_audit_acquire_plugins(THD *thd, uint event_class);


extern void mysql_audit_notify(THD *thd, uint event_class,
                               uint event_subtype, ...);
extern void mysql_audit_release(THD *thd);

#define MAX_USER_HOST_SIZE 512
static inline uint make_user_name(THD *thd, char *buf)
{
  Security_context *sctx= thd->security_ctx;
  return strxnmov(buf, MAX_USER_HOST_SIZE,
                  sctx->priv_user[0] ? sctx->priv_user : "", "[",
                  sctx->user ? sctx->user : "", "] @ ",
                  sctx->host ? sctx->host : "", " [",
                  sctx->ip ? sctx->ip : "", "]", NullS) - buf;
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
    CHARSET_INFO *clientcs= thd ? thd->variables.character_set_client
                                : global_system_variables.character_set_client;

    mysql_audit_notify(thd, MYSQL_AUDIT_GENERAL_CLASS, MYSQL_AUDIT_GENERAL_LOG,
                       0, time, user, userlen, cmd, cmdlen,
                       query, querylen, clientcs, 0);
  }
#endif
}

/**
  Call audit plugins of GENERAL audit class.
  event_subtype should be set to one of:
    MYSQL_AUDIT_GENERAL_ERROR
    MYSQL_AUDIT_GENERAL_RESULT
  
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
    const char *user;
    uint userlen;
    char user_buff[MAX_USER_HOST_SIZE];
    CSET_STRING query;
    ha_rows rows;

    if (thd)
    {
      query= thd->query_string;
      user= user_buff;
      userlen= make_user_name(thd, user_buff);
      rows= thd->warning_info->current_row_for_warning();
    }
    else
    {
      user= 0;
      userlen= 0;
      rows= 0;
    }

    mysql_audit_notify(thd, MYSQL_AUDIT_GENERAL_CLASS, event_subtype,
                       error_code, time, user, userlen, msg, msglen,
                       query.str(), query.length(), query.charset(), rows);
  }
#endif
}


#endif /* SQL_AUDIT_INCLUDED */
