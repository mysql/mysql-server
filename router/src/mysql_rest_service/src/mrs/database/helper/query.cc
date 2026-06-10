/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

using MySQLSession = QueryRaw::MySQLSession;

void QueryLog::query(MySQLSession *session, const std::string &q) {
  log_debug("query: %s", q.c_str());
  QueryRaw::query(session, q);
}

void QueryLog::prepare_and_execute(MySQLSession *session, const std::string &q,
                                   std::vector<MYSQL_BIND> pt,
                                   const OnResultSetEnd &on_resultset_end) {
  log_debug("Prepare: %s", q.c_str());
  QueryRaw::prepare_and_execute(session, q, pt, on_resultset_end);
}

void QueryRaw::query(MySQLSession *session, const std::string &q) {
  try {
    MySQLSession::ResultRowProcessor processor = [this](const ResultRow &r) {
      on_row(r);
      return true;
    };
    session->query(q, processor, [this](unsigned number, MYSQL_FIELD *fields) {
      on_metadata(number, fields);
    });
  } catch (const mysqlrouter::MySQLSession::Error &) {
    sqlstate_ = session->last_sqlstate();
    throw;
  } catch (...) {
    log_debug("Following query failed: '%s'", q.c_str());
    throw;
  }
}

std::unique_ptr<MySQLSession::ResultRow> QueryRaw::query_one(
    MySQLSession *session) {
  return query_one(session, query_);
}

std::unique_ptr<MySQLSession::ResultRow> QueryRaw::query_one(
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

void QueryRaw::execute(MySQLSession *session) { query(session, query_); }

void QueryRaw::prepare_and_execute(MySQLSession *session, const std::string &q,
                                   std::vector<MYSQL_BIND> pt,
                                   const OnResultSetEnd &on_resultset_end) {
  auto id = session->prepare(q);

  try {
    session->prepare_execute_with_bind_parameters(
        id, pt,
        [this](const auto &r) {
          on_row(r);
          return true;
        },
        [this](unsigned number, MYSQL_FIELD *fields) {
          on_metadata(number, fields);
        },
        on_resultset_end);
    session->prepare_remove(id);
  } catch (mysqlrouter::MySQLSession::Error &e) {
    sqlstate_ = session->last_sqlstate();
    session->prepare_remove(id);
    log_debug("Following query failed: '%s', error: '%s'", q.c_str(),
              e.message().c_str());
    throw;
  }
}

void QueryRaw::on_row([[maybe_unused]] const ResultRow &r) {}

void QueryRaw::on_metadata(unsigned number, MYSQL_FIELD *fields) {
  metadata_ = fields;
  num_of_metadata_ = number;
}

}  // namespace database

}  // namespace mrs
