/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include <string>

#include "plugin/x/tests/components/test_emit_gr_notifications.h"

bool udf_func_init(UDF_INIT *, UDF_ARGS *udf_args, char *) {
  if (udf_args->arg_count != 1) return true;

  std::string value = "utf8mb4";
  mysql_service_mysql_udf_metadata->argument_set(udf_args, "charset", 0,
                                                 &value[0]);
  return false;
}

std::string get_arg0_string(UDF_ARGS *args) {
  if (args->arg_count != 1) {
    return {};
  }

  if (args->arg_type[0] != STRING_RESULT) {
    return {};
  }

  return args->args[0];
}

long long udf_emit_member_role_change(  // NOLINT(runtime/int)
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *) {
  const auto arg1 = get_arg0_string(args);

  if (arg1.empty()) return 0;

  return mysql_service_group_member_status_listener->notify_member_role_change(
             arg1.c_str())
             ? 0
             : 1;
}

long long udf_emit_member_state_change(  // NOLINT(runtime/int)
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *) {
  const auto arg1 = get_arg0_string(args);

  if (arg1.empty()) return 0;

  return mysql_service_group_member_status_listener->notify_member_state_change(
             arg1.c_str())
             ? 0
             : 1;
}

long long udf_emit_view_change(  // NOLINT(runtime/int)
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *) {
  const auto arg1 = get_arg0_string(args);

  if (arg1.empty()) return 0;

  return mysql_service_group_membership_listener->notify_view_change(
             arg1.c_str())
             ? 0
             : 1;
}

long long udf_emit_quorum_loss(  // NOLINT(runtime/int)
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *) {
  const auto arg1 = get_arg0_string(args);

  if (arg1.empty()) return 0;

  return mysql_service_group_membership_listener->notify_quorum_loss(
             arg1.c_str())
             ? 0
             : 1;
}
