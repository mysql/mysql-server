/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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
