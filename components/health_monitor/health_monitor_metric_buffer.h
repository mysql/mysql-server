/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#include <atomic>

#include "health_monitor_stat_buffer.h"

namespace hm {

/**
  Metric_Buffer extends the circular buffer, Stat_Buffer, with a two-way
  read/write capability such that it can be shared between multiple
  collectors.
*/
template <typename T>
class Metric_Buffer : public Stat_Buffer<T> {
 public:
  Metric_Buffer() = default;

  Metric_Buffer(bool no_lock, PSI_memory_key memory_key)
      : Stat_Buffer<T>(no_lock, memory_key) {}

  Metric_Buffer(PSI_rwlock_key rwlock_key, PSI_memory_key memory_key)
      : Stat_Buffer<T>(rwlock_key, memory_key) {}

  bool writeable() { return m_writeable.load(); }
  void set_writeable(bool writeable) { m_writeable.store(writeable); }

  // Write to the time series buffer, then set 'writeable' to false,
  // indicating that the write is complete.
  // Note that this method intentionally hides Stat_Buffer::write().
  int write(const T &record) {
    int ret = Stat_Buffer<T>::write(record);
    if (ret == 0) {
      set_writeable(false);
    }
    return ret;
  }

  // Provide a means to directly convert an external type U to a time series
  // type T, e.g. Disk_Info -> Disk_Metric. The method is enabled only if
  // the target type T is constructable from U.
  template <typename U>
  auto write(const U &arg)
      -> std::enable_if_t<std::is_constructible_v<T, U>, int> {
    T record(arg);
    return write(record);
  }

 private:
  std::atomic<bool> m_writeable{false};
};

}  // namespace hm