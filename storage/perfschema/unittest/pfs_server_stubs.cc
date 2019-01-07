/* Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Minimal code to be able to link a unit test.
*/

#include <components/mysql_server/server_component.h>
#include <mysql/service_plugin_registry.h>
#include <stddef.h>
#include <sys/types.h>

#include "m_ctype.h"
#include "sql/mysqld.h"
#include "sql/sql_class.h"
#include "sql/sql_show.h"

struct System_status_var global_status_var;

struct sql_digest_storage;
volatile bool ready_to_exit = false;

uint lower_case_table_names = 0;
CHARSET_INFO *files_charset_info = NULL;
CHARSET_INFO *system_charset_info = NULL;

void compute_digest_hash(const sql_digest_storage *, unsigned char *) {}

void reset_status_vars() {}

struct System_status_var *get_thd_status_var(THD *, bool *) {
  return NULL;
}

unsigned int mysql_errno_to_sqlstate_index(unsigned int) { return 0; }

SERVICE_TYPE(registry) * mysql_plugin_registry_acquire() { return NULL; }

int mysql_plugin_registry_release(SERVICE_TYPE(registry) *
                                  reg MY_ATTRIBUTE((unused))) {
  return 0;
}

int log_message(int, ...) {
  /* Do not pollute the unit test output with annoying messages. */
  return 0;
}
