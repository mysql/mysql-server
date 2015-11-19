/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MYSQL_OBJECT_READER_INCLUDED
#define MYSQL_OBJECT_READER_INCLUDED

#include "i_object_reader.h"
#include "abstract_data_formatter_wrapper.h"
#include "abstract_mysql_chain_element_extension.h"
#include "mysql_object_reader_options.h"
#include "row_group_dump_task.h"
#include "table_rows_dump_task.h"
#include "mysql_field.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Parses any DB object(excluding rows and privileges for DB objects) data using
  connection to MySQL server.
 */
class Mysql_object_reader
  : public Abstract_data_formatter_wrapper, public I_object_reader,
  public Abstract_mysql_chain_element_extension
{
public:
  Mysql_object_reader(
    I_connection_provider* connection_provider,
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator,
    const Mysql_object_reader_options* options);

  void read_object(Item_processing_data* item_to_process);

  void format_rows(
    Item_processing_data* item_to_process, Row_group_dump_task* row_group);

private:
  void read_table_rows_task(Table_rows_dump_task* table_rows_dump_task,
    Item_processing_data* item_to_process);

  const Mysql_object_reader_options* m_options;

  class Rows_fetching_context
  {
  public:
    Rows_fetching_context(Mysql_object_reader* parent,
        Item_processing_data* item_processing, bool has_generated_column);

    int64 result_callback(
      const Mysql::Tools::Base::Mysql_query_runner::Row& row_data);

    void process_buffer();
    bool is_all_rows_processed();

  private:
    void acquire_fields_information(MYSQL_RES* mysql_result);

    Mysql_object_reader* m_parent;
    Item_processing_data* m_item_processing;
    Row_group_dump_task m_row_group;
    std::vector<Mysql_field> m_fields;
  };
};

}
}
}

#endif
