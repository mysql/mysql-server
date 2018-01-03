/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TEST_STRING_SERVICE_H
#define TEST_STRING_SERVICE_H

#include <mysql/components/component_implementation.h>
#include "mysql/components/services/mysql_string.h"

/* A place to specify component-wide declarations, including declarations of
  placeholders for Service dependencies. */

extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_character_access);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_byte_access);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_case);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_iterator);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_ctype);

#endif /* TEST_STRING_SERVICE_H */
