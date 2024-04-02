/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_ENTRY_FIELDS_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_ENTRY_FIELDS_H_

#include <vector>

#include "mrs/database/entry/field.h"
#include "mrs/database/helper/query.h"

namespace mrs {
namespace database {

class QueryEntryFields : private Query {
 public:
  using ResultSets = entry::ResultSets;
  using ResultObject = entry::ResultObject;

  virtual bool query_parameters(MySQLSession *session,
                                entry::UniversalId db_object_id);

  virtual ResultSets &get_result();

 private:
  void on_row_params(const ResultRow &r);
  void on_row_input_name(const ResultRow &r);
  void on_row_output_name(const ResultRow &r);
  void on_row(const ResultRow &r) override;

  enum class Row { k_fields, k_input_name, k_output_name };

  Row processing_;
  ResultObject *output_result_{nullptr};
  ResultSets result_;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_DATABASE_QUERY_ENTRY_FIELDS_H_
