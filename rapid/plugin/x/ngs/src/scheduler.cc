/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifdef WIN32
// Needed for importing PERFORMANCE_SCHEMA plugin API.
#define MYSQL_DYNAMIC_PLUGIN 1
#endif // WIN32

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include "ngs/scheduler.h"
#include "ngs/memory.h"
#include "my_rdtsc.h"

#define LOG_DOMAIN "ngs.client"
#include "ngs/log.h"


using namespace ngs;


const uint64_t MILLI_TO_NANO = 1000000;


Scheduler_dynamic::Scheduler_dynamic(const char* name)
: m_name(name),
  m_task_pending_mutex(KEY_mutex_x_scheduler_dynamic_task_pending),
  m_task_pending_cond(KEY_cond_x_scheduler_dynamic_task_pending),
  m_thread_exit_mutex(KEY_mutex_x_scheduler_dynamic_thread_exit),
  m_thread_exit_cond(KEY_cond_x_scheduler_dynamic_thread_exit),
  m_post_mutex(KEY_mutex_x_scheduler_dynamic_post),
  m_is_running(0),
  m_min_workers_count(1),
  m_workers_count(0),
  m_tasks_count(0),
  m_idle_worker_timeout(60 * 1000)
{
}


Scheduler_dynamic::~Scheduler_dynamic()
{
  stop();
}


void Scheduler_dynamic::launch()
{
  int32 int_0 = 0;
  if (my_atomic_cas32(&m_is_running, &int_0, 1))
  {
    create_min_num_workers();
    log_info("Scheduler \"%s\" started.", m_name.c_str());
  }
}


void Scheduler_dynamic::create_min_num_workers()
{
  while (is_running() &&
         my_atomic_load32(&m_workers_count) < my_atomic_load32(&m_min_workers_count))
  {
    create_thread();
  }
}


unsigned int Scheduler_dynamic::set_num_workers(unsigned int n)
{
  my_atomic_store32(&m_min_workers_count, n);
  try
  {
    create_min_num_workers();
  }
  catch (std::exception &e)
  {
    log_debug("Exception in set minimal number of workers \"%s\"", e.what());
    const int32 m = my_atomic_load32(&m_workers_count);
    log_warning("Unable to set minimal number of workers to %u; actual value is %i", n, m);
    my_atomic_store32(&m_min_workers_count, m);
    return m;
  }
  return n;
}


void Scheduler_dynamic::set_idle_worker_timeout(unsigned long long milliseconds)
{
  my_atomic_store64(&m_idle_worker_timeout, milliseconds);
  m_task_pending_cond.broadcast(m_task_pending_mutex);
}


void Scheduler_dynamic::stop()
{
  int32 int_1 = 1;
  if (my_atomic_cas32(&m_is_running, &int_1, 0))
  {
    while (m_tasks.empty() == false)
    {
      Task* task = NULL;

      if (m_tasks.pop(task))
        delete task;
    }

    m_task_pending_cond.broadcast(m_task_pending_mutex);

    {
      Mutex_lock lock(m_thread_exit_mutex);
      while (my_atomic_load32(&m_workers_count))
        m_thread_exit_cond.wait(m_thread_exit_mutex);
    }

    Thread_t thread;
    while(m_threads.pop(thread))
    {
      ngs::thread_join(&thread, NULL);
    }

    log_info("Scheduler \"%s\" stopped.", m_name.c_str());
  }
}


// NOTE: Scheduler takes ownership of the task and deletes it after
//       completion with delete operator.
bool Scheduler_dynamic::post(Task* task)
{
  if (is_running() == false || task == NULL)
    return false;

  {
    Mutex_lock lock(m_post_mutex);

    if (increase_tasks_count() >= my_atomic_load32(&m_workers_count))
    {
      try { create_thread(); }
      catch (std::exception &e)
      {
        log_error("Exception in post: %s", e.what());
        decrease_tasks_count();
        return false;
      }
    }
  }

  while (m_tasks.push(task) == false) {}
  m_task_pending_cond.signal(m_task_pending_mutex);

  return true;
}


bool Scheduler_dynamic::post(const Task& task)
{
  Task *copy_task = new (std::nothrow) Task(task);

  if (post(copy_task))
    return true;

  delete copy_task;
  return false;
}


bool Scheduler_dynamic::post_and_wait(const Task& task_to_be_posted)
{
  Wait_for_signal future;

  {
    ngs::Scheduler_dynamic::Task task = boost::bind(&Wait_for_signal::Signal_when_done::execute,
                                                    boost::make_shared<ngs::Wait_for_signal::Signal_when_done>(boost::ref(future), task_to_be_posted));

    if (!post(task))
    {
      log_error("Internal error scheduling task");
      return false;
    }
  }

  future.wait();

  return true;
}


// NOTE: Scheduler takes ownership of monitor.
void Scheduler_dynamic::set_monitor(Monitor *monitor)
{
  m_monitor.reset(monitor);
}


void *Scheduler_dynamic::worker_proxy(void *data)
{
  return reinterpret_cast<Scheduler_dynamic*>(data)->worker();
}


void *Scheduler_dynamic::worker()
{
  bool worker_timed_out = false;

  if (thread_init())
  {
    while (is_running() && worker_timed_out == false)
    {
      bool task_available = false;

      try
      {
        Task* task = NULL;

        while (is_running() &&
               m_tasks.empty() == false && task_available == false)
        {
          task_available = m_tasks.pop(task);
        }

        if (task_available && task)
        {
          Memory_new<Task>::Unique_ptr task_ptr(task);

          (*task_ptr)();
        }
      }
      catch (std::exception &e)
      {
        log_error("Exception in event loop:\"%s\": %s",
                  m_name.c_str(), e.what());
      }

      if (task_available)
        decrease_tasks_count();
      else
      {
        ulonglong wait_start = my_timer_milliseconds();
        Mutex_lock lock(m_task_pending_mutex);

        if (is_running())
          m_task_pending_cond.timed_wait(m_task_pending_mutex,
                                         my_atomic_load64(&m_idle_worker_timeout) * MILLI_TO_NANO);

        if (my_atomic_load32(&m_workers_count) >
            my_atomic_load32(&m_min_workers_count) &&
            int64(my_timer_milliseconds() - wait_start) >=
            my_atomic_load64(&m_idle_worker_timeout))
          worker_timed_out = true;
      }
    }
    thread_end();
  }

  {
    Mutex_lock lock(m_thread_exit_mutex);
    decrease_workers_count();
    m_thread_exit_cond.signal();
  }

  m_terminating_workers.push(my_thread_self());

  return NULL;
}

void Scheduler_dynamic::join_terminating_workers()
{
  my_thread_t tid;
  while (m_terminating_workers.pop(tid))
  {
    Thread_t thread;
    if (m_threads.remove_if(thread, boost::bind(Scheduler_dynamic::thread_id_matches, _1, tid)))
    {
      ngs::thread_join(&thread, NULL);
    }
  }
}


void Scheduler_dynamic::create_thread()
{
  if (is_running())
  {
    Thread_t thread;

    ngs::thread_create(0, &thread, NULL, worker_proxy, this);
    increase_workers_count();
    m_threads.push(thread);
  }
}


bool Scheduler_dynamic::is_running()
{
  return my_atomic_load32(&m_is_running) != 0;
}


int32 Scheduler_dynamic::increase_workers_count()
{
  if (m_monitor)
    m_monitor->on_worker_thread_create();

  return my_atomic_add32(&m_workers_count, 1);
}


int32 Scheduler_dynamic::decrease_workers_count()
{
  if (m_monitor)
    m_monitor->on_worker_thread_destroy();

  return my_atomic_add32(&m_workers_count, -1);
}


int32 Scheduler_dynamic::increase_tasks_count()
{
  if (m_monitor)
    m_monitor->on_task_start();

  return my_atomic_add32(&m_tasks_count, 1);
}


int32 Scheduler_dynamic::decrease_tasks_count()
{
  if (m_monitor)
    m_monitor->on_task_end();

  return my_atomic_add32(&m_tasks_count, -1);
}
