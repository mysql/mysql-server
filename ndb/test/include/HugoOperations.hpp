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
  HugoOperations(const NdbDictionary::Table&);
  ~HugoOperations();
  int startTransaction(Ndb*);
  int closeTransaction(Ndb*);
  NdbConnection* getTransaction();
  void refresh();
  
  int pkInsertRecord(Ndb*,
		     int recordNo,
		     int numRecords = 1,
		     int updatesValue = 0);
  
  int pkReadRecord(Ndb*,
		   int recordNo,
		   bool exclusive = false,
		   int numRecords = 1);
  
  int pkSimpleReadRecord(Ndb*,
			 int recordNo,
			 int numRecords = 1);
  
  int pkDirtyReadRecord(Ndb*,
			int recordNo,
			int numRecords = 1);
  
  int pkUpdateRecord(Ndb*,
		     int recordNo,
		     int numRecords = 1,
		     int updatesValue = 0);
  
  int pkDeleteRecord(Ndb*,
		     int recordNo,
		     int numRecords = 1);

  int scanReadRecords(Ndb* pNdb, 
		      Uint32 parallelism = 240, ScanLock lock = SL_Read);
  int executeScanRead(Ndb*);
  
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

  int verifyUpdatesValue(int updatesValue, int _numRows = 0);

  int indexReadRecords(Ndb*, const char * idxName, int recordNo,
		       bool exclusive = false,
		       int records = 1);
  
  int indexUpdateRecord(Ndb*,
			const char * idxName, 
			int recordNo,
			int numRecords = 1,
			int updatesValue = 0);
  
protected:
  void allocRows(int rows);
  void deallocRows();

  Vector<NDBT_ResultRow*> rows;
  HugoCalculator calc;

  Vector<BaseString> savedRecords;
private:
  NdbConnection* pTrans;

  struct ScanTmp {
    ScanTmp() { 
      pTrans = 0; 
      m_tmpRow = 0; 
      m_delete = true; 
      m_op = DONE;
    }
    ScanTmp(NdbConnection* a, NDBT_ResultRow* b){ 
      pTrans = a; 
      m_tmpRow = b; 
      m_delete = true; 
      m_op = DONE;
    }
    ScanTmp(const ScanTmp& org){
      * this = org;
    }
    ScanTmp& operator=(const ScanTmp& org){
      pTrans = org.pTrans; 
      m_tmpRow = org.m_tmpRow; 
      m_delete = org.m_delete;
      m_op = org.m_op;
      return * this;
    }
    
    ~ScanTmp() { 
      if(m_delete && pTrans)
	pTrans->close(); 
      if(m_delete && m_tmpRow)
	delete m_tmpRow;
    }
    
    NdbConnection * pTrans;
    NDBT_ResultRow * m_tmpRow;
    bool m_delete;
    enum { DONE, READ, UPDATE, DELETE } m_op;
  };
  Vector<ScanTmp> m_scans;
  int run(ScanTmp & tmp);

};

#endif
