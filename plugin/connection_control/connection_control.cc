/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/plugin_audit.h>         /* mysql_event_connection */
#include "mysql_version.h"
#include <my_global.h>
#include <my_atomic.h>

#include "connection_control.h"
#include "connection_delay_api.h"       /* connection_delay apis */
#include "connection_control_coordinator.h" /* g_connection_event_coordinator */

namespace connection_control
{
  class Connection_control_error_handler : public Error_handler
  {
  public:
    Connection_control_error_handler(MYSQL_PLUGIN plugin_info)
      : m_plugin_info(plugin_info)
    {}

    void handle_error(const char * error_message)
    {
      my_plugin_log_message(&m_plugin_info,
                            MY_ERROR_LEVEL,
                            error_message);
    }
  private:
    MYSQL_PLUGIN m_plugin_info;
  };
}

using connection_control::Connection_event_coordinator;
using connection_control::Connection_event_coordinator_services;
using connection_control::Connection_control_statistics;
using connection_control::Connection_control_variables;
using connection_control::Error_handler;
using connection_control::Connection_control_error_handler;

Connection_control_statistics g_statistics;
Connection_control_variables g_variables;

Connection_event_coordinator *g_connection_event_coordinator= 0;
MYSQL_PLUGIN connection_control_plugin_info= 0;


/**
  event_notify() implementation for connection_control

  For connection event, notify Connection_event_coordinator
  which in turn will notify subscribers.

  @param [in] thd            Handle to THD
  @param [in] event_class    Event class.
                             We are interested in MYSQL_AUDIT_CONNECTION_CLASS.
  @param [in] event          mysql_event_connection handle
*/

static int
connection_control_notify(MYSQL_THD thd,
                          mysql_event_class_t event_class,
                          const void *event)
{
  DBUG_ENTER("connection_control_notify");
  try
  {
    if (event_class == MYSQL_AUDIT_CONNECTION_CLASS)
    {
      const struct mysql_event_connection *connection_event=
        (const struct mysql_event_connection *) event;
      Connection_control_error_handler error_handler(connection_control_plugin_info);
      /** Notify event coordinator */
      g_connection_event_coordinator->notify_event(thd, &error_handler,
                                                   connection_event);
    }
  }
  catch (...)
  {
    /* Happily ignore any bad behavior */
  }

  DBUG_RETURN(0);
}


/**
  Plugin initialization function

  @param [in] plugin_info  MYSQL_PLUGIN information

  @returns initialization status
    @retval 0 Success
    @retval 1 Failure
*/

static int
connection_control_init(MYSQL_PLUGIN plugin_info)
{
  connection_control_plugin_info= plugin_info;
  Connection_control_error_handler error_handler(connection_control_plugin_info);
  g_connection_event_coordinator= new Connection_event_coordinator();
  if (!g_connection_event_coordinator)
  {
    error_handler.handle_error("Failed to initialize Connection_event_coordinator");
    return 1;
  }

  if (init_connection_delay_event((Connection_event_coordinator_services *)
                                  g_connection_event_coordinator,
                                  &error_handler))
  {
    delete g_connection_event_coordinator;
    return 1;
  }
  return 0;
}


/**
  Plugin deinitialization

  @param arg  Unused

  @returns success
*/

static int
connection_control_deinit(void *arg MY_ATTRIBUTE((unused)))
{
  delete g_connection_event_coordinator;
  g_connection_event_coordinator= 0;
  connection_control::deinit_connection_delay_event();
  connection_control_plugin_info= 0;
  return 0;
}


/** Connection_control plugin descriptor */
static struct st_mysql_audit connection_control_descriptor=
{
  MYSQL_AUDIT_INTERFACE_VERSION,                    /* interface version */
  NULL,                                             /* release_thd() */
  connection_control_notify,                        /* event_notify() */
  { 0,                                              /* MYSQL_AUDIT_GENERAL_CLASS */
    (unsigned long) MYSQL_AUDIT_CONNECTION_CONNECT |
    MYSQL_AUDIT_CONNECTION_CHANGE_USER,             /* MYSQL_AUDIT_CONNECTION_CLASS */
    0,                                              /* MYSQL_AUDIT_PARSE_CLASS */
    0,                                              /* MYSQL_AUDIT_AUTHORIZATION_CLASS */
    0,                                              /* MYSQL_AUDIT_TABLE_ACCESS_CLASS */
    0,                                              /* MYSQL_AUDIT_GLOBAL_VARIABLE_CLASS */
    0,                                              /* MYSQL_AUDIT_SERVER_STARTUP_CLASS */
    0,                                              /* MYSQL_AUDIT_SERVER_SHUTDOWN_CLASS */
    0,                                              /* MYSQL_AUDIT_COMMAND_CLASS */
    0,                                              /* MYSQL_AUDIT_QUERY_CLASS */
    0,                                              /* MYSQL_AUDIT_STORED_PROGRAM_CLASS */
  }        /* class mask */
};


/**
  check() function for connection_control_failed_connections_threshold

  Check whether new value is within valid bounds or not.

  @param thd        Not used.
  @param var        Not used.
  @param save       Not used.
  @param value      New value for the option.

  @returns whether new value is within valid bounds or not.
    @retval 0 Value is ok
    @retval 1 Value is not within valid bounds
*/

static int
check_failed_connections_threshold(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                                   struct st_mysql_sys_var *var MY_ATTRIBUTE((unused)),
                                   void *save MY_ATTRIBUTE((unused)),
                                   struct st_mysql_value *value)
{
  longlong new_value;
  if (value->val_int(value, &new_value))
    return 1;                           /* NULL value */

  if (new_value >= connection_control::MIN_THRESHOLD &&
      new_value <= connection_control::MAX_THRESHOLD)
  {
    *(reinterpret_cast<longlong *>(save))= new_value;
    return 0;
  }

  return 1;
}


/**
  update() function for connection_control_failed_connections_threshold

  Updates g_connection_event_coordinator with new value.
  Also notifies observers about the update.

  @param thd        Not used.
  @param var        Not used.
  @param var_ptr    Variable information
  @param save       New value for connection_control_failed_connections_threshold
*/

static void
update_failed_connections_threshold(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                                    struct st_mysql_sys_var *var MY_ATTRIBUTE((unused)),
                                    void *var_ptr, const void *save)
{
  /*
    This won't result in overflow because we have already checked that this is
    within valid bounds.
  */
  longlong new_value= *(reinterpret_cast<const longlong *>(save));
  g_variables.failed_connections_threshold= (int64)new_value;
  Connection_control_error_handler error_handler(connection_control_plugin_info);
  g_connection_event_coordinator->notify_sys_var(&error_handler,
                                                 OPT_FAILED_CONNECTIONS_THRESHOLD,
                                                 &new_value);
  return;
}


/** Declaration of connection_control_failed_connections_threshold */
static MYSQL_SYSVAR_LONGLONG(
  failed_connections_threshold,
  g_variables.failed_connections_threshold,
  PLUGIN_VAR_RQCMDARG,
  "Failed connection threshold to trigger delay. Default is 3.",
  check_failed_connections_threshold,
  update_failed_connections_threshold,
  connection_control::DEFAULT_THRESHOLD,
  connection_control::MIN_THRESHOLD,
  connection_control::MAX_THRESHOLD,
  1
);


/**
  check() function for connection_control_min_connection_delay

  Check whether new value is within valid bounds or not.

  @param thd        Not used.
  @param var        Not used.
  @param save       Not used.
  @param value      New value for the option.

  @returns whether new value is within valid bounds or not.
    @retval 0 Value is ok
    @retval 1 Value is not within valid bounds
*/

static int
check_min_connection_delay(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                           struct st_mysql_sys_var *var MY_ATTRIBUTE((unused)),
                           void *save MY_ATTRIBUTE((unused)),
                           struct st_mysql_value *value)
{
  long long new_value;
  int64 existing_value= g_variables.max_connection_delay;
  if (value->val_int(value, &new_value))
    return 1;                           /* NULL value */

  if (new_value >= connection_control::MIN_DELAY &&
      new_value <= connection_control::MAX_DELAY &&
      new_value <= existing_value)
  {
    *(reinterpret_cast<longlong *>(save))= new_value;
    return 0;
  }
  return 1;
}


/**
  update() function for connection_control_min_connection_delay

  Updates g_connection_event_coordinator with new value.
  Also notifies observers about the update.

  @param thd        Not used.
  @param var        Not used.
  @param var_ptr    Variable information
  @param save       New value for connection_control_min_connection_delay
*/

static void
update_min_connection_delay(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                            struct st_mysql_sys_var *var MY_ATTRIBUTE((unused)),
                            void *var_ptr, const void *save)
{
  longlong new_value= *(reinterpret_cast<const longlong *>(save));
  g_variables.min_connection_delay= (int64)new_value;
  Connection_control_error_handler error_handler(connection_control_plugin_info);
  g_connection_event_coordinator->notify_sys_var(&error_handler,
                                                 OPT_MIN_CONNECTION_DELAY,
                                                 &new_value);
  return;
}


/** Declaration of connection_control_max_connection_delay */
static MYSQL_SYSVAR_LONGLONG(
  min_connection_delay,
  g_variables.min_connection_delay,
  PLUGIN_VAR_RQCMDARG,
  "Maximum delay to be introduced. Default is 1000.",
  check_min_connection_delay,
  update_min_connection_delay,
  connection_control::DEFAULT_MIN_DELAY,
  connection_control::MIN_DELAY,
  connection_control::MAX_DELAY,
  1
);


/**
  check() function for connection_control_max_connection_delay

  Check whether new value is within valid bounds or not.

  @param thd        Not used.
  @param var        Not used.
  @param save       Not used.
  @param value      New value for the option.

  @returns whether new value is within valid bounds or not.
    @retval 0 Value is ok
    @retval 1 Value is not within valid bounds
*/

static int
check_max_connection_delay(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                           struct st_mysql_sys_var *var MY_ATTRIBUTE((unused)),
                           void *save MY_ATTRIBUTE((unused)),
                           struct st_mysql_value *value)
{
  long long new_value;
  int64 existing_value= my_atomic_load64(&g_variables.min_connection_delay);
  if (value->val_int(value, &new_value))
    return 1;                           /* NULL value */

  if (new_value >= connection_control::MIN_DELAY &&
      new_value <= connection_control::MAX_DELAY &&
      new_value >= existing_value)
  {
    *(reinterpret_cast<longlong *>(save))= new_value;
    return 0;
  }
  return 1;
}


/**
  update() function for connection_control_max_connection_delay

  Updates g_connection_event_coordinator with new value.
  Also notifies observers about the update.

  @param thd        Not used.
  @param var        Not used.
  @param var_ptr    Variable information
  @param save       New value for connection_control_max_connection_delay
*/

static void
update_max_connection_delay(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                            struct st_mysql_sys_var *var MY_ATTRIBUTE((unused)),
                            void *var_ptr, const void *save)
{
  longlong new_value= *(reinterpret_cast<const longlong *>(save));
  my_atomic_store64(&g_variables.max_connection_delay, (int64)new_value);
  Connection_control_error_handler error_handler(connection_control_plugin_info);
  g_connection_event_coordinator->notify_sys_var(&error_handler,
                                                 OPT_MAX_CONNECTION_DELAY,
                                                 &new_value);
  return;
}


/** Declaration of connection_control_max_connection_delay */
static MYSQL_SYSVAR_LONGLONG(
  max_connection_delay,
  g_variables.max_connection_delay,
  PLUGIN_VAR_RQCMDARG,
  "Maximum delay to be introduced. Default is 2147483647.",
  check_max_connection_delay,
  update_max_connection_delay,
  connection_control::DEFAULT_MAX_DELAY,
  connection_control::MIN_DELAY,
  connection_control::MAX_DELAY,
  1
);


/** Array of system variables. Used in plugin declaration. */
struct st_mysql_sys_var *
connection_control_system_variables[OPT_LAST + 1]=
{
  MYSQL_SYSVAR(failed_connections_threshold),
  MYSQL_SYSVAR(min_connection_delay),
  MYSQL_SYSVAR(max_connection_delay),
  NULL
};


/**
  Function to display value for status variable : Connection_control_delay_generated

  @param thd  MYSQL_THD handle. Unused.
  @param var  Status variable structure
  @param buff Value buffer.

  @returns Always returns success.
*/

static int show_delay_generated(MYSQL_THD,
                                struct st_mysql_show_var *var,
                                char *buff)
{
  var->type= SHOW_LONGLONG;
  var->value= buff;
  longlong *value= reinterpret_cast<longlong *>(buff);
  int64 current_val= my_atomic_load64(&g_statistics.stats_array[STAT_CONNECTION_DELAY_TRIGGERED]);
  *value= static_cast<longlong>(current_val);
  return 0;
}


/** Array of status variables. Used in plugin declaration. */
struct st_mysql_show_var
connection_control_status_variables[STAT_LAST + 1]={
  {
    "Connection_control_delay_generated",
    (char *)&show_delay_generated,
    SHOW_FUNC, SHOW_SCOPE_GLOBAL
  },
  {0, 0, enum_mysql_show_type(0),  enum_mysql_show_scope(0)}
};


mysql_declare_plugin(audit_log)
{
  MYSQL_AUDIT_PLUGIN,                   /* plugin type                   */
  &connection_control_descriptor,       /* type specific descriptor      */
  "CONNECTION_CONTROL",                 /* plugin name                   */
  "Oracle Inc",                         /* author                        */
  "Connection event processing",        /* description                   */
  PLUGIN_LICENSE_GPL        ,           /* license                       */
  connection_control_init,              /* plugin initializer            */
  connection_control_deinit,            /* plugin deinitializer          */
  0x0100,                               /* version                       */
  connection_control_status_variables,  /* status variables              */
  connection_control_system_variables,  /* system variables              */
  NULL,                                 /* reserverd                     */
  0                                     /* flags                         */
},
{
  MYSQL_INFORMATION_SCHEMA_PLUGIN,
   &connection_control_failed_attempts_view,
   "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS",
   "Oracle Inc",
   "I_S table providing a view into failed attempts statistics",
   PLUGIN_LICENSE_GPL,
   connection_control_failed_attempts_view_init,
   NULL,
   0x0100,
   NULL,
   NULL,
   NULL,
   0
}
mysql_declare_plugin_end;
