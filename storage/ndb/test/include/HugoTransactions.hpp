/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef HUGO_TRANSACTIONS_HPP
#define HUGO_TRANSACTIONS_HPP

#include <HugoCalculator.hpp>
#include <HugoOperations.hpp>
#include <NDBT.hpp>
class NDBT_Stats;

class HugoTransactions : public HugoOperations {
 public:
  struct HugoBound {
    int attr;
    int type;
    const void *value;
  };

  HugoTransactions(const NdbDictionary::Table &,
                   const NdbDictionary::Index *idx = 0);
  ~HugoTransactions();
  int loadTable(Ndb *, int records, int batch = 512,
                bool allowConstraintViolation = true, int doSleep = 0,
                bool oneTrans = false, int updateValue = 0, bool abort = false,
                bool abort_on_first_error = false, int row_step = 1);

  int loadTableStartFrom(Ndb *, int startFrom, int records, int batch = 512,
                         bool allowConstraintViolation = true, int doSleep = 0,
                         bool oneTrans = false, int updateValue = 0,
                         bool abort = false, bool abort_on_first_error = false,
                         int row_step = 1);

  int scanReadRecords(Ndb *, int records, int abort = 0, int parallelism = 0,
                      NdbOperation::LockMode = NdbOperation::LM_Read,
                      int scan_flags = 0, int force_check_flag = 0);

  int scanReadRecords(Ndb *, const NdbDictionary::Index *, int records,
                      int abort = 0, int parallelism = 0,
                      NdbOperation::LockMode = NdbOperation::LM_Read,
                      int scan_flags = 0, int bound_cnt = 0,
                      const HugoBound *bound_arr = 0);

  int pkReadRecords(Ndb *, int records, int batchsize = 1,
                    NdbOperation::LockMode = NdbOperation::LM_Read,
                    int rand = 0);

  int pkReadUnlockRecords(Ndb *, int records, int batchsize = 1,
                          NdbOperation::LockMode = NdbOperation::LM_Read);

  int scanUpdateRecords(Ndb *, NdbScanOperation::ScanFlag, int records,
                        int abort = 0, int parallelism = 0);

  int scanUpdateRecords(Ndb *, int records, int abort = 0, int parallelism = 0);

  int scanUpdateRecords1(Ndb *, int records, int abort = 0,
                         int parallelism = 0);
  int scanUpdateRecords2(Ndb *, int records, int abort = 0,
                         int parallelism = 0);
  int scanUpdateRecords3(Ndb *, int records, int abort = 0,
                         int parallelism = 0);

  int pkUpdateRecords(Ndb *, int records, int batchsize = 1, int doSleep = 0);
  int pkInterpretedUpdateRecords(Ndb *, int records, int batchsize = 1);
  int pkDelRecords(Ndb *, int records = 0, int batch = 1,
                   bool allowConstraintViolation = true, int doSleep = 0,
                   int start_record = 0, int step = 1);

  int pkRefreshRecords(Ndb *, int startFrom, int count = 1, int batch = 1);

  int lockRecords(Ndb *, int records, int percentToLock = 1,
                  int lockTime = 1000);

  int fillTable(Ndb *, int batch = 512);

  int fillTableStartFrom(Ndb *, int startFrom, int batch = 512);

  /**
   * Reading using UniqHashIndex with key = pk
   */
  int indexReadRecords(Ndb *, const char *idxName, int records,
                       int batchsize = 1);

  int indexUpdateRecords(Ndb *, const char *idxName, int records,
                         int batchsize = 1);

  void setRetryMax(int retryMax = 100) { m_retryMax = retryMax; }
  // XXX only for scanUpdateRecords
  bool getRetryMaxReached() const { return m_retryMaxReached; }

  Uint64 m_latest_gci;
  Uint32 get_high_latest_gci() { return Uint32(Uint64(m_latest_gci >> 32)); }
  Uint32 get_low_latest_gci() {
    return Uint32(Uint64(m_latest_gci & 0xFFFFFFFF));
  }

  void setStatsLatency(NDBT_Stats *stats) { m_stats_latency = stats; }

  // allows multiple threads to update separate batches
  void setThrInfo(int thr_count, int thr_no) {
    m_thr_count = thr_count;
    m_thr_no = thr_no;
  }

  // generate empty updates for testing
  void setAllowEmptyUpdates(bool allow) { m_empty_update = allow; }

 protected:
  NDBT_ResultRow row;
  int m_defaultScanUpdateMethod;
  int m_retryMax;
  bool m_retryMaxReached;

  NDBT_Stats *m_stats_latency;

  int m_thr_count;  // 0 if no separation between threads
  int m_thr_no;

  bool m_empty_update;
};

#endif
