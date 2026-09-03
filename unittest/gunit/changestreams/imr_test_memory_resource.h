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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef UNITTEST_GUNIT_CHANGESTREAMS_IMR_TEST_MEMORY_RESOURCE_H
#define UNITTEST_GUNIT_CHANGESTREAMS_IMR_TEST_MEMORY_RESOURCE_H

#include <cstddef>

#include "sql/changestreams/apply/resource/resource_map.h"      // Resource_map
#include "sql/changestreams/apply/resource/resource_monitor.h"  // Resource_monitor

namespace mysql::csa::unittests {

/// The channel instance id the IMR unit tests use.
constexpr std::size_t kTestInstance = 0;

/// Register the in-memory relay log memory resource with @p limit on the test
/// channel's monitor, as Imr_service::reconcile() does for a real channel, and
/// return the entry to hand to a Trx_envelope_queue / Imr_runtime.
/// Registering again replaces the entry and frees the previous one, so each
/// new queue starts with its whole limit available, and a queue built from an
/// earlier call must not be used after the next call.
inline Resource_entry *test_memory_resource(std::size_t limit) {
  auto &monitor = Resource_monitor::get(kTestInstance);
  monitor.register_resource(Resource_map::in_memory_relaylog_memory, limit);
  return monitor.get_resource_entry(Resource_map::in_memory_relaylog_memory);
}

/// Register the in-memory relay log spill disk resource with @p limit on the
/// test channel's monitor, as Imr_service::reconcile() does when
/// relay_log_space_limit is non-zero, and return the entry to hand to a
/// Trx_envelope_queue. Same replacement rule as test_memory_resource().
inline Resource_entry *test_spill_disk_resource(std::size_t limit) {
  auto &monitor = Resource_monitor::get(kTestInstance);
  monitor.register_resource(Resource_map::in_memory_relaylog_spill_disk, limit);
  return monitor.get_resource_entry(
      Resource_map::in_memory_relaylog_spill_disk);
}

}  // namespace mysql::csa::unittests

#endif  // UNITTEST_GUNIT_CHANGESTREAMS_IMR_TEST_MEMORY_RESOURCE_H
