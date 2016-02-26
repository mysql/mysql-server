/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include <algorithm>

#include "mysql_show_variable_wrapper.h"

#include <mysql/plugin.h>


using namespace mysqld;

xpl_show_var::xpl_show_var(st_mysql_show_var *var)
: m_var(var)
{}

void xpl_show_var::assign(const std::string &str)
{
  this->assign(str.c_str());
}

void xpl_show_var::assign(const char *str)
{
  m_var->type = SHOW_CHAR;
  strncpy(m_var->value, str, SHOW_VAR_FUNC_BUFF_SIZE);
  m_var->value[SHOW_VAR_FUNC_BUFF_SIZE - 1] = 0;
}

void xpl_show_var::assign(const long value)
{
  m_var->type = SHOW_LONG;
  memcpy(m_var->value, &value, std::min<std::size_t>(SHOW_VAR_FUNC_BUFF_SIZE, sizeof(value)));
}

void xpl_show_var::assign(const bool value)
{
  m_var->type = SHOW_BOOL;
  memcpy(m_var->value, &value, std::min<std::size_t>(SHOW_VAR_FUNC_BUFF_SIZE, sizeof(value)));
}


void xpl_show_var::assign(const long long value)
{
  m_var->type = SHOW_LONGLONG;
  memcpy(m_var->value, &value, std::min<std::size_t>(SHOW_VAR_FUNC_BUFF_SIZE, sizeof(value)));
}
