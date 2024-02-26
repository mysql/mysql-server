/***********************************************************************

Copyright (c) 2011, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

***********************************************************************/
/**************************************************/ /**
 @file innodb_cb_api.h

 Created 03/15/2011      Jimmy Yang
 *******************************************************/

#ifndef innodb_cb_api_h
#define innodb_cb_api_h

#include "api0api.h"

/** Following are callback function defines for InnoDB APIs, mapped to
functions defined in api0api.c */

/*
Generates declarations which look like:
    using cb_cursor_open_table_t = decltype(&ib_cursor_open_table);
for each api function.
*/

#define TYPE_DECLARATION_TRANSFORM(stem) \
  using cb_##stem##_t = decltype(&ib_##stem);

FOR_EACH_API_METHOD_NAME_STEM(TYPE_DECLARATION_TRANSFORM)

/*
Generates declaration which look like:
    extern cb_cursor_open_table_t ib_cb_cursor_open_table;
for each api function.
*/
#define EXTERN_DECLARATION_TRANSFORM(stem) extern cb_##stem##_t ib_cb_##stem;

FOR_EACH_API_METHOD_NAME_STEM(EXTERN_DECLARATION_TRANSFORM)

#endif /* innodb_cb_api_h */
