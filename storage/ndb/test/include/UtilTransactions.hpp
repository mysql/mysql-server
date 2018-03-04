/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef UTIL_TRANSACTIONS_HPP
#define UTIL_TRANSACTIONS_HPP

#include <NDBT.hpp>

typedef int (ReadCallBackFn)(NDBT_ResultRow*);

class UtilTransactions {
public:
  Uint64 m_latest_gci;
  Uint32 get_high_latest_gci()
  {
    return Uint32(Uint64(m_latest_gci >> 32));
  }
  Uint32 get_low_latest_gci()
  {
    return Uint32(Uint64(m_latest_gci & 0xFFFFFFFF));
  }
  UtilTransactions(const NdbDictionary::Table&,
		   const NdbDictionary::Index* idx = 0);
  UtilTransactions(Ndb* ndb, 
		   const char * tableName, const char * indexName = 0);
  
  int closeTransaction(Ndb*);
  
  int clearTable(Ndb*, 
                 NdbScanOperation::ScanFlag,
		 int records = 0,
		 int parallelism = 0);

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
		  NdbOperation::LockMode lm = NdbOperation::LM_CommittedRead);

  int scanReadRecords(Ndb*,
		      int parallelism,
		      NdbOperation::LockMode lm,
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
		
  /**
   * Compare this table with other_table
   *
   * return 0 - on equality
   *       -1 - on error
   *      >0 - otherwise
   */
  int compare(Ndb*, const char * other_table, int flags);
  
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
  const NdbDictionary::Index* idx;
  NdbConnection* pTrans;
  
  NdbOperation* getOperation(NdbConnection*, 
			     NdbOperation::OperationType);
  NdbScanOperation* getScanOperation(NdbConnection*);
};

#endif
