/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/*
  This test verifies that sql_class.h compiles by itself, and that it doesn't
  pull in a given set of headers that are known to increase compile time
  significantly.
*/

#include "sql/sql_class.h"

#ifdef SQL_LEX_INCLUDED
#error "sql_class.h includes sql_lex.h directly or indirectly; it should not."
#endif

#ifdef ITEM_INCLUDED
#error "sql_class.h includes item.h directly or indirectly; it should not."
#endif

#ifdef FIELD_INCLUDED
#error "sql_class.h includes field.h directly or indirectly; it should not."
#endif

#ifdef HANDLER_INCLUDED
#error "sql_class.h includes handler.h directly or indirectly; it should not."
#endif

#ifdef _SQL_PROFILE_H
#error "sql_class.h includes sql_profile.h directly or indirectly; it should not."
#endif
