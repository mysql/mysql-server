/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef ONE_ROW_RESULTSET_H_
#define ONE_ROW_RESULTSET_H_

#include <gmock/gmock.h>

#include <string>

#include "xpl_resultset.h"

namespace xpl {
namespace test {

class One_row_resultset : public xpl::Collect_resultset {
 public:
  struct Init {
    Init(const int v)  // NOLINT(runtime/explicit)
        : field(v, true),
          type(MYSQL_TYPE_LONGLONG) {}
    Init(const bool v)  // NOLINT(runtime/explicit)
        : field(v, true),
          type(MYSQL_TYPE_LONGLONG) {}
    Init(const char *v)  // NOLINT(runtime/explicit)
        : field(v, strlen(v)),
          type(MYSQL_TYPE_STRING) {}
    const xpl::Collect_resultset::Field field;
    const enum_field_types type;
  };
  One_row_resultset(std::initializer_list<Init> values) {
    xpl::Collect_resultset::Field_types types;
    xpl::Collect_resultset::Row_list rows(1);
    for (const Init &v : values) {
      types.push_back({v.type, 0});
      rows.begin()->fields.push_back(
          ngs::allocate_object<Field_value>(v.field));
    }
    set_field_types(types);
    set_row_list(rows);
  }
};

ACTION_P(SetUpResultset, init_data) {
  static_cast<xpl::Collect_resultset &>(*arg2) = init_data;
}

}  // namespace test
}  // namespace xpl

#endif  // ONE_ROW_RESULTSET_H_
