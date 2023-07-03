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

#ifndef ROUTING_SHARED_QUARANTINE_HANDLER_INCLUDED
#define ROUTING_SHARED_QUARANTINE_HANDLER_INCLUDED

#include <functional>
#include <string>
#include <utility>  // move

#include "destination.h"  // AllowedNodes
#include "tcp_address.h"

class SharedQuarantineHandler {
 public:
  using update_callback_type =
      std::function<bool(mysql_harness::TCPAddress, bool)>;
  using is_quarantined_callback_type =
      std::function<bool(mysql_harness::TCPAddress)>;
  using stop_callback_type = std::function<void()>;

  using refresh_callback_type = std::function<void(
      const std::string &, const bool, const AllowedNodes &)>;

  /**
   * Register a callback that can to be used to add a destination candidate
   * to the quarantine.
   *
   * @param[in] clb Callback called to quarantine a destination.
   */
  void on_update(update_callback_type clb) { on_update_ = std::move(clb); }

  bool update(const mysql_harness::TCPAddress &addr, bool success) {
    if (on_update_) return on_update_(addr, success);
    return false;
  }

  /**
   * Register a callback that can be used to check if the given destination
   * candidate is currently quarantined.
   *
   * @param[in] clb Callback called to check if the destination is quarantined.
   */
  void on_is_quarantined(is_quarantined_callback_type clb) {
    on_is_quarantined_ = std::move(clb);
  }

  bool is_quarantined(const mysql_harness::TCPAddress &addr) const {
    return on_is_quarantined_ ? on_is_quarantined_(addr) : false;
  }

  /**
   * Register a callback that can be used to stop the unreachable destination
   * candidates quarantine.
   *
   * @param[in] clb Callback called to remove all destinations from quarantine.
   */
  void on_stop(stop_callback_type clb) { on_stop_ = std::move(clb); }

  void stop() {
    if (on_stop_) on_stop_();
  }

  /**
   * Register a callback used for refreshing the quarantined destinations when
   * there are possible changes in the destination candidates set.
   *
   * @param[in] clb Callback called on metadata refresh.
   */
  void on_refresh(refresh_callback_type clb) { on_refresh_ = std::move(clb); }

  void refresh(const std::string &instance_name, bool some_bool,
               const AllowedNodes &allowed_nodes) {
    if (on_refresh_) on_refresh_(instance_name, some_bool, allowed_nodes);
  }

  /**
   * Unregister all of the destination candidates quarantine callbacks.
   */
  void reset() {
    on_update_ = nullptr;
    on_is_quarantined_ = nullptr;
    on_refresh_ = nullptr;
    on_stop_ = nullptr;
  }

 private:
  update_callback_type on_update_;

  is_quarantined_callback_type on_is_quarantined_;
  stop_callback_type on_stop_;
  refresh_callback_type on_refresh_;
};

#endif
