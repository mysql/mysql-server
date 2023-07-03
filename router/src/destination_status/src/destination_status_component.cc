/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "mysqlrouter/destination_status_component.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include "unreachable_destinations_quarantine.h"

DestinationStatusComponent::~DestinationStatusComponent() = default;

void DestinationStatusComponent::init(std::chrono::seconds quarantine_interval,
                                      uint32_t qurantine_threshold) {
  unreachable_destinations_quarantine_->init(quarantine_interval,
                                             qurantine_threshold);
}

bool DestinationStatusComponent::report_connection_result(
    const mysql_harness::TCPAddress &dest, bool success) {
  return unreachable_destinations_quarantine_->report_connection_result(
      dest, success);
}

bool DestinationStatusComponent::is_destination_quarantined(
    const mysql_harness::TCPAddress &dest) {
  return unreachable_destinations_quarantine_->is_quarantined(dest);
}

void DestinationStatusComponent::stop_unreachable_destinations_quarantine() {
  unreachable_destinations_quarantine_->stop_quarantine();
}

void DestinationStatusComponent::refresh_destinations_quarantine(
    const std::string &instance_name, const bool nodes_changed_on_md_refresh,
    const AllowedNodes &new_destinations) {
  unreachable_destinations_quarantine_->refresh_quarantine(
      instance_name, nodes_changed_on_md_refresh, new_destinations);
}

void DestinationStatusComponent::register_route(const std::string &name) {
  unreachable_destinations_quarantine_->register_route(name);
}

void DestinationStatusComponent::register_quarantine_callbacks(
    QuarantineRoutingCallbacks &&routing_callbacks) {
  unreachable_destinations_quarantine_->register_routing_callbacks(
      std::move(routing_callbacks));
}

void DestinationStatusComponent::unregister_quarantine_callbacks() {
  unreachable_destinations_quarantine_->unregister_routing_callbacks();
}

DestinationStatusComponent::DestinationStatusComponent()
    : unreachable_destinations_quarantine_(
          std::make_unique<UnreachableDestinationsQuarantine>()) {}

DestinationStatusComponent &DestinationStatusComponent::get_instance() {
  static DestinationStatusComponent instance;

  return instance;
}