/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "plugin/x/src/mysql_show_variable_wrapper.h"

#include <mysql/plugin.h>
#include <algorithm>

using namespace mysqld;

xpl_show_var::xpl_show_var(SHOW_VAR *var) : m_var(var) {}

void xpl_show_var::assign(const std::string &str) { this->assign(str.c_str()); }

void xpl_show_var::assign(const char *str) {
  m_var->type = SHOW_CHAR;
  strncpy(m_var->value, str, SHOW_VAR_FUNC_BUFF_SIZE);
  m_var->value[SHOW_VAR_FUNC_BUFF_SIZE - 1] = 0;
}

void xpl_show_var::assign(const long value) {
  m_var->type = SHOW_LONG;
  memcpy(m_var->value, &value,
         std::min<std::size_t>(SHOW_VAR_FUNC_BUFF_SIZE, sizeof(value)));
}

void xpl_show_var::assign(const bool value) {
  m_var->type = SHOW_BOOL;
  memcpy(m_var->value, &value,
         std::min<std::size_t>(SHOW_VAR_FUNC_BUFF_SIZE, sizeof(value)));
}

void xpl_show_var::assign(const long long value) {
  m_var->type = SHOW_LONGLONG;
  memcpy(m_var->value, &value,
         std::min<std::size_t>(SHOW_VAR_FUNC_BUFF_SIZE, sizeof(value)));
}
