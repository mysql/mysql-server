/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "dest_first_available.h"

#include <cstddef>   // size_t
#include <iterator>  // advance
#include <memory>    // make_unique
#include <mutex>     // lock_guard
#include <string>
#include <system_error>  // error_code

#include "mysqlrouter/destination.h"

class FirstAvailableDestination : public Destination {
 public:
  FirstAvailableDestination(std::string id, std::string hostname, uint16_t port,
                            DestFirstAvailable *balancer, size_t ndx)
      : Destination(std::move(id), std::move(hostname), port),
        balancer_{balancer},
        ndx_{ndx} {}

  void connect_status(std::error_code ec) override {
    if (ec != std::error_code{}) {
      // mark the current ndx as invalid
      balancer_->mark_ndx_invalid(ndx_);
    }
  }

 private:
  DestFirstAvailable *balancer_;

  size_t ndx_;
};

Destinations DestFirstAvailable::destinations() {
  Destinations dests;

  {
    std::lock_guard<std::mutex> lk(mutex_update_);

    const auto end = destinations_.end();
    const auto begin = destinations_.begin();

    if (valid_ndx_ >= destinations_.size()) {
      valid_ndx_ = 0;
    }

    auto cur = begin;

    std::advance(cur, valid_ndx_);

    // capture last for the 2nd round.
    const auto last = cur;

    for (size_t ndx{valid_ndx_}; cur != end; ++cur, ++ndx) {
      dests.push_back(std::make_unique<FirstAvailableDestination>(
          cur->str(), cur->address(), cur->port(), this, ndx));
    }

    cur = begin;
    for (size_t ndx{0}; cur != last; ++cur, ++ndx) {
      dests.push_back(std::make_unique<FirstAvailableDestination>(
          cur->str(), cur->address(), cur->port(), this, ndx));
    }
  }

  return dests;
}
