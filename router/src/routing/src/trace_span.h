/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTING_TRACE_SPAN_INCLUDED
#define ROUTING_TRACE_SPAN_INCLUDED

#include <chrono>
#include <list>
#include <string>
#include <variant>
#include <vector>

struct TraceEvent {
  using element_type =
      std::pair<std::string,
                std::variant<std::monostate, int64_t, bool, std::string>>;
  using attributes_type = std::vector<element_type>;

  TraceEvent(std::string name_, attributes_type attrs_)
      : start_time_system(std::chrono::system_clock::now()),
        start_time(std::chrono::steady_clock::now()),

        end_time(start_time),
        name(std::move(name_)),
        attrs(std::move(attrs_)) {}

  TraceEvent(std::string name_)
      : start_time_system(std::chrono::system_clock::now()),
        start_time(std::chrono::steady_clock::now()),
        end_time(start_time),
        name(std::move(name_)) {}

  std::chrono::system_clock::time_point start_time_system;
  std::chrono::steady_clock::time_point start_time;
  std::chrono::steady_clock::time_point end_time;

  std::list<TraceEvent> events;

  std::string name;

  attributes_type attrs;

  enum class StatusCode {
    kUnset,
    kOk,
    kError,
  };

  StatusCode status_code{StatusCode::kUnset};
};

/**
 * Events of a command.
 */
class TraceSpan {
 public:
  [[nodiscard]] bool active() const { return active_; }
  void active(bool v) { active_ = v; }

  [[nodiscard]] std::chrono::system_clock::time_point start_system_time_point()
      const {
    return start_system_time_point_;
  }

  [[nodiscard]] std::chrono::steady_clock::time_point start_time_point() const {
    return start_time_point_;
  }

  const std::list<TraceEvent> &events() const { return events_; }

  std::list<TraceEvent> &events() { return events_; }

  void clear() { events_.clear(); }

  [[nodiscard]] bool empty() const { return events_.empty(); }

  operator bool() const { return active_; }

 private:
  std::list<TraceEvent> events_;

  std::chrono::system_clock::time_point start_system_time_point_{
      std::chrono::system_clock::now()};
  std::chrono::steady_clock::time_point start_time_point_{
      std::chrono::steady_clock::now()};

  bool active_{false};
};

#endif
