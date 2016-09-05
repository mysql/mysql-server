/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <ngs/scheduler.h>
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>


namespace xpl
{

namespace test
{


template<typename T>
struct Result_collector
{
  void task(const T& value)
  {
    {
      Mutex_lock lock(m_result_mutex);

      m_result.push_back(value);
    }

    m_check_task_count_cond.signal(m_check_task_count_mutex);
  }

  ngs::Scheduler_dynamic::Task* new_task(const T& value)
  {
    return new ngs::Scheduler_dynamic::Task(
      ngs::bind(&Result_collector::task, this, value)
    );
  }

  void wait(size_t task_count)
  {
    Mutex_lock lock(m_check_task_count_mutex);

    while (m_result.size() != task_count)
      m_check_task_count_cond.wait(m_check_task_count_mutex);
  }

  ngs::Mutex m_check_task_count_mutex;
  ngs::Cond m_check_task_count_cond;
  ngs::Mutex m_result_mutex;
  std::vector<T> m_result;
};


TEST(xpl_scheduler_dynamic, DISABLED_run_1000_tasks)
{
  const unsigned int TASK_COUNT = 1000;

  ngs::Scheduler_dynamic scheduler("name");
  Result_collector<unsigned int> result_set;

  scheduler.launch();
  for (unsigned int idx = 0; idx < TASK_COUNT; ++idx)
    scheduler.post(result_set.new_task(idx));
  result_set.wait(TASK_COUNT);
  scheduler.stop();
  ASSERT_EQ(TASK_COUNT, result_set.m_result.size());

  std::sort(result_set.m_result.begin(), result_set.m_result.end());
  for (unsigned int idx = 0; idx < result_set.m_result.size(); ++idx)
    ASSERT_EQ(idx, result_set.m_result[idx]);
}


} // namespace test

} // namespace xpl
