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


#ifndef _XPL_MYSQL_VARIABLE_H_
#define _XPL_MYSQL_VARIABLE_H_

struct charset_info_st;
typedef struct charset_info_st CHARSET_INFO;

namespace mysqld
{

//XXX temporary wrapper for server variables
//    it should be removed after plugin correctly handles dynamic plugin macro
bool is_terminating();
const char *get_my_localhost();
const CHARSET_INFO *get_charset_utf8mb4_general_ci();

} // namespace mysqld

#endif // _XPL_MYSQL_VARIABLE_H_
