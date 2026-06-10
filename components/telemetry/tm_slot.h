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

#ifndef TELEMETRY_SLOT_H_INCLUDED
#define TELEMETRY_SLOT_H_INCLUDED

#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/tracer_provider.h>

#include "mysql/components/services/mysql_thd_store_service.h"

#include <vector>

namespace telemetry {

int register_telemetry_slot();
void unregister_telemetry_slot();

class Session_data {
 public:
  static Session_data *get(MYSQL_THD thd);
  static void set(MYSQL_THD thd, Session_data *data);

  static Session_data *create(MYSQL_THD thd, bool trace);
  static void destroy(Session_data *data);

  explicit Session_data(MYSQL_THD thd, bool trace);
  ~Session_data() = default;

  void close();

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
      m_session_tracer;
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> m_session_span;
  std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>
      m_stmt_stack;
  MYSQL_THD m_thd;
  size_t m_depth;
  bool m_query_attributes_seen;
  bool m_closed;
  bool m_used_in_telemetry;
  bool m_trace;
};

}  // namespace telemetry

#endif /* TELEMETRY_SLOT_H_INCLUDED */
