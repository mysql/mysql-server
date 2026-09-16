// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <cassert>
#include "mysql/scheduler/statistics_instance_monitor.h"
#include "mysql/scheduler/statistics_type_traits.h"

namespace mysql::scheduler {

template <Statistic_allowed_type Type>
void Statistics_instance_monitor::register_stat(const std::string &name,
                                                std::size_t thread_num,
                                                bool enable) {
  auto ins_res = m_registry.insert(
      std::make_pair(name, typename Statistic_type_traits<Type>::type()));
  auto &stat = ins_res.first->second;
  stat.init(thread_num, enable);
}

template <Statistic_allowed_type Type>
std::optional<
    std::reference_wrapper<typename Statistic_type_traits<Type>::type>>
Statistics_instance_monitor::find_stat(const std::string &name) {
  auto it = m_registry.find(name);
  if (it != m_registry.end()) {
    return it->second;
  }
  return {};
}

template <Statistic_allowed_type Type>
typename Statistic_type_traits<Type>::type &
Statistics_instance_monitor::get_stat(const std::string &name) {
  return m_registry[name];
}

}  // namespace mysql::scheduler
