/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mrs/monitored/gtid_functions.h"

#include <string>
#include <vector>

#include "mrs/router_observation_entities.h"

#include "helper/json/rapid_json_to_text.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace monitored {

std::string get_most_relevant_gtid(const std::vector<std::string> &gtids) {
  for (auto &g : gtids) {
    log_debug("Received gtid: %s", g.c_str());
  }
  if (gtids.size() > 0) return gtids[0];
  return {};
}

std::string get_session_tracked_gtids_for_metadata_response(
    collector::CountedMySQLSession *session, mrs::GtidManager *gtid_manager) {
  auto gtids = session->get_session_tracker_data(SESSION_TRACK_GTIDS);
  if (gtids.size()) {
    Counter<kEntityCounterRestMetadataGtids>::increment();
    auto addr = session->get_connection_parameters().conn_opts.destination;
    for (auto &gtid : gtids) {
      gtid_manager->remember(addr, {gtid});
    }

    return get_most_relevant_gtid(gtids);
  }

  return {};
}

}  // namespace monitored
}  // namespace mrs
