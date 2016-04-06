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

#include "item_processing_data.h"

using namespace Mysql::Tools::Dump;

Item_processing_data::Item_processing_data(
  Chain_data* chain_data, I_dump_task* process_task_object,
  I_chain_element* chain_element,
  const Mysql::I_callable<void, Item_processing_data*>* completion_callback,
  Item_processing_data* parent_item_data)
  : m_chain_data(chain_data),
  m_process_task_object(process_task_object),
  m_chain_element(chain_element),
  m_completion_callback(completion_callback),
  m_parent_item_data(parent_item_data),
  m_active_executions(0),
  m_had_chain_created(false)
{}

Item_processing_data::~Item_processing_data()
{
  // Check if it is last item in the chain.
  if (get_parent_item_data() == NULL)
    delete m_chain_data;
}

Chain_data* Item_processing_data::get_chain() const
{
  return m_chain_data;
}

Item_processing_data* Item_processing_data::get_parent_item_data() const
{
  return m_parent_item_data;
}

void Item_processing_data::start_processing()
{
  m_active_executions++;
}

bool Item_processing_data::end_processing()
{
  if (--m_active_executions == 0)
  {
    return true;
  }
  return false;
}

I_dump_task* Item_processing_data::get_process_task_object() const
{
  return m_process_task_object;
}

I_chain_element* Item_processing_data::get_processing_chain_element() const
{
  return m_chain_element;
}

bool Item_processing_data::have_completion_callback()
{
  return m_completion_callback != NULL;
}

bool Item_processing_data::had_chain_created() const
{
  return m_had_chain_created;
}

void Item_processing_data::set_had_chain_created()
{
  m_had_chain_created= true;
}

void Item_processing_data::set_chain(Chain_data* chain_data)
{
  m_chain_data= chain_data;
}

bool Item_processing_data::call_completion_callback_at_end()
{
  if (m_active_executions == 0)
  {
    if (m_completion_callback != NULL)
    {
      (*m_completion_callback)(this);
      m_completion_callback= NULL;
    }
    return true;
  }
  return false;
}
