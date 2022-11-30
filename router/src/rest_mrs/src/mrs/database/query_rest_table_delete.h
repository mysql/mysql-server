/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_DELETE_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_DELETE_H_

#include <stdexcept>
#include <string>
#include <string_view>

#include "helper/json/text_to.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryRestObjectDelete : private QueryLog {
 public:
  bool execute_delete(MySQLSession *session, const std::string &schema,
                      const std::string &object, const std::string &filter) {
    FilterObjectGenerator fog;
    fog.parse(helper::json::text_to_document(filter));
    auto result = fog.get_result();
    if (result.empty())
      throw std::runtime_error("Filter must contain valid JSON object.");
    if (fog.has_order())
      throw std::runtime_error(
          "Filter must not contain ordering informations.");
    query_ = {"DELETE FROM !.! WHERE ?"};
    query_ << schema << object << mysqlrouter::sqlstring(result.c_str());
    execute(session);
    return true;
  }
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_REST_DELETE_H_
