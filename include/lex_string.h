/*
   Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef LEX_STRING_INCLUDED
#define LEX_STRING_INCLUDED

#include "mysql/mysql_lex_string.h"

/*
  LEX_STRING -- a pair of a C-string and its length.
  (it's part of the plugin API as a MYSQL_LEX_STRING)
  Ditto LEX_CSTRING/MYSQL_LEX_CSTRING.
*/

typedef struct st_mysql_lex_string LEX_STRING;
typedef struct st_mysql_const_lex_string LEX_CSTRING;

#endif
