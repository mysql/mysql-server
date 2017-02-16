#ifndef SQL_AUDIT_INCLUDED
#define SQL_AUDIT_INCLUDED

/* Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

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

extern unsigned long mysql_global_audit_mask[];


extern void mysql_audit_initialize();
extern void mysql_audit_finalize();


extern void mysql_audit_init_thd(THD *thd);
extern void mysql_audit_free_thd(THD *thd);
extern void mysql_audit_acquire_plugins(THD *thd, ulong *event_class_mask);


#ifndef EMBEDDED_LIBRARY
extern void mysql_audit_notify(THD *thd, uint event_class,
                               uint event_subtype, ...);

static inline bool mysql_audit_general_enabled()
{
  return mysql_global_audit_mask[0] & MYSQL_AUDIT_GENERAL_CLASSMASK;
}

static inline bool mysql_audit_table_enabled()
{
  return mysql_global_audit_mask[0] & MYSQL_AUDIT_TABLE_CLASSMASK;
}

#else
static inline void mysql_audit_notify(THD *thd, uint event_class,
                                      uint event_subtype, ...) { }
#define mysql_audit_general_enabled() 0
#define mysql_audit_table_enabled() 0
#endif
extern void mysql_audit_release(THD *thd);

#define MAX_USER_HOST_SIZE 512
static inline uint make_user_name(THD *thd, char *buf)
{
  const Security_context *sctx= thd->security_ctx;
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
  if (mysql_audit_general_enabled())
  {
    CHARSET_INFO *clientcs= thd ? thd->variables.character_set_client
                                : global_system_variables.character_set_client;
    const char *db= thd ? thd->db : "";
    size_t db_length= thd ? thd->db_length : 0;

    mysql_audit_notify(thd, MYSQL_AUDIT_GENERAL_CLASS, MYSQL_AUDIT_GENERAL_LOG,
                       0, time, user, userlen, cmd, cmdlen,
                       query, querylen, clientcs, (ha_rows) 0,
                       db, db_length);
  }
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
  if (mysql_audit_general_enabled())
  {
    time_t time= my_time(0);
    uint msglen= msg ? strlen(msg) : 0;
    const char *user;
    uint userlen;
    char user_buff[MAX_USER_HOST_SIZE];
    CSET_STRING query;
    ha_rows rows;
    const char *db;
    size_t db_length;

    if (thd)
    {
      query= thd->query_string;
      user= user_buff;
      userlen= make_user_name(thd, user_buff);
      rows= thd->warning_info->current_row_for_warning();
      db= thd->db;
      db_length= thd->db_length;
    }
    else
    {
      user= 0;
      userlen= 0;
      rows= 0;
      db= "";
      db_length= 0;
    }

    mysql_audit_notify(thd, MYSQL_AUDIT_GENERAL_CLASS, event_subtype,
                       error_code, time, user, userlen, msg, msglen,
                       query.str(), query.length(), query.charset(), rows,
                       db, db_length);
  }
}

#define MYSQL_AUDIT_NOTIFY_CONNECTION_CONNECT(thd) mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_CONNECT,\
  (thd)->stmt_da->is_error() ? (thd)->stmt_da->sql_errno() : 0,\
  (thd)->thread_id, (thd)->security_ctx->user,\
  (thd)->security_ctx->user ? strlen((thd)->security_ctx->user) : 0,\
  (thd)->security_ctx->priv_user, strlen((thd)->security_ctx->priv_user),\
  (thd)->security_ctx->external_user,\
  (thd)->security_ctx->external_user ?\
    strlen((thd)->security_ctx->external_user) : 0,\
  (thd)->security_ctx->proxy_user, strlen((thd)->security_ctx->proxy_user),\
  (thd)->security_ctx->host,\
  (thd)->security_ctx->host ? strlen((thd)->security_ctx->host) : 0,\
  (thd)->security_ctx->ip,\
  (thd)->security_ctx->ip ? strlen((thd)->security_ctx->ip) : 0,\
  (thd)->db, (thd)->db ? strlen((thd)->db) : 0)

#define MYSQL_AUDIT_NOTIFY_CONNECTION_DISCONNECT(thd, errcode)\
  mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_DISCONNECT,\
  (errcode), (thd)->thread_id, (thd)->security_ctx->user,\
        (thd)->security_ctx->user ? strlen((thd)->security_ctx->user) : 0,\
         0, 0, 0, 0, 0, 0, (thd)->security_ctx->host,\
         (thd)->security_ctx->host ? strlen((thd)->security_ctx->host) : 0,\
         0, 0, 0, 0)

#define MYSQL_AUDIT_NOTIFY_CONNECTION_CHANGE_USER(thd) mysql_audit_notify(\
  (thd), MYSQL_AUDIT_CONNECTION_CLASS, MYSQL_AUDIT_CONNECTION_CHANGE_USER,\
  (thd)->stmt_da->is_error() ? (thd)->stmt_da->sql_errno() : 0,\
  (thd)->thread_id, (thd)->security_ctx->user,\
  (thd)->security_ctx->user ? strlen((thd)->security_ctx->user) : 0,\
  (thd)->security_ctx->priv_user, strlen((thd)->security_ctx->priv_user),\
  (thd)->security_ctx->external_user,\
  (thd)->security_ctx->external_user ?\
    strlen((thd)->security_ctx->external_user) : 0,\
  (thd)->security_ctx->proxy_user, strlen((thd)->security_ctx->proxy_user),\
  (thd)->security_ctx->host,\
  (thd)->security_ctx->host ? strlen((thd)->security_ctx->host) : 0,\
  (thd)->security_ctx->ip,\
  (thd)->security_ctx->ip ? strlen((thd)->security_ctx->ip) : 0,\
  (thd)->db, (thd)->db ? strlen((thd)->db) : 0)

static inline
void mysql_audit_external_lock(THD *thd, TABLE_SHARE *share, int lock)
{
  if (lock != F_UNLCK && mysql_audit_table_enabled())
  {
    const Security_context *sctx= thd->security_ctx;
    mysql_audit_notify(thd, MYSQL_AUDIT_TABLE_CLASS, MYSQL_AUDIT_TABLE_LOCK,
                       (int)(lock == F_RDLCK), (ulong)thd->thread_id,
                       sctx->user, sctx->priv_user, sctx->priv_host,
                       sctx->external_user, sctx->proxy_user, sctx->host,
                       sctx->ip, share->db.str, (uint)share->db.length,
                       share->table_name.str, (uint)share->table_name.length,
                       0,0,0,0);
  }
}

static inline
void mysql_audit_create_table(TABLE *table)
{
  if (mysql_audit_table_enabled())
  {
    THD *thd= table->in_use;
    const TABLE_SHARE *share= table->s;
    const Security_context *sctx= thd->security_ctx;
    mysql_audit_notify(thd, MYSQL_AUDIT_TABLE_CLASS, MYSQL_AUDIT_TABLE_CREATE,
                       0, (ulong)thd->thread_id,
                       sctx->user, sctx->priv_user, sctx->priv_host,
                       sctx->external_user, sctx->proxy_user, sctx->host,
                       sctx->ip, share->db.str, (uint)share->db.length,
                       share->table_name.str, (uint)share->table_name.length,
                       0,0,0,0);
  }
}

static inline
void mysql_audit_drop_table(THD *thd, TABLE_LIST *table)
{
  if (mysql_audit_table_enabled())
  {
    const Security_context *sctx= thd->security_ctx;
    mysql_audit_notify(thd, MYSQL_AUDIT_TABLE_CLASS, MYSQL_AUDIT_TABLE_DROP,
                       0, (ulong)thd->thread_id,
                       sctx->user, sctx->priv_user, sctx->priv_host,
                       sctx->external_user, sctx->proxy_user, sctx->host,
                       sctx->ip, table->db, (uint)table->db_length,
                       table->table_name, (uint)table->table_name_length,
                       0,0,0,0);
  }
}

static inline
void mysql_audit_rename_table(THD *thd, const char *old_db, const char *old_tb,
                              const char *new_db, const char *new_tb)
{
  if (mysql_audit_table_enabled())
  {
    const Security_context *sctx= thd->security_ctx;
    mysql_audit_notify(thd, MYSQL_AUDIT_TABLE_CLASS, MYSQL_AUDIT_TABLE_RENAME,
                       0, (ulong)thd->thread_id,
                       sctx->user, sctx->priv_user, sctx->priv_host,
                       sctx->external_user, sctx->proxy_user, sctx->host,
                       sctx->ip,
                       old_db, (uint)strlen(old_db), old_tb, (uint)strlen(old_tb),
                       new_db, (uint)strlen(new_db), new_tb, (uint)strlen(new_tb));
  }
}

static inline
void mysql_audit_alter_table(THD *thd, TABLE_LIST *table)
{
  if (mysql_audit_table_enabled())
  {
    const Security_context *sctx= thd->security_ctx;
    mysql_audit_notify(thd, MYSQL_AUDIT_TABLE_CLASS, MYSQL_AUDIT_TABLE_ALTER,
                       0, (ulong)thd->thread_id,
                       sctx->user, sctx->priv_user, sctx->priv_host,
                       sctx->external_user, sctx->proxy_user, sctx->host,
                       sctx->ip, table->db, (uint)table->db_length,
                       table->table_name, (uint)table->table_name_length,
                       0,0,0,0);
  }
}

#endif /* SQL_AUDIT_INCLUDED */
