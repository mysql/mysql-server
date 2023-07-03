/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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
*/

#include "client/dump/mysql_object_reader.h"

#include <sys/types.h>
#include <boost/algorithm/string.hpp>
#include <functional>

using namespace Mysql::Tools::Dump;
using std::placeholders::_1;

void Mysql_object_reader::Rows_fetching_context::acquire_fields_information(
    MYSQL_RES *mysql_result) {
  MYSQL_FIELD *fields = mysql_fetch_fields(mysql_result);
  uint field_count = mysql_num_fields(mysql_result);
  m_fields.reserve(field_count);
  for (uint i = 0; i < field_count; ++i)
    m_fields.push_back(Mysql_field(&fields[i]));
}

void Mysql_object_reader::Rows_fetching_context::process_buffer() {
  if (m_row_group.m_rows.size() == 0) return;
  m_parent->format_rows(m_item_processing, &m_row_group);

  m_row_group.m_rows.clear();
}

int64 Mysql_object_reader::Rows_fetching_context::result_callback(
    const Mysql::Tools::Base::Mysql_query_runner::Row &row_data) {
  if (unlikely(m_fields.size() == 0)) {
    this->acquire_fields_information(row_data.get_mysql_result_info());
  }
  m_row_group.m_rows.push_back(new Row(row_data));

  if (m_row_group.m_rows.size() >= m_parent->m_options->m_row_group_size) {
    this->process_buffer();
  }

  return 0;
}

Mysql_object_reader::Rows_fetching_context::Rows_fetching_context(
    Mysql_object_reader *parent, Item_processing_data *item_processing,
    bool has_generated_columns, bool has_invisible_columns)
    : m_parent(parent),
      m_item_processing(item_processing),
      m_row_group((Table *)item_processing->get_process_task_object()
                      ->get_related_db_object(),
                  m_fields, has_generated_columns, has_invisible_columns) {
  m_row_group.m_rows.reserve((size_t)m_parent->m_options->m_row_group_size);
}

bool Mysql_object_reader::Rows_fetching_context::is_all_rows_processed() {
  return m_row_group.m_rows.size() == 0;
}

void Mysql_object_reader::read_table_rows_task(
    Table_rows_dump_task *table_rows_dump_task,
    Item_processing_data *item_to_process) {
  Mysql::Tools::Base::Mysql_query_runner *runner = this->get_runner();

  if (!runner) return;

  if (m_options->m_skip_gipk)
    runner->run_query(
        "/*!80030 SET SESSION "
        "show_gipk_in_create_table_and_information_schema = OFF */");

  Table *table = table_rows_dump_task->get_related_table();

  std::vector<const Mysql::Tools::Base::Mysql_query_runner::Row *> columns;

  runner->run_query_store(
      "SELECT `COLUMN_NAME`, `EXTRA` FROM " +
          this->get_quoted_object_full_name("INFORMATION_SCHEMA", "COLUMNS") +
          "WHERE TABLE_SCHEMA ='" + runner->escape_string(table->get_schema()) +
          "' AND TABLE_NAME ='" + runner->escape_string(table->get_name()) +
          "'" + " ORDER BY ORDINAL_POSITION ",
      &columns);

  std::string column_names;
  bool has_generated_columns = false;
  bool has_invisible_columns = false;
  for (std::vector<
           const Mysql::Tools::Base::Mysql_query_runner::Row *>::iterator it =
           columns.begin();
       it != columns.end(); ++it) {
    const Mysql::Tools::Base::Mysql_query_runner::Row &column_data = **it;

    /*
      Find "STORED GENERATED or "VIRTUAL GENERATED" , but not
      "DEFAULT_GENERATED" in the EXTRA column.
    */
    if (column_data[1].find(" GENERATED") != std::string::npos) {
      has_generated_columns = true;
    } else {
      column_names += this->quote_name(column_data[0]) + ",";
    }

    if (!has_invisible_columns)
      has_invisible_columns =
          column_data[1].find("INVISIBLE") != std::string::npos;
  }

  /* remove last comma from column_names */
  column_names = boost::algorithm::replace_last_copy(column_names, ",", "");

  Mysql::Tools::Base::Mysql_query_runner::cleanup_result(&columns);

  Rows_fetching_context *row_fetching_context = new Rows_fetching_context(
      this, item_to_process, has_generated_columns, has_invisible_columns);

  runner->run_query("SELECT " + column_names + "  FROM " +
                        this->get_quoted_object_full_name(table),
                    new std::function<int64(
                        const Mysql::Tools::Base::Mysql_query_runner::Row &)>(
                        std::bind(&Rows_fetching_context::result_callback,
                                  row_fetching_context, _1)));

  row_fetching_context->process_buffer();
  if (row_fetching_context->is_all_rows_processed())
    delete row_fetching_context;
  if (m_options->m_skip_gipk)
    runner->run_query(
        "/*!80030 SET SESSION "
        "show_gipk_in_create_table_and_information_schema = default */");
  delete runner;
}

void Mysql_object_reader::format_rows(Item_processing_data *item_to_process,
                                      Row_group_dump_task *row_group) {
  this->format_object(this->new_chain_created(item_to_process, row_group));
}

void Mysql_object_reader::read_object(Item_processing_data *item_to_process) {
  this->object_processing_starts(item_to_process);

  if (!(this->try_process_task<Table_rows_dump_task>(
          item_to_process, &Mysql_object_reader::read_table_rows_task))) {
    this->format_object(item_to_process);
  }

  this->object_processing_ends(item_to_process);
}

Mysql_object_reader::Mysql_object_reader(
    I_connection_provider *connection_provider,
    std::function<bool(const Mysql::Tools::Base::Message_data &)>
        *message_handler,
    Simple_id_generator *object_id_generator,
    const Mysql_object_reader_options *options)
    : Abstract_data_formatter_wrapper(message_handler, object_id_generator),
      Abstract_mysql_chain_element_extension(
          connection_provider, message_handler,
          options->m_mysql_chain_element_options),
      m_options(options) {}
