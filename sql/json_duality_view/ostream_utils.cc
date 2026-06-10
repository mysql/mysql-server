/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "sql/json_duality_view/ostream_utils.h"
#include "sql-common/json_dom.h"
#include "sql-common/json_path.h"

namespace jdv {
const char *str(const Content_tree_node::Type &ctnt) {
  switch (ctnt) {
    case Content_tree_node::Type::INVALID:
      return "INVALID";
    case Content_tree_node::Type::ROOT:
      return "ROOT";
    case Content_tree_node::Type::SINGLETON_CHILD:
      return "SINGLETON_CHILD";
    case Content_tree_node::Type::NESTED_CHILD:
      return "NESTED_CHILD";
  }
  assert(false);
  return "Unknown Content_tree_node::Type";
}

std::ostream &operator<<(std::ostream &os, const Content_tree_node &ctn) {
  os << ctn.name() << "{" << str(ctn.type()) << ",";
  os << ctn.qualified_table_name() << ", dw:" << ctn.dependency_weight() << "}";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Key_column_info &kci) {
  os << kci.key() << "," << kci.column_name() << "("
     << (kci.is_column_projected() ? "P)" : "J)");
  return os;
}

std::string path_str(const Json_dom *doc) {
  return func_or(
      doc,
      [](const Json_dom &d) {
        String buf;
        d.get_location().to_string(&buf);
        return std::string{buf.ptr(), buf.length()};
      },
      std::string{"nullptr"});
}
}  // namespace jdv
