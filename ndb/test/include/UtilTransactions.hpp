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

#ifndef UTIL_TRANSACTIONS_HPP
#define UTIL_TRANSACTIONS_HPP

#include <NDBT.hpp>

typedef int (ReadCallBackFn)(NDBT_ResultRow*);

class UtilTransactions {
public:
  enum ScanLock {
    SL_Read = 0,
    SL_ReadHold = 1,
    SL_Exclusive = 2
  };

  UtilTransactions(const NdbDictionary::Table&);
  UtilTransactions(Ndb* ndb, const char * tableName);

  int clearTable(Ndb*, 
		 int records = 0,
		 int parallelism = 0);
  
  // Delete all records from the table using a scan
  int clearTable1(Ndb*, 
		  int records = 0,
		  int parallelism = 0);
  // Delete all records from the table using a scan
  // Using batching
  int clearTable2(Ndb*, 
		  int records = 0,
		  int parallelism = 0);
  
  int clearTable3(Ndb*, 
		  int records = 0,
		  int parallelism = 0);
  
  int selectCount(Ndb*, 
		  int parallelism = 0,
		  int* count_rows = NULL,
		  ScanLock lock = SL_Read,
		  NdbConnection* pTrans = NULL);
  int scanReadRecords(Ndb*,
		      int parallelism,
		      bool exclusive,
		      int records,
		      int noAttribs,
		      int* attrib_list,
		      ReadCallBackFn* fn = NULL);
  int verifyIndex(Ndb*,
		  const char* indexName,
		  int parallelism = 0,
		  bool transactional = false);

  int copyTableData(Ndb*,
		const char* destName);
		
  
private:
  static int takeOverAndDeleteRecord(Ndb*, 
				     NdbOperation*);

  int addRowToDelete(Ndb* pNdb, 
		     NdbConnection* pDelTrans,
		     NdbOperation* pOrgOp);

  
  int addRowToInsert(Ndb* pNdb, 
		     NdbConnection* pInsTrans,
		     NDBT_ResultRow & row,
		     const char* insertTabName);


  int verifyUniqueIndex(Ndb*,
			const NdbDictionary::Index *,
			int parallelism = 0,
			bool transactional = false);

  int scanAndCompareUniqueIndex(Ndb* pNdb,
				const NdbDictionary::Index *,
				int parallelism,
				bool transactional);
  
  int readRowFromTableAndIndex(Ndb* pNdb,
			       NdbConnection* pTrans,
			       const NdbDictionary::Index *,
			       NDBT_ResultRow& row );

  int verifyOrderedIndex(Ndb*,
			 const NdbDictionary::Index *,
			 int parallelism = 0,
			 bool transactional = false);
  

  int get_values(NdbOperation* op, NDBT_ResultRow& dst);
  int equal(const NdbDictionary::Table*, NdbOperation*, const NDBT_ResultRow&);
  int equal(const NdbDictionary::Index*, NdbOperation*, const NDBT_ResultRow&);

protected:
  int m_defaultClearMethod;
  const NdbDictionary::Table& tab;
};

#endif
