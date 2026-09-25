/* Copyright (c) 2017, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_USER_DEFINED_TYPE_SERVICE_H
#define MYSQL_USER_DEFINED_TYPE_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/mysql_user_defined_type_bits.h>

BEGIN_SERVICE_DEFINITION(udt_registration)

DECLARE_METHOD(int, register_type, (mysql_type_descriptor_t * td, void *impl));

DECLARE_METHOD(int, unregister_type, (mysql_type_descriptor_t * td));

DECLARE_METHOD(int, register_function,
               (mysql_function_descriptor_t * fd, eval_function_t impl));

DECLARE_METHOD(int, unregister_function, (mysql_function_descriptor_t * fd));

END_SERVICE_DEFINITION(udt_registration)

//-------------------------------------------------------------------
// UDT_value
//-------------------------------------------------------------------

BEGIN_SERVICE_DEFINITION(udt_value_null)

DECLARE_METHOD(void, set_null, (UDT_value_out * f, bool is_null));
DECLARE_METHOD(void, get_null, (UDT_value_in * f, bool *is_null));

END_SERVICE_DEFINITION(udt_value_null)

BEGIN_SERVICE_DEFINITION(udt_value_string)

DECLARE_METHOD(void, set_utf8mb4,
               (UDT_value_out * f, const char *value, unsigned int length));
DECLARE_METHOD(void, get_utf8mb4,
               (UDT_value_in * f, const char **str, unsigned int *length));

END_SERVICE_DEFINITION(udt_value_string)

BEGIN_SERVICE_DEFINITION(udt_value_blob)

DECLARE_METHOD(void, set,
               (UDT_value_out * f, const unsigned char *val, unsigned int len));
DECLARE_METHOD(void, get,
               (UDT_value_in * f, const unsigned char **val,
                unsigned int *len));

END_SERVICE_DEFINITION(udt_value_blob)

#endif
