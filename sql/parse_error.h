/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PARSE_ERROR_INCLUDED_H
#define PARSE_ERROR_INCLUDED_H

#include <stddef.h>
#include <cstdarg>

#include "parse_location.h"

class THD;

void my_syntax_error(THD *thd, const char *s);
void syntax_error_at(THD *thd, const YYLTYPE &location, const char *s= NULL);
void vsyntax_error_at(THD *thd, const YYLTYPE &location,
                      const char *format, va_list args);

#endif /* PARSE_ERROR_INCLUDED_H */
