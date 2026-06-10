/*
 * Copyright (c) 2015, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_DATE_H_
#define ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_DATE_H_

#include <string>

#include "mysqlrouter/jit_executor_value.h"
#include "native_wrappers/polyglot_object_bridge.h"

namespace shcore {
namespace polyglot {

class Date : public Object_bridge {
 public:
  Date(const Date &date);
  Date(int year, int month, int day, int hour, int min, int sec, int usec);
  Date(int hour, int min, int sec, int usec);
  Date(int year, int month, int day);

  std::string class_name() const override { return "Date"; }

  std::string &append_descr(std::string &s_out, int indent = -1,
                            int quote_strings = 0) const override;
  std::string &append_repr(std::string &s_out) const override;
  void append_json(shcore::JSON_dumper &dumper) const override;

  virtual bool operator==(const Object_bridge &other) const;
  bool operator==(const Date &other) const;

  int64_t as_ms() const;

  int get_year() const { return _year; }
  int get_month() const { return _month + 1; }
  int get_day() const { return _day; }
  int get_hour() const { return _hour; }
  int get_min() const { return _min; }
  int get_sec() const { return _sec; }
  int get_usec() const { return _usec; }

  bool has_time() const { return _has_time; }
  bool has_date() const { return _has_date; }

 public:
  static Date unrepr(const std::string &s);
  static Date from_ms(int64_t ms_since_epoch);

 private:
  void validate();

  int _year;
  int _month;
  int _day;
  int _hour;
  int _min;
  int _sec;
  int _usec;
  bool _has_time;
  bool _has_date;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // ROUTER_SRC_JIT_EXECUTOR_SRC_OBJECTS_POLYGLOT_DATE_H_
