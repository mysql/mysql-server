/*
  Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLDUMP_TOOL_CHAIN_MAKER_INCLUDED
#define MYSQLDUMP_TOOL_CHAIN_MAKER_INCLUDED

#include <functional>
#include <map>
#include <vector>

#include "abstract_chain_element.h"
#include "abstract_mysql_chain_element_extension.h"
#include "chain_data.h"
#include "i_chain_maker.h"
#include "i_dump_task.h"
#include "i_object_reader.h"
#include "my_inttypes.h"
#include "mysql_object_reader.h"
#include "mysqldump_tool_chain_maker_options.h"
#include "object_queue.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Chain maker implemented in Mysql_dump application, constructs chain based on
  command line options that are compatible with these available in previous
  implementation.
 */
class Mysqldump_tool_chain_maker : public I_chain_maker,
  public Abstract_chain_element, public Abstract_mysql_chain_element_extension
{
public:
  Mysqldump_tool_chain_maker(
    I_connection_provider* connection_provider,
    std::function<bool(const Mysql::Tools::Base::Message_data&)>*
    message_handler, Simple_id_generator* object_id_generator,
    Mysqldump_tool_chain_maker_options* options,
    Mysql::Tools::Base::Abstract_program* program);

  ~Mysqldump_tool_chain_maker();

  I_object_reader* create_chain(
    Chain_data* chain_data, I_dump_task* dump_task);

  void delete_chain(uint64 chain_id, I_object_reader* chain);

  // Fix "inherits ... via dominance" warnings
  void register_progress_watcher( I_progress_watcher* new_progress_watcher)
  { Abstract_chain_element::register_progress_watcher(new_progress_watcher); }

  // Fix "inherits ... via dominance" warnings
  uint64 get_id() const
  { return Abstract_chain_element::get_id(); }

protected:
  // Fix "inherits ... via dominance" warnings
  void item_completion_in_child_callback(Item_processing_data* item_processed)
  { Abstract_chain_element::item_completion_in_child_callback(item_processed); }

private:
  void mysql_thread_callback(bool is_starting);

  void stop_queues();

  Mysqldump_tool_chain_maker_options* m_options;

  Mysql_object_reader* m_main_object_reader;
  std::map<int, Object_queue*> m_object_queues;
  std::vector<I_chain_element*> m_all_created_elements;
  Mysql::Tools::Base::Abstract_program* m_program;
};

}
}
}

#endif
