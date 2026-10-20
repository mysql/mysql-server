/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace hm {

/**
  Store and synchronize the effective automatic monitored path.
*/
class Monitored_path_cache {
 public:
  void store(const std::optional<std::string> &path) {
    std::scoped_lock guard{m_mutex};
    ++m_generation;
    m_refresh_in_progress = false;
    m_path = path;
  }

  void request_refresh() {
    std::scoped_lock guard{m_mutex};
    m_refresh_requested = true;
    ++m_generation;
  }

  [[nodiscard]] uint64_t generation() const {
    std::scoped_lock guard{m_mutex};
    return m_generation;
  }

  bool begin_refresh() {
    std::scoped_lock guard{m_mutex};
    if ((!m_refresh_requested && m_path.has_value()) || m_refresh_in_progress) {
      return false;
    }
    m_refresh_requested = false;
    m_refresh_in_progress = true;
    return true;
  }

  bool select(const std::string &configured_path, std::string &effective_path,
              uint64_t &generation) const {
    std::scoped_lock guard{m_mutex};
    generation = m_generation;
    if (!configured_path.empty()) {
      effective_path = configured_path;
      return true;
    }
    if (!m_path.has_value()) {
      effective_path.clear();
      return false;
    }
    effective_path = *m_path;
    return true;
  }

 private:
  mutable std::mutex m_mutex;
  std::optional<std::string> m_path;
  bool m_refresh_requested{false};
  bool m_refresh_in_progress{false};
  uint64_t m_generation{0};
};

}  // namespace hm
