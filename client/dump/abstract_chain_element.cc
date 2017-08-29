/*
  Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.

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

#include "abstract_chain_element.h"

using namespace Mysql::Tools::Dump;

my_boost::atomic_uint64_t Abstract_chain_element::next_id;

uint64 Abstract_chain_element::get_id() const
{
  return m_id;
}

Abstract_chain_element::Abstract_chain_element(
  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator)
  : m_id(Abstract_chain_element::next_id++),
  m_message_handler(message_handler),
  m_item_processed_callback(this,
  &Abstract_chain_element::item_completion_in_child_callback_wrapper),
  m_item_processed_complete_callback(this,
  &Abstract_chain_element::
  item_completion_in_child_completes_task_callback),
  m_object_id_generator(object_id_generator)
{}

void Abstract_chain_element::object_processing_starts(
  Item_processing_data* item_to_process)
{
  this->report_object_processing_started(item_to_process);
  item_to_process->start_processing();
}

Item_processing_data* Abstract_chain_element::object_to_be_processed_in_child(
  Item_processing_data* current_item_data,
  I_chain_element* child_chain_element)
{
  return this->task_to_be_processed_in_child(
    current_item_data, child_chain_element,
    current_item_data->get_process_task_object(),
    (current_item_data->have_completion_callback()
      || this->need_callbacks_in_child())
        ? &m_item_processed_callback : NULL);
}

Item_processing_data* Abstract_chain_element::new_task_created(
  I_dump_task* dump_task_created)
{
  return new Item_processing_data(
    NULL, dump_task_created, this,
    this->need_callbacks_in_child()
      ? &m_item_processed_complete_callback : NULL,
    NULL);
}

Item_processing_data* Abstract_chain_element::new_chain_created(
  Chain_data* new_chain_data, Item_processing_data* parent_processing_data,
  I_chain_element* child_chain_element)
{
  Item_processing_data* new_item_to_process=
    this->object_to_be_processed_in_child(
    parent_processing_data, child_chain_element);
  parent_processing_data->set_had_chain_created();
  this->report_new_chain_created(new_item_to_process);
  return new_item_to_process;
}

Item_processing_data* Abstract_chain_element::new_chain_created(
  Item_processing_data* current_item_data, I_dump_task* dump_task_created)
{
  Item_processing_data* new_item_to_process=
    this->task_to_be_processed_in_child(
    current_item_data, this, dump_task_created,
    (current_item_data->have_completion_callback()
    || this->need_callbacks_in_child())
    ? &m_item_processed_complete_callback : NULL);
  current_item_data->set_had_chain_created();
  this->report_new_chain_created(new_item_to_process);
  return new_item_to_process;
}

void Abstract_chain_element::object_processing_ends(
  Item_processing_data* processed_item)
{
  if (processed_item != NULL && processed_item->end_processing())
  {
    this->report_object_processing_ended(processed_item);
    if (processed_item->call_completion_callback_at_end())
    {
      delete processed_item;
    }
  }
}

uint64 Abstract_chain_element::generate_new_object_id()
{
  return m_object_id_generator->create_id();
}

Simple_id_generator* Abstract_chain_element::get_object_id_generator() const
{
  return m_object_id_generator;
}

void Abstract_chain_element::pass_message(
  const Mysql::Tools::Base::Message_data& message_data)
{
  if (m_message_handler)
  {
    (*m_message_handler)(message_data);
  }
}

Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
  Abstract_chain_element::get_message_handler() const
{
  return m_message_handler;
}

bool Abstract_chain_element::need_callbacks_in_child()
{
  return this->have_progress_watcher();
}

void Abstract_chain_element::item_completion_in_child_callback_wrapper(
  Item_processing_data* item_processed)
{
  this->item_completion_in_child_callback(item_processed);
}

void Abstract_chain_element::item_completion_in_child_completes_task_callback(
  Item_processing_data* item_processed)
{
  item_processed->get_process_task_object()->set_completed();
  this->item_completion_in_child_callback_wrapper(item_processed);
}

void Abstract_chain_element::item_completion_in_child_callback(
  Item_processing_data* item_processed)
{
  this->object_processing_ends(item_processed->get_parent_item_data());
}

Item_processing_data* Abstract_chain_element::task_to_be_processed_in_child(
  Item_processing_data* current_item_data,
  I_chain_element* child_chain_element, I_dump_task* task_to_be_processed,
  Mysql::Instance_callback<
    void, Item_processing_data*, Abstract_chain_element>* callback)
{
  current_item_data->start_processing();
  return new Item_processing_data(current_item_data->get_chain(),
    task_to_be_processed, child_chain_element,
    callback, current_item_data);
}
