/*
  Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_CRAWLER_INCLUDED
#define MYSQL_CRAWLER_INCLUDED

#include "abstract_crawler.h"
#include "abstract_mysql_chain_element_extension.h"
#include "i_connection_provider.h"
#include "i_callable.h"
#include "dump_start_dump_task.h"
#include "abstract_dump_task.h"
#include "database.h"
#include "table.h"
#include "dump_end_dump_task.h"
#include "mysql_chain_element_options.h"
#include "database_start_dump_task.h"
#include "database_end_dump_task.h"
#include "tables_definition_ready_dump_task.h"
#include "simple_id_generator.h"
#include "base/message_data.h"
#include "base/abstract_program.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Searches DB objects using connection to MYSQL server.
 */
class Mysql_crawler
  : public Abstract_crawler, public Abstract_mysql_chain_element_extension
{
public:
  Mysql_crawler(
    I_connection_provider* connection_provider,
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
      message_handler, Simple_id_generator* object_id_generator,
      Mysql_chain_element_options* options,
      Mysql::Tools::Base::Abstract_program* program);
  /**
    Enumerates all objects it can access, gets chains from all registered
    chain_maker for each object and then execute each chain.
   */
  virtual void enumerate_objects();

private:
  void enumerate_database_objects(const Database& db);

  void enumerate_tables(const Database& db);

  void enumerate_table_triggers(const Table& table,
    Abstract_dump_task* dependency);

  void enumerate_views(const Database& db);

  template<typename TObject>void enumerate_functions(
    const Database& db, std::string type);

  void enumerate_event_scheduler_events(const Database& db);

  void enumerate_users();

  /**
    Rewrite statement, enclosing it with version specific comment and with
    DEFINER clause enclosed in version-specific comment.

    This function parses any CREATE statement and encloses DEFINER-clause in
    version-specific comment:
      input query:     CREATE DEFINER=a@b FUNCTION ...
      rewritten query: / *!50003 CREATE * / / *!50020 DEFINER=a@b * / / *!50003
      FUNCTION ... * /
   */
  std::string get_version_specific_statement(std::string create_string,
    const std::string& keyword, std::string main_version,
    std::string definer_version);

  Dump_start_dump_task* m_dump_start_task;
  Dump_end_dump_task* m_dump_end_task;
  Database_start_dump_task* m_current_database_start_dump_task;
  Database_end_dump_task* m_current_database_end_dump_task;
  Tables_definition_ready_dump_task* m_tables_definition_ready_dump_task;
};

}
}
}

#endif
