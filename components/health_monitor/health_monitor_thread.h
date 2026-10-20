/* Copyright (c) 2021, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#pragma once

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <mysql/components/services/mysql_cond.h>
#include <mysql/components/services/mysql_mutex.h>
#include <mysql/components/services/mysql_rwlock.h>
#include <mysql/components/services/mysql_system_variable.h>
#include <mysql/components/services/psi_memory.h>
#include <mysql/components/services/psi_thread.h>

namespace hm {

class Thread {
 public:
  Thread() = default;
  Thread(const char *name, const char *instr_name, const char *os_name,
         const char *desc);
  virtual ~Thread() = 0;

  const char *name() const { return m_name.c_str(); }
  virtual bool running() const { return m_thread_running; }

  // Thread control
  virtual int start();
  virtual void stop();
  virtual void run_process();
  static void cleanup();
  void signal();
  static int join(my_thread_handle *thread, void **value_ptr);
  static bool joinable(const my_thread_handle &thread);

 protected:
  int start_thread();
  void stop_thread();
  void request_thread_stop();
  void join_thread();
  bool stop_requested();
  virtual void pre_process() {}
  virtual void process() {}
  virtual void post_process() {}

  // Synchronize start, stop and update
  std::mutex m_command_lock;

  const std::string m_name;
  const std::string m_instr_name;
  const std::string m_os_name;
  const std::string m_description;
  std::atomic<bool> m_thread_running{false};
  bool m_stop{false};

  my_thread_handle m_thread;
  PSI_thread_key m_thread_key{0};
  PSI_thread_info m_thread_info{
      &m_thread_key,
      m_instr_name.c_str(),
      m_os_name.c_str(),
      PSI_FLAG_SINGLETON | PSI_FLAG_THREAD_SYSTEM,
      PSI_VOLATILITY_PROVISIONING,  // instruments are created/destroyed
      m_description.c_str()};

  mysql_mutex_t m_wait_lock;
  PSI_mutex_key m_mutex_key{0};
  PSI_mutex_info m_mutex_info{&m_mutex_key, m_instr_name.c_str(),
                              PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING,
                              PSI_DOCUMENT_ME};
  mysql_cond_t m_wait;
  PSI_cond_key m_cond_key{0};
  PSI_cond_info m_cond_info{&m_cond_key, m_instr_name.c_str(),
                            PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING,
                            PSI_DOCUMENT_ME};
};

}  // namespace hm
