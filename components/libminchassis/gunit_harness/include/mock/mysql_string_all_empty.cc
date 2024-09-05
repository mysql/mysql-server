/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#include <stdio.h>
#include "mysql/components/component_implementation.h"
#include "mysql/components/service_implementation.h"
#include "mysql/components/services/mysql_string.h"

namespace mysql_string_spc {

static DEFINE_BOOL_METHOD(convert_from_buffer,
                          (my_h_string * /*out_string*/,
                           const char * /*in_buffer*/, uint64 /*length*/,
                           const char * /*charset_name*/)) {
  return 0;
}
static DEFINE_BOOL_METHOD(convert_to_buffer,
                          (my_h_string /*in_string*/, char * /*out_buffer*/,
                           uint64 /*length*/, const char * /*charset_name*/)) {
  return 0;
}

static DEFINE_BOOL_METHOD(create, (my_h_string * /*out_string*/)) { return 0; }

static DEFINE_METHOD(void, destroy, (my_h_string /*string*/)) {}

}  // namespace mysql_string_spc

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME, mysql_string_converter)
mysql_string_spc::convert_from_buffer, mysql_string_spc::convert_to_buffer,
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME, mysql_string_factory)
mysql_string_spc::create, mysql_string_spc::destroy,
    END_SERVICE_IMPLEMENTATION();

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME,
                             mysql_string_get_data_in_charset)
nullptr END_SERVICE_IMPLEMENTATION();
