/*  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#pragma once

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/bulk_data_service.h>

namespace Bulk_data_convert {

DEFINE_METHOD(int, mysql_format,
              (THD * thd, const TABLE *table, const Rows_text &text_rows,
               size_t &next_index, char *buffer, size_t &buffer_length,
               const CHARSET_INFO *charset, const Row_meta &metadata,
               Rows_mysql &sql_rows,
               Bulk_load_error_location_details &error_details));

DEFINE_METHOD(int, mysql_format_from_raw,
              (char *buffer, size_t buffer_length, const Row_meta &metadata,
               size_t start_index, size_t &consumed_length,
               Rows_mysql &sql_rows));

DEFINE_METHOD(int, mysql_format_using_key,
              (const Row_meta &metadata, const Rows_mysql &sql_keys,
               size_t key_offset, Rows_mysql &sql_rows, size_t sql_index));

DEFINE_METHOD(bool, is_killed, (THD * thd));

DEFINE_METHOD(int, compare_keys,
              (const Column_mysql &key1, const Column_mysql &key2,
               const Column_meta &col_meta));

DEFINE_METHOD(bool, get_row_metadata,
              (THD * thd, const TABLE *table, bool have_key,
               Row_meta &metadata));

}  // namespace Bulk_data_convert

namespace Bulk_data_load {

DEFINE_METHOD(void *, begin,
              (THD * thd, const TABLE *table, size_t data_size, size_t memory,
               size_t num_threads));

DEFINE_METHOD(bool, load,
              (THD * thd, void *ctx, const TABLE *table,
               const Rows_mysql &sql_rows, size_t thread,
               Bulk_load::Stat_callbacks &wait_cbks));

DEFINE_METHOD(bool, end,
              (THD * thd, void *ctx, const TABLE *table, bool error));

DEFINE_METHOD(bool, is_table_supported, (THD * thd, const TABLE *table));

DEFINE_METHOD(size_t, get_se_memory_size, (THD * thd, const TABLE *table));

}  // namespace Bulk_data_load
