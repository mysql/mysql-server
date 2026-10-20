/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#pragma once

#include <map>
#include <shared_mutex>
#include <string>

#include "component_command.h"
#include "health_monitor_collector.h"
#include "health_monitor_lock.h"
#include "health_monitor_query.h"

#include "my_rapidjson_size_t.h"
#include "rapidjson/document.h"

namespace hm {

struct Report {
  void issue_report();
  std::string title;
  std::string footer;
  std::vector<Query> queries;
  uint interval;
  int next;
};

using Internal_Queries = std::map<const std::string, Query>;
using Reports = std::map<const std::string, Report>;

/**
  The Report_Definition_Manager keeps track of all the report definitions for
  a reporter. If a new definition is assigned, through a global server
  variable, the Report_Definition_Manager will update its internal definition.
 */
class Report_Definition_Manager {
 public:
  Report_Definition_Manager() = default;
  ~Report_Definition_Manager() = default;

  void update_settings(const char *);
  bool settings_changed(const char *);
  static bool settings_valid(const char *, rapidjson::Document *doc = nullptr);
  static Internal_Queries m_internal_queries;
  Reports *reports() { return &m_reports; }

 private:
  const char *m_cfg = nullptr;
  Reports m_reports;
};

}  // namespace hm
