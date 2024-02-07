/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "server_telemetry_data.h"
#include <cassert>
#include "required_services.h"

namespace test_telemetry {

mysql_thd_store_slot g_telemetry_slot = nullptr;
static const char *slot_name = "test_telemetry_server";

static int free_resource_callback(void *resource [[maybe_unused]]) {
  // fallback for cases that do not emit tm_session_destroy or THD already
  // destroyed
  auto *data = reinterpret_cast<Session_data *>(resource);
  Session_data::destroy(data);
  return 0;
}

bool register_server_telemetry_slot(FileLogger &log) {
  const bool failure = thd_store_srv->register_slot(
      slot_name, free_resource_callback, &g_telemetry_slot);
  if (failure) log.write("Failed to register telemetry slot.\n");
  return failure;
}

void unregister_server_telemetry_slot(FileLogger &log) {
  const bool failure = thd_store_srv->unregister_slot(g_telemetry_slot);
  if (failure) log.write("Failed to unregister telemetry slot.\n");
}

Session_data *Session_data::get(MYSQL_THD thd) {
  void *opaque = thd_store_srv->get(thd, g_telemetry_slot);
  auto *data = reinterpret_cast<Session_data *>(opaque);
  return data;
}

void Session_data::set(MYSQL_THD thd, Session_data *data, FileLogger &log) {
  const bool failure = thd_store_srv->set(thd, g_telemetry_slot, data);
  if (failure) log.write("Failed to set session data to a registered slot.");
}

Session_data *Session_data::create() {
  auto *data = new Session_data();
  return data;
}

void Session_data::destroy(Session_data *data) { delete data; }

void Session_data::discard_stmt() {
  assert(!m_stmt_stack.empty());
  m_stmt_stack.pop_back();
}

}  // namespace test_telemetry
