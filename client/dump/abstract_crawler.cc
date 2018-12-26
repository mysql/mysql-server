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

#include "abstract_crawler.h"
#include "dump_end_dump_task.h"
#include "this_thread.h"
#include <boost/date_time.hpp>

using namespace Mysql::Tools::Dump;

my_boost::atomic_uint64_t Abstract_crawler::next_chain_id;

Abstract_crawler::Abstract_crawler(
  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator,
    Mysql::Tools::Base::Abstract_program* program)
  : Abstract_chain_element(message_handler, object_id_generator),
    m_program(program)
{}

Abstract_crawler::~Abstract_crawler()
{
  for (std::vector<I_dump_task*>::iterator it= m_dump_tasks_created.begin();
    it != m_dump_tasks_created.end(); ++it)
  {
    delete *it;
  }
}

void Abstract_crawler::register_chain_maker(I_chain_maker* new_chain_maker)
{
  m_chain_makers.push_back(new_chain_maker);
}

Mysql::Tools::Base::Abstract_program* Abstract_crawler::get_program()
{
  return m_program;
}

void Abstract_crawler::process_dump_task(I_dump_task* new_dump_task)
{
  /* in case of error stop all further processing */
  if (get_program()->get_error_code())
    return;

  m_dump_tasks_created.push_back(new_dump_task);

  Item_processing_data* main_item_processing_data=
    this->new_task_created(new_dump_task);

  this->object_processing_starts(main_item_processing_data);

  for (std::vector<I_chain_maker*>::iterator it= m_chain_makers.begin();
    it != m_chain_makers.end(); ++it)
  {
    uint64 new_chain_id= next_chain_id++;
    Chain_data* chain_data= new Chain_data(new_chain_id);

    I_object_reader* chain= (*it)->create_chain(chain_data, new_dump_task);
    if (chain != NULL)
    {
      main_item_processing_data->set_chain(chain_data);
      chain->read_object(
        this->new_chain_created(
        chain_data, main_item_processing_data, chain));
    }
    else
    {
      delete chain_data;
    }
  }
  this->object_processing_ends(main_item_processing_data);
}

void Abstract_crawler::wait_for_tasks_completion()
{
  for (std::vector<I_dump_task*>::iterator it= m_dump_tasks_created.begin();
    it != m_dump_tasks_created.end(); ++it)
  {
    while ((*it)->is_completed() == false)
    {
      /* in case of error stop all running queues */
      if (get_program()->get_error_code())
      {
        for (std::vector<I_chain_maker*>::iterator it= m_chain_makers.begin();
          it != m_chain_makers.end(); ++it)
        {
          (*it)->stop_queues();
        }
        return;
      }
      my_boost::this_thread::sleep(boost::posix_time::milliseconds(1));
    }
  }
}

bool Abstract_crawler::need_callbacks_in_child()
{
  return true;
}
