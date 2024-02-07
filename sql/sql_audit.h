#ifndef SQL_AUDIT_INCLUDED
#define SQL_AUDIT_INCLUDED

/* Copyright (c) 2007, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string.h>

#include "lex_string.h"
#include "my_command.h"
#include "mysql/plugin_audit.h"
#include "sql/error_handler.h"

#include "mysql/components/services/defs/event_tracking_authentication_defs.h"
#include "mysql/components/services/defs/event_tracking_command_defs.h"
#include "mysql/components/services/defs/event_tracking_common_defs.h"
#include "mysql/components/services/defs/event_tracking_connection_defs.h"
#include "mysql/components/services/defs/event_tracking_general_defs.h"
#include "mysql/components/services/defs/event_tracking_global_variable_defs.h"
#include "mysql/components/services/defs/event_tracking_lifecycle_defs.h"
#include "mysql/components/services/defs/event_tracking_message_defs.h"
#include "mysql/components/services/defs/event_tracking_parse_defs.h"
#include "mysql/components/services/defs/event_tracking_query_defs.h"
#include "mysql/components/services/defs/event_tracking_stored_program_defs.h"
#include "mysql/components/services/defs/event_tracking_table_access_defs.h"

#include "sql/sql_event_tracking_to_audit_event_mapping.h"

class THD;
class Security_context;
class Table_ref;

static const size_t MAX_USER_HOST_SIZE = 512;

class Event_tracking_information;
struct st_mysql_event_generic {
  Event_tracking_class event_class;
  const void *event;
  const Event_tracking_information *event_information;
};

struct st_mysql_event_plugin_generic {
  mysql_event_class_t event_class;
  const void *event;
};

/**
  Audit API event to string expanding macro.
*/
#define AUDIT_EVENT(x) x, #x

bool is_audit_plugin_class_active(THD *thd, unsigned long event_class);
bool is_global_audit_mask_set();

size_t make_user_name(Security_context *sctx, char *buf);

struct st_plugin_int;

int initialize_audit_plugin(st_plugin_int *plugin);
int finalize_audit_plugin(st_plugin_int *plugin);

void mysql_audit_initialize();
void mysql_audit_finalize();

void mysql_audit_init_thd(THD *thd);
void mysql_audit_free_thd(THD *thd);
int mysql_audit_acquire_plugins(THD *thd, mysql_event_class_t event_class,
                                unsigned long event_subclass,
                                bool check_audited = true);
void mysql_audit_release(THD *thd);

/**
  Enable auditing of the specified THD.

  @param[in] thd THD whose auditing capability is turned on.
*/
void mysql_audit_enable_auditing(THD *thd);

/**
  Notify consumers of AUTHENTICATION event tracking events.

  @param[in] thd                    Current thread data.
  @param[in] subclass               Type of the authentication audit event.
  @param[in] subclass_name          Name of the subclass.
  @param[in] status                 Status of the event.
  @param[in] user                   Name of the user.
  @param[in] host                   Name of the host.
  @param[in] authentication_plugin  Current authentication plugin for user.
  @param[in] is_role                Whether given AuthID is a role or not
  @param[in] new_user               Name of the new user - In case of rename
  @param[in] new_host               Name of the new host - In case of rename

  @return 0 continue server flow, otherwise abort.
*/
int mysql_event_tracking_authentication_notify(
    THD *thd, mysql_event_tracking_authentication_subclass_t subclass,
    const char *subclass_name, int status, const char *user, const char *host,
    const char *authentication_plugin, bool is_role, const char *new_user,
    const char *new_host);

/**
  Notify consumers of COMMAND event tracking events.

  Internal connection info is extracted from the thd object.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the command audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] command       Command id value.
  @param[in] command_text  Command string value.

  @return 0 continue server flow, otherwise abort.
*/

int mysql_event_tracking_command_notify(
    THD *thd, mysql_event_tracking_command_subclass_t subclass,
    const char *subclass_name, enum_server_command command,
    const char *command_text);

/**
  Notify consumers of CONNECTION event tracking events.

  @param[in] thd              Current thread context.
  @param[in] subclass         Type of the connection audit event.
  @param[in] subclass_name    Name of the subclass.
  @param[in] errcode          Error code.

  @return 0 continue server flow, otherwise abort.
*/
int mysql_event_tracking_connection_notify(
    THD *thd, mysql_event_tracking_connection_subclass_t subclass,
    const char *subclass_name, int errcode);

/**
  Notify consumers of CONNECTION event tracking events.

  Internal connection info is extracted from the thd object.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the connection audit event.
  @param[in] subclass_name Name of the subclass.

  @return 0 continue server flow, otherwise abort.
*/
int mysql_event_tracking_connection_notify(
    THD *thd, mysql_event_tracking_connection_subclass_t subclass,
    const char *subclass_name);

/**
  Notify consumers of GENERAL event tracking events.

  @param[in] thd              Current thread data.
  @param[in] subclass         Type of general audit event.
  @param[in] subclass_name    Subclass name.
  @param[in] error_code       Error code
  @param[in] msg              Message
  @param[in] msg_len          Message length.

  @return Value returned is not taken into consideration by the server.
*/
int mysql_event_tracking_general_notify(
    THD *thd, mysql_event_tracking_general_subclass_t subclass,
    const char *subclass_name, int error_code, const char *msg, size_t msg_len);

/**
  Notify consumers of GENERAL event tracking events.

  @param[in] thd    Current thread data.
  @param[in] cmd    Command text.
  @param[in] cmdlen Command text length.

  @return Value returned is not taken into consideration by the server.
*/
inline static int mysql_event_tracking_general_notify(THD *thd, const char *cmd,
                                                      size_t cmdlen) {
  return mysql_event_tracking_general_notify(
      thd, AUDIT_EVENT(EVENT_TRACKING_GENERAL_LOG), 0, cmd, cmdlen);
}

/**
  Notify consumers of GLOBAL VARIABLE event tracking events.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the global variable audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] name          Name of the variable.
  @param[in] value         Textual value of the variable.
  @param[in] value_length  Textual value length.

  @return 0 continue server flow, otherwise abort.
*/
int mysql_event_tracking_global_variable_notify(
    THD *thd, mysql_event_tracking_global_variable_subclass_t subclass,
    const char *subclass_name, const char *name, const char *value,
    const unsigned int value_length);

/**
  Notify consumers of MESSAGE event tracking events.

  @param[in] thd                  Current thread data.
  @param[in] subclass             Message class subclass name.
  @param[in] subclass_name        Subclass name length.
  @param[in] component            Component name.
  @param[in] component_length     Component name length.
  @param[in] producer             Producer name.
  @param[in] producer_length      Producer name length.
  @param[in] message              Message text.
  @param[in] message_length       Message text length.
  @param[in] key_value_map        Key value map pointer.
  @param[in] key_value_map_length Key value map length.

  @return 0 continue server flow.
*/
int mysql_event_tracking_message_notify(
    THD *thd, mysql_event_tracking_message_subclass_t subclass,
    const char *subclass_name, const char *component, size_t component_length,
    const char *producer, size_t producer_length, const char *message,
    size_t message_length,
    mysql_event_tracking_message_key_value_t *key_value_map,
    size_t key_value_map_length);

/**
  Notify consumers of PARSE event tracking events.

  @param[in]  thd             Current thread context.
  @param[in]  subclass        Type of the parse audit event.
  @param[in]  subclass_name   Name of the subclass.
  @param[out] flags           Rewritten query flags.
  @param[out] rewritten_query Rewritten query

  @return 0 continue server flow, otherwise abort.
*/
int mysql_event_tracking_parse_notify(
    THD *thd, mysql_event_tracking_parse_subclass_t subclass,
    const char *subclass_name,
    mysql_event_tracking_parse_rewrite_plugin_flag *flags,
    mysql_cstring_with_length *rewritten_query);

/**
  Notify consumers of QUERY event tracking events.

  Internal query info is extracted from the thd object.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the query audit event.
  @param[in] subclass_name Name of the subclass.

  @return 0 continue server flow, otherwise abort.
*/
int mysql_event_tracking_query_notify(
    THD *thd, mysql_event_tracking_query_subclass_t subclass,
    const char *subclass_name);

/**
  Notify consumers of LIFECYCLE (Shutdown) event tracking events.

  @param[in] subclass  Type of the server abort audit event.
  @param[in] subclass_name Name of the subclass
  @param[in] reason    Reason code of the shutdown.
  @param[in] exit_code Abort exit code.

  @return Value returned is not taken into consideration by the server.
*/
int mysql_event_tracking_shutdown_notify(
    mysql_event_tracking_shutdown_subclass_t subclass,
    const char *subclass_name, mysql_event_tracking_shutdown_reason_t reason,
    int exit_code);

/**
  Notify consumers of LIFECYCLE (Starup) event tracking events.

  @param[in] subclass Type of the server startup audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] argv     Array of program arguments.
  @param[in] argc     Program arguments array length.

  @return 0 continue server start, otherwise abort.
*/
int mysql_event_tracking_startup_notify(
    mysql_event_tracking_startup_subclass_t subclass, const char *subclass_name,
    const char **argv, unsigned int argc);

/**
  Notify consumers of STORED PROGRAM event tracking events.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the stored program audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] database      Stored program database name.
  @param[in] name          Name of the stored program.
  @param[in] parameters    Parameters of the stored program execution.

  @return 0 continue server flow, otherwise abort.
*/
int mysql_event_tracking_stored_program_notify(
    THD *thd, mysql_event_tracking_stored_program_subclass_t subclass,
    const char *subclass_name, const char *database, const char *name,
    void *parameters);

/**
  Notify consumers of TABLE ACCESS event tracking events for all tables
  available in the list.

  Event subclass value depends on the thd->lex->sql_command value.

  The event is generated for 'USER' and 'SYS' tables only.

  @param[in] thd    Current thread data.
  @param[in] table  Connected list of tables, for which event is generated.

  @return 0 - continue server flow, otherwise abort.
*/
int mysql_event_tracking_table_access_notify(THD *thd, Table_ref *table);

#if 0  /* Function commented out. No Audit API calls yet. */
/**
  Call audit plugins of AUTHORIZATION audit class.

  @param[in] thd              Thread data.
  @param[in] subclass         Type of the connection audit event.
  @param[in] subclass_name    Name of the subclass.
  @param[in] database         object database
  @param[in] database_length  object database length
  @param[in] name             object name
  @param[in] name_length      object name length

  @return 0 continue server flow, otherwise abort.
*/
int mysql_audit_notify(THD *thd, mysql_event_authorization_subclass_t subclass,
                       const char *subclass_name, const char *database,
                       unsigned int database_length, const char *name,
                       unsigned int name_length);

/**
  Call audit plugins of AUTHORIZATION audit class.

  @param[in] thd           Current thread data.
  @param[in] subclass      Type of the authorization audit event.
  @param[in] subclass_name Name of the subclass.
  @param[in] database      Database name.
  @param[in] table         Table name.
  @param[in] object        Object name associated with the authorization event.

  @return 0 continue server flow, otherwise abort.
*/

int mysql_audit_notify(THD *thd,
                       mysql_event_authorization_subclass_t subclass,
                       const char *subclass_name,
                       const char *database,
                       const char *table,
                       const char *object);
#endif /* 0 */

class Event_tracking_information {
 public:
  mysql_cstring_with_length command_;
  Event_tracking_information() : command_{nullptr, 0} {}
  Event_tracking_information(const char *command_name, size_t command_length)
      : command_{command_name, command_length} {}
  Event_tracking_information(const Event_tracking_information &src) = default;
  virtual ~Event_tracking_information() {}
};

class Event_tracking_authentication_information final
    : public Event_tracking_information {
 public:
  mysql_event_tracking_authentication_subclass_t subclass_;
  std::vector<const char *> authentication_methods_;
  bool is_role_;
  mysql_cstring_with_length new_user_;
  mysql_cstring_with_length new_host_;

  explicit Event_tracking_authentication_information(
      mysql_event_tracking_authentication_subclass_t subclass,
      std::vector<const char *> &auth_methods, bool is_role,
      const char *new_user, const char *new_host)
      : Event_tracking_information(),
        subclass_(subclass),
        authentication_methods_{auth_methods},
        is_role_{is_role},
        new_user_{new_user, new_user ? strlen(new_user) : 0},
        new_host_{new_host, new_host ? strlen(new_host) : 0} {}
};

class Event_tracking_general_information final
    : public Event_tracking_information {
 public:
  mysql_event_tracking_general_subclass_t subclass_;
  uint64_t rows_;
  uint64_t time_;
  mysql_cstring_with_length external_user_;

  explicit Event_tracking_general_information(
      mysql_event_tracking_general_subclass_t subclass, uint64_t rows,
      uint64_t time, LEX_CSTRING external_user, const char *command_name,
      size_t command_length)
      : Event_tracking_information{command_name, command_length},
        subclass_{subclass},
        rows_{rows},
        time_{time},
        external_user_{external_user.str, external_user.length} {}
};

#endif /* SQL_AUDIT_INCLUDED */
