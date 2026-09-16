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

#include "health_monitor_metrics.h"
#include "health_monitor.h"
#include "health_monitor_disk.h"
#include "health_monitor_memory.h"
#include "health_monitor_process_memory_mapping.h"
#include "health_monitor_process_stats_mapping.h"
#include "health_monitor_stat_buffer.h"
#include "health_monitor_storage_mapping.h"
#include "health_monitor_system.h"

namespace hm {

// clang-format off
template <typename T, typename U>
constexpr bool CanTypeFitValue(const U value) {
  return ((value > U(0)) == (T(value) > T(0))) && U(T(value)) == value; // NOLINT
}

template <typename T, typename U>
T Clamp(U x) {
  return CanTypeFitValue<T>(x) ? static_cast<T>(x)
                               : x < 0 ? std::numeric_limits<T>::min()
                                       : std::numeric_limits<T>::max();
}
// clang-format on

//
// Telemetry metric sources instrumented within health monitor component
// are being defined below (metric info array, measurement callback
// implementations).
//

static void get_metric_disk_used_percent(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const double value = Health_Monitor::disk_collector()->m_used_pct;
  delivery->value_double_attr(
      delivery_context, value, "device",
      Health_Monitor::disk_collector()->m_info.device_name.c_str());
}

static void get_metric_memory_used_percent(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const double value = Health_Monitor::memory_collector()->m_used_pct;
  delivery->value_double(delivery_context, value);
}

static void get_metric_disk_available(void * /* measurement_context */,
                                      measurement_delivery_callback_t delivery,
                                      void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::Disk_Stats_Buffer stats =
      Health_Monitor::disk_collector()->buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  Disk_Info disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.timestamp;
    else if (timestamp != disk.timestamp)
      break;
    const auto measurement = disk.avail_bytes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "device",
                               disk.device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_disk_total(void * /* measurement_context */,
                                  measurement_delivery_callback_t delivery,
                                  void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::Disk_Stats_Buffer stats =
      Health_Monitor::disk_collector()->buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  Disk_Info disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.timestamp;
    else if (timestamp != disk.timestamp)
      break;
    const auto measurement = disk.total_bytes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "device",
                               disk.device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_cpu_system_time(void * /* measurement_context */,
                                       measurement_delivery_callback_t delivery,
                                       void *delivery_context) {
  constexpr size_t cpu_attr_count = 1;
  static const char *cpu_attr_names[cpu_attr_count] = {"state"};

  // return measurements for all available CPU cores
  assert(delivery != nullptr);
  const hm::System_CPU_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_cpu_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_CPU_Stats cpu;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(cpu, i);
    if (timestamp == 0)
      timestamp = cpu.m_timestamp;
    else if (timestamp != cpu.m_timestamp)
      break;
    // per-core values are handled in a separate metric
    if (cpu.m_cpu != "cpu") continue;

    const char *values[cpu_attr_count];

    values[0] = "user";
    auto value = Clamp<int64_t>(cpu.m_user_ms);
    delivery->value_int64_attrs(delivery_context, value, cpu_attr_names, values,
                                cpu_attr_count);

    values[0] = "nice";
    value = Clamp<int64_t>(cpu.m_nice_ms);
    delivery->value_int64_attrs(delivery_context, value, cpu_attr_names, values,
                                cpu_attr_count);

    values[0] = "system";
    value = Clamp<int64_t>(cpu.m_system_ms);
    delivery->value_int64_attrs(delivery_context, value, cpu_attr_names, values,
                                cpu_attr_count);

    values[0] = "idle";
    value = Clamp<int64_t>(cpu.m_idle_ms);
    delivery->value_int64_attrs(delivery_context, value, cpu_attr_names, values,
                                cpu_attr_count);

    if (cpu.m_extended_times_available) {
      values[0] = "iowait";
      value = Clamp<int64_t>(cpu.m_iowait_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[0] = "irq";
      value = Clamp<int64_t>(cpu.m_irq_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[0] = "softirq";
      value = Clamp<int64_t>(cpu.m_softirq_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[0] = "steal";
      value = Clamp<int64_t>(cpu.m_steal_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[0] = "guest";
      value = Clamp<int64_t>(cpu.m_guest_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[0] = "guest_nice";
      value = Clamp<int64_t>(cpu.m_guest_nice_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);
    }
  }
  stats->unlock();
}

static void get_metric_cpu_system_time_per_core(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  constexpr size_t cpu_attr_count = 2;
  static const char *cpu_attr_names[cpu_attr_count] = {"cpu", "state"};

  // return measurements for all available CPU cores
  assert(delivery != nullptr);
  const hm::System_CPU_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_cpu_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_CPU_Stats cpu;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(cpu, i);
    if (timestamp == 0)
      timestamp = cpu.m_timestamp;
    else if (timestamp != cpu.m_timestamp)
      break;
    // total (for all cores) is handled in a separate metric
    if (cpu.m_cpu == "cpu") continue;

    const char *values[cpu_attr_count];
    values[0] = cpu.m_cpu.c_str();

    values[1] = "user";
    auto value = Clamp<int64_t>(cpu.m_user_ms);
    delivery->value_int64_attrs(delivery_context, value, cpu_attr_names, values,
                                cpu_attr_count);

    values[1] = "nice";
    value = Clamp<int64_t>(cpu.m_nice_ms);
    delivery->value_int64_attrs(delivery_context, value, cpu_attr_names, values,
                                cpu_attr_count);

    values[1] = "system";
    value = Clamp<int64_t>(cpu.m_system_ms);
    delivery->value_int64_attrs(delivery_context, value, cpu_attr_names, values,
                                cpu_attr_count);

    values[1] = "idle";
    value = Clamp<int64_t>(cpu.m_idle_ms);
    delivery->value_int64_attrs(delivery_context, value, cpu_attr_names, values,
                                cpu_attr_count);

    if (cpu.m_extended_times_available) {
      values[1] = "iowait";
      value = Clamp<int64_t>(cpu.m_iowait_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[1] = "irq";
      value = Clamp<int64_t>(cpu.m_irq_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[1] = "softirq";
      value = Clamp<int64_t>(cpu.m_softirq_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[1] = "steal";
      value = Clamp<int64_t>(cpu.m_steal_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[1] = "guest";
      value = Clamp<int64_t>(cpu.m_guest_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);

      values[1] = "guest_nice";
      value = Clamp<int64_t>(cpu.m_guest_nice_ms);
      delivery->value_int64_attrs(delivery_context, value, cpu_attr_names,
                                  values, cpu_attr_count);
    }
  }
  stats->unlock();
}

static void get_metric_system_disk_reads(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists totals per entire disk
    if (!system_disk_read_write_metric_eligible(disk, false)) continue;
    const auto measurement = disk.m_reads;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_reads_per_partition(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists info per partition
    if (!system_disk_read_write_metric_eligible(disk, true)) continue;
    const auto measurement = disk.m_reads;
    const auto value = Clamp<int64_t>(measurement);

    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_reads_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists totals per entire disk
    if (!system_disk_read_write_metric_eligible(disk, false)) continue;
    const auto measurement = disk.m_read_bytes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_reads_bytes_per_partition(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists info per partition
    if (!system_disk_read_write_metric_eligible(disk, true)) continue;
    const auto measurement = disk.m_read_bytes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_reads_time(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists totals per entire disk
    if (!system_disk_read_write_metric_eligible(disk, false)) continue;
    const auto measurement = disk.m_read_time_ms;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_reads_time_per_partition(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists info per partition
    if (!system_disk_read_write_metric_eligible(disk, true)) continue;
    const auto measurement = disk.m_read_time_ms;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_writes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists totals per entire disk
    if (!system_disk_read_write_metric_eligible(disk, false)) continue;
    const auto measurement = disk.m_writes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_writes_per_partition(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists info per partition
    if (!system_disk_read_write_metric_eligible(disk, true)) continue;
    const auto measurement = disk.m_writes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_writes_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists totals per entire disk
    if (!system_disk_read_write_metric_eligible(disk, false)) continue;
    const auto measurement = disk.m_write_bytes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_writes_bytes_per_partition(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists info per partition
    if (!system_disk_read_write_metric_eligible(disk, true)) continue;
    const auto measurement = disk.m_write_bytes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_writes_time(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists totals per entire disk
    if (!system_disk_read_write_metric_eligible(disk, false)) continue;
    const auto measurement = disk.m_write_time_ms;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_writes_time_per_partition(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists info per partition
    if (!system_disk_read_write_metric_eligible(disk, true)) continue;
    const auto measurement = disk.m_write_time_ms;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_flushes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists totals per entire disk
    if (!system_disk_flush_metric_eligible(disk, false)) continue;
    const auto measurement = disk.m_flushes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_flushes_per_partition(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists info per partition
    if (!system_disk_flush_metric_eligible(disk, true)) continue;
    const auto measurement = disk.m_flushes;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_flushes_time(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists totals per entire disk
    if (!system_disk_flush_metric_eligible(disk, false)) continue;
    const auto measurement = disk.m_flush_time_ms;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_disk_flushes_time_per_partition(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return measurements for all monitored disks
  assert(delivery != nullptr);
  const hm::System_Disk_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_disk_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Disk_Stats disk;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(disk, i);
    if (timestamp == 0)
      timestamp = disk.m_timestamp;
    else if (timestamp != disk.m_timestamp)
      break;
    // this metric only lists info per partition
    if (!system_disk_flush_metric_eligible(disk, true)) continue;
    const auto measurement = disk.m_flush_time_ms;
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attr(delivery_context, value, "disk",
                               disk.m_device_name.c_str());
  }
  stats->unlock();
}

static void get_metric_system_proc_time(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  constexpr size_t proc_time_attr_count = 5;
  static const char *proc_time_attr_names[proc_time_attr_count] = {
      "pid", "tid", "process_name", "thread_name", "state"};

  // return measurements for all mysqld threads
  assert(delivery != nullptr);
  const hm::System_Process_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_process_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Process_Stats proc;
  // take last measurement set, boundary deduced by timestamp change
  time_t timestamp = 0;
  if (stats->lock_read() != 0) return;
  for (int i = count - 1; i >= 0; --i) {
    stats->read_no_lock(proc, i);
    if (timestamp == 0)
      timestamp = proc.m_timestamp;
    else if (timestamp != proc.m_timestamp)
      break;

    const std::string pid = std::to_string(proc.m_pid);
    const std::string tid = std::to_string(proc.m_tid);

    const char *values[proc_time_attr_count];
    values[0] = pid.c_str();
    values[1] = tid.c_str();
    values[2] = proc.m_process_name.c_str();
    values[3] = proc.m_thread_name.c_str();

    const auto deliver_time = [&](const uint64_t measurement) {
      const auto value = Clamp<int64_t>(measurement);
      delivery->value_int64_attrs(delivery_context, value, proc_time_attr_names,
                                  values, proc_time_attr_count);
    };

    values[4] = "utime";
    deliver_process_thread_metric(proc, Process_thread_metric::user_time,
                                  deliver_time);
    values[4] = "stime";
    deliver_process_thread_metric(proc, Process_thread_metric::system_time,
                                  deliver_time);
    values[4] = "cutime";
    deliver_process_thread_metric(proc, Process_thread_metric::child_user_time,
                                  deliver_time);
    values[4] = "cstime";
    deliver_process_thread_metric(
        proc, Process_thread_metric::child_system_time, deliver_time);
  }
  stats->unlock();
}

constexpr size_t proc_attr_count = 4;

static const char *proc_attr_names[proc_attr_count] = {
    "pid", "tid", "process_name", "thread_name"};

static bool read_latest_process_metric(const Process_thread_metric metric,
                                       System_Process_Stats &record) {
  const System_Process_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_process_buffer();
  if (!stats) return false;
  if (stats->lock_read() != 0) return false;

  const int count = stats->count();
  const bool found =
      count != 0 && select_latest_process_thread_metric(
                        count, metric,
                        [&](const int index, System_Process_Stats &candidate) {
                          return stats->read_no_lock(candidate, index) == 0;
                        },
                        record);
  stats->unlock();
  return found;
}

static void deliver_single_process_metric(
    const Process_thread_metric metric,
    measurement_delivery_callback_t delivery, void *delivery_context) {
  System_Process_Stats proc;
  if (!read_latest_process_metric(metric, proc)) return;

  const std::string pid = std::to_string(proc.m_pid);
  const std::string tid = std::to_string(proc.m_tid);
  const char *values[proc_attr_count] = {pid.c_str(), tid.c_str(),
                                         proc.m_process_name.c_str(),
                                         proc.m_thread_name.c_str()};
  deliver_process_thread_metric(proc, metric, [&](const uint64_t measurement) {
    const auto value = Clamp<int64_t>(measurement);
    delivery->value_int64_attrs(delivery_context, value, proc_attr_names,
                                values, proc_attr_count);
  });
}

static void get_metric_system_proc_num_threads(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::thread_count, delivery,
                                delivery_context);
}

static void get_metric_system_proc_vsize(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::virtual_bytes, delivery,
                                delivery_context);
}

static void get_metric_system_proc_rss(void * /* measurement_context */,
                                       measurement_delivery_callback_t delivery,
                                       void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::resident_bytes, delivery,
                                delivery_context);
}

static void get_metric_system_proc_rsslim(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::resident_limit_bytes,
                                delivery, delivery_context);
}

static void get_metric_system_proc_processor(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::processor, delivery,
                                delivery_context);
}

static void get_metric_system_proc_delayacct_blkio(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::block_io_delay, delivery,
                                delivery_context);
}

static void get_metric_system_proc_guest_time(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::guest_time, delivery,
                                delivery_context);
}

static void get_metric_system_proc_cguest_time(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::child_guest_time,
                                delivery, delivery_context);
}

static void get_metric_system_proc_read_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::read_bytes, delivery,
                                delivery_context);
}

static void get_metric_system_proc_write_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  deliver_single_process_metric(Process_thread_metric::write_bytes, delivery,
                                delivery_context);
}

static void get_metric_system_memory_total_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  const auto measurement = mem.m_total_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_free_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  const auto measurement = mem.m_free_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_avail_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  const auto measurement = mem.m_avail_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_used_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  const auto measurement = mem.m_used_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_buffers_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  if (!mem.m_breakdown_available) return;
  const auto measurement = mem.m_buffers_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_cached_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  if (!mem.m_breakdown_available) return;
  const auto measurement = mem.m_cached_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_slab_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  if (!mem.m_breakdown_available) return;
  const auto measurement = mem.m_slab_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_swap_total_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  if (!mem.m_swap_capacity_available) return;
  const auto measurement = mem.m_swap_total_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_swap_free_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  if (!mem.m_swap_capacity_available) return;
  const auto measurement = mem.m_swap_free_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_swap_used_bytes(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  if (!mem.m_swap_capacity_available) return;
  const auto measurement = mem.m_swap_used_bytes;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_swap_in(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  if (!mem.m_swap_activity_available) return;
  const auto measurement = mem.m_swap_in;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_system_memory_swap_out(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const hm::System_Memory_Stats_Buffer stats =
      Health_Monitor::system_collector()->sys_memory_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::System_Memory_Stats mem;
  stats->read(mem, count - 1);
  if (!mem.m_swap_activity_available) return;
  const auto measurement = mem.m_swap_out;
  const auto value = Clamp<int64_t>(measurement);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_super_read_only_disk_full(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // based on 'super_read_only_disk_full' status variable
  assert(delivery != nullptr);
  const int64_t value = static_cast<int64_t>(
      Health_Monitor::disk_collector()->m_status_super_read_only_disk_full);
  delivery->value_int64(delivery_context, value);
}

static void get_metric_memory_process_page_faults(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const std::shared_ptr<Stat_Buffer<Process_Memory_Info>> stats =
      Health_Monitor::memory_collector()->proc_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::Process_Memory_Info mem;
  stats->read(mem, count - 1);
  deliver_process_memory_metric(
      mem, Process_memory_metric::page_faults,
      [delivery, delivery_context](const uint64_t measurement,
                                   const uint64_t pid) {
        const auto value = Clamp<int64_t>(measurement);
        delivery->value_int64_attr(delivery_context, value, "pid",
                                   std::to_string(pid).c_str());
      });
}

static void get_metric_memory_process_vmrss(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const std::shared_ptr<Stat_Buffer<Process_Memory_Info>> stats =
      Health_Monitor::memory_collector()->proc_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::Process_Memory_Info mem;
  stats->read(mem, count - 1);
  deliver_process_memory_metric(
      mem, Process_memory_metric::resident_bytes,
      [delivery, delivery_context](const uint64_t measurement,
                                   const uint64_t pid) {
        const auto value = Clamp<int64_t>(measurement);
        delivery->value_int64_attr(delivery_context, value, "pid",
                                   std::to_string(pid).c_str());
      });
}

static void get_metric_memory_process_vmdata(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const std::shared_ptr<Stat_Buffer<Process_Memory_Info>> stats =
      Health_Monitor::memory_collector()->proc_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::Process_Memory_Info mem;
  stats->read(mem, count - 1);
  deliver_process_memory_metric(
      mem, Process_memory_metric::data_bytes,
      [delivery, delivery_context](const uint64_t measurement,
                                   const uint64_t pid) {
        const auto value = Clamp<int64_t>(measurement);
        delivery->value_int64_attr(delivery_context, value, "pid",
                                   std::to_string(pid).c_str());
      });
}

static void get_metric_memory_process_vmswap(
    void * /* measurement_context */, measurement_delivery_callback_t delivery,
    void *delivery_context) {
  // return single (latest) measurement
  assert(delivery != nullptr);
  const std::shared_ptr<Stat_Buffer<Process_Memory_Info>> stats =
      Health_Monitor::memory_collector()->proc_buffer();
  if (!stats) return;
  const int count = stats->count();
  if (count == 0) return;
  hm::Process_Memory_Info mem;
  stats->read(mem, count - 1);
  deliver_process_memory_metric(
      mem, Process_memory_metric::swap_bytes,
      [delivery, delivery_context](const uint64_t measurement,
                                   const uint64_t pid) {
        const auto value = Clamp<int64_t>(measurement);
        delivery->value_int64_attr(delivery_context, value, "pid",
                                   std::to_string(pid).c_str());
      });
}

static PSI_metric_info_v1 disk_metrics[] = {
    {"available", METRIC_UNIT_BYTES, "Available monitored disk space in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_disk_available, nullptr},
    {"total", METRIC_UNIT_BYTES, "Total monitored disk space in bytes.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_disk_total, nullptr},
    {"used.percent", "%", "Used disk space percentage.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_DOUBLE, 0, 0,
     get_metric_disk_used_percent, nullptr},
    {"system.flushes", "", "Number of system disk flushes (per disk).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_flushes, nullptr},
    {"system.flushes_per_partition", "",
     "Number of system disk flushes (per partition).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_flushes_per_partition, nullptr},
    {"system.flushes.time", METRIC_UNIT_MILLISECONDS,
     "Total read time for system disk (per disk).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_flushes_time, nullptr},
    {"system.flushes.time_per_partition", METRIC_UNIT_MILLISECONDS,
     "Total read time for system disk (per partition).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_flushes_time_per_partition, nullptr},
    {"system.reads", "", "Number of system disk reads (per disk).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_reads, nullptr},
    {"system.reads_per_partition", "",
     "Number of system disk reads (per partition).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_reads_per_partition, nullptr},
    {"system.reads.bytes", METRIC_UNIT_BYTES,
     "Total number of bytes read from system disk (per disk).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_reads_bytes, nullptr},
    {"system.reads.bytes_per_partition", METRIC_UNIT_BYTES,
     "Total number of bytes read from system disk (per partition).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_reads_bytes_per_partition, nullptr},
    {"system.reads.time", METRIC_UNIT_MILLISECONDS,
     "Total read time for system disk (per disk).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_reads_time, nullptr},
    {"system.reads.time_per_partition", METRIC_UNIT_MILLISECONDS,
     "Total read time for system disk (per partition).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_reads_time_per_partition, nullptr},
    {"system.writes", "", "Number of system disk writes (per disk).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_writes, nullptr},
    {"system.writes_per_partition", "",
     "Number of system disk writes (per partition).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_writes_per_partition, nullptr},
    {"system.writes.bytes", METRIC_UNIT_BYTES,
     "Total number of bytes written to system disk (per disk).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_writes_bytes, nullptr},
    {"system.writes.bytes_per_partition", METRIC_UNIT_BYTES,
     "Total number of bytes written to system disk (per partition).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_writes_bytes_per_partition, nullptr},
    {"system.writes.time", METRIC_UNIT_MILLISECONDS,
     "Total write time for system disk (per disk).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_writes_time, nullptr},
    {"system.writes.time_per_partition", METRIC_UNIT_MILLISECONDS,
     "Total write time for system disk (per partition).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_disk_writes_time_per_partition, nullptr},
    {"super_read_only_disk_full", "",
     "Is server in offline mode due to disk being full "
     "(super_read_only_disk_full).",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_super_read_only_disk_full, nullptr}};

static PSI_metric_info_v1 memory_metrics[] = {
    {"process.vsize", METRIC_UNIT_BYTES, "Virtual memory size in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_vsize, nullptr},
    {"process.rss", METRIC_UNIT_BYTES,
     "Number of bytes the process has in real memory.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_rss, nullptr},
    {"process.rsslim", METRIC_UNIT_BYTES,
     "Current soft limit in bytes on the rss of the process.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_rsslim, nullptr},
    {"process.page_faults", "", "Count of page faults in the current process.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_memory_process_page_faults, nullptr},
    {"process.vmdata", METRIC_UNIT_BYTES,
     "Address space size (with stack and shared pages excluded) of the process "
     "in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_memory_process_vmdata, nullptr},
    {"process.vmrss", METRIC_UNIT_BYTES,
     "Current usage of 'physical RAM' of the process in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_memory_process_vmrss, nullptr},
    {"process.vmswap", METRIC_UNIT_BYTES,
     "Size of swap usage (without shmem) of the process in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_memory_process_vmswap, nullptr},
    {"used.percent", "%", "Used memory percentage.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_DOUBLE, 0, 0,
     get_metric_memory_used_percent, nullptr},
    {"system.total", METRIC_UNIT_BYTES, "Total size of system memory in bytes.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_total_bytes, nullptr},
    {"system.free", METRIC_UNIT_BYTES, "Size of free system memory in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_free_bytes, nullptr},
    {"system.available", METRIC_UNIT_BYTES,
     "Size of available system memory in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_avail_bytes, nullptr},
    {"system.used", METRIC_UNIT_BYTES, "Size of used system memory in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_used_bytes, nullptr},
    {"system.buffers", METRIC_UNIT_BYTES,
     "Size of system memory buffers in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_buffers_bytes, nullptr},
    {"system.cached", METRIC_UNIT_BYTES, "Size of page cache in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_cached_bytes, nullptr},
    {"system.slab", METRIC_UNIT_BYTES,
     "Size of in-kernel data structures cache in bytes.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_slab_bytes, nullptr},
    {"system.swap.total", METRIC_UNIT_BYTES,
     "Total size of swap memory in bytes.", MetricOTELType::ASYNC_GAUGE_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_swap_total_bytes, nullptr},
    {"system.swap.free", METRIC_UNIT_BYTES,
     "Size of free swap memory in bytes.", MetricOTELType::ASYNC_GAUGE_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_swap_free_bytes, nullptr},
    {"system.swap.used", METRIC_UNIT_BYTES,
     "Size of used swap memory in bytes.", MetricOTELType::ASYNC_GAUGE_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_swap_used_bytes, nullptr},
    {"system.swap-in", "", "Number of swap-in events in system.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_swap_in, nullptr},
    {"system.swap-out", "", "Number of swap-out events in system.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_memory_swap_out, nullptr}};

static PSI_metric_info_v1 cpu_metrics[] = {
    {"system.time", METRIC_UNIT_MILLISECONDS, "System CPU times per all cores.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_cpu_system_time, nullptr},
    {"system.time_per_core", METRIC_UNIT_MILLISECONDS,
     "System CPU times per logical core.", MetricOTELType::ASYNC_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_cpu_system_time_per_core,
     nullptr},
    {"process.time", METRIC_UNIT_MILLISECONDS,
     "Amount of time process threads or waited-for children have been "
     "scheduled in user or kernel mode.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_time, nullptr},
    {"process.threads", "", "Number of threads existing in this process.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_num_threads, nullptr},
    {"process.processor", "", "CPU number last executed on.",
     MetricOTELType::ASYNC_GAUGE_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_processor, nullptr},
    {"process.delayacct_blkio", METRIC_UNIT_MILLISECONDS,
     "Aggregated block I/O delays.", MetricOTELType::ASYNC_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_delayacct_blkio, nullptr},
    {"process.guest_time", METRIC_UNIT_MILLISECONDS,
     "Guest time of the process (time spent running a virtual CPU for a guest "
     "operating system).",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_guest_time, nullptr},
    {"process.cguest_time", METRIC_UNIT_MILLISECONDS,
     "Guest time of the process's children.", MetricOTELType::ASYNC_COUNTER,
     MetricNumType::METRIC_INTEGER, 0, 0, get_metric_system_proc_cguest_time,
     nullptr},
    {"process.read_bytes", METRIC_UNIT_BYTES, "Read I/O per thread.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_read_bytes, nullptr},
    {"process.write_bytes", METRIC_UNIT_BYTES, "Write I/O per thread.",
     MetricOTELType::ASYNC_COUNTER, MetricNumType::METRIC_INTEGER, 0, 0,
     get_metric_system_proc_write_bytes, nullptr}};

PSI_meter_info_v1 health_meter[] = {
    {"mysql.health.disk", "Health monitor disk metrics", 30, 0, 0, disk_metrics,
     std::size(disk_metrics)},
    {"mysql.health.memory", "Health monitor memory metrics", 30, 0, 0,
     memory_metrics, std::size(memory_metrics)},
    {"mysql.health.cpu", "Health monitor CPU metrics", 30, 0, 0, cpu_metrics,
     std::size(cpu_metrics)}};

size_t get_health_meter_count() { return std::size(health_meter); }

}  // namespace hm
