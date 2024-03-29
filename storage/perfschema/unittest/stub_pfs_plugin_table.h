/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include <mysql/components/services/pfs_plugin_table_service.h>

#include "storage/perfschema/pfs_plugin_table.h"

void init_pfs_plugin_table() {}

void cleanup_pfs_plugin_table() {}

SERVICE_TYPE(pfs_plugin_table_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_table_v1){
    nullptr, nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_tiny_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_tiny_v1){
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_small_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_small_v1){
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_medium_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_medium_v1){
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_integer_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_integer_v1){
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_bigint_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_bigint_v1){
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_decimal_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_decimal_v1){
    nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_float_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_float_v1){nullptr,
                                                                       nullptr};

SERVICE_TYPE(pfs_plugin_column_double_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_double_v1){
    nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_string_v2)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_string_v2){
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_blob_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_blob_v1){nullptr,
                                                                      nullptr};

SERVICE_TYPE(pfs_plugin_column_enum_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_enum_v1){nullptr,
                                                                      nullptr};

SERVICE_TYPE(pfs_plugin_column_date_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_date_v1){nullptr,
                                                                      nullptr};

SERVICE_TYPE(pfs_plugin_column_time_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_time_v1){nullptr,
                                                                      nullptr};

SERVICE_TYPE(pfs_plugin_column_datetime_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_datetime_v1){
    nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_timestamp_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_timestamp_v1){
    nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_timestamp_v2)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_timestamp_v2){
    nullptr, nullptr, nullptr};

SERVICE_TYPE(pfs_plugin_column_year_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_year_v1){nullptr,
                                                                      nullptr};

SERVICE_TYPE(pfs_plugin_column_text_v1)
SERVICE_IMPLEMENTATION(performance_schema, pfs_plugin_column_text_v1){nullptr,
                                                                      nullptr};
