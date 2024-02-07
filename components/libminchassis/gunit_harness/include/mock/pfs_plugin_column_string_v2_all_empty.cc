/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
#include "mysql/components/services/pfs_plugin_table_service.h"

namespace pfs_plugin_column_string_v2_all_empty {

DEFINE_METHOD(void, set_char_utf8mb4,
              (PSI_field * /*f*/, const char * /*value*/,
               unsigned int /*length*/)) {}
DEFINE_METHOD(void, get_char_utf8mb4,
              (PSI_field * /*f*/, char * /*str*/, unsigned int * /*length*/)) {}
DEFINE_METHOD(void, read_key_string,
              (PSI_key_reader * /*reader*/, PSI_plugin_key_string * /*key*/,
               int /*find_flag*/)) {}
DEFINE_METHOD(bool, match_key_string,
              (bool /*record_null*/, const char * /*record_string_value*/,
               unsigned int /*record_string_length*/,
               PSI_plugin_key_string * /*key*/)) {
  return false;
}
/* VARCHAR */
DEFINE_METHOD(void, get_varchar_utf8mb4,
              (PSI_field * /*f*/, char * /*str*/, unsigned int * /*length*/)) {}
DEFINE_METHOD(void, set_varchar_utf8mb4,
              (PSI_field * /*f*/, const char * /*str*/)) {}
DEFINE_METHOD(void, set_varchar_utf8mb4_len,
              (PSI_field * /*f*/, const char * /*str*/, unsigned int /*len*/)) {
}

}  // namespace pfs_plugin_column_string_v2_all_empty

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME,
                             pfs_plugin_column_string_v2)
pfs_plugin_column_string_v2_all_empty::set_char_utf8mb4,
    pfs_plugin_column_string_v2_all_empty::get_char_utf8mb4,
    pfs_plugin_column_string_v2_all_empty::read_key_string,
    pfs_plugin_column_string_v2_all_empty::match_key_string,
    pfs_plugin_column_string_v2_all_empty::get_varchar_utf8mb4,
    pfs_plugin_column_string_v2_all_empty::set_varchar_utf8mb4,
    pfs_plugin_column_string_v2_all_empty::set_varchar_utf8mb4_len
    END_SERVICE_IMPLEMENTATION();
