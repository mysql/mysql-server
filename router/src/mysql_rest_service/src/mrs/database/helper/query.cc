/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mrs/database/helper/query.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

using MySQLSession = Query::MySQLSession;

void QueryLog::query(MySQLSession *session, const std::string &q) {
  log_debug("query: %s", q.c_str());
  Query::query(session, q);
}

void QueryLog::prepare_and_execute(MySQLSession *session, const std::string &q,
                                   std::vector<enum_field_types> pt) {
  log_debug("Prepare: %s", q.c_str());
  Query::prepare_and_execute(session, q, pt);
}

void Query::query(MySQLSession *session, const std::string &q) {
  try {
    MySQLSession::ResultRowProcessor processor = [this](const ResultRow &r) {
      on_row(r);
      return true;
    };
    session->query(q, processor, [this](unsigned number, MYSQL_FIELD *fields) {
      on_metadata(number, fields);
    });
  } catch (const mysqlrouter::MySQLSession::Error &e) {
    sqlstate_ = session->last_sqlstate();
    throw;
  } catch (...) {
    log_debug("Following query failed: '%s'", q.c_str());
    throw;
  }
}

std::unique_ptr<MySQLSession::ResultRow> Query::query_one(
    MySQLSession *session) {
  return query_one(session, query_);
}

std::unique_ptr<MySQLSession::ResultRow> Query::query_one(
    MySQLSession *session, const std::string &q) {
  try {
    log_debug("Executing query: '%s'", q.c_str());

    auto result =
        session->query_one(q, [this](unsigned number, MYSQL_FIELD *fields) {
          on_metadata(number, fields);
        });

    return result;
  } catch (...) {
    log_debug("Following query failed: '%s'", q.c_str());
    throw;
  }

  return {};
}

void Query::execute(MySQLSession *session) { query(session, query_); }
void Query::prepare_and_execute(MySQLSession *session, const std::string &q,
                                std::vector<enum_field_types> pt) {
  auto id = session->prepare(q);

  try {
    session->prepare_execute(
        id, pt,
        [this](const auto &r) {
          on_row(r);
          return true;
        },
        [this](unsigned number, MYSQL_FIELD *fields) {
          on_metadata(number, fields);
        });
    session->prepare_remove(id);
  } catch (mysqlrouter::MySQLSession::Error &e) {
    sqlstate_ = session->last_sqlstate();
    session->prepare_remove(id);
    log_debug("Following query failed: '%s'", e.message().c_str());
    throw;
  }
}

void Query::on_row([[maybe_unused]] const ResultRow &r) {}

void Query::on_metadata(unsigned number, MYSQL_FIELD *fields) {
  metadata_ = fields;
  num_of_metadata_ = number;
}

}  // namespace database

}  // namespace mrs
