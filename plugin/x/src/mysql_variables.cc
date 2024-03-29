/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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

#include "plugin/x/src/mysql_variables.h"

#include <mysql/plugin.h>

#include "sql/mysqld.h"
#include "sql/ssl_acceptor_context_operator.h"

struct CHARSET_INFO;

namespace mysqld {

bool is_terminating() { return connection_events_loop_aborted(); }

bool get_initialize() { return opt_initialize; }
const char *get_my_localhost() { return my_localhost; }

const CHARSET_INFO *get_default_charset() {
  return &my_charset_utf8mb4_0900_ai_ci;
}

sigset_t get_mysqld_signal_mask() { return mysqld_signal_mask; }

bool have_ssl() {
  if (mysql_main != nullptr) {
    Lock_and_access_ssl_acceptor_context context(mysql_main);
    if (context.have_ssl()) return true;
  }
  return false;
}

}  // namespace mysqld
