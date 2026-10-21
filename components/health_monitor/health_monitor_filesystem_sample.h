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

#include <mysql/components/library_mysys/system_info.h>

#include <ctime>
#include <functional>
#include <string>

#include "health_monitor_disk_bits.h"

namespace hm {

enum class Filesystem_change {
  initial,
  unchanged,
  capacity_changed,
  identity_changed
};

struct Filesystem_sample_state {
  std::string filesystem_id;
  uint64_t capacity_bytes{0};
  bool initialized{false};
};

using Disk_series_reset_action = std::function<void()>;
using Disk_record_writer = std::function<void(const Disk_Info &)>;

/**
  Convert a filesystem snapshot into a Disk Collector record.

  The snapshot must be complete to be converted into a Disk Collector record.
  - Filesystem_identity and Filesystem_usage must have a valid System_result.
  - 'Unavailable' means there is no equivalent on this platform.
  - 'Failed' means that the collection attempt failed.

  @param[in] snapshot Filesystem snapshot to convert
  @param[in,out] output Destination record, unchanged on failure
  @return true on success, false on failure
*/
bool convert_filesystem_snapshot(
    const mysql::system_info::Filesystem_snapshot &snapshot, Disk_Info &output);

/**
  Classify a complete sample against the last successful sample.

  @param[in] state Last successful filesystem identity and capacity
  @param[in] sample Complete current filesystem sample
  @return Classification of the current sample
*/
Filesystem_change classify_filesystem_change(
    const Filesystem_sample_state &state, const Disk_Info &sample);

/**
  Accept a complete snapshot and update the last successful sample state.

  @param[in] snapshot Filesystem snapshot to convert
  @param[in,out] state Last successful filesystem identity and capacity
  @param[in,out] output Destination record, unchanged on failure
  @param[in,out] change Sample classification, unchanged on failure
  @return true on success, false on failure
*/
bool accept_filesystem_snapshot(
    const mysql::system_info::Filesystem_snapshot &snapshot,
    Filesystem_sample_state &state, Disk_Info &output,
    Filesystem_change &change);

/**
  Publish the records for one complete filesystem sample.

  A sample is one-time filesystem observation. 'Publishing' the sample means:
  - Set the timestamp.
  - Write to the disk stats buffer (health_block_device).
  - Write to the disk metrics buffer (disk_metrics).

  @param[in,out] sample Complete sample to timestamp and publish
  @param[in] timestamp Timestamp used for every published record
  @param[in] write_stat Whether to publish the Performance Schema record
  @param[in] write_metric Whether to publish the disk metric
  @param[in] change Current sample classification
  @param[in] reset_series If true, then reset the time series
  @param[in] stat_writer health_block_device record writer
  @param[in] metric_writer disk_metric record writer
*/
void publish_filesystem_records(Disk_Info &sample, time_t timestamp,
                                bool write_stat, bool write_metric,
                                Filesystem_change change,
                                const Disk_series_reset_action &reset_series,
                                const Disk_record_writer &stat_writer,
                                const Disk_record_writer &metric_writer);

}  // namespace hm
