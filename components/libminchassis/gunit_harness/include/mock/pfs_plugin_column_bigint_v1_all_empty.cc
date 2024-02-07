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

namespace pfs_plugin_column_bigint_v1_all_empty {
DEFINE_METHOD(void, set, (PSI_field * /*f*/, PSI_bigint /*value*/)) {}
DEFINE_METHOD(void, set_unsigned, (PSI_field * /*f*/, PSI_ubigint /*value*/)) {}
DEFINE_METHOD(void, get, (PSI_field * /*f*/, PSI_bigint * /*value*/)) {}
DEFINE_METHOD(void, get_unsigned,
              (PSI_field * /*f*/, PSI_ubigint * /*value*/)) {}
DEFINE_METHOD(void, read_key,
              (PSI_key_reader * /*reader*/, PSI_plugin_key_bigint * /*key*/,
               int /*find_flag*/)) {}
DEFINE_METHOD(void, read_key_unsigned,
              (PSI_key_reader * /*reader*/, PSI_plugin_key_ubigint * /*key*/,
               int /*find_flag*/)) {}
DEFINE_METHOD(bool, match_key,
              (bool /*record_null*/, long long /*record_value*/,
               PSI_plugin_key_bigint * /*key*/)) {
  return false;
}
DEFINE_METHOD(bool, match_key_unsigned,
              (bool /*record_null*/, unsigned long long /*record_value*/,
               PSI_plugin_key_ubigint * /*key*/)) {
  return false;
}

}  // namespace pfs_plugin_column_bigint_v1_all_empty

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME,
                             pfs_plugin_column_bigint_v1)
pfs_plugin_column_bigint_v1_all_empty::set,
    pfs_plugin_column_bigint_v1_all_empty::set_unsigned,
    pfs_plugin_column_bigint_v1_all_empty::get,
    pfs_plugin_column_bigint_v1_all_empty::get_unsigned,
    pfs_plugin_column_bigint_v1_all_empty::read_key,
    pfs_plugin_column_bigint_v1_all_empty::read_key_unsigned,
    pfs_plugin_column_bigint_v1_all_empty::match_key,
    pfs_plugin_column_bigint_v1_all_empty::match_key_unsigned,
    END_SERVICE_IMPLEMENTATION();
