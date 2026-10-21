/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <string_view>

#include "mysql/components/services/mysql_runtime_error_service.h"

#include "mrs_component.h"
#include "mrs_component_udfs.h"

namespace mrs_component {

unsigned long constexpr MAX_SAFE_LENGTH = 255;

constexpr const char kRestartUdfName[] = "mysql_rest_service_restart";
constexpr const char kStopUdfName[] = "mysql_rest_service_stop";
constexpr const char kStartUdfName[] = "mysql_rest_service_start";

namespace {

bool has_mysql_rest_service_admin() {
  MYSQL_THD current_thd = nullptr;
  Security_context_handle security_context = nullptr;

  if (mysql_service_mysql_current_thread_reader->get(&current_thd) != 0 ||
      current_thd == nullptr)
    return false;

  if (thread_security_ctx->get(current_thd, &security_context) != 0 ||
      security_context == nullptr)
    return false;

  bool has_grant = global_grants_checker->has_global_grant(
                       security_context, kMysqlRestServiceAdmin,
                       std::strlen(kMysqlRestServiceAdmin)) != 0;

  return has_grant;
}

bool validate_lifecycle_udf_call(const char *udf_name) {
  if (!has_mysql_rest_service_admin()) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), kMysqlRestServiceAdmin);
    return false;
  }

  MYSQL_THD current_thd = nullptr;
  mysql_cstring_with_length sql_command{};
  bool is_substatement = true;

  if (mysql_service_mysql_current_thread_reader->get(&current_thd) != 0 ||
      current_thd == nullptr ||
      thd_attributes->get(current_thd, "sql_command", &sql_command) != 0 ||
      thd_attributes->get(current_thd, "is_substatement", &is_substatement) !=
          0 ||
      sql_command.str == nullptr ||
      std::string_view(sql_command.str, sql_command.length) != "select" ||
      is_substatement) {
    my_error(ER_MRS_COMPONENT_UDF_ERROR, MYF(0), udf_name,
             "This function must be called from a SELECT statement outside a "
             "stored function or trigger");
    return false;
  }

  return true;
}

}  // namespace

void UDFs::throw_udf_error(const char *udf_name, const char *error_message) {
  my_error(ER_MRS_COMPONENT_UDF_ERROR, MYF(0), udf_name, error_message);

  MRS_LOG_ERROR("%s.", error_message);
}

/**
  Initialize the MySQL Rest Service Component UDFs.
*/
int UDFs::initialize_all() {
  // mysql_rest_service_restart()
  {
    UDF restart_mrs;
    restart_mrs.name = kRestartUdfName;
    restart_mrs.return_type = Item_result::INT_RESULT;
    restart_mrs.function =
        reinterpret_cast<Udf_func_any>(mysql_rest_service_restart);
    restart_mrs.init_function =
        reinterpret_cast<Udf_func_init>(mysql_rest_service_restart_init);
    restart_mrs.deinit_function =
        reinterpret_cast<Udf_func_deinit>(mysql_rest_service_restart_deinit);

    m_udf_list.insert(restart_mrs);
  }

  // mysql_rest_service_stop()
  {
    UDF stop_mrs;
    stop_mrs.name = kStopUdfName;
    stop_mrs.return_type = Item_result::INT_RESULT;
    stop_mrs.function = reinterpret_cast<Udf_func_any>(mysql_rest_service_stop);
    stop_mrs.init_function =
        reinterpret_cast<Udf_func_init>(mysql_rest_service_stop_init);
    stop_mrs.deinit_function =
        reinterpret_cast<Udf_func_deinit>(mysql_rest_service_stop_deinit);

    m_udf_list.insert(stop_mrs);
  }

  // mysql_rest_service_start()
  {
    UDF start_mrs;
    start_mrs.name = kStartUdfName;
    start_mrs.return_type = Item_result::INT_RESULT;
    start_mrs.function =
        reinterpret_cast<Udf_func_any>(mysql_rest_service_start);
    start_mrs.init_function =
        reinterpret_cast<Udf_func_init>(mysql_rest_service_start_init);
    start_mrs.deinit_function =
        reinterpret_cast<Udf_func_deinit>(mysql_rest_service_start_deinit);

    m_udf_list.insert(start_mrs);
  }

  return 0;
}

/**
  Register the MySQL Rest Service Component UDFs in the server.
*/
int UDFs::register_all() {
  for (const auto &udf : m_udf_list) {
    if (udf_registry->udf_register(udf.name, udf.return_type, udf.function,
                                   udf.init_function,
                                   udf.deinit_function) != 0) {
      MRS_LOG_ERROR("Failed to register UDF '%s'", udf.name);
      return 1;
    }

    m_registered_udfs.emplace(udf.name);
  }

  return 0;
}

/**
  Unregister the MySQL Rest Service Component UDFs from the server.
*/
int UDFs::unregister_all() {
  int error = 0;
  auto it = m_registered_udfs.begin();
  while (it != m_registered_udfs.end()) {
    const auto &udf_name = *it;
    int was_present = 0;
    int rc = udf_registry->udf_unregister(udf_name.c_str(), &was_present);

    if (rc != 0 && was_present != 0) {
      MRS_LOG_ERROR("Failed to unregister UDF '%s'", udf_name.c_str());
      error = 1;
      ++it;
    } else {
      it = m_registered_udfs.erase(it);
    }
  }

  return error;
}

/**
   Callback function for initialization of UDF "mysql_rest_service_restart".
*/
bool UDFs::mysql_rest_service_restart_init(UDF_INIT *initid, UDF_ARGS *args,
                                           char *message) {
  initid->ptr = nullptr;

  if (args->arg_count != 0) {
    (void)snprintf(message, MAX_SAFE_LENGTH, "%s() expects no arguments",
                   kRestartUdfName);

    return true;
  }

  return false;
}

/**
   Callback function for de-initialization of UDF "mysql_rest_service_restart".
*/
void UDFs::mysql_rest_service_restart_deinit(UDF_INIT *initid
                                             [[maybe_unused]]) {}

/**
  UDF for "mysql_rest_service_restart"
*/
long long UDFs::mysql_rest_service_restart(UDF_INIT *, UDF_ARGS *,
                                           unsigned char *,
                                           unsigned char *error) {
  if (!validate_lifecycle_udf_call(kRestartUdfName)) {
    *error = 1;
    return 1;
  }

  MRS_LOG_INFO("Restarting the MySQL Rest Service component...");
  // Restart waits for the old backend to stop, but the replacement backend
  // continues starting asynchronously after this UDF returns.
  if (mrs_component().backend_restart() != 0) {
    throw_udf_error(kRestartUdfName,
                    "Failed to restart MySQL Rest Service component");
    *error = 1;
    return 1;
  }

  MRS_LOG_INFO("MySQL Rest Service component restart initiated.");
  return 0;
}

/**
   Callback function for initialization of UDF "mysql_rest_service_stop".
*/
bool UDFs::mysql_rest_service_stop_init(UDF_INIT *initid, UDF_ARGS *args,
                                        char *message) {
  initid->ptr = nullptr;

  if (args->arg_count != 0) {
    (void)snprintf(message, MAX_SAFE_LENGTH, "%s() expects no arguments",
                   kStopUdfName);
    return true;
  }

  return false;
}

/**
   Callback function for de-initialization of UDF "mysql_rest_service_stop".
*/
void UDFs::mysql_rest_service_stop_deinit(UDF_INIT *initid [[maybe_unused]]) {}

/**
  UDF for "mysql_rest_service_stop"
*/
long long UDFs::mysql_rest_service_stop(UDF_INIT *, UDF_ARGS *, unsigned char *,
                                        unsigned char *error) {
  if (!validate_lifecycle_udf_call(kStopUdfName)) {
    *error = 1;
    return 1;
  }

  MRS_LOG_INFO("Stopping the MySQL Rest Service component...");
  // Stop is synchronous: success means the backend reached STOPPED.
  if (mrs_component().backend_stop() != 0) {
    throw_udf_error(kStopUdfName,
                    "Failed to stop MySQL Rest Service component");
    *error = 1;
    return 1;
  }

  MRS_LOG_INFO("MySQL Rest Service component stopped successfully.");
  return 0;
}

/**
   Callback function for initialization of UDF "mysql_rest_service_start".
*/
bool UDFs::mysql_rest_service_start_init(UDF_INIT *initid, UDF_ARGS *args,
                                         char *message) {
  initid->ptr = nullptr;

  if (args->arg_count != 0) {
    (void)snprintf(message, MAX_SAFE_LENGTH, "%s() expects no arguments",
                   kStartUdfName);
    return true;
  }

  return false;
}

/**
   Callback function for de-initialization of UDF "mysql_rest_service_start".
*/
void UDFs::mysql_rest_service_start_deinit(UDF_INIT *initid [[maybe_unused]]) {}

/**
  UDF for "mysql_rest_service_start"
*/
long long UDFs::mysql_rest_service_start(UDF_INIT *, UDF_ARGS *,
                                         unsigned char *is_null,
                                         unsigned char *is_error) {
  if (!validate_lifecycle_udf_call(kStartUdfName)) {
    *is_null = 1;
    *is_error = 1;
    return 1;
  }

  MRS_LOG_INFO("Starting the MySQL Rest Service component...");

  // Success accepts asynchronous startup; final completion is reported by
  // mysql_rest_service.state and the Server error log.
  if (mrs_component().backend_start() != 0) {
    throw_udf_error(kStartUdfName,
                    "Failed to start MySQL Rest Service backend");

    *is_null = 1;
    *is_error = 1;

    return 1;
  }

  MRS_LOG_INFO("MySQL Rest Service component start request accepted.");
  return 0;
}

}  // namespace mrs_component
