/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef NGS_CHRONO_H_
#define NGS_CHRONO_H_

#include <boost/date_time/posix_time/posix_time.hpp>

namespace ngs {
namespace chrono {

using boost::posix_time::milliseconds;
using boost::posix_time::seconds;
typedef boost::posix_time::ptime time_point;
typedef boost::posix_time::time_duration duration;

inline time_point now() {
  return boost::posix_time::microsec_clock::universal_time();
}

inline duration::tick_type to_milliseconds(const duration &d) {
  return d.total_milliseconds();
}

inline duration::sec_type to_seconds(const duration &d) {
  return d.total_seconds();
}

inline bool is_valid(const time_point &p) { return !p.is_not_a_date_time(); }

}  // namespace chrono
}  // namespcae ngs

#endif  // NGS_CHRONO_H_
