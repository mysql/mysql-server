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

#include "sql/changestreams/apply/resource/resource_map.h"
#include "mysql/scheduler/constants.h"
#include "sql/changestreams/apply/context/tune.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/resource/resource_monitor.h"

namespace mysql::csa {

bool Resource_map::init_resources(
    std::size_t instance_id, std::size_t channel_memory,
    PSI_stage_info *waiting_for_channel_memory_stage) {
  if (!tune::enabled_resource_tracking) {
    return false;
  }
  if (instance_id >= scheduler::Constants::max_instances) {
    return true;
  }
  auto &res_monitor = Resource_monitor::get(instance_id);
  res_monitor.register_resource(declared_channel_memory, channel_memory,
                                waiting_for_channel_memory_stage);
  return false;
}

}  // namespace mysql::csa
