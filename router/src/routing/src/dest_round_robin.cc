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
#include "dest_round_robin.h"

#include "mysql/harness/net_ts/internet.h"
#include "mysqlrouter/destination.h"

using mysql_harness::TCPAddress;

Destinations DestRoundRobin::destinations() {
  Destinations dests;

  {
    std::lock_guard<std::mutex> lk(mutex_update_);

    const auto end = destinations_.end();
    const auto begin = destinations_.begin();
    const auto sz = destinations_.size();
    auto cur = begin;

    // move iterator forward and remember the position as 'last'
    std::advance(cur, start_pos_);
    auto last = cur;
    size_t n = start_pos_;

    // for start_pos == 2:
    //
    // 0 1 2 3 4 x
    // ^   ^     ^
    // |   |     `- end
    // |   `- last|cur
    // `- begin

    // from last to end;
    //
    // dests = [2 3 4]

    for (; cur != end; ++cur, ++n) {
      auto const &dest = *cur;

      dests.push_back(std::make_unique<Destination>(dest.str(), dest.address(),
                                                    dest.port()));
    }

    // from begin to before-last
    //
    // dests = [2 3 4] + [0 1]
    //
    for (cur = begin, n = 0; cur != last; ++cur, ++n) {
      auto const &dest = *cur;

      dests.push_back(std::make_unique<Destination>(dest.str(), dest.address(),
                                                    dest.port()));
    }

    if (++start_pos_ >= sz) start_pos_ = 0;
  }

  return dests;
}
