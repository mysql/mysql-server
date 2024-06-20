/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_CHECK_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_CHECK_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "mrs/database/duality_view/change.h"
#include "mrs/database/duality_view/common.h"
#include "mrs/database/duality_view/errors.h"
#include "mrs/database/duality_view/json_input.h"
#include "mrs/database/entry/object.h"
#include "mrs/database/filter_object_generator.h"
#include "mrs/database/helper/object_row_ownership.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {
namespace dv {

class Check : public DualityViewUpdater::Operation {
 public:
  Check(std::shared_ptr<Check> parent, std::shared_ptr<Table> table,
        const ObjectRowOwnership &row_ownership, bool for_update,
        std::shared_ptr<std::set<std::string>> invalid_fields = {},
        bool unnested = false)
      : Operation(parent, table, row_ownership),
        invalid_fields_(invalid_fields),
        unnested_(unnested),
        for_update_(for_update) {
    if (!invalid_fields_) {
      invalid_fields_ = std::make_shared<std::set<std::string>>();
    }
  }

  Check(std::shared_ptr<Table> table, const ObjectRowOwnership &row_ownership,
        bool for_update,
        std::shared_ptr<std::set<std::string>> invalid_fields = {},
        bool unnested = false)
      : Check({}, table, row_ownership, for_update, invalid_fields, unnested) {}

  void run(MySQLSession *) override { throw std::logic_error("invalid call"); }

  void process(JSONInputObject input) override;

  void on_value(const Column &column,
                const JSONInputObject::MemberReference &value) override;

  void on_no_value(const Column &column,
                   const JSONInputObject::MemberReference &) override;

  void process_to_many(const ForeignKeyReference &ref,
                       JSONInputArray input) override;

  void process_to_one(const ForeignKeyReference &ref,
                      JSONInputObject input) override;

 private:
  std::shared_ptr<std::set<std::string>> invalid_fields_;
  bool unnested_ = false;
  bool has_unnested_pk_ = false;
  bool for_update_;
};

}  // namespace dv
}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_DUALITY_VIEW_CHECK_H_
