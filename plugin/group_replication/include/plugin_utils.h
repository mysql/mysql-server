/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#ifndef PLUGIN_UTILS_INCLUDED
#define PLUGIN_UTILS_INCLUDED

#include <errno.h>
#include <mysql/group_replication_priv.h>
#include <stddef.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <queue>
#include <random>
#include <string>
#include <vector>

#include "my_dbug.h"
#include "my_systime.h"
#include "plugin/group_replication/include/plugin_psi.h"
#include "sql/malloc_allocator.h"
#include "string_with_len.h"

void log_primary_member_details();

void abort_plugin_process(const char *message);

struct thread_state {
  /**
   * @enum  thread_state_enum
   * @brief Maintains thread status
   */
  enum thread_state_enum {
    THREAD_NONE = 0, /**< THREAD_NOT_CREATED */
    THREAD_CREATED,  /**< THREAD_CREATED */
    THREAD_INIT,     /**< THREAD_INIT */

    THREAD_RUNNING, /**< THREAD_RUNNING */

    THREAD_TERMINATED, /**< THREAD_EXIT */
    THREAD_END         /**< END OF ENUM */
  };

 private:
  thread_state_enum thread_state_var;

 public:
  thread_state() : thread_state_var(thread_state_enum::THREAD_NONE) {}

  void set_running() { thread_state_var = thread_state_enum::THREAD_RUNNING; }

  void set_terminated() {
    thread_state_var = thread_state_enum::THREAD_TERMINATED;
  }

  void set_initialized() { thread_state_var = thread_state_enum::THREAD_INIT; }

  void set_created() { thread_state_var = thread_state_enum::THREAD_CREATED; }

  bool is_initialized() const {
    return ((thread_state_var >= thread_state_enum::THREAD_INIT) &&
            (thread_state_var < thread_state_enum::THREAD_TERMINATED));
  }

  bool is_running() const {
    return thread_state_var == thread_state_enum::THREAD_RUNNING;
  }

  bool is_alive_not_running() const {
    return thread_state_var < thread_state_enum::THREAD_RUNNING;
  }

  bool is_thread_alive() const {
    return ((thread_state_var >= thread_state_enum::THREAD_CREATED) &&
            (thread_state_var < thread_state_enum::THREAD_TERMINATED));
  }

  bool is_thread_dead() const { return !is_thread_alive(); }
};

class Blocked_transaction_handler {
 public:
  Blocked_transaction_handler();
  virtual ~Blocked_transaction_handler();

  /**
    This method instructs all local transactions to rollback when certification
    is no longer possible.
  */
  void unblock_waiting_transactions();

 private:
  /* The lock that disallows concurrent method executions */
  mysql_mutex_t unblocking_process_lock;
};

/**
 @class Synchronized_queue_interface

 Interface that defines a queue protected against multi thread access.

 */

template <typename T>
class Synchronized_queue_interface {
 public:
  virtual ~Synchronized_queue_interface() = default;

  /**
    Checks if the queue is empty
    @return if is empty
      @retval true  empty
      @retval false not empty
  */
  virtual bool empty() = 0;

  /**
    Inserts an element in the queue.
    Alerts any other thread lock on pop() or front()
    @param value The value to insert

    @return  false, operation always succeeded
   */
  virtual bool push(const T &value) = 0;

  /**
    Fetches the front of the queue and removes it.
    @note The method will block if the queue is empty until a element is pushed

    @param out  The fetched reference.

    @return  false, operation always succeeded
  */
  virtual bool pop(T *out) = 0;

  /**
    Pops the front of the queue removing it.
    @note The method will block if the queue is empty until a element is pushed

    @return  true if method was aborted, false otherwise
  */
  virtual bool pop() = 0;

  /**
    Fetches the front of the queue but does not remove it.
    @note The method will block if the queue is empty until a element is pushed

    @param out  The fetched reference.

    @return  false, operation always succeeded
  */
  virtual bool front(T *out) = 0;

  /**
    Checks the queue size
    @return the size of the queue
  */
  virtual size_t size() = 0;
};

template <typename T>
class Synchronized_queue : public Synchronized_queue_interface<T> {
 public:
  Synchronized_queue(PSI_memory_key key) : queue(Malloc_allocator<T>(key)) {
    mysql_mutex_init(key_GR_LOCK_synchronized_queue, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_GR_COND_synchronized_queue, &cond);
  }

  ~Synchronized_queue() override { mysql_mutex_destroy(&lock); }

  bool empty() override {
    bool res = true;
    mysql_mutex_lock(&lock);
    res = queue.empty();
    mysql_mutex_unlock(&lock);

    return res;
  }

  bool push(const T &value) override {
    mysql_mutex_lock(&lock);
    queue.push(value);
    mysql_cond_broadcast(&cond);
    mysql_mutex_unlock(&lock);

    return false;
  }

  bool pop(T *out) override {
    *out = nullptr;
    mysql_mutex_lock(&lock);
    while (queue.empty()) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&cond, &lock, &abstime);
    }
    *out = queue.front();
    queue.pop();
    mysql_mutex_unlock(&lock);

    return false;
  }

  bool pop() override {
    mysql_mutex_lock(&lock);
    while (queue.empty()) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&cond, &lock, &abstime);
    }
    queue.pop();
    mysql_mutex_unlock(&lock);

    return false;
  }

  bool front(T *out) override {
    *out = nullptr;
    mysql_mutex_lock(&lock);
    while (queue.empty()) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&cond, &lock, &abstime);
    }
    *out = queue.front();
    mysql_mutex_unlock(&lock);

    return false;
  }

  size_t size() override {
    size_t qsize = 0;
    mysql_mutex_lock(&lock);
    qsize = queue.size();
    mysql_mutex_unlock(&lock);

    return qsize;
  }

 protected:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  std::queue<T, std::list<T, Malloc_allocator<T>>> queue;
};

/**
 Abortable synchronized queue extends synchronized queue allowing to
 abort methods waiting for elements on queue.
*/

template <typename T>
class Abortable_synchronized_queue : public Synchronized_queue<T> {
 public:
  Abortable_synchronized_queue(PSI_memory_key key)
      : Synchronized_queue<T>(key), m_abort(false) {}

  ~Abortable_synchronized_queue() override = default;

  /**
    Inserts an element in the queue.
    Alerts any other thread lock on pop() or front()
    @note The method will not push if abort was executed.

    @param value The value to insert

    @return  false, operation always succeeded
   */

  bool push(const T &value) override {
    bool res = false;
    mysql_mutex_lock(&this->lock);
    if (m_abort) {
      res = true;
    } else {
      this->queue.push(value);
      mysql_cond_broadcast(&this->cond);
    }

    mysql_mutex_unlock(&this->lock);
    return res;
  }

  /**
    Fetches the front of the queue and removes it.
    @note The method will block if the queue is empty until a element is pushed
    or abort is executed

    @param out  The fetched reference.

    @return  true if method was aborted, false otherwise
  */
  bool pop(T *out) override {
    *out = nullptr;
    mysql_mutex_lock(&this->lock);
    while (this->queue.empty() && !m_abort) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&this->cond, &this->lock, &abstime);
    }

    if (!m_abort) {
      *out = this->queue.front();
      this->queue.pop();
    }

    const bool result = m_abort;
    mysql_mutex_unlock(&this->lock);
    return result;
  }

  /**
    Pops the front of the queue removing it.
    @note The method will block if the queue is empty until a element is pushed
    or abort is executed

    @return  false, operation always succeeded
  */
  bool pop() override {
    mysql_mutex_lock(&this->lock);
    while (this->queue.empty() && !m_abort) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&this->cond, &this->lock, &abstime);
    }

    if (!m_abort) {
      this->queue.pop();
    }

    const bool result = m_abort;
    mysql_mutex_unlock(&this->lock);
    return result;
  }

  /**
    Fetches the front of the queue but does not remove it.
    @note The method will block if the queue is empty until a element is pushed
    or abort is executed

    @param out  The fetched reference.

    @return  true if method was aborted, false otherwise
  */
  bool front(T *out) override {
    *out = nullptr;
    mysql_mutex_lock(&this->lock);
    while (this->queue.empty() && !m_abort) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&this->cond, &this->lock, &abstime);
    }

    if (!m_abort) {
      *out = this->queue.front();
    }

    const bool result = m_abort;
    mysql_mutex_unlock(&this->lock);
    return result;
  }

  /**
   Remove all elements, abort current and future waits on retrieving elements
   from queue.

   @param delete_elements When true, apart from emptying the queue, it also
                          delete each element.
                          When false, the delete (memory release) responsibility
                          belongs to the `push()` caller.
  */
  void abort(bool delete_elements) {
    mysql_mutex_lock(&this->lock);
    while (this->queue.size()) {
      T elem;
      elem = this->queue.front();
      this->queue.pop();
      if (delete_elements) {
        delete elem;
      }
    }
    m_abort = true;
    mysql_cond_broadcast(&this->cond);
    mysql_mutex_unlock(&this->lock);
  }

 private:
  bool m_abort;
};

/**
  Synchronization auxiliary class that allows one or more threads
  to wait on a given number of requirements.

  Usage:
    CountDownLatch(count):
      Create the latch with the number of requirements to wait.
    wait():
      Block until the number of requirements reaches zero.
    countDown():
      Decrease the number of requirements by one.
*/
class CountDownLatch {
 public:
  /**
    Create the latch with the number of requirements to wait.

    @param       count     The number of requirements to wait
  */
  CountDownLatch(uint count) : count(count), error(false) {
    mysql_mutex_init(key_GR_LOCK_count_down_latch, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_GR_COND_count_down_latch, &cond);
  }

  virtual ~CountDownLatch() {
    mysql_cond_destroy(&cond);
    mysql_mutex_destroy(&lock);
  }

  /**
    Block until the number of requirements reaches zero.
  */
  void wait(ulong timeout = 0) {
    mysql_mutex_lock(&lock);

    if (timeout > 0) {
      ulong time_lapsed = 0;
      struct timespec abstime;

      while (count > 0 && timeout > time_lapsed) {
        set_timespec(&abstime, 1);
        mysql_cond_timedwait(&cond, &lock, &abstime);
        time_lapsed++;
      }

      if (count > 0 && timeout == time_lapsed) {
        error = true;
      }
    } else {
      while (count > 0) {
        struct timespec abstime;
        set_timespec(&abstime, 1);
        mysql_cond_timedwait(&cond, &lock, &abstime);
      }
    }

    mysql_mutex_unlock(&lock);
  }

  /**
    Decrease the number of requirements by one.
  */
  void countDown() {
    mysql_mutex_lock(&lock);
    --count;
    if (count == 0) mysql_cond_broadcast(&cond);
    mysql_mutex_unlock(&lock);
  }

  /**
    Get current number requirements.

    @return      the number of requirements
  */
  uint getCount() {
    uint res = 0;
    mysql_mutex_lock(&lock);
    res = count;
    mysql_mutex_unlock(&lock);
    return res;
  }

  /**
    Set error flag, once this latch is release the waiter can check
    if it was due to a error or due to correct termination.
  */
  void set_error() { error = true; }

  /**
    Get latch release reason.

    @return  true   the latch was released due to a error
             false  the latch was released on correct termination
  */
  bool get_error() { return error; }

 private:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  int count;
  bool error;
};

/**
  Ticket register/wait auxiliary class.
  Usage:
    registerTicket(k):
      create a ticket with key k with status ongoing.
    releaseTicket(k):
      set ticket with key k status to done.
    waitTicket(k):
      wait until ticket with key k status is changed to done.
*/
template <typename K>
class Wait_ticket {
 public:
  Wait_ticket() : blocked(false), waiting(false) {
    mysql_mutex_init(key_GR_LOCK_wait_ticket, &lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_GR_COND_wait_ticket, &cond);
  }

  virtual ~Wait_ticket() {
    clear();
    mysql_cond_destroy(&cond);
    mysql_mutex_destroy(&lock);
  }

  void clear() {
    mysql_mutex_lock(&lock);
    assert(false == blocked);
    assert(false == waiting);

    for (typename std::map<K, CountDownLatch *>::iterator it = map.begin();
         it != map.end(); ++it)
      delete it->second; /* purecov: inspected */
    map.clear();
    mysql_mutex_unlock(&lock);
  }

  /**
    Check if there are waiting tickets.

    @retval true    empty
    @retval false   otherwise
  */
  bool empty() {
    bool result = false;

    mysql_mutex_lock(&lock);
    result = map.empty();
    mysql_mutex_unlock(&lock);

    return result;
  }

  /**
    Register ticker with status ongoing.

    @param       key     The key that identifies the ticket
    @retval 0       success
    @retval !=0     key already exists, error on insert or it is blocked
  */
  int registerTicket(const K &key) {
    int error = 0;

    mysql_mutex_lock(&lock);

    if (blocked) {
      mysql_mutex_unlock(&lock); /* purecov: inspected */
      return 1;                  /* purecov: inspected */
    }

    typename std::map<K, CountDownLatch *>::iterator it = map.find(key);
    if (it != map.end()) {
      mysql_mutex_unlock(&lock); /* purecov: inspected */
      return 1;                  /* purecov: inspected */
    }

    CountDownLatch *cdl = new CountDownLatch(1);
    std::pair<typename std::map<K, CountDownLatch *>::iterator, bool> ret;
    ret = map.insert(std::pair<K, CountDownLatch *>(key, cdl));
    if (ret.second == false) {
      error = 1;  /* purecov: inspected */
      delete cdl; /* purecov: inspected */
    }

    mysql_mutex_unlock(&lock);
    return error;
  }

  /**
   Wait until ticket status is done.
   @note The ticket is removed after the wait.

    @param       key       The key that identifies the ticket
    @param       timeout   maximum time in seconds to wait
                           by default is 0, which means no timeout
    @retval 0         success
    @retval !=0       key doesn't exist, or the Ticket is blocked
  */
  int waitTicket(const K &key, ulong timeout = 0) {
    int error = 0;
    CountDownLatch *cdl = nullptr;

    mysql_mutex_lock(&lock);

    if (blocked) {
      mysql_mutex_unlock(&lock); /* purecov: inspected */
      return 1;                  /* purecov: inspected */
    }

    typename std::map<K, CountDownLatch *>::iterator it = map.find(key);
    if (it == map.end())
      error = 1;
    else
      cdl = it->second;
    mysql_mutex_unlock(&lock);

    if (cdl != nullptr) {
      cdl->wait(timeout);
      error = cdl->get_error() ? 1 : 0;

      mysql_mutex_lock(&lock);
      delete cdl;
      map.erase(it);

      if (waiting) {
        if (map.empty()) {
          mysql_cond_broadcast(&cond);
        }
      }
      mysql_mutex_unlock(&lock);
    }

    return error;
  }

  /**
   Set ticket status to done.

    @param       key                   The key that identifies the ticket
    @param       release_due_to_error  Inform the thread waiting that the
                                        release is due to a error
    @retval 0         success
    @retval !=0       (key doesn't exist)
  */
  int releaseTicket(const K &key, bool release_due_to_error = false) {
    int error = 0;

    mysql_mutex_lock(&lock);
    typename std::map<K, CountDownLatch *>::iterator it = map.find(key);
    if (it == map.end())
      error = 1;
    else {
      if (release_due_to_error) {
        it->second->set_error();
      }
      it->second->countDown();
    }
    mysql_mutex_unlock(&lock);

    return error;
  }

  /**
    Gets all the waiting keys.

    @param[out] key_list  all the keys to return
  */
  void get_all_waiting_keys(std::vector<K> &key_list) {
    mysql_mutex_lock(&lock);
    for (typename std::map<K, CountDownLatch *>::iterator iter = map.begin();
         iter != map.end(); ++iter) {
      K key = iter->first;
      key_list.push_back(key);
    }
    mysql_mutex_unlock(&lock);
  }

  /**
    Blocks or unblocks the class from receiving waiting requests.

    @param[in] blocked_flag  if the class should block or not
  */
  void set_blocked_status(bool blocked_flag) {
    mysql_mutex_lock(&lock);
    blocked = blocked_flag;
    mysql_mutex_unlock(&lock);
  }

  int block_until_empty(int timeout) {
    mysql_mutex_lock(&lock);
    waiting = true;
    while (!map.empty()) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
#ifndef NDEBUG
      int error =
#endif
          mysql_cond_timedwait(&cond, &lock, &abstime);
      assert(error == ETIMEDOUT || error == 0);
      if (timeout >= 1) {
        timeout = timeout - 1;
      } else if (!map.empty()) {
        // time out
        waiting = false;
        mysql_mutex_unlock(&lock);
        return 1;
      }
    }
    waiting = false;
    mysql_mutex_unlock(&lock);
    return 0;
  }

 private:
  mysql_mutex_t lock;
  mysql_cond_t cond;
  std::map<K, CountDownLatch *> map;
  bool blocked;
  bool waiting;
};

class Shared_writelock {
 public:
  Shared_writelock(Checkable_rwlock *arg)
      : shared_write_lock(arg), write_lock_in_use(false) {
    DBUG_TRACE;

    assert(arg != nullptr);

    mysql_mutex_init(key_GR_LOCK_write_lock_protection, &write_lock,
                     MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_GR_COND_write_lock_protection, &write_lock_protection);

    return;
  }

  virtual ~Shared_writelock() {
    mysql_mutex_destroy(&write_lock);
    mysql_cond_destroy(&write_lock_protection);
  }

  int try_grab_write_lock() {
    int res = 0;
    mysql_mutex_lock(&write_lock);

    if (write_lock_in_use)
      res = 1; /* purecov: inspected */
    else {
      shared_write_lock->wrlock();
      write_lock_in_use = true;
    }

    mysql_mutex_unlock(&write_lock);
    return res;
  }

  void grab_write_lock() {
    mysql_mutex_lock(&write_lock);
    DBUG_EXECUTE_IF("group_replication_continue_kill_pending_transaction", {
      const char act[] = "now SIGNAL signal.gr_applier_early_failure";
      assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
    };);
    while (write_lock_in_use == true) {
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(&write_lock_protection, &write_lock, &abstime);
    }

    shared_write_lock->wrlock();
    write_lock_in_use = true;
    mysql_mutex_unlock(&write_lock);
  }

  void release_write_lock() {
    mysql_mutex_lock(&write_lock);
    shared_write_lock->unlock();
    write_lock_in_use = false;
    mysql_cond_broadcast(&write_lock_protection);
    mysql_mutex_unlock(&write_lock);
  }

  /**
    Grab a read lock only if there is no write lock acquired.

    @retval 0         read lock acquired
    @retval !=0       there is a write lock acquired
  */
  int try_grab_read_lock() {
    int res = 0;
    mysql_mutex_lock(&write_lock);

    if (write_lock_in_use)
      res = 1;
    else
      shared_write_lock->rdlock();

    mysql_mutex_unlock(&write_lock);
    return res;
  }

  void grab_read_lock() { shared_write_lock->rdlock(); }

  void release_read_lock() { shared_write_lock->unlock(); }

 private:
  Checkable_rwlock *shared_write_lock;
  mysql_mutex_t write_lock;
  mysql_cond_t write_lock_protection;
  bool write_lock_in_use;
};

class Plugin_waitlock {
 public:
  /**
    Constructor.
    Instantiate the mutex lock, mutex condition,
    mutex and condition key.

    @param  lock  the mutex lock for access to class and condition variables
    @param  cond  the condition variable calling thread will wait on
    @param  lock_key mutex instrumentation key
    @param  cond_key cond instrumentation key
  */
  Plugin_waitlock(mysql_mutex_t *lock, mysql_cond_t *cond,
                  PSI_mutex_key lock_key, PSI_cond_key cond_key)
      : wait_lock(lock),
        wait_cond(cond),
        key_lock(lock_key),
        key_cond(cond_key),
        wait_status(false) {
    DBUG_TRACE;

    mysql_mutex_init(key_lock, wait_lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_cond, wait_cond);

    return;
  }

  /**
    Destructor.
    Destroys the mutex and condition objects.
  */
  virtual ~Plugin_waitlock() {
    mysql_mutex_destroy(wait_lock);
    mysql_cond_destroy(wait_cond);
  }

  /**
    Set condition to block or unblock the calling threads

    @param[in] status  if the thread should be blocked or not
  */
  void set_wait_lock(bool status) {
    mysql_mutex_lock(wait_lock);
    wait_status = status;
    mysql_mutex_unlock(wait_lock);
  }

  /**
    Blocks the calling thread
  */
  void start_waitlock() {
    DBUG_TRACE;
    mysql_mutex_lock(wait_lock);
    while (wait_status) {
      DBUG_PRINT("sleep", ("Waiting in Plugin_waitlock::start_waitlock()"));
      struct timespec abstime;
      set_timespec(&abstime, 1);
      mysql_cond_timedwait(wait_cond, wait_lock, &abstime);
    }
    mysql_mutex_unlock(wait_lock);
    return;
  }

  /**
    Release the blocked thread
  */
  void end_wait_lock() {
    mysql_mutex_lock(wait_lock);
    wait_status = false;
    mysql_cond_broadcast(wait_cond);
    mysql_mutex_unlock(wait_lock);
  }

  /**
    Checks whether thread should be blocked

    @retval true      thread should be blocked
    @retval false     thread should not be blocked
  */
  bool is_waiting() {
    mysql_mutex_lock(wait_lock);
    bool result = wait_status;
    mysql_mutex_unlock(wait_lock);
    return result;
  }

 private:
  /** the mutex lock for access to class and condition variables */
  mysql_mutex_t *wait_lock;
  /** the condition variable calling thread will wait on */
  mysql_cond_t *wait_cond;
  /** mutex instrumentation key */
  PSI_mutex_key key_lock;
  /** cond instrumentation key */
  PSI_cond_key key_cond;
  /** determine whether calling thread should be blocked or not */
  bool wait_status;
};

/**
  Simple method to escape character on a string

  @note based on escape_string_for_mysql
  @note the result is stored in the parameter string

  @param[in,out] string_to_escape the string to escape
*/
void plugin_escape_string(std::string &string_to_escape);

/**
  Rearranges the given vector elements randomly.
  @param[in,out] v the vector to shuffle
*/
template <typename T>
void vector_random_shuffle(std::vector<T, Malloc_allocator<T>> *v) {
  auto seed{std::chrono::system_clock::now().time_since_epoch().count()};
  std::shuffle(v->begin(), v->end(),
               std::default_random_engine(
                   static_cast<std::default_random_engine::result_type>(seed)));
}

#endif /* PLUGIN_UTILS_INCLUDED */
