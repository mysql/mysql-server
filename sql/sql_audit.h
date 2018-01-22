#ifndef SQL_AUDIT_INCLUDED
#define SQL_AUDIT_INCLUDED

/* Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  Audit API event to string expanding macro.
*/
#define AUDIT_EVENT(x) x, #x

bool is_audit_plugin_class_active(THD *thd, unsigned long event_class);
bool is_global_audit_mask_set();

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
int mysql_audit_acquire_plugins(THD *thd, mysql_event_class_t event_class,
                                unsigned long event_subclass);
void mysql_audit_release(THD *thd);

/**
  Call audit plugins of GENERAL audit class.

  @param[in] thd              Current thread data.
  @param[in] subclass         Type of general audit event.
  @param[in] subclass_name    Subclass name.
  @param[in] error_code       Error code
  @param[in] msg              Message
  @param[in] msg_len          Message length.

  @result Value returned is not taken into consideration by the server.
*/
int mysql_audit_notify(THD *thd, mysql_event_general_subclass_t subclass,
                       const char* subclass_name,
                       int error_code, const char *msg, size_t msg_len);
/**
  Call audit plugins of GENERAL LOG audit class.

  @param[in] thd    Current thread data.
  @param[in] cmd    Command text.
  @param[in] cmdlen Command text length.

  @result Value returned is not taken into consideration by the server.
*/
inline static
int mysql_audit_general_log(THD *thd, const char *cmd, size_t cmdlen)
{
  return mysql_audit_notify(thd, AUDIT_EVENT(MYSQL_AUDIT_GENERAL_LOG),
                            0, cmd, cmdlen);
}

/**
  Call audit plugins of CONNECTION audit class.

  @param[in] thd              Current thread context.
  @param[in] subclass         Type of the connection audit event.
  @param[in] subclass_name    Name of the subclass.
  @param[in] errcode          Error code.

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd, mysql_event_connection_subclass_t subclass,
                       const char *subclass_name, int errcode);

/**
  Call audit plugins of PARSE audit class.

  @param[in]  thd             Current thread context.
  @param[in]  subclass        Type of the parse audit event.
  @param[in]  subclass_name   Name of the subclass.
  @param[out] flags           Rewritten query flags.
  @param[out] rewritten_query Rewritten query

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd, mysql_event_parse_subclass_t subclass,
                       const char* subclass_name,
                       mysql_event_parse_rewrite_plugin_flag *flags,
                       LEX_CSTRING *rewritten_query);

/**
  Call audit plugins of AUTHORIZATION audit class.

  @param[in] thd
  @param[in] subclass         Type of the connection audit event.
  @param[in] subclass_name    Name of the subclass.
  @param[in] database         object database
  @param[in] database_length  object database length
  @param[in] name             object name
  @param[in] name_length      object name length

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd, mysql_event_authorization_subclass_t subclass,
                       const char *subclass_name,
                       const char *database, unsigned int database_length,
                       const char *name, unsigned int name_length);
/**
  Call audit plugins of TABLE ACCESS audit class events for all tables
  available in the list.

  Event subclass value depends on the thd->lex->sql_command value.

  The event is generated for 'USER' and 'SYS' tables only.

  @param[in] thd    Current thread data.
  @param[in] table  Connected list of tables, for which event is generated.

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_table_access_notify(THD *thd, TABLE_LIST *table);

/**
  Call audit plugins of GLOBAL VARIABLE audit class.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the global variable audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] name          Name of the variable.
  @param[in] value         Textual value of the variable.
  @param[in] value_length  Textual value length.

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd, mysql_event_global_variable_subclass_t subclass,
                       const char *subclass_name,
                       const char *name,
                       const char *value, const unsigned int value_length);
/**
  Call audit plugins of SERVER STARTUP audit class.

  @param[in] subclass Type of the server startup audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] argv     Array of program arguments.
  @parma[in] argc     Program arguments array length.

  @result 0 - continue server start, otherwise abort.
*/
int mysql_audit_notify(mysql_event_server_startup_subclass_t subclass,
                       const char *subclass_name,
                       const char **argv,
                       unsigned int argc);

/**
  Call audit plugins of SERVER SHUTDOWN audit class.

  @param[in] subclass  Type of the server abort audit event.
  @param[in] reason    Reason code of the shutdown.
  @param[in] exit_code Abort exit code.

  @result Value returned is not taken into consideration by the server.
*/
int mysql_audit_notify(mysql_event_server_shutdown_subclass_t subclass,
                       mysql_server_shutdown_reason_t reason, int exit_code);

/**
  Call audit plugins of AUTHORIZATION audit class.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the authorization audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] database      Database name.
  @param[in] table         Table name.
  @param[in] object        Object name associated with the authorization event.

  @result 0 - continue server flow, otherwise abort.
*/
/*
  Function commented out. No Audit API calls yet.

int mysql_audit_notify(THD *thd,
                       mysql_event_authorization_subclass_t subclass,
                       const char *subclass_name,
                       const char *database,
                       const char *table,
                       const char *object);
*/
/**
  Call audit plugins of CONNECTION audit class.

  Internal connection info is extracted from the thd object.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the connection audit event.
  @param[in] subclass_name Name of the subclass.

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd, mysql_event_connection_subclass_t subclass,
                       const char *subclass_name);

/**
  Call audit plugins of COMMAND audit class.

  Internal connection info is extracted from the thd object.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the command audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] command       Command id value.
  @param[in] command_text  Command string value.

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd, mysql_event_command_subclass_t subclass,
                       const char *subclass_name,
                       enum_server_command command,
                       const char *command_text);
/**
  Call audit plugins of QUERY audit class.

  Internal query info is extracted from the thd object.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the query audit event.
  @param[in] subclass_name Name of the subclass.

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd, mysql_event_query_subclass_t subclass,
                       const char *subclass_name);

/**
  Call audit plugins of STORED PROGRAM audit class.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the stored program audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] database      Stored program database name.
  @param[in] name          Name of the stored program.
  @param[in] parameters    Parameters of the stored program execution.

  @result 0 - continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd,
                       mysql_event_stored_program_subclass_t subclass,
                       const char *subclass_name,
                       const char *database,
                       const char *name,
                       void *parameters);

#endif /* !EMBEDDED_LIBRARY */
#endif /* SQL_AUDIT_INCLUDED */
