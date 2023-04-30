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

#include "collector/counted_mysql_session.h"

#include "mrs/router_observation_entities.h"

namespace collector {

using MySQLSession = mysqlrouter::MySQLSession;

CountedMySQLSession::CountedMySQLSession() {
  mrs::Counter<kEntityCounterMySQLConnectionsActive>::increment();
}

CountedMySQLSession::~CountedMySQLSession() {
  mrs::Counter<kEntityCounterMySQLConnectionsActive>::increment(-1);
}

void CountedMySQLSession::change_user(const std::string &user,
                                      const std::string &password,
                                      const std::string &db) {
  mrs::Counter<kEntityCounterMySQLChangeUser>::increment();
  MySQLSession::change_user(user, password, db);
}

void CountedMySQLSession::reset() { MySQLSession::reset(); }

uint64_t CountedMySQLSession::prepare(const std::string &query) {
  mrs::Counter<kEntityCounterMySQLPrepare>::increment();
  return MySQLSession::prepare(query);
}

void CountedMySQLSession::prepare_execute(uint64_t ps_id,
                                          std::vector<enum_field_types> pt,
                                          const ResultRowProcessor &processor,
                                          const FieldValidator &validator) {
  mrs::Counter<kEntityCounterMySQLPrepareExecute>::increment();
  MySQLSession::prepare_execute(ps_id, pt, processor, validator);
}

void CountedMySQLSession::prepare_remove(uint64_t ps_id) {
  mrs::Counter<kEntityCounterMySQLPrepareRemove>::increment();
  MySQLSession::prepare_remove(ps_id);
}

void CountedMySQLSession::execute(const std::string &query) {
  mrs::Counter<kEntityCounterMySQLQueries>::increment();
  MySQLSession::execute(query);
}

void CountedMySQLSession::query(const std::string &query,
                                const ResultRowProcessor &processor,
                                const FieldValidator &validator) {
  mrs::Counter<kEntityCounterMySQLQueries>::increment();
  MySQLSession::query(query, processor, validator);
}

std::unique_ptr<MySQLSession::ResultRow> CountedMySQLSession::query_one(
    const std::string &query, const FieldValidator &validator) {
  mrs::Counter<kEntityCounterMySQLQueries>::increment();
  return MySQLSession::query_one(query, validator);
}

std::unique_ptr<MySQLSession::ResultRow> CountedMySQLSession::query_one(
    const std::string &query) {
  // It calls query_one with two arguments. There is no need to count this
  // call.
  return MySQLSession::query_one(query);
}

}  // namespace collector
