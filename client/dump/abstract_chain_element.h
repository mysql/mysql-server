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

#ifndef ABSTRACT_CHAIN_ELEMENT_INCLUDED
#define ABSTRACT_CHAIN_ELEMENT_INCLUDED

#include "i_chain_element.h"
#include "abstract_progress_reporter.h"
#include "base/message_data.h"
#include "i_callable.h"
#include "simple_id_generator.h"
#include "instance_callback.h"
#include "item_processing_data.h"

namespace Mysql{
namespace Tools{
namespace Dump{

class Abstract_chain_element : public virtual I_chain_element,
  public virtual Abstract_progress_reporter
{
public:
  /**
    Returns an application unique ID of this chain element object. This helps
    progress watching with multiple parts of chain during all objects
    processing.
   */
  uint64 get_id() const;

protected:
  Abstract_chain_element(
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
      message_handler, Simple_id_generator* object_id_generator);

  /**
    Process task object with specified function if that task object can be
    casted to type TType. Returns true if object was processed.
   */
  template<typename TType, typename TClass> bool try_process_task(
    Item_processing_data* item_to_process,
    void (TClass::* processing_func)(TType*, Item_processing_data*))
  {
    TType* casted_object= dynamic_cast<TType*>(
      item_to_process->get_process_task_object());
    if (casted_object != NULL)
      (((TClass*)this)->*processing_func)(casted_object, item_to_process);
    return casted_object != NULL;
  }

  /**
    Process task object with specified function if that task object can be
    casted to type TType. Returns true if object was processed.
   */
  template<typename TType, typename TClass> bool try_process_task(
    Item_processing_data* item_to_process,
    void(TClass::* processing_func)(TType*))
  {
    TType* casted_object= dynamic_cast<TType*>(
      item_to_process->get_process_task_object());
    if (casted_object != NULL)
      (((TClass*)this)->*processing_func)(casted_object);
    return casted_object != NULL;
  }

  void object_processing_starts(Item_processing_data* item_to_process);

  Item_processing_data* object_to_be_processed_in_child(
    Item_processing_data* current_item_data,
    I_chain_element* child_chain_element);

  Item_processing_data* new_task_created(I_dump_task* dump_task_created);

  Item_processing_data* new_chain_created(
    Chain_data* new_chain_data, Item_processing_data* parent_processing_data,
    I_chain_element* child_chain_element);

  Item_processing_data* new_chain_created(
    Item_processing_data* current_item_data,
    I_dump_task* dump_task_created);

  void object_processing_ends(Item_processing_data* processed_item);

  uint64 generate_new_object_id();

  Simple_id_generator* get_object_id_generator() const;

  /**
    Passes message to message callback.
   */
  void pass_message(const Mysql::Tools::Base::Message_data& message_data);

  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
    get_message_handler() const;

protected:
  virtual bool need_callbacks_in_child();


private:
  /**
    Wrapper on item_completion_in_child_callback which allows creation of
    pointer to function which will then fetch correct virtual method pointer.
   */
  void item_completion_in_child_callback_wrapper(
    Item_processing_data* item_processed);
  /**
    Wrapper on item_completion_in_child_callback_wrapper which also sets
    precessed task to be fully completed.
   */
  void item_completion_in_child_completes_task_callback(
    Item_processing_data* item_processed);

  void item_completion_in_child_callback(Item_processing_data* item_processed);

  Item_processing_data* task_to_be_processed_in_child(
    Item_processing_data* current_item_data,
    I_chain_element* child_chain_element,
    I_dump_task* task_to_be_processed,
    Mysql::Instance_callback<
      void, Item_processing_data*, Abstract_chain_element>*
      callback);

  uint64 m_id;
  Mysql::I_callable<bool, const Mysql::Tools::Base:: Message_data&>*
    m_message_handler;
  Mysql::Instance_callback<void, Item_processing_data*, Abstract_chain_element>
    m_item_processed_callback;
  Mysql::Instance_callback<void, Item_processing_data*, Abstract_chain_element>
    m_item_processed_complete_callback;
  Simple_id_generator* m_object_id_generator;

  /**
    Stores next chain element ID to be used. Used as ID generator.
   */
  static my_boost::atomic_uint64_t next_id;
};

}
}
}

#endif
