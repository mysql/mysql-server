/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_STAGE_MANAGER
#define RPL_STAGE_MANAGER

#include <atomic>
#include <utility>

#include "my_dbug.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "sql/sql_class.h"
#include "thr_mutex.h"

class THD;


/**
  Class for maintaining the commit stages for binary log group commit.
 */
class Stage_manager {
 public:
  class Mutex_queue {
    friend class Stage_manager;

   public:
    Mutex_queue() : m_first(NULL), m_last(&m_first), m_size(0) {}

    void init(PSI_mutex_key key_LOCK_queue) {
      mysql_mutex_init(key_LOCK_queue, &m_lock, MY_MUTEX_INIT_FAST);
    }

    void deinit() { mysql_mutex_destroy(&m_lock); }

    bool is_empty() const { return m_first == NULL; }

    /**
      Append a linked list of threads to the queue.
      @retval true The queue was empty before this operation.
      @retval false The queue was non-empty before this operation.
    */
    bool append(THD *first);

    /**
       Fetch the entire queue for a stage.

       This will fetch the entire queue in one go.
    */
    THD *fetch_and_empty();

    std::pair<bool, THD *> pop_front();

    inline int32 get_size() { return m_size.load(); }

   private:
    void lock() { mysql_mutex_lock(&m_lock); }
    void unlock() { mysql_mutex_unlock(&m_lock); }

    /**
       Pointer to the first thread in the queue, or NULL if the queue is
       empty.
    */
    THD *m_first;

    /**
       Pointer to the location holding the end of the queue.

       This is either @c &first, or a pointer to the @c next_to_commit of
       the last thread that is enqueued.
    */
    THD **m_last;

    /** size of the queue */
    std::atomic<int32> m_size;

    /** Lock for protecting the queue. */
    mysql_mutex_t m_lock;

    /*
      This attribute did not have the desired effect, at least not according
      to -fsanitize=undefined with gcc 5.2.1
     */
  };  // MY_ATTRIBUTE((aligned(CPU_LEVEL1_DCACHE_LINESIZE)));

 public:
  Stage_manager() {}

  ~Stage_manager() {}

  /**
     Constants for queues for different stages.
   */
  enum StageID { FLUSH_STAGE, SYNC_STAGE, COMMIT_STAGE, STAGE_COUNTER };

  void init(PSI_mutex_key key_LOCK_flush_queue,
            PSI_mutex_key key_LOCK_sync_queue,
            PSI_mutex_key key_LOCK_commit_queue, PSI_mutex_key key_LOCK_done,
            PSI_cond_key key_COND_done) {
    mysql_mutex_init(key_LOCK_done, &m_lock_done, MY_MUTEX_INIT_FAST);
    mysql_cond_init(key_COND_done, &m_cond_done);
#ifndef DBUG_OFF
    /* reuse key_COND_done 'cos a new PSI object would be wasteful in !DBUG_OFF
     */
    mysql_cond_init(key_COND_done, &m_cond_preempt);
#endif
    m_queue[FLUSH_STAGE].init(key_LOCK_flush_queue);
    m_queue[SYNC_STAGE].init(key_LOCK_sync_queue);
    m_queue[COMMIT_STAGE].init(key_LOCK_commit_queue);
  }

  void deinit() {
    for (size_t i = 0; i < STAGE_COUNTER; ++i) m_queue[i].deinit();
    mysql_cond_destroy(&m_cond_done);
    mysql_mutex_destroy(&m_lock_done);
  }

  /**
    Enroll a set of sessions for a stage.

    This will queue the session thread for writing and flushing.

    If the thread being queued is assigned as stage leader, it will
    return immediately.

    If wait_if_follower is true the thread is not the stage leader,
    the thread will be wait for the queue to be processed by the
    leader before it returns.
    In DBUG-ON version the follower marks is preempt status as ready.

    @param stage Stage identifier for the queue to append to.
    @param first Queue to append.
    @param stage_mutex
                 Pointer to the currently held stage mutex, or NULL if
                 we're not in a stage.

    @retval true  Thread is stage leader.
    @retval false Thread was not stage leader and processing has been done.
   */
  bool enroll_for(StageID stage, THD *first, mysql_mutex_t *stage_mutex);

  std::pair<bool, THD *> pop_front(StageID stage) {
    return m_queue[stage].pop_front();
  }

#ifndef DBUG_OFF
  /**
     The method ensures the follower's execution path can be preempted
     by the leader's thread.
     Preempt status of @c head follower is checked to engange the leader
     into waiting when set.

     @param head  THD* of a follower thread
  */
  void clear_preempt_status(THD *head);
#endif

  /**
    Fetch the entire queue and empty it.

    @return Pointer to the first session of the queue.
   */
  THD *fetch_queue_for(StageID stage) {
    DBUG_PRINT("debug", ("Fetching queue for stage %d", stage));
    return m_queue[stage].fetch_and_empty();
  }

  /**
    Introduces a wait operation on the executing thread.  The
    waiting is done until the timeout elapses or count is
    reached (whichever comes first).

    If count == 0, then the session will wait until the timeout
    elapses. If timeout == 0, then there is no waiting.

    @param usec     the number of microseconds to wait.
    @param count    wait for as many as count to join the queue the
                    session is waiting on
    @param stage    which stage queue size to compare count against.
   */
  void wait_count_or_timeout(ulong count, long usec, StageID stage);

  void signal_done(THD *queue);

 private:
  /**
     Queues for sessions.

     We need two queues:
     - Waiting. Threads waiting to be processed
     - Committing. Threads waiting to be committed.
   */
  Mutex_queue m_queue[STAGE_COUNTER];

  /** Condition variable to indicate that the commit was processed */
  mysql_cond_t m_cond_done;

  /** Mutex used for the condition variable above */
  mysql_mutex_t m_lock_done;
#ifndef DBUG_OFF
  /** Flag is set by Leader when it starts waiting for follower's all-clear */
  bool leader_await_preempt_status;

  /** Condition variable to indicate a follower started waiting for commit */
  mysql_cond_t m_cond_preempt;
#endif
};

#endif /*RPL_STAGE_MANAGER*/
