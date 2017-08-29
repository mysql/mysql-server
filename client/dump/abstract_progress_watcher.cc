/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "abstract_progress_watcher.h"
#include <algorithm>
#include "table_rows_dump_task.h"
#include "table_definition_dump_task.h"
#include "row_group_dump_task.h"

using namespace Mysql::Tools::Dump;

void Abstract_progress_watcher::progress_changed()
{
  if (--m_step_countdown == 0)
  {
    boost::chrono::system_clock::time_point now=
      boost::chrono::system_clock::now();

    double stages_past=
      std::max(boost::chrono::duration_cast<
      boost::chrono::duration<double> >(
      now - m_last_stage_time) /
      boost::chrono::milliseconds(REPORT_DELAY_MS / STAGES),
      0.1); //  Do not expand stage by more than 10 times the steps.

    m_step_countdown= m_last_step_countdown= std::max(1LL,
      ((int64)(m_last_step_countdown / stages_past)
      + m_last_step_countdown) / 2);
    m_last_stage_time= now;

    uint64 stages_past_int= 1000 * std::min(stages_past, 10.0);
    uint64 last_stage= m_stage_countdown.fetch_sub(stages_past_int);

    if (last_stage <= stages_past_int)
    {
      Progress_data change= m_progress - m_last_progress;
      m_last_progress= m_progress;

      this->process_progress_step(change);

      m_stage_countdown= STAGES * 1000;
    }
  }
}

Abstract_progress_watcher::Abstract_progress_watcher(
  Mysql::I_callable <bool, const Mysql::Tools::Base::Message_data&>*
    message_handler, Simple_id_generator* object_id_generator)
  : Abstract_chain_element(message_handler, object_id_generator),
  m_step_countdown(1),
  m_stage_countdown(STAGES * 1000),
  m_last_step_countdown(1)
{}

void Abstract_progress_watcher::crawler_completed(I_crawler* crawler)
{}

void Abstract_progress_watcher::object_processing_ended(
  Item_processing_data* finished_process_data)
{
  // Check if it is last task in the chain.
  if (finished_process_data->get_parent_item_data() != NULL &&
    finished_process_data->get_parent_item_data()->get_process_task_object()
    == finished_process_data->get_process_task_object())
  {
    return;
  }
  Table_rows_dump_task* processed_table_task=
    dynamic_cast<Table_rows_dump_task*>(
    finished_process_data->get_process_task_object());
  if (processed_table_task != NULL
    && finished_process_data->had_chain_created())
  {
    m_progress.m_table_count++;
    this->progress_changed();
    return;
  }

  Row_group_dump_task* processed_row_group=
    dynamic_cast<Row_group_dump_task*>(
    finished_process_data->get_process_task_object());
  if (processed_row_group != NULL && processed_row_group->is_completed())
  {
    m_progress.m_row_count+= processed_row_group->m_rows.size();
    this->progress_changed();
    return;
  }
}

void Abstract_progress_watcher::object_processing_started(
  Item_processing_data* process_data)
{}

void Abstract_progress_watcher::new_chain_created(
  Item_processing_data* new_chain_process_data)
{
  Table_definition_dump_task* new_table_task=
    dynamic_cast<Table_definition_dump_task*>(
    new_chain_process_data->get_process_task_object());
  if (new_table_task != NULL)
  {
    Table* new_table= new_table_task->get_related_table();

    m_total.m_table_count++;
    m_total.m_row_data+= new_table->get_row_data_lenght();
    m_total.m_row_count+= new_table->get_row_count();
  }
}

Abstract_progress_watcher::Progress_data
  Abstract_progress_watcher::Progress_data::operator-(
    const Progress_data& to_subtract)
{
  Progress_data res;
  res.m_table_count= (uint64)m_table_count - (uint64)to_subtract.m_table_count;
  res.m_row_data= (uint64)m_row_data - (uint64)to_subtract.m_row_data;
  res.m_row_count= (uint64)m_row_count - (uint64)to_subtract.m_row_count;

  return res;
}

Abstract_progress_watcher::Progress_data&
  Abstract_progress_watcher::Progress_data::operator=(
    const Progress_data& to_copy)
{
  m_table_count= to_copy.m_table_count.load();
  m_row_data= to_copy.m_row_data.load();
  m_row_count= to_copy.m_row_count.load();

  return *this;
}

Abstract_progress_watcher::Progress_data::Progress_data(
  const Abstract_progress_watcher::Progress_data& to_copy)
{
  *this= to_copy;
}

Abstract_progress_watcher::Progress_data::Progress_data()
  : m_table_count(0),
  m_row_data(0),
  m_row_count(0)
{}
