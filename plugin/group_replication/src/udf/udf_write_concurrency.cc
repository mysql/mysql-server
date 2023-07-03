/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/udf/udf_write_concurrency.h"
#include <cinttypes>
#include "mysql/plugin.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/udf/udf_utils.h"

const char *const wrong_nr_args_str = "UDF takes one integer argument.";

static bool group_replication_get_write_concurrency_init(UDF_INIT *,
                                                         UDF_ARGS *args,
                                                         char *message) {
  bool constexpr failure = true;
  bool constexpr success = false;
  bool result = failure;

  /*
    Increment only after verifying the plugin is not stopping
    Do NOT increment before accessing volatile plugin structures.
    Stop is checked again after increment as the plugin might have stopped
  */
  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return result;
  }
  UDF_counter udf_counter;

  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    goto end;
  }

  if (args->arg_count != 0) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, "UDF does not take arguments.");
    goto end;
  }
  if (!member_online_with_majority()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    goto end;
  }
  result = success;
  udf_counter.succeeded();
end:

  return result;
}

static void group_replication_get_write_concurrency_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

static long long group_replication_get_write_concurrency(UDF_INIT *, UDF_ARGS *,
                                                         unsigned char *is_null,
                                                         unsigned char *error) {
  assert(member_online_with_majority());
  uint32_t write_concurrency = 0;
  gcs_module->get_write_concurrency(write_concurrency);
  *is_null = 0;  // result is not null
  *error = 0;
  return write_concurrency;
}

udf_descriptor get_write_concurrency_udf() {
  return {
      "group_replication_get_write_concurrency", Item_result::INT_RESULT,
      reinterpret_cast<Udf_func_any>(group_replication_get_write_concurrency),
      group_replication_get_write_concurrency_init,
      group_replication_get_write_concurrency_deinit};
}

const char *const value_outside_domain_str =
    "Argument must be between %" PRIu32 " and %" PRIu32 ".";

static bool group_replication_set_write_concurrency_init(UDF_INIT *initid,
                                                         UDF_ARGS *args,
                                                         char *message) {
  bool constexpr failure = true;
  bool constexpr success = false;
  bool result = failure;

  /*
    Increment only after verifying the plugin is not stopping
    Do NOT increment before accessing volatile plugin structures.
    Stop is checked again after increment as the plugin might have stopped
  */
  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    return result;
  }
  UDF_counter udf_counter;

  privilege_result privilege = privilege_result::error();
  bool const wrong_number_of_args = args->arg_count != 1;
  bool const wrong_arg_type =
      !wrong_number_of_args && args->arg_type[0] != INT_RESULT;

  if (get_plugin_is_stopping()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    goto end;
  }

  if (wrong_number_of_args || wrong_arg_type) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, wrong_nr_args_str);
    goto end;
  }
  if (!member_online_with_majority()) {
    std::snprintf(message, MYSQL_ERRMSG_SIZE, member_offline_or_minority_str);
    goto end;
  }
  privilege = user_has_gr_admin_privilege();
  log_privilege_status_result(privilege, message);
  switch (privilege.status) {
      // Something is wrong and we were unable to access MySQL services.
    case privilege_status::error:
    case privilege_status::no_privilege:
      goto end;
    case privilege_status::ok:
      break;
  }
  if (args->args[0] != nullptr) {
    uint32_t new_write_concurrency =
        *reinterpret_cast<long long *>(args->args[0]);
    uint32_t min_write_concurrency =
        gcs_module->get_minimum_write_concurrency();
    uint32_t max_write_concurrency =
        gcs_module->get_maximum_write_concurrency();
    bool const invalid_write_concurrency =
        new_write_concurrency < min_write_concurrency ||
        max_write_concurrency < new_write_concurrency;
    if (invalid_write_concurrency) {
      std::snprintf(message, MYSQL_ERRMSG_SIZE, value_outside_domain_str,
                    min_write_concurrency, max_write_concurrency);
      goto end;
    }
  }
  if (Charset_service::set_return_value_charset(initid)) goto end;

  result = success;
  udf_counter.succeeded();
end:
  return result;
}

static void group_replication_set_write_concurrency_deinit(UDF_INIT *) {
  UDF_counter::terminated();
}

static char *group_replication_set_write_concurrency(UDF_INIT *, UDF_ARGS *args,
                                                     char *result,
                                                     unsigned long *length,
                                                     unsigned char *is_null,
                                                     unsigned char *error) {
  /* According to sql/udf_example.cc, result has at least 255 bytes */
  unsigned long constexpr max_safe_length = 255;
  assert(member_online_with_majority());
  assert(user_has_gr_admin_privilege().status == privilege_status::ok);
  *is_null = 0;  // result is not null
  *error = 0;
  bool throw_error = false;
  bool log_error = false;
  uint32_t new_write_concurrency = 0;
  enum enum_gcs_error gcs_result = GCS_NOK;
  uint32_t min_write_concurrency = gcs_module->get_minimum_write_concurrency();
  uint32_t max_write_concurrency = gcs_module->get_maximum_write_concurrency();
  if (args->args[0] == nullptr) {
    std::snprintf(result, max_safe_length, wrong_nr_args_str);
    throw_error = true;
    goto end;
  }
  new_write_concurrency = *reinterpret_cast<long long *>(args->args[0]);
  if (new_write_concurrency < min_write_concurrency ||
      max_write_concurrency < new_write_concurrency) {
    std::snprintf(result, max_safe_length, value_outside_domain_str,
                  min_write_concurrency, max_write_concurrency);
    throw_error = true;
    goto end;
  }
  gcs_result = gcs_module->set_write_concurrency(new_write_concurrency);
  if (gcs_result == GCS_OK) {
    std::snprintf(result, max_safe_length,
                  "UDF is asynchronous, check log or call "
                  "group_replication_get_write_concurrency().");
  } else {
    std::snprintf(
        result, max_safe_length,
        "Could not set, please check the error log of group members.");
    throw_error = true;
    log_error = true;
  }
end:
  if (throw_error) {
    *error = 1;
    throw_udf_error("group_replication_set_write_concurrency", result,
                    log_error);
  }
  *length = strlen(result);
  return result;
}

udf_descriptor set_write_concurrency_udf() {
  return {
      "group_replication_set_write_concurrency", Item_result::STRING_RESULT,
      reinterpret_cast<Udf_func_any>(group_replication_set_write_concurrency),
      group_replication_set_write_concurrency_init,
      group_replication_set_write_concurrency_deinit};
}
