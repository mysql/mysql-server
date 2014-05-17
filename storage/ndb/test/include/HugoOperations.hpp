/*
   Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef HUGO_OPERATIONS_HPP
#define HUGO_OPERATIONS_HPP

#include <NDBT.hpp>
#include <HugoCalculator.hpp>
#include <UtilTransactions.hpp>
#include <Vector.hpp>

class HugoOperations : public UtilTransactions {
public:  
  HugoOperations(const NdbDictionary::Table&,
		 const NdbDictionary::Index* idx = 0);

  ~HugoOperations();
  int startTransaction(Ndb*, const NdbDictionary::Table *table= 0,
                       const char  *keyData= 0, Uint32 keyLen= 0);
  int startTransaction(Ndb*, Uint32 node_id, Uint32 instance_id);
  int setTransaction(NdbTransaction*,bool not_null_ok= false);
  int closeTransaction(Ndb*);
  NdbTransaction* getTransaction();
  void refresh();

  void setTransactionId(Uint64);
  
  int pkInsertRecord(Ndb*,
		     int recordNo,
		     int numRecords = 1,
		     int updatesValue = 0);
  
  int pkWriteRecord(Ndb*,
		    int recordNo,
		    int numRecords = 1,
		    int updatesValue = 0);

  int pkWritePartialRecord(Ndb*,
			   int recordNo,
			   int numRecords = 1);
  
  int pkReadRecord(Ndb*,
                   int record,
                   int numRecords = 1,
                   NdbOperation::LockMode lm = NdbOperation::LM_Read,
                   NdbOperation::LockMode * lmused = 0);
  
  int pkReadRandRecord(Ndb*,
                       int records,
                       int numRecords = 1,
                       NdbOperation::LockMode lm = NdbOperation::LM_Read,
                       NdbOperation::LockMode * lmused = 0);

  int pkReadRecordLockHandle(Ndb*,
                             Vector<const NdbLockHandle*>& lockHandles,
                             int record,
                             int numRecords = 1,
                             NdbOperation::LockMode lm = NdbOperation::LM_Read,
                             NdbOperation::LockMode * lmused = 0);
  
  int pkUnlockRecord(Ndb*,
                     Vector<const NdbLockHandle*>& lockHandles,
                     int offset = 0,
                     int numRecords = ~(0),
                     NdbOperation::AbortOption ao = NdbOperation::AbortOnError);

  int pkUpdateRecord(Ndb*,
		     int recordNo,
		     int numRecords = 1,
		     int updatesValue = 0);
  
  int pkDeleteRecord(Ndb*,
		     int recordNo,
		     int numRecords = 1);
  
  int pkRefreshRecord(Ndb*,
                      int recordNo,
                      int numRecords = 1,
                      int anyValueInfo = 0); /* 0 - none, 1+ Val | record */

  int execute_Commit(Ndb*, 
		     AbortOption ao = AbortOnError);
  int execute_NoCommit(Ndb*,
		       AbortOption ao = AbortOnError);
  int execute_Rollback(Ndb*);
  
  int saveCopyOfRecord(int numRecords = 1);
  int compareRecordToCopy(int numRecords = 1);

  BaseString getRecordStr(int recordNum);
  int getRecordGci(int recordNum);

  int setValueForAttr(NdbOperation*,
		      int attrId, 
		      int rowId,
		      int updateId);
  
  int equalForAttr(NdbOperation*,
		   int attrId, 
		   int rowId);

  int equalForRow(NdbOperation*, int rowid);

  bool getPartIdForRow(const NdbOperation* pOp, int rowid, Uint32& partId);
  
  int setValues(NdbOperation*, int rowId, int updateId);
  int setNonPkValues(NdbOperation*, int rowId, int updateId);

  int verifyUpdatesValue(int updatesValue, int _numRows = 0);

  int indexReadRecords(Ndb*, const char * idxName, int recordNo,
		       bool exclusive = false,
		       int records = 1);
  
  int indexUpdateRecord(Ndb*,
			const char * idxName, 
			int recordNo,
			int numRecords = 1,
			int updatesValue = 0);

  int scanReadRecords(Ndb*, NdbScanOperation::LockMode = 
		      NdbScanOperation::LM_CommittedRead, 
		      int numRecords = 1);

  NdbIndexScanOperation* pIndexScanOp;

  NDBT_ResultRow& get_row(Uint32 idx) { return *rows[idx];}

  int execute_async(Ndb*, NdbTransaction::ExecType, NdbOperation::AbortOption = NdbOperation::AbortOnError);
  int execute_async_prepare(Ndb*, NdbTransaction::ExecType, NdbOperation::AbortOption = NdbOperation::AbortOnError);
  
  int wait_async(Ndb*, int timeout = -1);

  int releaseLockHandles(Ndb*,
                         Vector<const NdbLockHandle*>& lockHandles,
                         int offset = 0,
                         int numRecords = ~(0));

  const NdbError& getNdbError() const;
  void setQuiet() { m_quiet = true; }

  typedef Uint32 (*AnyValueCallback)(Ndb*, NdbTransaction*, int rowid, int updVal);

  void setAnyValueCallback(AnyValueCallback);

protected:
  void allocRows(int rows);
  void deallocRows();

  Vector<NDBT_ResultRow*> rows;
  Vector<NdbIndexScanOperation*> indexScans;
  HugoCalculator calc;

  Vector<BaseString> savedRecords;

  struct RsPair { NdbScanOperation* m_result_set; int records; };
  Vector<RsPair> m_result_sets;
  Vector<RsPair> m_executed_result_sets;

  int m_async_reply;
  int m_async_return;
  friend void HugoOperations_async_callback(int, NdbTransaction*, void*);
  void callback(int res, NdbTransaction*);
  Uint32 getAnyValueForRowUpd(int row, int update);


  void setNdbError(const NdbError& error);
  NdbError m_error;
  bool m_quiet;
  AnyValueCallback avCallback;
};

#endif
