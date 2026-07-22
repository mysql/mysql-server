/* Copyright (c) 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MRS_COMPONENT_EMBEDDED_MYSQL_RESULT_H
#define MRS_COMPONENT_EMBEDDED_MYSQL_RESULT_H

#include <cstdint>
#include <memory>

#include "mysql.h"
#include "mysql/components/services/mysql_command_services.h"
#include "mysql/components/services/mysql_statement_service.h"
#include "mysqlrouter/mysql_session.h"

namespace mrs_component {

void configure_statement_handle(my_h_statement stmt);
uint64_t mysql_bind_type_to_statement_type(enum_field_types type);
unsigned long mysql_bind_data_length(const MYSQL_BIND &bind);

std::unique_ptr<mysqlrouter::MySQLSession::ResultRow> clone_result_row(
    const mysqlrouter::MySQLSession::ResultRow &row);

enum class StatementResultStatus { kError, kResult, kNoResult };

StatementResultStatus process_statement_result(
    my_h_statement stmt,
    const mysqlrouter::MySQLSession::ResultRowProcessor &processor,
    const mysqlrouter::MySQLSession::FieldValidator &validator);

enum class CommandResultStatus { kError, kResult, kNoResult };

CommandResultStatus process_command_result(
    MYSQL_H mysql_h,
    const mysqlrouter::MySQLSession::ResultRowProcessor &processor,
    const mysqlrouter::MySQLSession::FieldValidator &validator);

}  // namespace mrs_component

#endif  // MRS_COMPONENT_EMBEDDED_MYSQL_RESULT_H
