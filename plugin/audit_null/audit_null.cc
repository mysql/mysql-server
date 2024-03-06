/* Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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

#include <mysql/plugin.h>
#include <mysql/plugin_audit.h>
#include <mysqld_error.h>
#include <stdio.h>
#include <sys/types.h>

#include "lex_string.h"
#include "m_string.h"
#include "my_compiler.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "my_sys.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/strings/m_ctype.h"
#include "nulls.h"
#include "string_with_len.h"
#include "strxnmov.h"
#include "thr_mutex.h"

/** Event strings. */
LEX_CSTRING event_names[][6] = {
    /** MYSQL_AUDIT_GENERAL_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_GENERAL_LOG")},
        {STRING_WITH_LEN("MYSQL_AUDIT_GENERAL_ERROR")},
        {STRING_WITH_LEN("MYSQL_AUDIT_GENERAL_RESULT")},
        {STRING_WITH_LEN("MYSQL_AUDIT_GENERAL_STATUS")},
    },
    /** MYSQL_AUDIT_CONNECTION_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_CONNECTION_CONNECT")},
        {STRING_WITH_LEN("MYSQL_AUDIT_CONNECTION_DISCONNECT")},
        {STRING_WITH_LEN("MYSQL_AUDIT_CONNECTION_CHANGE_USER")},
        {STRING_WITH_LEN("MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE")},
    },
    /** MYSQL_AUDIT_PARSE_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_PARSE_PREPARSE")},
        {STRING_WITH_LEN("MYSQL_AUDIT_PARSE_POSTPARSE")},
    },
    /** MYSQL_AUDIT_AUTHORIZATION_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHORIZATION_USER")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHORIZATION_DB")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHORIZATION_TABLE")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHORIZATION_COLUMN")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHORIZATION_PROCEDURE")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHORIZATION_PROXY")},
    },
    /** MYSQL_AUDIT_TABLE_ROW_ACCES_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_TABLE_ACCESS_READ")},
        {STRING_WITH_LEN("MYSQL_AUDIT_TABLE_ACCESS_INSERT")},
        {STRING_WITH_LEN("MYSQL_AUDIT_TABLE_ACCESS_UPDATE")},
        {STRING_WITH_LEN("MYSQL_AUDIT_TABLE_ACCESS_DELETE")},
    },
    /** MYSQL_AUDIT_GLOBAL_VARIABLE_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_GLOBAL_VARIABLE_GET")},
        {STRING_WITH_LEN("MYSQL_AUDIT_GLOBAL_VARIABLE_SET")},
    },
    /** MYSQL_AUDIT_SERVER_STARTUP_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_SERVER_STARTUP_STARTUP")},
    },
    /** MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_SERVER_SHUTDOWN_SHUTDOWN")},
    },
    /** MYSQL_AUDIT_COMMAND_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_COMMAND_START")},
        {STRING_WITH_LEN("MYSQL_AUDIT_COMMAND_END")},
    },
    /** MYSQL_AUDIT_QUERY_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_QUERY_START")},
        {STRING_WITH_LEN("MYSQL_AUDIT_QUERY_NESTED_START")},
        {STRING_WITH_LEN("MYSQL_AUDIT_QUERY_STATUS_END")},
        {STRING_WITH_LEN("MYSQL_AUDIT_QUERY_NESTED_STATUS_END")},
    },
    /** MYSQL_AUDIT_STORED_PROGRAM_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_STORED_PROGRAM_EXECUTE")},
    },
    /** MYSQL_AUDIT_AUTHENTICATION_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHENTICATION_FLUSH")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHENTICATION_AUTHID_CREATE")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHENTICATION_CREDENTIAL_CHANGE")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHENTICATION_AUTHID_RENAME")},
        {STRING_WITH_LEN("MYSQL_AUDIT_AUTHENTICATION_AUTHID_DROP")},
    },
    /** MYSQL_AUDIT_MESSAGE_CLASS */
    {
        {STRING_WITH_LEN("MYSQL_AUDIT_MESSAGE_INTERNAL")},
        {STRING_WITH_LEN("MYSQL_AUDIT_MESSAGE_USER")},
    }};

static volatile int number_of_calls;

/*
  Plugin has been installed.
*/
static bool g_plugin_installed = false;

/*
  Record buffer mutex.
*/
static mysql_mutex_t g_record_buffer_mutex;

/*
  Event recording buffer.
*/
static char *g_record_buffer;

#define AUDIT_NULL_VAR(x) static volatile int number_of_calls_##x;
#include "plugin/audit_null/audit_null_variables.h"

#undef AUDIT_NULL_VAR

/*
  Plugin status variables for SHOW STATUS
*/

static SHOW_VAR simple_status[] = {
    {"Audit_null_called",
     const_cast<char *>(reinterpret_cast<volatile char *>(&number_of_calls)),
     SHOW_INT, SHOW_SCOPE_GLOBAL},

#define AUDIT_NULL_VAR(x)                                        \
  {"Audit_null_" #x,                                             \
   const_cast<char *>(                                           \
       reinterpret_cast<volatile char *>(&number_of_calls_##x)), \
   SHOW_INT, SHOW_SCOPE_GLOBAL},
#include "plugin/audit_null/audit_null_variables.h"

#undef AUDIT_NULL_VAR

    {nullptr, nullptr, SHOW_UNDEF, SHOW_SCOPE_GLOBAL}};

static void increment_counter(volatile int *counter) {
  int value = *counter;
  int new_value = value + 1;
  *counter = new_value;
}

/*
  Define plugin variables.
*/

static MYSQL_THDVAR_STR(abort_message,
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
                        "Custom message for event abort.", nullptr, nullptr,
                        nullptr);

static MYSQL_THDVAR_INT(abort_value, PLUGIN_VAR_RQCMDARG, "Event abort value.",
                        nullptr, nullptr, 1, -1, 150, 0);

static MYSQL_THDVAR_STR(event_order_check,
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
                        "Event order check string", nullptr, nullptr, nullptr);

static MYSQL_THDVAR_UINT(event_order_check_consume_ignore_count,
                         PLUGIN_VAR_RQCMDARG,
                         "Do not consume event order string specified "
                         "number of times.",
                         nullptr, nullptr, 0, 0, UINT_MAX, 1);

static MYSQL_THDVAR_INT(event_order_started, PLUGIN_VAR_RQCMDARG,
                        "Plugin is in the event order check.", nullptr, nullptr,
                        0, 0, 1, 0);

static MYSQL_THDVAR_INT(event_order_check_exact, PLUGIN_VAR_RQCMDARG,
                        "Plugin checks exact event order.", nullptr, nullptr, 1,
                        0, 1, 0);

static MYSQL_THDVAR_STR(event_record_def,
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
                        "Event recording definition", nullptr, nullptr,
                        nullptr);

static MYSQL_THDVAR_STR(event_record,
                        PLUGIN_VAR_READONLY | PLUGIN_VAR_RQCMDARG |
                            PLUGIN_VAR_MEMALLOC,
                        "Event recording", nullptr, nullptr, nullptr);
/*
  Initialize the plugin at server start or plugin installation.

  SYNOPSIS
    audit_null_plugin_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int audit_null_plugin_init(void *arg [[maybe_unused]]) {
  SHOW_VAR *var;

  for (var = simple_status; var->value != nullptr; var++) {
    *((int *)var->value) = 0;
  }

  mysql_mutex_init(PSI_NOT_INSTRUMENTED, &g_record_buffer_mutex,
                   MY_MUTEX_INIT_FAST);

  g_record_buffer = nullptr;
  g_plugin_installed = true;

  return (0);
}

/*
  Terminate the plugin at server shutdown or plugin deinstallation.

  SYNOPSIS
    audit_null_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int audit_null_plugin_deinit(void *arg [[maybe_unused]]) {
  assert(arg);
  if (g_plugin_installed == true) {
    my_free((void *)(g_record_buffer));

    g_record_buffer = nullptr;

    mysql_mutex_destroy(&g_record_buffer_mutex);

    g_plugin_installed = false;
  }

  return (0);
}

/**
  @brief Converts event_class and event_subclass into a string.

  @param [in] event_class    Event class value.
  @param [in] event_subclass Event subclass value.

  @retval Event name.
*/
static LEX_CSTRING event_to_str(unsigned int event_class,
                                unsigned long event_subclass) {
  int count;
  for (count = 0; event_subclass; count++, event_subclass >>= 1)
    ;

  return event_names[event_class][count - 1];
}

/**
  @brief Read token delimited by a semicolon from a string.

  @param [in,out] str Pointer to a string containing text.
                     Pointer is moved to a new token after the function ends.

  @retval Token retrieved from a string.
*/
static LEX_CSTRING get_token(char **str) {
  LEX_CSTRING ret = {nullptr, 0};

  if (*str != nullptr) {
    ret.str = *str;

    if (*ret.str != '\0') {
      /* Find a param delimiter. */
      while (**str && **str != ';') (*str)++;

      ret.length = *str - ret.str;

      /* Skip a delimiter. */
      if (**str == ';') (*str)++;
    }
  }

  return ret;
}

static char *add_event(const char *var, LEX_CSTRING event, const char *data,
                       size_t data_length) {
  LEX_CSTRING str;
  size_t size;
  char *buffer;

  lex_cstring_set(&str, var);

  size = str.length + event.length + data_length + 4;
  buffer = (char *)my_malloc(PSI_NOT_INSTRUMENTED, size, MYF(MY_FAE));

  snprintf(buffer, size, "%s%s%s;%s;", var, str.length == 0 ? "" : "\n",
           event.str, data);

  buffer[size - (str.length == 0 ? 2 : 1)] = '\0';

  return buffer;
}

static void process_event_record(MYSQL_THD thd, LEX_CSTRING event_name,
                                 const char *data, size_t data_length) {
  char *record_str = THDVAR(thd, event_record_def);
  const LEX_CSTRING record_begin = get_token(&record_str);
  const LEX_CSTRING record_end = get_token(&record_str);

  if (record_str == nullptr) {
    return;
  }

  if (record_end.length == 0) {
    /* We are already in the consuming phase. Add a new event name into
       a record variable */

    const char *buffer = THDVAR(thd, event_record);
    char *new_buffer = nullptr;

    /* Add event. */
    mysql_mutex_lock(&g_record_buffer_mutex);

    /* Only one THD is capable of adding events into the buffer. */
    if (buffer && (buffer == g_record_buffer)) {
      new_buffer = add_event(buffer, event_name, data, data_length);
      g_record_buffer = new_buffer;
      my_free(const_cast<char *>(buffer));
    }

    mysql_mutex_unlock(&g_record_buffer_mutex);

    THDVAR(thd, event_record) = new_buffer;

    if (!my_charset_latin1.coll->strnncoll(
            &my_charset_latin1, (const uchar *)record_begin.str,
            record_begin.length, (const uchar *)event_name.str,
            event_name.length, false)) {
      /* Do not expect any more events. */
      THDVAR(thd, event_record_def) = nullptr;
    }
  } else {
    const char *buffer;

    /* We have not started recording of events yet. */
    if (my_charset_latin1.coll->strnncoll(
            &my_charset_latin1, (const uchar *)record_begin.str,
            record_begin.length, (const uchar *)event_name.str,
            event_name.length, false)) {
      /* Event not matching. */
      return;
    }

    buffer = THDVAR(thd, event_record);

    mysql_mutex_lock(&g_record_buffer_mutex);

    if (buffer == g_record_buffer) {
      my_free(const_cast<char *>(buffer));

      g_record_buffer = add_event("", event_name, data, data_length);

      THDVAR(thd, event_record) = g_record_buffer;
    }

    mysql_mutex_unlock(&g_record_buffer_mutex);

    /* Add event. */

    record_str = THDVAR(thd, event_record_def);

    /* Remove starting event. */
    memmove(record_str, record_end.str, record_end.length + 1);
  }
}

static int process_command(MYSQL_THD thd, LEX_CSTRING event_command,
                           bool consume_event) {
  const LEX_CSTRING abort_ret_command = {STRING_WITH_LEN("ABORT_RET")};

  if (!my_charset_latin1.coll->strnncoll(
          &my_charset_latin1, (const uchar *)event_command.str,
          event_command.length, (const uchar *)abort_ret_command.str,
          abort_ret_command.length, false)) {
    const int ret_code = (int)THDVAR(thd, abort_value);
    const char *err_message = (const char *)THDVAR(thd, abort_message);
    const LEX_CSTRING status = {STRING_WITH_LEN("EVENT-ORDER-ABORT")};

    char *order_str = THDVAR(thd, event_order_check);

    /* Do not replace order string yet. */
    if (consume_event) {
      memmove(order_str, status.str, status.length + 1);

      THDVAR(thd, abort_value) = 1;
      THDVAR(thd, abort_message) = nullptr;
    }

    if (err_message) {
      my_message(ER_AUDIT_API_ABORT, err_message, MYF(0));
      THDVAR(thd, event_order_check) = order_str;
    }

    return ret_code;
  }

  return 0;
}

/**
  @brief Plugin function handler.

  @param [in] thd         Connection context.
  @param [in] event_class Event class value.
  @param [in] event       Event data.

  @retval Value indicating, whether the server should abort continuation
          of the current operation.
*/
static int audit_null_notify(MYSQL_THD thd, mysql_event_class_t event_class,
                             const void *event) {
  char buffer[2000] = {
      0,
  };
  int buffer_data = 0;
  const unsigned long event_subclass =
      static_cast<unsigned long>(*static_cast<const int *>(event));
  char *order_str = THDVAR(thd, event_order_check);
  const int event_order_started = (int)THDVAR(thd, event_order_started);
  const int exact_check = (int)THDVAR(thd, event_order_check_exact);
  const LEX_CSTRING event_name = event_to_str(event_class, event_subclass);
  const LEX_CSTRING event_token = get_token(&order_str);
  const LEX_CSTRING event_data = get_token(&order_str);
  LEX_CSTRING event_command = get_token(&order_str);
  bool consume_event = true;

  /* prone to races, oh well */
  increment_counter(&number_of_calls);

  if (event_class == MYSQL_AUDIT_GENERAL_CLASS) {
    const struct mysql_event_general *event_general =
        (const struct mysql_event_general *)event;

    switch (event_general->event_subclass) {
      case MYSQL_AUDIT_GENERAL_LOG:
        increment_counter(&number_of_calls_general_log);
        break;
      case MYSQL_AUDIT_GENERAL_ERROR:
        increment_counter(&number_of_calls_general_error);
        break;
      case MYSQL_AUDIT_GENERAL_RESULT:
        increment_counter(&number_of_calls_general_result);
        break;
      case MYSQL_AUDIT_GENERAL_STATUS:
        increment_counter(&number_of_calls_general_status);
        break;
      default:
        break;
    }
  } else if (event_class == MYSQL_AUDIT_CONNECTION_CLASS) {
    const struct mysql_event_connection *event_connection =
        (const struct mysql_event_connection *)event;

    switch (event_connection->event_subclass) {
      case MYSQL_AUDIT_CONNECTION_CONNECT:
        increment_counter(&number_of_calls_connection_connect);
        break;
      case MYSQL_AUDIT_CONNECTION_DISCONNECT:
        increment_counter(&number_of_calls_connection_disconnect);
        break;
      case MYSQL_AUDIT_CONNECTION_CHANGE_USER:
        increment_counter(&number_of_calls_connection_change_user);
        break;
      case MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE:
        increment_counter(&number_of_calls_connection_pre_authenticate);
        break;
      default:
        break;
    }
  } else if (event_class == MYSQL_AUDIT_PARSE_CLASS) {
    const struct mysql_event_parse *event_parse =
        (const struct mysql_event_parse *)event;

    switch (event_parse->event_subclass) {
      case MYSQL_AUDIT_PARSE_PREPARSE:
        increment_counter(&number_of_calls_parse_preparse);
        break;
      case MYSQL_AUDIT_PARSE_POSTPARSE:
        increment_counter(&number_of_calls_parse_postparse);
        break;
      default:
        break;
    }
  }
#if 0
  /**
    Currently events not active.

  else if (event_class == MYSQL_AUDIT_AUTHORIZATION_CLASS)
  {
    const struct mysql_event_authorization *event_grant =
                             (const struct mysql_event_authorization *)event;

    buffer_data= sprintf(buffer, "db=\"%s\" table=\"%s\" object=\"%s\" "
                         "requested=\"0x%08x\" granted=\"0x%08x\"",
                         event_grant->database.str ? event_grant->database.str : "<NULL>",
                         event_grant->table.str ? event_grant->table.str : "<NULL>",
                         event_grant->object.str ? event_grant->object.str : "<NULL>",
                         event_grant->requested_privilege,
                         event_grant->granted_privilege);

    switch (event_grant->event_subclass)
    {
    case MYSQL_AUDIT_AUTHORIZATION_USER:
      number_of_calls_authorization_user++;
      break;
    case MYSQL_AUDIT_AUTHORIZATION_DB:
      number_of_calls_authorization_db++;
      break;
    case MYSQL_AUDIT_AUTHORIZATION_TABLE:
      number_of_calls_authorization_table++;
      break;
    case MYSQL_AUDIT_AUTHORIZATION_COLUMN:
      number_of_calls_authorization_column++;
      break;
    case MYSQL_AUDIT_AUTHORIZATION_PROCEDURE:
      number_of_calls_authorization_procedure++;
      break;
    case MYSQL_AUDIT_AUTHORIZATION_PROXY:
      number_of_calls_authorization_proxy++;
      break;
    default:
      break;
    }
  }
  */
#endif
  else if (event_class == MYSQL_AUDIT_SERVER_STARTUP_CLASS) {
    /* const struct mysql_event_server_startup *event_startup=
       (const struct mysql_event_server_startup *) event; */
    increment_counter(&number_of_calls_server_startup);
  } else if (event_class == MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS) {
    /* const struct mysql_event_server_shutdown *event_startup=
       (const struct mysql_event_server_shutdown *) event; */
    increment_counter(&number_of_calls_server_shutdown);
  } else if (event_class == MYSQL_AUDIT_COMMAND_CLASS) {
    const struct mysql_event_command *local_event_command =
        (const struct mysql_event_command *)event;

    buffer_data =
        sprintf(buffer, "command_id=\"%d\"", local_event_command->command_id);

    switch (local_event_command->event_subclass) {
      case MYSQL_AUDIT_COMMAND_START:
        increment_counter(&number_of_calls_command_start);
        break;
      case MYSQL_AUDIT_COMMAND_END:
        increment_counter(&number_of_calls_command_end);
        break;
      default:
        break;
    }
  } else if (event_class == MYSQL_AUDIT_QUERY_CLASS) {
    const struct mysql_event_query *event_query =
        (const struct mysql_event_query *)event;

    buffer_data = sprintf(buffer, "sql_command_id=\"%d\"",
                          (int)event_query->sql_command_id);

    switch (event_query->event_subclass) {
      case MYSQL_AUDIT_QUERY_START:
        increment_counter(&number_of_calls_query_start);
        break;
      case MYSQL_AUDIT_QUERY_NESTED_START:
        increment_counter(&number_of_calls_query_nested_start);
        break;
      case MYSQL_AUDIT_QUERY_STATUS_END:
        increment_counter(&number_of_calls_query_status_end);
        break;
      case MYSQL_AUDIT_QUERY_NESTED_STATUS_END:
        increment_counter(&number_of_calls_query_nested_status_end);
        break;
      default:
        break;
    }
  } else if (event_class == MYSQL_AUDIT_TABLE_ACCESS_CLASS) {
    const struct mysql_event_table_access *event_table =
        (const struct mysql_event_table_access *)event;

    buffer_data =
        sprintf(buffer, "db=\"%s\" table=\"%s\"",
                event_table->table_database.str, event_table->table_name.str);

    switch (event_table->event_subclass) {
      case MYSQL_AUDIT_TABLE_ACCESS_INSERT:
        increment_counter(&number_of_calls_table_access_insert);
        break;
      case MYSQL_AUDIT_TABLE_ACCESS_DELETE:
        increment_counter(&number_of_calls_table_access_delete);
        break;
      case MYSQL_AUDIT_TABLE_ACCESS_UPDATE:
        increment_counter(&number_of_calls_table_access_update);
        break;
      case MYSQL_AUDIT_TABLE_ACCESS_READ:
        increment_counter(&number_of_calls_table_access_read);
        break;
      default:
        break;
    }
  } else if (event_class == MYSQL_AUDIT_GLOBAL_VARIABLE_CLASS) {
    const struct mysql_event_global_variable *event_gvar =
        (const struct mysql_event_global_variable *)event;

    /* Copy the variable content into the buffer. We do not guarantee that the
       variable value will fit into buffer. The buffer should be large enough
       to be used for the test purposes. */
    buffer_data =
        sprintf(buffer, "name=\"%.*s\"",
                static_cast<int>(std::min(event_gvar->variable_name.length,
                                          (sizeof(buffer) - 8))),
                event_gvar->variable_name.str);

    buffer_data +=
        sprintf(buffer + buffer_data, " value=\"%.*s\"",
                static_cast<int>(std::min(event_gvar->variable_value.length,
                                          (sizeof(buffer) - 16))),
                event_gvar->variable_value.str);
    buffer[buffer_data] = '\0';

    switch (event_gvar->event_subclass) {
      case MYSQL_AUDIT_GLOBAL_VARIABLE_GET:
        increment_counter(&number_of_calls_global_variable_get);
        break;
      case MYSQL_AUDIT_GLOBAL_VARIABLE_SET:
        increment_counter(&number_of_calls_global_variable_set);
        break;
      default:
        break;
    }
  } else if (event_class == MYSQL_AUDIT_MESSAGE_CLASS) {
    const struct mysql_event_message *evt =
        reinterpret_cast<const struct mysql_event_message *>(event);

    buffer_data +=
        snprintf(buffer, sizeof(buffer) - 1,
                 "component=\"%.*s\" producer=\"%.*s\" message=\"%.*s\"",
                 static_cast<int>(evt->component.length), evt->component.str,
                 static_cast<int>(evt->producer.length), evt->producer.str,
                 static_cast<int>(evt->message.length), evt->message.str);

    for (size_t i = 0; i < evt->key_value_map_length; ++i) {
      if (evt->key_value_map[i].value_type ==
              MYSQL_AUDIT_MESSAGE_VALUE_TYPE_STR &&
          evt->key_value_map[i].value.str.str == nullptr)
        buffer_data +=
            snprintf(buffer + buffer_data, sizeof(buffer) - buffer_data - 1,
                     " key[%zu]=\"%.*s\" value[%zu]=null", i,
                     static_cast<int>(evt->key_value_map[i].key.length),
                     evt->key_value_map[i].key.str, i);
      else if (evt->key_value_map[i].value_type ==
               MYSQL_AUDIT_MESSAGE_VALUE_TYPE_STR)
        buffer_data +=
            snprintf(buffer + buffer_data, sizeof(buffer) - buffer_data - 1,
                     " key[%zu]=\"%.*s\" value[%zu]=\"%.*s\"", i,
                     static_cast<int>(evt->key_value_map[i].key.length),
                     evt->key_value_map[i].key.str, i,
                     static_cast<int>(evt->key_value_map[i].value.str.length),
                     evt->key_value_map[i].value.str.str);
      else if (evt->key_value_map[i].value_type ==
               MYSQL_AUDIT_MESSAGE_VALUE_TYPE_NUM)
        buffer_data += snprintf(
            buffer + buffer_data, sizeof(buffer) - buffer_data - 1,
            " key[%zu]=\"%.*s\" value[%zu]=%lld", i,
            static_cast<int>(evt->key_value_map[i].key.length),
            evt->key_value_map[i].key.str, i, evt->key_value_map[i].value.num);
    }

    buffer[buffer_data] = '\0';

    switch (evt->event_subclass) {
      case MYSQL_AUDIT_MESSAGE_INTERNAL:
        increment_counter(&number_of_calls_message_internal);
        break;
      case MYSQL_AUDIT_MESSAGE_USER:
        increment_counter(&number_of_calls_message_user);
        break;
      default:
        break;
    }
  }

  process_event_record(thd, event_name, buffer, buffer_data);

  if (my_charset_latin1.coll->strnncoll(
          &my_charset_latin1, (const uchar *)event_name.str, event_name.length,
          (const uchar *)event_token.str, event_token.length, false)) {
    /* Clear event command. */
    event_command.str = nullptr;
    event_command.length = 0;

    if (exact_check == 1 && event_order_started == 1) {
      if (!(event_class == MYSQL_AUDIT_GENERAL_CLASS &&
            event_subclass == MYSQL_AUDIT_GENERAL_ERROR)) {
        strxnmov(buffer, sizeof(buffer), event_name.str, " instead of ",
                 event_token.str, NullS);
        my_message(ER_AUDIT_API_ABORT, buffer, MYF(0));
      }

      THDVAR(thd, event_order_started) = 0;
      THDVAR(thd, event_order_check) = nullptr;

      return 1;
    }
  } else {
    const LEX_CSTRING ignore = {STRING_WITH_LEN("<IGNORE>")};

    /* When we are not in the event order check, check if the specified
       data corresponds to the actual event data. */
    if (my_charset_latin1.coll->strnncoll(
            &my_charset_latin1, (const uchar *)event_data.str,
            event_data.length, (const uchar *)ignore.str, ignore.length,
            false) &&
        my_charset_latin1.coll->strnncoll(
            &my_charset_latin1, (const uchar *)event_data.str,
            event_data.length, (const uchar *)buffer, (size_t)buffer_data,
            false)) {
      if (exact_check == 1 && event_order_started == 1) {
        char invalid_data_buffer[sizeof(buffer)] = {
            0,
        };
        const LEX_CSTRING status = {
            STRING_WITH_LEN("EVENT-ORDER-INVALID-DATA")};

        char *local_order_str = THDVAR(thd, event_order_check);

        memmove(local_order_str, status.str, status.length + 1);

        strxnmov(invalid_data_buffer, sizeof(invalid_data_buffer),
                 "Invalid data for '", event_name.str, "' -> ", buffer, NullS);
        my_message(ER_AUDIT_API_ABORT, invalid_data_buffer, MYF(0));

        THDVAR(thd, event_order_started) = 0;
        THDVAR(thd, event_order_check) = local_order_str;

        return 1;
      }

      /* Clear event command. */
      event_command.str = nullptr;
      event_command.length = 0;
    } else {
      LEX_STRING order_cstr;
      const ulong consume = THDVAR(thd, event_order_check_consume_ignore_count);
      lex_string_set(&order_cstr, THDVAR(thd, event_order_check));

      THDVAR(thd, event_order_started) = 1;

      if (consume) {
        /*
          Do not consume event this time. Just decrease value and wait until
          the next event is matched.
        */
        THDVAR(thd, event_order_check_consume_ignore_count) = consume - 1;
        consume_event = false;
      } else {
        /* Consume matched event. */
        memmove(order_cstr.str, order_str,
                order_cstr.length - (order_str - order_cstr.str) + 1);

        /* Count new length. */
        lex_string_set(&order_cstr, order_cstr.str);

        if (order_cstr.length == 0) {
          const LEX_CSTRING status = {STRING_WITH_LEN("EVENT-ORDER-OK")};

          memmove(order_cstr.str, status.str, status.length + 1);

          /* event_order_started contains message. Do not verify it. */
          THDVAR(thd, event_order_started) = 0;
        }
      }
    }
  }

  return process_command(thd, event_command, consume_event);
}

/*
  Plugin type-specific descriptor
*/

static struct st_mysql_audit audit_null_descriptor = {
    MYSQL_AUDIT_INTERFACE_VERSION, /* interface version    */
    nullptr,                       /* release_thd function */
    audit_null_notify,             /* notify function      */
    {(unsigned long)MYSQL_AUDIT_GENERAL_ALL,
     (unsigned long)MYSQL_AUDIT_CONNECTION_ALL,
     (unsigned long)MYSQL_AUDIT_PARSE_ALL,
     0, /* This event class is currently not supported. */
     (unsigned long)MYSQL_AUDIT_TABLE_ACCESS_ALL,
     (unsigned long)MYSQL_AUDIT_GLOBAL_VARIABLE_ALL,
     (unsigned long)MYSQL_AUDIT_SERVER_STARTUP_ALL,
     (unsigned long)MYSQL_AUDIT_SERVER_SHUTDOWN_ALL,
     (unsigned long)MYSQL_AUDIT_COMMAND_ALL,
     (unsigned long)MYSQL_AUDIT_QUERY_ALL,
     (unsigned long)MYSQL_AUDIT_STORED_PROGRAM_ALL,
     (unsigned long)MYSQL_AUDIT_AUTHENTICATION_ALL,
     (unsigned long)MYSQL_AUDIT_MESSAGE_ALL}};

static SYS_VAR *system_variables[] = {

    MYSQL_SYSVAR(abort_message),
    MYSQL_SYSVAR(abort_value),

    MYSQL_SYSVAR(event_order_check),
    MYSQL_SYSVAR(event_order_check_consume_ignore_count),
    MYSQL_SYSVAR(event_order_started),
    MYSQL_SYSVAR(event_order_check_exact),

    MYSQL_SYSVAR(event_record_def),
    MYSQL_SYSVAR(event_record),
    nullptr};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(audit_null){
    MYSQL_AUDIT_PLUGIN,     /* type                            */
    &audit_null_descriptor, /* descriptor                      */
    "NULL_AUDIT",           /* name                            */
    PLUGIN_AUTHOR_ORACLE,   /* author                          */
    "Simple NULL Audit",    /* description                     */
    PLUGIN_LICENSE_GPL,
    audit_null_plugin_init,   /* init function (when loaded)     */
    nullptr,                  /* check uninstall function        */
    audit_null_plugin_deinit, /* deinit function (when unloaded) */
    0x0003,                   /* version                         */
    simple_status,            /* status variables                */
    system_variables,         /* system variables                */
    nullptr,
    0,
} mysql_declare_plugin_end;
