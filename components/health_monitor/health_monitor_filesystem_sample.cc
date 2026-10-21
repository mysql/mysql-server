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

#include "health_monitor_filesystem_sample.h"

namespace hm {
namespace {

bool is_available(const mysql::system_info::Result_state state) {
  return state == mysql::system_info::Result_state::available;
}

}  // namespace

bool convert_filesystem_snapshot(
    const mysql::system_info::Filesystem_snapshot &snapshot,
    Disk_Info &output) {
  if (!is_available(snapshot.identity.state()) ||
      !is_available(snapshot.usage.state())) {
    return false;
  }

  const auto &identity = snapshot.identity.value();
  const auto &usage = snapshot.usage.value();
  if (identity.filesystem_id.empty() || identity.mount_point.empty() ||
      identity.mount_source.empty() || identity.filesystem_type.empty() ||
      usage.capacity_bytes == 0 ||
      usage.available_bytes > usage.capacity_bytes) {
    return false;
  }

  Disk_Info converted;
  converted.device_name = identity.mount_source;
  converted.total_bytes = usage.capacity_bytes;
  converted.avail_bytes = usage.available_bytes;
  converted.mount_point = identity.mount_point;
  converted.mount_source = identity.mount_source;
  converted.filesystem_id = identity.filesystem_id;
  converted.filesystem_type = identity.filesystem_type;
  output = std::move(converted);
  return true;
}

Filesystem_change classify_filesystem_change(
    const Filesystem_sample_state &state, const Disk_Info &sample) {
  if (!state.initialized) return Filesystem_change::initial;
  if (state.filesystem_id != sample.filesystem_id) {
    return Filesystem_change::identity_changed;
  }
  if (state.capacity_bytes != sample.total_bytes) {
    return Filesystem_change::capacity_changed;
  }
  return Filesystem_change::unchanged;
}

bool accept_filesystem_snapshot(
    const mysql::system_info::Filesystem_snapshot &snapshot,
    Filesystem_sample_state &state, Disk_Info &output,
    Filesystem_change &change) {
  Disk_Info converted;
  if (!convert_filesystem_snapshot(snapshot, converted)) return false;

  const Filesystem_change converted_change =
      classify_filesystem_change(state, converted);
  Filesystem_sample_state converted_state;
  converted_state.filesystem_id = converted.filesystem_id;
  converted_state.capacity_bytes = converted.total_bytes;
  converted_state.initialized = true;

  output = std::move(converted);
  state = std::move(converted_state);
  change = converted_change;
  return true;
}

void publish_filesystem_records(Disk_Info &sample, time_t timestamp,
                                bool write_stat, bool write_metric,
                                Filesystem_change change,
                                const Disk_series_reset_action &reset_series,
                                const Disk_record_writer &stat_writer,
                                const Disk_record_writer &metric_writer) {
  sample.timestamp = timestamp;
  // Reset the time series if the filesystem identity changes, e.g. a different
  // monitored disk. Do not reset the time series if the volume size changes.
  if (change == Filesystem_change::identity_changed && reset_series) {
    reset_series();
  }
  // Write to health_block_device table buffer.
  if (write_stat && stat_writer) stat_writer(sample);
  // Write to the disk_metric table buffer.
  if (write_metric && metric_writer) metric_writer(sample);
}

}  // namespace hm
