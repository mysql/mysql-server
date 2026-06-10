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

#include "mrs/database/query_rest_task_status.h"
#include "helper/json/to_string.h"
#include "mysqlrouter/utils_sqlstring.h"

namespace mrs {
namespace database {

void QueryRestTaskStatus::query_status(collector::CountedMySQLSession *session,
                                       const std::string &url,
                                       const mysqlrouter::sqlstring &user_id,
                                       const MysqlTaskOptions &task_options,
                                       const std::string &task_id) {
  if (task_options.driver == MysqlTaskOptions::DriverType::kNone)
    throw std::invalid_argument("Not supported");

  url_ = url;
  status = HttpStatusCode::Ok;

  // query status from task_log
  query_ = {"SELECT mysql_tasks.app_task_status_brief(?, ?)"};
  query_ << user_id << task_id;

  execute(session);
}

void QueryRestTaskStatus::on_row(const ResultRow &r) {
  if (!r[0]) {
    status = HttpStatusCode::NotFound;
  } else {
    response = r[0];
  }
}

}  // namespace database
}  // namespace mrs
