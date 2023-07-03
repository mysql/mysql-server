/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CONSUMER_HPP
#define CONSUMER_HPP

#include "Restore.hpp"
#include "restore_tables.h"
#include <NdbThread.h>
#include <NdbCondition.h>

class BackupConsumer {
public:
  BackupConsumer() {}
  virtual ~BackupConsumer() { }
  virtual bool init(Uint32 tableCompabilityMask) { return true;}
  virtual bool object(Uint32 tableType, const void*) { return true;}
  virtual bool table(const TableS &){return true;}
  virtual bool fk(Uint32 tableType, const void*) { return true;}
  virtual bool endOfTables() { return true; }
  virtual bool endOfTablesFK() { return true; }
  virtual bool tuple(const TupleS &, Uint32 fragId) { return true; }
  virtual void tuple_free(){}
  virtual void endOfTuples(){}
  virtual bool logEntry(const LogEntry &) { return true; }
  virtual void endOfLogEntrys(){}
  virtual bool prepare_staging(const TableS &){return true;}
  virtual bool finalize_staging(const TableS &){return true;}
  virtual bool finalize_table(const TableS &){return true;}
  virtual bool rebuild_indexes(const TableS &) { return true;}
  virtual bool createSystable(const TableS &){ return true;}
  virtual bool update_apply_status(const RestoreMetaData &metaData, bool snapshotstart)
    {return true;}
  virtual bool delete_epoch_tuple()
    {return true;}
  virtual bool report_started(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool report_meta_data(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool report_data(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool report_log(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool report_completed(unsigned backup_id, unsigned node_id)
    {return true;}
  virtual bool isMissingTable(const TableS &){return false;}
  virtual bool has_temp_error() {return false;}
  virtual bool table_equal(const TableS &) { return true; }
  virtual bool table_compatible_check(TableS &) {return true;}
  virtual bool check_blobs(TableS &) {return true;}
  virtual bool handle_index_stat_tables() {return true;}
#ifdef ERROR_INSERT
  virtual void error_insert(unsigned int code) {}
#endif
};

/*
 * CyclicBarrier class to sync multiple threads.
 * To be used where there are N threads which we want to
 * synchronize periodically at some gating point (the barrier).
 */
class CyclicBarrier
{
private:
  NdbMutex m_mutex;
  NdbCondition m_condition;

  const Uint32 m_threads;   /* Num threads as barrier */
  Uint32 m_waiters;         /* Num threads waiting */
  Uint32 m_round;           /* Barrier round */
  bool m_cancelled;         /* Has barrier been cancelled */
public:
  /* Create a barrier, waiting for giving number of threads */
  CyclicBarrier(const Uint32 threads):
    m_threads(threads),
    m_waiters(0),
    m_round(0),
    m_cancelled(false)
  {
    assert(threads > 0);
    NdbMutex_Init(&m_mutex);
    NdbCondition_Init(&m_condition);
  }

  /* Destroy barrier */
  ~CyclicBarrier()
  {
    /* Cancel and wait for any waiters to exit */
    cancel();
    NdbMutex_Deinit(&m_mutex);
  }

  /**
   * Wait for all threads to enter barrier
   * Return true if all arrived
   * Return false if barrier cancelled
   */
  bool wait()
  {
    NdbMutex_Lock(&m_mutex);

    if (!m_cancelled)
    {
      Uint32 round = m_round;
      assert(m_waiters < m_threads);
      m_waiters ++;
      if (m_waiters == m_threads)
      {
        /* Barrier opens and re-cycles */
        m_round ++;
        m_waiters = 0;
        NdbCondition_Broadcast(&m_condition);
      }
      else
      {
        /* Not everyone here yet, wait */
        while ((round == m_round) &&
               (!m_cancelled))
        {
          NdbCondition_Wait(&m_condition,
                            &m_mutex);
        }

        if (m_cancelled)
        {
          /**
           * If we were not yet woken
           * when the barrier was cancelled
           * then account for #waiters
           * to allow safe cleanup
           */
          if (round == m_round)
          {
            assert(m_waiters > 0);
            m_waiters --;
            NdbCondition_Signal(&m_condition);
          }
        }
      }
    }
    bool normal_wake = !m_cancelled;
    NdbMutex_Unlock(&m_mutex);

    return normal_wake;
  }

  /**
   * Cancel barrier
   * Any waiters will be woken with an error
   * No further use can be made of the barrier.
   */
  void cancel()
  {
    NdbMutex_Lock(&m_mutex);
    {
      m_cancelled = true;
      NdbCondition_Broadcast(&m_condition);
      while (m_waiters > 0)
      {
        NdbCondition_Wait(&m_condition,
                          &m_mutex);
      }
    }
    NdbMutex_Unlock(&m_mutex);
  }
};

class RestoreThreadData {
public:
  Uint32 m_part_id;
  int m_result;
  bool m_restore_meta;
  NdbThread *m_thread;
  Vector<BackupConsumer*> m_consumers;
  RestoreThreadData(Uint32 part_id)
                    : m_part_id(part_id), m_result(0), m_restore_meta(false),
                      m_thread(NULL) {}
  CyclicBarrier *m_barrier;
  RestoreThreadData(Uint32 partId, CyclicBarrier *barrier): m_part_id(partId),
     m_result(0), m_restore_meta(false), m_thread(NULL), m_barrier(barrier) {}
  ~RestoreThreadData() {}
};

#endif
