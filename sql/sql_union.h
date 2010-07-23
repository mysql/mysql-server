/* Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_UNION_INCLUDED
#define SQL_UNION_INCLUDED

#include "my_global.h"                          /* ulong */

class THD;
class select_result;
struct LEX;

typedef class st_select_lex_unit SELECT_LEX_UNIT;

bool mysql_union(THD *thd, LEX *lex, select_result *result,
                 SELECT_LEX_UNIT *unit, ulong setup_tables_done_option);


#endif /* SQL_UNION_INCLUDED */
