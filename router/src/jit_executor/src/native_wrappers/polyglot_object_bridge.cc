/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates.
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

#include "native_wrappers/polyglot_object_bridge.h"

#include <algorithm>

namespace shcore {
namespace polyglot {

void Object_bridge::append_json(JSON_dumper &dumper) const {
  dumper.start_object();
  dumper.append_string("class", class_name());
  dumper.end_object();
}

std::string &Object_bridge::append_descr(std::string &s_out, int, int) const {
  s_out.append("<" + class_name() + ">");
  return s_out;
}

std::string &Object_bridge::append_repr(std::string &s_out) const {
  return append_descr(s_out, 0, '"');
}

bool Object_bridge::has_member(const std::string &prop) const {
  auto props = properties();
  return (props &&
          std::find(props->begin(), props->end(), prop) != props->end()) ||
         has_method(prop);
}

bool Object_bridge::has_method(const std::string &name) const {
  auto meths = methods();
  if (meths && std::find(meths->begin(), meths->end(), name) != meths->end()) {
    return true;
  }
  return false;
}

std::vector<std::string> Object_bridge::get_members() const {
  std::vector<std::string> members;

  for (const auto *items : {properties(), methods()}) {
    if (items) {
      std::copy(items->begin(), items->end(), std::back_inserter(members));
    }
  }

  return members;
}

}  // namespace polyglot
}  // namespace shcore
