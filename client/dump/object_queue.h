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

#ifndef OBJECT_QUEUE_INCLUDED
#define OBJECT_QUEUE_INCLUDED

#include <sys/types.h>
#include <functional>
#include <map>
#include <queue>

#include "abstract_dump_task.h"
#include "abstract_object_reader_wrapper.h"
#include "base/abstract_program.h"
#include "base/atomic.h"
#include "base/mutex.h"
#include "i_object_reader.h"
#include "my_inttypes.h"
#include "thread_group.h"

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Wrapper to another Object Reader, adds all objects to read on queue. Allows
  specified number of threads to dequeue and process objects.
 */
class Object_queue : public Abstract_object_reader_wrapper,
  public I_object_reader
{
public:
  Object_queue(
    std::function<bool(const Mysql::Tools::Base::Message_data&)>*
      message_handler, Simple_id_generator* object_id_generator,
    uint threads_count, std::function<void(bool)>* thread_callback,
    Mysql::Tools::Base::Abstract_program* program);

  ~Object_queue();

  void read_object(Item_processing_data* item_to_process);

  // Fix "inherits ... via dominance" warnings
  void register_progress_watcher(I_progress_watcher* new_progress_watcher)
  { Abstract_chain_element::register_progress_watcher(new_progress_watcher); }

  // Fix "inherits ... via dominance" warnings
  uint64 get_id() const
  { return Abstract_chain_element::get_id(); }

  void stop_queue();

protected:
  // Fix "inherits ... via dominance" warnings
  void item_completion_in_child_callback(Item_processing_data* item_processed)
  { Abstract_chain_element::item_completion_in_child_callback(item_processed); }

private:
  void queue_thread();

  void task_availability_callback(const Abstract_dump_task* available_task);

  void add_ready_items_to_queue(
    std::map<const I_dump_task*, std::vector<Item_processing_data*>* >
    ::iterator it);

  /*
    Group of threads to process objects on queue.
  */
  my_boost::thread_group m_thread_group;
  my_boost::mutex m_queue_mutex;
  /*
    Maps task to all processing items that processes specified task.
  */
  std::map<const I_dump_task*, std::vector<Item_processing_data*>*>
    m_tasks_map;
  std::queue<Item_processing_data*> m_items_ready_for_processing;
  /*
    Standard callback on task completion to run all possible dependent tasks.
  */
  std::function<void(const Abstract_dump_task*)>
    m_task_availability_callback;
  /*
    Indicates if queue is running. If set to false, all pending and being
    processed tasks should complete, then queue is ready to close.
  */
  my_boost::atomic_bool m_is_queue_running;
  /*
    Callback called when created thread is starting or exiting. Call is done in
    execution context of created thread. Parameter value
    of true is used for thread start, false for thread exit.
  */
  std::function<void(bool)>* m_thread_callback;
  Mysql::Tools::Base::Abstract_program* m_program;
};

}
}
}

#endif
