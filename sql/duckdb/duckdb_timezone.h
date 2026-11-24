/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#pragma once

#include <map>
#include <sstream>
#include <string>
#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "sql/tztime.h"

namespace myduck {
constexpr long days_at_timestart = 719528;

class TimeZoneOffsetHelper {
 public:
  struct TimeZoneInfo {
    int offset;
    std::string name;
  };

  static void init_timezone() {
    add_timezone(50400, "Etc/GMT-14");
    add_timezone(46800, "Etc/GMT-13");
    add_timezone(43200, "Etc/GMT-12");
    add_timezone(39600, "Etc/GMT-11");
    add_timezone(36000, "Etc/GMT-10");
    add_timezone(32400, "Etc/GMT-9");
    add_timezone(28800, "Etc/GMT-8");
    add_timezone(25200, "Etc/GMT-7");
    add_timezone(21600, "Etc/GMT-6");
    add_timezone(18000, "Etc/GMT-5");
    add_timezone(14400, "Etc/GMT-4");
    add_timezone(10800, "Etc/GMT-3");
    add_timezone(7200, "Etc/GMT-2");
    add_timezone(3600, "Etc/GMT-1");
    add_timezone(0, "Etc/GMT");
    add_timezone(-3600, "Etc/GMT+1");
    add_timezone(-7200, "Etc/GMT+2");
    add_timezone(-10800, "Etc/GMT+3");
    add_timezone(-14400, "Etc/GMT+4");
    add_timezone(-18000, "Etc/GMT+5");
    add_timezone(-21600, "Etc/GMT+6");
    add_timezone(-25200, "Etc/GMT+7");
    add_timezone(-28800, "Etc/GMT+8");
    add_timezone(-32400, "Etc/GMT+9");
    add_timezone(-36000, "Etc/GMT+10");
    add_timezone(-39600, "Etc/GMT+11");
    add_timezone(-43200, "Etc/GMT+12");
  }

  static std::string get_name_by_offset(int64_t offset, std::string &warn_msg) {
    auto it = timezone_offset_map.find(offset);
    if (it != timezone_offset_map.end()) {
      return it->second;
    } else {
      std::ostringstream osst;
      osst << "Can't find corresponding duckdb time_zone, using Etc/GMT.";
      warn_msg = osst.str();

      return "Etc/GMT";
    }
  }

 private:
  static void add_timezone(int64_t offset, const std::string &name) {
    timezone_offset_map[offset] = name;
  }

  static std::map<int64_t, std::string> timezone_offset_map;
};

/**
  Convert a MySQL Time_zone object to a DuckDB-compatible IANA timezone name.

  @param tz        MySQL Time_zone object to convert
  @param warn_msg  [out] warning message if conversion is imprecise
  @return DuckDB-compatible IANA timezone name (e.g., 'Etc/GMT-8')
*/
std::string convert_timezone_for_duckdb(Time_zone *tz, std::string &warn_msg);

/** Sets duckdb's time_zone for current thread
  @param thd   THD
*/
std::string get_timezone_according_thd(THD *thd, std::string &warn_msg);

}  // namespace myduck
