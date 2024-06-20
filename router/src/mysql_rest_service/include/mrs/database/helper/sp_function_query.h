/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_HELPER_SP_FUNCTION_QUERY_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_HELPER_SP_FUNCTION_QUERY_H_

#include <vector>

#include "http/base/uri.h"
#include "mrs/database/duality_view/select.h"
#include "mrs/database/entry/row_user_ownership.h"
#include "mrs/interface/universal_id.h"

namespace mrs {
namespace database {

ColumnValues create_function_argument_list(
    const entry::Object *object, const std::vector<uint8_t> &json_document,
    const entry::RowUserOwnership &ownership, mrs::UniversalId *user_id);

ColumnValues create_function_argument_list(
    const entry::Object *object,
    const ::http::base::Uri::QueryElements &url_query,
    const entry::RowUserOwnership &ownership, mrs::UniversalId *user_id);

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_HELPER_SP_FUNCTION_QUERY_H_
