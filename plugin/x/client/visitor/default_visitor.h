/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef PLUGIN_X_CLIENT_VISITOR_DEFAULT_VISITOR_H_
#define PLUGIN_X_CLIENT_VISITOR_DEFAULT_VISITOR_H_

#include <string>

#include "mysqlxclient/xargument.h"

namespace xcl {

class Default_visitor : public Argument_visitor {
 public:
  void visit_null() override {}
  void visit_integer(const int64_t /*value*/) override {}
  void visit_uinteger(const uint64_t /*value*/) override {}
  void visit_double(const double /*value*/) override {}
  void visit_float(const float /*value*/) override {}
  void visit_bool(const bool /*value*/) override {}
  void visit_object(const Argument_object & /*value*/) override {}
  void visit_uobject(const Argument_uobject & /*value*/) override {}
  void visit_array(const Argument_array & /*value*/) override {}
  void visit_string(const std::string & /*value*/) override {}
  void visit_octets(const std::string & /*value*/) override {}
  void visit_decimal(const std::string & /*value*/) override {}
};

}  // namespace xcl

#endif /* PLUGIN_X_CLIENT_VISITOR_DEFAULT_VISITOR_H_ */
