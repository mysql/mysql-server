/*
  Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTING_DEST_NEXT_AVAILABLE
#define ROUTING_DEST_NEXT_AVAILABLE

#include "destination.h"          // Destinations
#include "mysqlrouter/routing.h"  // RouteDestination

class DestNextAvailable final : public RouteDestination {
 public:
  using RouteDestination::RouteDestination;

  Destinations destinations() override;

  // first valid index
  size_t valid_ndx() const noexcept { return valid_ndx_; }

  // mark index as invalid
  void mark_ndx_invalid(size_t ndx) noexcept {
    if (ndx + 1 > valid_ndx_) {
      valid_ndx_ = ndx + 1;
    }
  }

 private:
  size_t valid_ndx_{};
};

#endif  // ROUTING_DEST_NEXT_AVAILABLE
