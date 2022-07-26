/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQL_HARNESS_EVENT_STATE_TRACKER_INCLUDED
#define MYSQL_HARNESS_EVENT_STATE_TRACKER_INCLUDED

#include "harness_export.h"

#include <map>
#include <mutex>
#include <string>

namespace mysql_harness {

/**
 * @brief EventStateTracker singleton object keeps track of the current known
 * state of selected event. Can be used to track the changes of the state of
 * selected event (for conditional logging etc.)
 *
 */
class HARNESS_EXPORT EventStateTracker {
 public:
  /**
   * @brief List of the events that can currently be tracked
   *
   */
  enum class EventId : size_t {
    MetadataServerConnectedOk,
    MetadataRefreshOk,
    GRMemberConnectedOk,
    MetadataNodeInGR,
    GRNodeInMetadata,
    TargetClusterPresentInOptions,
    ClusterInvalidatedInMetadata,
    ClusterWasBootstrappedAgainstClusterset,
    NoRightsToUpdateRouterAttributes
  };

  /**
   * @brief Returns information about the selected event state change (and sets
   * the new state of event if changed).
   *
   * @param state current state of the event
   * @param event_id id of the event
   * @param additional_tag optional tag
   *
   * @return information about the event state change
   * @retval true the event state has changed since the last call
   * @retval false the event state has NOT changed since the last call
   */
  bool state_changed(const int state, const EventId event_id,
                     const std::string &additional_tag = "");

  /**
   * @brief Get the singleton object of EventStateTracker
   *
   */
  static EventStateTracker &instance();

  /**
   * @brief Remove the state for a given tag for all event_id that it has
   * registered
   *
   * @param tag tag of the events being removed
   *
   */
  void remove_tag(const std::string &tag);

  /**
   * @brief Remove stored state for all events
   *
   */
  void clear();

 private:
  EventStateTracker() = default;
  EventStateTracker(const EventStateTracker &) = delete;
  EventStateTracker operator=(const EventStateTracker &) = delete;
  using Key = std::pair<size_t, size_t>;
  std::map<Key, int> events_;
  std::mutex events_mtx_;
};

}  // namespace mysql_harness

#endif /* MYSQL_HARNESS_EVENT_STATE_TRACKER_INCLUDED */
