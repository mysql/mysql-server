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

#ifndef _XPL_MYSQL_SHOW_VARIABLE_H_
#define _XPL_MYSQL_SHOW_VARIABLE_H_

#include <string>


struct st_mysql_show_var;

namespace mysqld
{

  class xpl_show_var
  {
  public:
    xpl_show_var(st_mysql_show_var *var);

    void assign(const std::string &str);
    void assign(const char *str);
    void assign(const bool value);
    void assign(const long value);
    void assign(const long long value);

  private:
    st_mysql_show_var *m_var;
  };

} // namespace mysqld

#endif // _XPL_MYSQL_SHOW_VARIABLE_H_
