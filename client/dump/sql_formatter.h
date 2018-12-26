/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_FORMATTER_INCLUDED
#define SQL_FORMATTER_INCLUDED

#include "abstract_output_writer_wrapper.h"
#include "i_data_formatter.h"
#include "abstract_mysql_chain_element_extension.h"
#include "abstract_plain_sql_object_dump_task.h"
#include "dump_start_dump_task.h"
#include "dump_end_dump_task.h"
#include "database_start_dump_task.h"
#include "database_end_dump_task.h"
#include "table_definition_dump_task.h"
#include "table_deferred_indexes_dump_task.h"
#include "row_group_dump_task.h"
#include "sql_formatter_options.h"
#include "mysqldump_tool_chain_maker_options.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Prints object data in SQL format.
 */
class Sql_formatter
  : public Abstract_output_writer_wrapper,
  public Abstract_mysql_chain_element_extension,
  public virtual I_data_formatter
{
public:
  Sql_formatter(
    I_connection_provider* connection_provider,
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
      message_handler, Simple_id_generator* object_id_generator,
      const Mysqldump_tool_chain_maker_options* mysqldump_tool_options,
      const Sql_formatter_options* options);

    ~Sql_formatter();

  /**
    Creates string representation for output of DB object related to specified
    dump task object.
   */
  void format_object(Item_processing_data* item_to_process);

private:
  void format_plain_sql_object(
    Abstract_plain_sql_object_dump_task* plain_sql_dump_task);

  void format_dump_start(Dump_start_dump_task* dump_start_dump_task);

  void format_dump_end(Dump_end_dump_task* dump_start_dump_task);

  void format_database_start(
    Database_start_dump_task* database_definition_dump_task);

  void format_table_definition(
    Table_definition_dump_task* table_definition_dump_task);

  void format_table_indexes(
    Table_deferred_indexes_dump_task* table_indexes_dump_task);

  void format_row_group(Row_group_dump_task* row_group);

  void format_sql_objects_definer(
    Abstract_plain_sql_object_dump_task* , std::string);

  Mysql::Tools::Base::Mysql_query_runner* m_escaping_runner;
  const Mysqldump_tool_chain_maker_options* m_mysqldump_tool_options;
  const Sql_formatter_options* m_options;
};

}
}
}

#endif
