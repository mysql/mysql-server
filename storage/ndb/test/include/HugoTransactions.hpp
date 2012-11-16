/*
   Copyright (C) 2003-2007 MySQL AB, 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef HUGO_TRANSACTIONS_HPP
#define HUGO_TRANSACTIONS_HPP


#include <NDBT.hpp>
#include <HugoCalculator.hpp>
#include <HugoOperations.hpp>
class NDBT_Stats;

class HugoTransactions : public HugoOperations {
public:
  struct HugoBound { int attr; int type; const void* value; };

  HugoTransactions(const NdbDictionary::Table&,
		   const NdbDictionary::Index* idx = 0);
  ~HugoTransactions();
  int loadTable(Ndb*, 
		int records,
		int batch = 512,
		bool allowConstraintViolation = true,
		int doSleep = 0,
                bool oneTrans = false,
		int updateValue = 0,
		bool abort = false);

  int loadTableStartFrom(Ndb*, 
                         int startFrom,
                         int records,
                         int batch = 512,
                         bool allowConstraintViolation = true,
                         int doSleep = 0,
                         bool oneTrans = false,
                         int updateValue = 0,
                         bool abort = false);

  int scanReadRecords(Ndb*, 
		      int records,
		      int abort = 0,
		      int parallelism = 0,
		      NdbOperation::LockMode = NdbOperation::LM_Read,
                      int scan_flags = 0);

  int scanReadRecords(Ndb*, 
		      const NdbDictionary::Index*,
		      int records,
		      int abort = 0,
		      int parallelism = 0,
		      NdbOperation::LockMode = NdbOperation::LM_Read,
                      int scan_flags = 0,
                      int bound_cnt = 0, const HugoBound* bound_arr = 0);

  int pkReadRecords(Ndb*, 
		    int records,
		    int batchsize = 1,
		    NdbOperation::LockMode = NdbOperation::LM_Read,
                    int rand = 0);
  
  int pkReadUnlockRecords(Ndb*,
                          int records,
                          int batchsize = 1,
                          NdbOperation::LockMode = NdbOperation::LM_Read);

  int scanUpdateRecords(Ndb*, NdbScanOperation::ScanFlag, 
			int records,
			int abort = 0,
			int parallelism = 0);

  int scanUpdateRecords(Ndb*, 
			int records,
			int abort = 0,
			int parallelism = 0);

  int scanUpdateRecords1(Ndb*, 
			 int records,
			 int abort = 0,
			 int parallelism = 0);
  int scanUpdateRecords2(Ndb*, 
			 int records,
			 int abort = 0,
			 int parallelism = 0);
  int scanUpdateRecords3(Ndb*, 
			 int records,
			 int abort = 0,
			 int parallelism = 0);

  int pkUpdateRecords(Ndb*, 
		      int records,
		      int batchsize = 1,
		      int doSleep = 0);
  int pkInterpretedUpdateRecords(Ndb*, 
				 int records,
				 int batchsize = 1);
  int pkDelRecords(Ndb*, 
		   int records = 0,
		   int batch = 1,
		   bool allowConstraintViolation = true,
		   int doSleep = 0);

  int pkRefreshRecords(Ndb*, int startFrom, int count = 1, int batch = 1);

  int lockRecords(Ndb*,
		  int records,
		  int percentToLock = 1,
		  int lockTime = 1000);

  int fillTable(Ndb*,
		int batch=512);

  int fillTableStartFrom(Ndb*, int startFrom, int batch=512);

  /**
   * Reading using UniqHashIndex with key = pk
   */
  int indexReadRecords(Ndb*, 
		       const char * idxName,
		       int records,
		       int batchsize = 1);

  int indexUpdateRecords(Ndb*,
			 const char * idxName,
			 int records,
			 int batchsize = 1);

  void setRetryMax(int retryMax = 100) { m_retryMax = retryMax; }
  
  Uint64 m_latest_gci;

  void setStatsLatency(NDBT_Stats* stats) { m_stats_latency = stats; }

  // allows multiple threads to update separate batches
  void setThrInfo(int thr_count, int thr_no) {
    m_thr_count = thr_count;
    m_thr_no = thr_no;
  }

protected:  
  NDBT_ResultRow row;
  int m_defaultScanUpdateMethod;
  int m_retryMax;

  NDBT_Stats* m_stats_latency;

  int m_thr_count;      // 0 if no separation between threads
  int m_thr_no;
};




#endif

