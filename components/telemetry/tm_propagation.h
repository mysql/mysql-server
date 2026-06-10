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

#ifndef TELEMETRY_PROPAGATION_H_INCLUDED
#define TELEMETRY_PROPAGATION_H_INCLUDED

#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>

#include "mysql/components/services/bits/thd.h"

#include "tm_required_services.h"

namespace telemetry {

class QueryAttributeTextMapCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  explicit QueryAttributeTextMapCarrier(MYSQL_THD thd);

  ~QueryAttributeTextMapCarrier() override;

  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override;

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override;

  bool Keys(opentelemetry::nostd::function_ref<
            bool(opentelemetry::nostd::string_view)>
                callback) const noexcept override;

 private:
  MYSQL_THD m_thd;

  /*
    Keeps all keys seen,
    so that Get() can return a string_view.
    Pointer because Get() is const.
  */
  std::vector<my_h_string> *m_seen;
};

class QueryAttributeTextMapPropagator
    : public opentelemetry::trace::propagation::HttpTraceContext {};

}  // namespace telemetry

#endif /* TELEMETRY_PROPAGATION_H_INCLUDED */
