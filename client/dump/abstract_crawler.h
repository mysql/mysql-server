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

#ifndef ABSTRACT_CRAWLER_INCLUDED
#define ABSTRACT_CRAWLER_INCLUDED

#include "i_crawler.h"
#include "abstract_chain_element.h"
#include "i_chain_maker.h"
#include "i_dump_task.h"
#include "base/abstract_program.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class Abstract_crawler : public Abstract_chain_element,
  public virtual I_crawler
{
public:
  /**
    Adds new Chain Maker to ask for chains for found objects.
   */
  virtual void register_chain_maker(I_chain_maker* new_chain_maker);

  ~Abstract_crawler();
protected:
  Abstract_crawler(
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
      message_handler, Simple_id_generator* object_id_generator,
      Mysql::Tools::Base::Abstract_program* program);
  /**
    Routine for performing common work on each enumerated DB object.
   */
  void process_dump_task(I_dump_task* new_dump_task);

  void wait_for_tasks_completion();

  bool need_callbacks_in_child();

  Mysql::Tools::Base::Abstract_program* get_program();
private:
  std::vector<I_chain_maker*> m_chain_makers;
  std::vector<I_dump_task*> m_dump_tasks_created;
  /**
    Stores next chain ID to be used. Used as ID generator.
   */
  static my_boost::atomic_uint64_t next_chain_id;
  Mysql::Tools::Base::Abstract_program* m_program;
};

}
}
}

#endif
