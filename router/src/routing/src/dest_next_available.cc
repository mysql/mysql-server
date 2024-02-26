/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include "dest_next_available.h"

#include <memory>  // make_unique
#include <mutex>   // lock_guard
#include <string>
#include <system_error>  // error_code

class StateTrackingDestination : public Destination {
 public:
  StateTrackingDestination(std::string id, std::string addr, uint16_t port,
                           DestNextAvailable *balancer, size_t ndx)
      : Destination(std::move(id), std::move(addr), port),
        balancer_{balancer},
        ndx_{ndx} {}

  void connect_status(std::error_code ec) override {
    if (ec != std::error_code{}) {
      // mark the current ndx as invalid
      balancer_->mark_ndx_invalid(ndx_);
    }
  }

  bool good() const override { return ndx_ >= balancer_->valid_ndx(); }

 private:
  DestNextAvailable *balancer_;

  size_t ndx_;
};

Destinations DestNextAvailable::destinations() {
  Destinations dests;

  {
    std::lock_guard<std::mutex> lk(mutex_update_);
    const auto end = destinations_.end();
    auto cur = destinations_.begin();

    for (size_t ndx{}; cur != end; ++cur, ++ndx) {
      dests.push_back(std::make_unique<StateTrackingDestination>(
          cur->str(), cur->address(), cur->port(), this, ndx));
    }
  }

  return dests;
}
