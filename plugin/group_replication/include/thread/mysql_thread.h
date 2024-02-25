/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_THREAD_INCLUDE
#define MYSQL_THREAD_INCLUDE

#include <atomic>
#include "plugin/group_replication/include/plugin_psi.h"
#include "plugin/group_replication/include/plugin_server_include.h"
#include "plugin/group_replication/include/plugin_utils.h"

class THD;

/**
  @class Mysql_thread_body_parameters

  Interface for Mysql_thread_body parameters.
*/
class Mysql_thread_body_parameters {
 public:
  /*
    Allocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] size    memory size to be allocated
    @param[in] nval    When the nothrow constant is passed as second parameter
                       to operator new, operator new returns a null-pointer on
                       failure instead of throwing a bad_alloc exception.

    @return pointer to the allocated memory, or NULL if memory could not
            be allocated.
  */
  void *operator new(size_t size, const std::nothrow_t &) noexcept {
    /*
      Call my_malloc() with the MY_WME flag to make sure that it will
      write an error message if the memory could not be allocated.
    */
    return my_malloc(key_mysql_thread_queued_task, size, MYF(MY_WME));
  }

  /*
    Deallocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] ptr     pointer to the allocated memory
    @param[in] nval    When the nothrow constant is passed as second parameter
                       to operator new, operator new returns a null-pointer on
                       failure instead of throwing a bad_alloc exception.
  */
  void operator delete(void *ptr, const std::nothrow_t &) noexcept {
    my_free(ptr);
  }

  /**
    Allocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] size    memory size to be allocated

    @return pointer to the allocated memory, or NULL if memory could not
            be allocated.
  */
  void *operator new(size_t size) noexcept {
    /*
      Call my_malloc() with the MY_WME flag to make sure that it will
      write an error message if the memory could not be allocated.
    */
    return my_malloc(key_mysql_thread_queued_task, size, MYF(MY_WME));
  }

  /**
    Deallocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] ptr     pointer to the allocated memory
  */
  void operator delete(void *ptr) noexcept { my_free(ptr); }

  Mysql_thread_body_parameters() {}
  virtual ~Mysql_thread_body_parameters() {}
};

/**
  @class Mysql_thread_body

  Interface for Mysql_thread_body, the task of a Mysql_thread.
*/
class Mysql_thread_body {
 public:
  virtual ~Mysql_thread_body() {}
  virtual void run(Mysql_thread_body_parameters *parameters) = 0;
};

class Mysql_thread_task {
 public:
  /*
    Allocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] size    memory size to be allocated
    @param[in] nothrow When the nothrow constant is passed as second parameter
                       to operator new, operator new returns a null-pointer on
                       failure instead of throwing a bad_alloc exception.

    @return pointer to the allocated memory, or NULL if memory could not
            be allocated.
  */
  void *operator new(size_t size, const std::nothrow_t &) noexcept {
    /*
      Call my_malloc() with the MY_WME flag to make sure that it will
      write an error message if the memory could not be allocated.
    */
    return my_malloc(key_mysql_thread_queued_task, size, MYF(MY_WME));
  }

  /*
    Deallocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] ptr     pointer to the allocated memory
    @param[in] nothrow When the nothrow constant is passed as second parameter
                       to operator new, operator new returns a null-pointer on
                       failure instead of throwing a bad_alloc exception.
  */
  void operator delete(void *ptr, const std::nothrow_t &) noexcept {
    my_free(ptr);
  }

  /**
    Allocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] size    memory size to be allocated

    @return pointer to the allocated memory, or NULL if memory could not
            be allocated.
  */
  void *operator new(size_t size) noexcept {
    /*
      Call my_malloc() with the MY_WME flag to make sure that it will
      write an error message if the memory could not be allocated.
    */
    return my_malloc(key_mysql_thread_queued_task, size, MYF(MY_WME));
  }

  /**
    Deallocate memory on the heap with instrumented memory allocation, so
    that memory consumption can be tracked.

    @param[in] ptr     pointer to the allocated memory
  */
  void operator delete(void *ptr) noexcept { my_free(ptr); }

  Mysql_thread_task(Mysql_thread_body *body,
                    Mysql_thread_body_parameters *parameters)
      : m_body(body), m_parameters(parameters){};
  virtual ~Mysql_thread_task() {
    delete m_parameters;
    m_parameters = nullptr;
  };

  /**
    Execute task, calling body function with parameters
    */
  void execute();

  /**
    Check if the task did finish.

    @return did the task finish?
      @retval false  No
      @retval true   Yes
    */
  bool is_finished();

 private:
  // cannot be deleted, represent class where method will run
  Mysql_thread_body *m_body{nullptr};
  Mysql_thread_body_parameters *m_parameters{nullptr};
  std::atomic<bool> m_finished{false};
};

/**
  @class Mysql_thread

  A generic single thread executor.
*/
class Mysql_thread {
 public:
  /**
    Mysql_thread constructor
  */
  Mysql_thread(PSI_thread_key thread_key, PSI_mutex_key run_mutex_key,
               PSI_cond_key run_cond_key, PSI_mutex_key dispatcher_mutex_key,
               PSI_cond_key dispatcher_cond_key);
  virtual ~Mysql_thread();

  /**
    Initialize the thread.

    @return the operation status
      @retval false  Successful
      @retval true   Error
  */
  bool initialize();

  /**
    Terminate the thread.

    @return the operation status
      @retval false  Successful
      @retval true   Error
  */
  bool terminate();

  /**
    Thread worker method.
  */
  void dispatcher();

  /**
    Trigger a task to run synchronously.

    @param[in] task  task to run

    @return the operation status
      @retval false  Successful
      @retval true   Error
  */
  bool trigger(Mysql_thread_task *task);

 private:
  PSI_thread_key m_thread_key;
  PSI_mutex_key m_mutex_key;
  PSI_cond_key m_cond_key;
  PSI_mutex_key m_dispatcher_mutex_key;
  PSI_cond_key m_dispatcher_cond_key;

  THD *m_thd{nullptr};
  my_thread_handle m_pthd;
  mysql_mutex_t m_run_lock;
  mysql_cond_t m_run_cond;
  thread_state m_state;
  std::atomic<bool> m_aborted{false};

  mysql_mutex_t m_dispatcher_lock;
  mysql_cond_t m_dispatcher_cond;

  Abortable_synchronized_queue<Mysql_thread_task *> *m_trigger_queue{nullptr};
};

#endif /* MYSQL_THREAD_INCLUDE */
