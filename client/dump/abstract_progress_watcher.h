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

#ifndef ABSTRACT_PROGRESS_WATCHER_INCLUDED
#define ABSTRACT_PROGRESS_WATCHER_INCLUDED

#include "i_progress_watcher.h"
#include "abstract_chain_element.h"
#include "base/atomic.h"
#include <boost/chrono.hpp>

namespace Mysql{
namespace Tools{
namespace Dump{

/**
  Gathers information about progress of current dump progress and format
  messages on progress.Also it should expose API for receiving processed
  progress information: collected objects and rows information along with time
  elapsed, ETA.
 */
class Abstract_progress_watcher
  : public virtual I_progress_watcher, public Abstract_chain_element
{
public:
  void new_chain_created(Item_processing_data* new_chain_process_data);

  void object_processing_started(Item_processing_data* process_data);

  void object_processing_ended(Item_processing_data* finished_process_data);

  void crawler_completed(I_crawler* crawler);

protected:
  Abstract_progress_watcher(Mysql::I_callable
    <bool, const Mysql::Tools::Base::Message_data&>*
      message_handler, Simple_id_generator* object_id_generator);

  class Progress_data
  {
  public:
    Progress_data();
    Progress_data(const Progress_data& to_copy);
    Progress_data& operator=(const Progress_data& to_copy);
    Progress_data operator-(const Progress_data& to_subtract);
    my_boost::atomic_uint64_t m_table_count;
    my_boost::atomic_uint64_t m_row_data;
    my_boost::atomic_uint64_t m_row_count;
  };

  virtual void process_progress_step(Progress_data& change)= 0;

  Progress_data m_total;
  Progress_data m_progress;
  Progress_data m_last_progress;

private:
  /**
    Throttles progress changes to be reported to progress_changed() about 1 in
    second. It uses 10 stages, each 100ms long, in each there is number of
    iterations to prevent calling boost::chrono::system_clock::now() on each
    function call.
   */
  void progress_changed();

  static const int STAGES= 10;
  static const int REPORT_DELAY_MS= 1000;

  boost::chrono::system_clock::time_point m_last_stage_time;
  my_boost::atomic_int64_t m_step_countdown;
  my_boost::atomic_int64_t m_stage_countdown;
  int64 m_last_step_countdown;
};

}
}
}

#endif
