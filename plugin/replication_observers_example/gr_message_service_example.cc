/* Copyright (c) 2019, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/replication_observers_example/gr_message_service_example.h"
#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_message_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_runtime_error_service.h>
#include <mysql/components/services/registry.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/service_plugin_registry.h>
#include <cstring>
#include <string>
#include "include/my_dbug.h"
#include "include/my_inttypes.h"
#include "mysqld_error.h"

DEFINE_BOOL_METHOD(recv, (const char *tag, const unsigned char *data,
                          size_t data_length)) {
  DBUG_TRACE;

  std::string buffer;

  DBUG_EXECUTE_IF("gr_message_service_fail_recv", { return true; });

  buffer.append("Service message recv TAG: ");
  // LogPluginErr truncate output when messages have a size > +-8059

  if (strlen(tag) < 4001) {
    buffer.append("\"");
    buffer.append(tag);
    buffer.append("\"");
  } else {
    buffer.append("over 4k bytes");
  }

  buffer.append(", TAG_SIZE: ");
  buffer.append(std::to_string(strlen(tag)));

  buffer.append(", MSG: ");
  if (data_length < 4001) {
    buffer.append("\"");
    buffer.append(reinterpret_cast<const char *>(data), data_length);
    buffer.append("\"");
  } else {
    buffer.append("over 4k bytes");
  }

  buffer.append(", MSG_SIZE: ");
  buffer.append(std::to_string(data_length));
  buffer.append(".");

  LogPluginErr(INFORMATION_LEVEL, ER_LOG_PRINTF_MSG, buffer.c_str());

  return false;
}

BEGIN_SERVICE_IMPLEMENTATION(replication_observers_example,
                             group_replication_message_service_recv)
recv, END_SERVICE_IMPLEMENTATION();

bool register_gr_message_service_recv() {
  DBUG_TRACE;
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      plugin_registry);
  using group_replication_message_service_recv_t =
      SERVICE_TYPE_NO_CONST(group_replication_message_service_recv);
  bool result = reg->register_service(
      "group_replication_message_service_recv.replication_observers_example",
      reinterpret_cast<my_h_service>(
          const_cast<group_replication_message_service_recv_t *>(
              &SERVICE_IMPLEMENTATION(
                  replication_observers_example,
                  group_replication_message_service_recv))));

  mysql_plugin_registry_release(plugin_registry);
  return result;
}

bool unregister_gr_message_service_recv() {
  DBUG_TRACE;
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  my_service<SERVICE_TYPE(registry_registration)> reg("registry_registration",
                                                      plugin_registry);
  bool result = reg->unregister(
      "group_replication_message_service_recv.replication_observers_example");

  mysql_plugin_registry_release(plugin_registry);
  return result;
}

static std::string send_udf_name("group_replication_service_message_send");
GR_message_service_send_example example_service_send;

#ifdef __clang__
// Clang UBSAN false positive?
// Call to function through pointer to incorrect function type
char *GR_message_service_send_example::udf(UDF_INIT *, UDF_ARGS *args,
                                           char *result, unsigned long *length,
                                           unsigned char *,
                                           unsigned char *) SUPPRESS_UBSAN {
#else

char *GR_message_service_send_example::udf(UDF_INIT *, UDF_ARGS *args,
                                           char *result, unsigned long *length,
                                           unsigned char *, unsigned char *) {
#endif  // __clang__
  DBUG_TRACE;

  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();
  my_service<SERVICE_TYPE(group_replication_message_service_send)> svc(
      "group_replication_message_service_send", plugin_registry);
  my_service<SERVICE_TYPE(mysql_runtime_error)> svc_error("mysql_runtime_error",
                                                          plugin_registry);

  if (svc.is_valid()) {
    const size_t payload_length = static_cast<size_t>(args->lengths[1]);
    const bool error = svc->send(
        reinterpret_cast<const char *>(args->args[0]),
        reinterpret_cast<const unsigned char *>(args->args[1]), payload_length);
    if (error) {
      const char *return_message =
          "Service failed sending message to the group.";
      size_t return_length = strlen(return_message);
      strcpy(result, return_message);
      *length = return_length;
      if (svc_error.is_valid()) {
        mysql_error_service_emit_printf(svc_error, ER_UDF_ERROR, 0,
                                        send_udf_name.c_str(), return_message);
      }
    } else {
      const char *return_message = "The tag and message was sent to the group.";
      size_t return_length = strlen(return_message);
      strcpy(result, return_message);
      *length = return_length;
    }
  } else {
    const char *return_message =
        "No send service to propagate message to a group.";
    size_t return_length = strlen(return_message);
    strcpy(result, return_message);
    *length = return_length;
    if (svc_error.is_valid()) {
      mysql_error_service_emit_printf(svc_error, ER_UDF_ERROR, 0,
                                      send_udf_name.c_str(), return_message);
    }
  }

  mysql_plugin_registry_release(plugin_registry);
  return result;
}

bool GR_message_service_send_example::udf_init(UDF_INIT *init_id,
                                               UDF_ARGS *args, char *message) {
  DBUG_TRACE;

  if (args->arg_count != 2 || args->arg_type[0] != STRING_RESULT ||
      args->arg_type[1] != STRING_RESULT) {
    strcpy(
        message,
        "Wrong arguments: You need to specify a tag and message to be sent.");
    return true;
  }

  init_id->maybe_null = false;

  return false;
}

bool GR_message_service_send_example::register_example() {
  DBUG_TRACE;
  bool error = false;
  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();

  if (plugin_registry == nullptr) {
    /* purecov: begin inspected */
    error = true;
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Could not execute the installation of GR message service UDF "
                 "functions. Check for other errors in the log and try to "
                 "reinstall the plugin");
    goto end;
    /* purecov: end */
  }

  {
    /* We open a new scope so that udf_register is (automatically) destroyed
      before plugin_registry. */
    my_service<SERVICE_TYPE(udf_registration)> udf_register("udf_registration",
                                                            plugin_registry);
    if (udf_register.is_valid()) {
      error = udf_register->udf_register(
          send_udf_name.c_str(), Item_result::STRING_RESULT,
          reinterpret_cast<Udf_func_any>(GR_message_service_send_example::udf),
          GR_message_service_send_example::udf_init, nullptr);
      if (error) {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                     "Could not execute the installation of GR message service "
                     "UDF function: group_replication_service_message_send. "
                     "Check if the function is already present, if so, try to "
                     "remove it");
        /* purecov: end */
      }

      if (error) {
        /* purecov: begin inspected */
        int was_present;
        // Don't care about errors since we are already erroring out.
        udf_register->udf_unregister(send_udf_name.c_str(), &was_present);
        /* purecov: end */
      }
    } else {
      /* purecov: begin inspected */
      error = true;
      LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                   "Could not execute the installation of Group Replication UDF"
                   "functions. Check for other errors in the log and try to"
                   "reinstall the plugin");
      /* purecov: end */
    }
  }
  mysql_plugin_registry_release(plugin_registry);
end:
  return error;
}

bool GR_message_service_send_example::unregister_example() {
  DBUG_TRACE;

  bool error = false;

  SERVICE_TYPE(registry) *plugin_registry = mysql_plugin_registry_acquire();

  if (plugin_registry == nullptr) {
    /* purecov: begin inspected */
    error = true;
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Could not uninstall GR message service UDF functions. Try to "
                 "remove them manually if present.");
    goto end;
    /* purecov: end */
  }

  {
    /* We open a new scope so that udf_registry is (automatically) destroyed
      before plugin_registry. */
    my_service<SERVICE_TYPE(udf_registration)> udf_registry("udf_registration",
                                                            plugin_registry);
    if (udf_registry.is_valid()) {
      int was_present;
      error = udf_registry->udf_unregister(send_udf_name.c_str(), &was_present);
    } else {
      error = true; /* purecov: inspected */
    }

    if (error) {
      LogPluginErr(
          ERROR_LEVEL, ER_LOG_PRINTF_MSG,
          "Could not uninstall GR message service UDF functions. Try "
          "to remove them manually if present."); /* purecov: inspected */
    }
  }
  mysql_plugin_registry_release(plugin_registry);
end:
  return error;
}

bool gr_service_message_example_init() {
  DBUG_TRACE;
  bool failed = false;

  if (example_service_send.register_example()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failed to register udf functions.");
    failed = true;
    /* purecov: end */
  }

  if (register_gr_message_service_recv()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failed to register recv service.");
    failed = true;
    /* purecov: end */
  }
  return failed;
}

bool gr_service_message_example_deinit() {
  DBUG_TRACE;
  bool failed = false;

  if (example_service_send.unregister_example()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failed to unregister udf functions.");
    failed = true;
    /* purecov: end */
  }

  if (unregister_gr_message_service_recv()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG,
                 "Failed to unregister recv service.");
    failed = true;
    /* purecov: end */
  }
  return failed;
}
