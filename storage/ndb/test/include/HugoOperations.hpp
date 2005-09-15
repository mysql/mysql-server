/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
  int startTransaction(Ndb*);
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
		   int recordNo,
		   int numRecords = 1,
		   NdbOperation::LockMode lm = NdbOperation::LM_Read);
  
  int pkUpdateRecord(Ndb*,
		     int recordNo,
		     int numRecords = 1,
		     int updatesValue = 0);
  
  int pkDeleteRecord(Ndb*,
		     int recordNo,
		     int numRecords = 1);
  
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
  
  int setValues(NdbOperation*, int rowId, int updateId);
  
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

  int execute_async(Ndb*, NdbTransaction::ExecType, NdbTransaction::AbortOption = NdbTransaction::AbortOnError);
  int wait_async(Ndb*, int timeout = -1);

protected:
  void allocRows(int rows);
  void deallocRows();

  Vector<NDBT_ResultRow*> rows;
  HugoCalculator calc;

  Vector<BaseString> savedRecords;

  struct RsPair { NdbScanOperation* m_result_set; int records; };
  Vector<RsPair> m_result_sets;
  Vector<RsPair> m_executed_result_sets;

  int m_async_reply;
  int m_async_return;
  friend void HugoOperations_async_callback(int, NdbTransaction*, void*);
  void callback(int res, NdbTransaction*);
};

#endif
