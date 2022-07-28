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

#include <algorithm>  // std::find
#include "binlog.h"   // mysql_bin_log
#include "mysqld.h"   // key_rwlock_gtid_mode_lock
#include "sql/rpl_gtid.h"
#include "sql/rpl_msr.h"  // channel_map

Gtid_mode global_gtid_mode;

ulong Gtid_mode::sysvar_mode;

const char *Gtid_mode::names[] = {"OFF", "OFF_PERMISSIVE", "ON_PERMISSIVE",
                                  "ON", NullS};

Checkable_rwlock Gtid_mode::lock{
#ifdef HAVE_PSI_INTERFACE
    key_rwlock_gtid_mode_lock
#endif
};

void Gtid_mode::set(value_type value) {
  m_atomic_mode.store(value, std::memory_order_release);
}

Gtid_mode::value_type Gtid_mode::get() const {
  return (value_type)m_atomic_mode.load(std::memory_order_acquire);
}

#ifndef NDEBUG
const char *Gtid_mode::get_string() const { return to_string(get()); }
#endif  // ifndef NDEBUG

std::pair<bool, Gtid_mode::value_type> Gtid_mode::from_string(
    const std::string s) {
  // Subtract 1 to exclude the null
  auto end = names + sizeof(names) / sizeof(names[0]) - 1;
  auto ret = std::find(names, end, s);
  if (ret == end)
    return std::pair<bool, Gtid_mode::value_type>(true, OFF);
  else
    return std::pair<bool, Gtid_mode::value_type>(
        false, static_cast<value_type>(ret - names));
}

const char *Gtid_mode::to_string(Gtid_mode::value_type value) {
  return names[value];
}

std::ostream &operator<<(std::ostream &oss, Gtid_mode::value_type const &mode) {
  oss << Gtid_mode::to_string(mode);
  return oss;
}

#ifndef NDEBUG
std::ostream &operator<<(std::ostream &oss, Gtid_mode const &mode) {
  oss << mode.get_string();
  return oss;
}
#endif
