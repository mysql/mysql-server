/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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
  Uint64 m_util_latest_gci;
  Uint32 get_high_latest_gci()
  {
    return Uint32(Uint64(m_util_latest_gci >> 32));
  }
  Uint32 get_low_latest_gci()
  {
    return Uint32(Uint64(m_util_latest_gci & 0xFFFFFFFF));
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

  /**
   * verify index content relative to table.
   * Specifically :
   *   Every row in the table is accessable by PK, and also
   *   via the index
   * This is a legacy method, see newer variants below.
   */
  int verifyIndex(Ndb*,
		  const char* indexName,
		  int parallelism = 0,
		  bool transactional = false);


  /**
   * Verifying consistency
   *
   * Some methods below allow aspects of data consistency to be
   * checked between various [views of] various structures
   * associated with the current table.
   *
   * These are also exposed via the verify_index tool.
   *
   * Table <-> Index consistency
   *  - verifyIndex / verifyAllIndexes / verifyTableAndAllIndexes
   *    Check that all rows found by scanning the table are
   *    present in the index [including those with null values]
   *    [Optionally check that all rows found by scanning the
   *    index are present in the table]
   *
   * Table replica/view consistency
   *  - verifyTableReplicas / verifyTableAndAllIndexes
   *    Check view of table data using 'local' pk reads from all
   *    data nodes, driven by a scan from one or each data node.
   *    Checks basic replica alignment, including for FR tables.
   *    Works for all table types.
   *
   * Index replica/view consistency
   *  - verifyIndexViews / verifyAllIndexes / verifyTableAndAllIndexes
   *    Check views of the index from all data nodes.
   *    Checks view/replica alignment.
   *    For RP tables : View alignment
   *    For RB tables : View/Replica alignment
   *    For FR tables : Replica alignment
   *
   * Observations
   *  - Checking between different related objects
   *    (tables and their indexes) is different to
   *    checking between replicas of an object, and
   *    these have been separated in the code.
   *    Ideally, it is possible to check these independently
   *    and get a full cross check, without needing all-all checks
   *
   *  - Unique indexes have no scan capability currently,
   *    so some checks are not performed for them.
   *    This could/should be fixed.
   *
   *  - Checking from all nodes trivially covers the FR table case
   *    and also gives some coverage of 'view' correctness, but
   *    becomes relatively more expensive as the #nodes increases.
   *
   *  - Potential improvements :
   *    - Table scan view/replica equivalence check
   *    - UI view checks using hinted lookups (similar to checkTableReplicas)
   *    - UI (table) scan source implementation to check for orphans
   *    - FK relationship checks
   *    - Performance improvement
   *    - Blob handling
   */

  /**
   * verifyIndex
   * General function to verify the content of an index
   * on a table.
   * Algorithm :
   *   - Scan source (table | OI | UI (not implemented yet))
   *   - For each row found
   *     - Lookup row by PK
   *     - [Check row can be found via index]
   *
   * Variants :
   *   targetIndex    : Index, can be OI, UI or NULL
   *                    Where target is NULL, only PK lookup is done
   *   checkFromIndex : Use index as scan source, implies only PK lookup
   *   findNulls      : Whether to check the index for values with NULLs
   *                    or skip those (UI always skips nulls)
   * Examples :
   *   targetIndex type    checkFromIndex    effect
   *   UI                  false             Scan table, lookup PK, lookup UI
   *   UI                  true              Not implemented yet
   *   OI                  false             Scan table, lookup PK, scan OI
   *   OI                  true              Scan OI, lookup PK
   *   NULL                *                 Scan table, lookup PK
   *
   * This can be used to check the equivalence of a table's indexes and
   * row content.
   * Using the table as source ensures all stored rows are indexed.
   * Using an index as source ensures that all indexed rows are stored.
   */
  int verifyIndex(Ndb*,
                  const NdbDictionary::Index* targetIndex,
                  bool checkFromIndex,     /* false= tab scan, true = index scan */
                  bool findNulls);         /* Match entries with NULL values */

  /**
   * verifyTableReplicas
   * Scan table, and then lookup each row using SimpleRead
   * from every data node, checking that the table data is
   * the same in all replicas, as observed from all data
   * nodes.
   * Optional allSources controls whether the table scan
   * is run from one (random) data node, or from every
   * data node in turn.
   * Running from every data node takes longer, but
   * ensures that all scan reachable rows in every view
   * are replicated correctly.
   */
  int verifyTableReplicas(Ndb*, bool allSources = false);

  /**
   * verifyIndexViews
   * Verify views of an index are the same from all
   * data nodes.
   * For an RP table, this checks distributed access
   * For an RB table, this checks distributed access
   *  and replica consistency
   * For an FR table, this checks replica consistency
   *
   * Currently only checks for ordered indexes.
   */
  int verifyIndexViews(Ndb* pNdb,
                       const NdbDictionary::Index* pIndex);

  /**
   * verifyAllIndexes
   * Verify all indexes of the table :
   *   - Check that all table rows are contained in the indexes
   *   - [findNulls] : Check also values containing nulls
   *   - [bidirectional : Check that all index rows are contained in the table]
   *   - [views] : Check that the index contains the same content when viewed
   *               from every data node
   */
  int verifyAllIndexes(Ndb* pNdb,
                       bool findNulls = true,
                       bool bidirectional = true,
                       bool views = true);

  /**
   * verifyTable
   * Verify table data and indexes :
   *   - Scan table and check that all replicas return the same content
   *   - [allSources] : Perform above check for scan from every node
   *   - Verify all indexes of the table using verifyAllIndexes, and the
   *     - findNulls, bidirectional, views parameters
   */
  int verifyTableAndAllIndexes(Ndb* pNdb,
                               bool findNulls = true,
                               bool bidirectional = true,
                               bool views = true,
                               bool allSources = true);

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

  void setVerbosity(Uint32);
  Uint32 getVerbosity() const;

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
			 const NdbDictionary::Index *sourceIndex,
                         const NdbDictionary::Index *destIndex,
			 int parallelism = 0,
			 bool transactional = false,
                         bool findNulls = false);

  int verifyTableReplicasWithSource(Ndb*, Uint32 sourceNodeId=0);

  int verifyOrderedIndexViews(Ndb* pNdb,
                              const NdbDictionary::Index* index);
  int verifyTwoOrderedIndexViews(Ndb* pNdb,
                                 const NdbDictionary::Index* index,
                                 Uint32 node1,
                                 Uint32 node2);
  int defineOrderedScan(Ndb* pNdb,
                        const NdbDictionary::Index* index,
                        Uint32 nodeId,
                        NdbTransaction*& scanTrans,
                        NdbIndexScanOperation*& scanOp,
                        NDBT_ResultRow& row);

  int get_values(NdbOperation* op, NDBT_ResultRow& dst);
  int equal(const NdbDictionary::Table*, NdbOperation*, const NDBT_ResultRow&);
  int equal(const NdbDictionary::Index*,
            NdbOperation*,
            const NDBT_ResultRow&,
            bool skipNull=false);

protected:
  int m_defaultClearMethod;
  const NdbDictionary::Table& tab;
  const NdbDictionary::Index* idx;
  NdbConnection* pTrans;
  Uint32 m_verbosity;
  
  NdbOperation* getOperation(NdbConnection*, 
			     NdbOperation::OperationType);
  NdbScanOperation* getScanOperation(NdbConnection*);
};

#endif
