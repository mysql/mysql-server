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

#include "sql/changestreams/apply/context/tune.h"
#include "mysql/scheduler/logger_stream.h"

namespace mysql::csa::tune {

namespace {

const char *enabled_label(bool value) { return value ? "enabled" : "disabled"; }

}  // namespace

void csa_show_config_info(const char *channel_name) {
  MYSQL_LIB_LOG_INFO()
      << "CSA channel '" << channel_name
      << "' initialized with the following parameters: "
      << " provider: synchronous"
      << ", reader thresholds: " << tune::provider_max_read_event_bytes
      << " and " << tune::provider_max_read_payload_bytes
      << ", reader auto tune: " << enabled_label(tune::csa_provider_enable_tune)
      << ", thread pool queue size: " << tune::scheduler_tp_queue_size
      << ", clock capacity: " << tune::scheduler_clock_capacity
      << ", session cache capacity: " << tune::csa_session_default_cache_size
      << ", ongoing transactions maximum count multiplier (by worker count): "
      << tune::scheduler_max_task_number_factor << ".";
}

}  // namespace mysql::csa::tune
