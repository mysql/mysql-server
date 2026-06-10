#ifndef UNITTEST_GUNIT_MYSYS_UTIL_H_
#define UNITTEST_GUNIT_MYSYS_UTIL_H_
/* Copyright (c) 2018, 2026, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys_util.h
  Utilities for working with mysys.
*/

#include "my_inttypes.h"
#include "my_temporal.h"
#include "mysql_time.h"

/// Adds sensible constructors to MYSQL_TIME.
struct MysqlTime : public Datetime_val {
  MysqlTime() : Datetime_val() {}

  MysqlTime(uint32_t year_arg, uint32_t month_arg, uint32_t day_arg)
      : Datetime_val(year_arg, month_arg, day_arg) {}

  MysqlTime(uint32_t year_arg, uint32_t month_arg, uint32_t day_arg,
            uint32_t hour_arg, uint32_t minute_arg, uint32_t second_arg,
            uint32_t microsecond_arg)
      : Datetime_val(year_arg, month_arg, day_arg, hour_arg, minute_arg,
                     second_arg, microsecond_arg) {}

  MysqlTime(uint32_t year_arg, uint32_t month_arg, uint32_t day_arg,
            uint32_t hour_arg, uint32_t minute_arg, uint32_t second_arg,
            uint32_t microsecond_arg, int32_t time_zone_displacement_arg)
      : Datetime_val{year_arg,        month_arg,
                     day_arg,         hour_arg,
                     minute_arg,      second_arg,
                     microsecond_arg, time_zone_displacement_arg} {}
};

#endif  // UNITTEST_GUNIT_MYSYS_UTIL_H_
