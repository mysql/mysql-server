/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_SHOW_STATUS_H
#define SQL_SHOW_STATUS_H

#include "my_global.h"
#include "sql_class.h" // THD

class String;

SELECT_LEX*
build_show_global_status(const POS &pos, THD *thd, const String *wild, Item *where_cond);

SELECT_LEX*
build_show_session_status(const POS &pos, THD *thd, const String *wild, Item *where_cond);

SELECT_LEX*
build_show_global_variables(const POS &pos, THD *thd, const String *wild, Item *where_cond);

SELECT_LEX*
build_show_session_variables(const POS &pos, THD *thd, const String *wild, Item *where_cond);

#endif /* SQL_SHOW_STATUS_H */
