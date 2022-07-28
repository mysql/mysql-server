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

#ifndef PLUGIN_X_SRC_COMPONENTS_TEST_EMIT_GR_NOTIFICATIONS_H_
#define PLUGIN_X_SRC_COMPONENTS_TEST_EMIT_GR_NOTIFICATIONS_H_

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/group_member_status_listener.h>
#include <mysql/components/services/group_membership_listener.h>
#include <mysql/components/services/udf_metadata.h>

extern REQUIRES_SERVICE_PLACEHOLDER(group_member_status_listener);
extern REQUIRES_SERVICE_PLACEHOLDER(group_membership_listener);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_udf_metadata);

bool udf_func_init(UDF_INIT *, UDF_ARGS *udf_args, char *);

long long udf_emit_member_role_change(  // NOLINT(runtime/int)
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *);

long long udf_emit_member_state_change(  // NOLINT(runtime/int)
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *);
long long udf_emit_view_change(  // NOLINT(runtime/int)
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *);

long long udf_emit_quorum_loss(  // NOLINT(runtime/int)
    UDF_INIT *, UDF_ARGS *args, unsigned char *, unsigned char *);

const char *const k_udf_emit_member_role_change = "emit_member_role_change";
const char *const k_udf_emit_member_state_change = "emit_member_state_change";
const char *const k_udf_emit_view_change = "emit_view_change";
const char *const k_udf_emit_quorum_loss = "emit_quorum_loss";

#endif  // PLUGIN_X_SRC_COMPONENTS_TEST_EMIT_GR_NOTIFICATIONS_H_
