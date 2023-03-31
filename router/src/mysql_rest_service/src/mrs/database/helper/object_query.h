/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_OBJECT_QUERY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_OBJECT_QUERY_H_

#include <set>
#include <string>
#include <vector>
#include "mrs/database/entry/object.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class ObjectFieldFilter {
 public:
  static ObjectFieldFilter from_url_filter(
      const entry::Object &object, const std::vector<std::string> &filter);
  static ObjectFieldFilter from_object(const entry::Object &object);

  bool is_included(const std::string &field) const;
  size_t num_included_fields() const;
  std::string get_first_included() const;

 private:
  std::set<std::string> m_filter;
  bool m_exclusive;

  bool is_parent_included(const std::string &field) const;
};

mysqlrouter::sqlstring build_sql_json_object(
    const entry::Object &object, const ObjectFieldFilter &field_filter);

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_OBJECT_QUERY_H_
