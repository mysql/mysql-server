/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef MYSQL_COMMAND_BACKEND_H
#define MYSQL_COMMAND_BACKEND_H

#include <include/sql_common.h>
#include <sql-common/client_async_authentication.h>  // mysql_state_machine_status

namespace cs {
extern MYSQL_METHODS mysql_methods;

/**
command service state machine.
*/
mysql_state_machine_status cssm_begin_connect(mysql_async_connect *ctx);

MYSQL *csi_connect(mysql_async_connect *ctx);
bool csi_read_query_result(MYSQL *mysql);
bool csi_advanced_command(MYSQL *mysql, enum enum_server_command command,
                          const uchar *header, size_t header_length,
                          const uchar *arg, size_t arg_length, bool skip_check,
                          MYSQL_STMT *stmt);
MYSQL_ROW csi_fetch_row(MYSQL_RES *);
void csi_flush_use_result(MYSQL *mysql, bool);
MYSQL_DATA *csi_read_rows(MYSQL *mysql,
                          MYSQL_FIELD *mysql_fields [[maybe_unused]],
                          unsigned int fields [[maybe_unused]]);
MYSQL_RES *csi_use_result(MYSQL *mysql);
void csi_fetch_lengths(ulong *to, MYSQL_ROW column, unsigned int field_count);
int csi_read_change_user_result(MYSQL *mysql);

}  // namespace cs

#endif  // MYSQL_COMMAND_BACKEND_H
