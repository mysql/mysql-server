/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "plugin/x/src/services/mysqlx_group_membership_listener.h"

#include <mysql/components/service_implementation.h>

#include "plugin/x/src/module_mysqlx.h"
#include "plugin/x/src/ngs/notice_descriptor.h"

DEFINE_BOOL_METHOD(notify_view_change, (const char *view_id)) {
  auto queue = modules::Module_mysqlx::get_instance_notice_queue();

  if (queue.container()) {
    queue->emplace(ngs::Notice_type::k_group_replication_view_changed, view_id);
  }

  return false;
}

DEFINE_BOOL_METHOD(notify_quorum_loss, (const char *view_id)) {
  auto queue = modules::Module_mysqlx::get_instance_notice_queue();
  if (queue.container()) {
    queue->emplace(ngs::Notice_type::k_group_replication_quorum_loss, view_id);
  }

  return false;
}

SERVICE_TYPE_NO_CONST(group_membership_listener)
SERVICE_IMPLEMENTATION(mysqlx, group_membership_listener) = {
    notify_view_change, notify_quorum_loss};
