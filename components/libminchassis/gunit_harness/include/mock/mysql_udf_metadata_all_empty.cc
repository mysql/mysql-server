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
#include "mysql/components/services/udf_metadata.h"

namespace mysql_udf_metadata_all_empty_spc {

static DEFINE_BOOL_METHOD(argument_get,
                          (UDF_ARGS * /*udf_args*/,
                           const char * /*extension_type*/,
                           unsigned int /*index*/, void ** /*out_value*/)) {
  return 0;
}

static DEFINE_BOOL_METHOD(result_get, (UDF_INIT * /*udf_init*/,
                                       const char * /*extension_type*/,
                                       void ** /*out_value*/)) {
  return 0;
}

static DEFINE_BOOL_METHOD(argument_set,
                          (UDF_ARGS * /*udf_args*/,
                           const char * /*extension_type*/,
                           unsigned int /*index*/, void * /*in_value*/)) {
  return 0;
}

static DEFINE_BOOL_METHOD(result_set, (UDF_INIT * /*udf_init*/,
                                       const char * /*extension_type*/,
                                       void * /*in_value*/)) {
  return 0;
}

}  // namespace mysql_udf_metadata_all_empty_spc

BEGIN_SERVICE_IMPLEMENTATION(HARNESS_COMPONENT_NAME, mysql_udf_metadata)
mysql_udf_metadata_all_empty_spc::argument_get,
    mysql_udf_metadata_all_empty_spc::result_get,
    mysql_udf_metadata_all_empty_spc::argument_set,
    mysql_udf_metadata_all_empty_spc::result_set END_SERVICE_IMPLEMENTATION();