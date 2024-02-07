/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/event_state_tracker.h"

#include <algorithm>

namespace mysql_harness {

/*static*/ EventStateTracker &EventStateTracker::instance() {
  static EventStateTracker instance_;
  return instance_;
}

bool EventStateTracker::state_changed(
    const int state, const EventId event_id,
    const std::string &additional_tag /*= ""*/) {
  const Key key{static_cast<size_t>(event_id),
                std::hash<std::string>{}(additional_tag)};

  std::unique_lock<std::mutex> lock(events_mtx_);

  auto it = events_.find(key);
  if (it == events_.end()) {
    events_.emplace(key, state);
    return true;
  } else {
    if (it->second != state) {
      it->second = state;
      return true;
    } else {
      return false;
    }
  }
}

void EventStateTracker::remove_tag(const std::string &tag) {
  std::unique_lock<std::mutex> lock(events_mtx_);

  const auto hashed_tag = std::hash<std::string>{}(tag);
  for (auto it = events_.begin(); it != events_.end();) {
    if (hashed_tag == it->first.second) {
      it = events_.erase(it);
    } else {
      ++it;
    }
  }
}

void EventStateTracker::clear() {
  std::unique_lock<std::mutex> lock(events_mtx_);
  events_.clear();
}

}  // namespace mysql_harness
