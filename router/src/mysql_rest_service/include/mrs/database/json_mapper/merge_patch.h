/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_JSON_MAPPER_VIEW_MERGE_PATCH_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_JSON_MAPPER_VIEW_MERGE_PATCH_H_

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <memory>
#include <string>

#include "mrs/database/helper/object_row_ownership.h"
#include "mrs/database/json_mapper/change.h"
#include "mrs/database/json_mapper/common.h"
#include "mrs/database/json_mapper/errors.h"
#include "mrs/database/json_mapper/insert.h"
#include "mrs/database/json_mapper/update.h"

namespace mrs {
namespace database {
namespace dv {

class RowMergePatch : public RowChangeOperation {
 public:
  RowMergePatch(std::shared_ptr<RowMergePatch> parent,
                std::shared_ptr<Table> table, const PrimaryKeyColumnValues &pk,
                const ObjectRowOwnership &row_ownership);

  void process_to_many(const ForeignKeyReference &fk,
                       JSONInputArray input) override;

  void run(MySQLSession *session) override;
};

}  // namespace dv
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_MRS_DATABASE_JSON_MAPPER_VIEW_MERGE_PATCH_H_
