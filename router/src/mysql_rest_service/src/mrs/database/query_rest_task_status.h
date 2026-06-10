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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_TASK_STATUS_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_TASK_STATUS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "collector/counted_mysql_session.h"
#include "helper/mysql_column.h"
#include "http/base/status_code.h"
#include "mrs/database/entry/field.h"
#include "mrs/database/helper/query.h"
#include "mrs/database/json_template.h"
#include "mrs/gtid_manager.h"
#include "mrs/interface/rest_handler.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

class QueryRestTaskStatus : private Query {
  using Row = Query::Row;
  using ResultSets = entry::ResultSets;
  using MysqlTaskOptions = interface::Options::MysqlTask;

 public:
  virtual void query_status(collector::CountedMySQLSession *session,
                            const std::string &url,
                            const mysqlrouter::sqlstring &user_id,
                            const MysqlTaskOptions &task_options,
                            const std::string &task_id);

  // To be fed to HTTP Result
  std::string response;
  HttpStatusCode::key_type status = HttpStatusCode::Ok;

 protected:
  std::string url_;

  void on_row(const ResultRow &r) override;
};

}  // namespace database
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_DATABASE_TASK_STATUS_H_
