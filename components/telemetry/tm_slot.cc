/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "tm_slot.h"
#include "tm_global.h"
#include "tm_log.h"
#include "tm_required_services.h"

namespace telemetry {

static bool telemetry_slot_is_registered = false;

mysql_thd_store_slot g_telemetry_slot = nullptr;
static const char *slot_name = "telemetry";

int free_telemetry_slot(void *) { return 0; }

int register_telemetry_slot() {
  assert(!telemetry_slot_is_registered);
  const int rc = thd_store_srv->register_slot(slot_name, free_telemetry_slot,
                                              &g_telemetry_slot);
  if (rc != 0) {
    log_error("%s: Failed to register telemetry slot.", component_name);
  } else {
    telemetry_slot_is_registered = true;
  }
  return rc;
}

void unregister_telemetry_slot() {
  if (telemetry_slot_is_registered) {
    const int rc = thd_store_srv->unregister_slot(g_telemetry_slot);
    if (rc != 0) {
      log_error("%s: Failed to unregister telemetry slot.", component_name);
    } else {
      telemetry_slot_is_registered = false;
    }
  }
}

Session_data *Session_data::get(MYSQL_THD thd) {
  void *opaque = thd_store_srv->get(thd, g_telemetry_slot);
  auto *data = reinterpret_cast<Session_data *>(opaque);
  return data;
}

void Session_data::set(MYSQL_THD thd, Session_data *data) {
  const int rc = thd_store_srv->set(thd, g_telemetry_slot, data);
  if (rc != 0) {
    log_error("%s: Failed to set telemetry slot in session data.",
              component_name);
  }
}

Session_data *Session_data::create(MYSQL_THD thd, bool trace) {
  auto *data = new Session_data(thd, trace);
  return data;
}

void Session_data::destroy(Session_data *data) {
  assert(data);
  assert(data->m_closed);
  assert(!data->m_used_in_telemetry);
  delete data;
}

Session_data::Session_data(MYSQL_THD thd, bool trace)
    : m_session_tracer(g_tracer),
      m_session_span(nullptr),
      m_stmt_stack(),
      m_thd(thd),
      m_depth(0),
      m_query_attributes_seen(false),
      m_closed(false),
      m_used_in_telemetry(false),
      m_trace(trace) {}

void Session_data::close() {
  if (!m_closed) {
    if (m_session_span != nullptr) {
      m_session_span->End();
    }

    m_closed = true;
  }
}

}  // namespace telemetry
