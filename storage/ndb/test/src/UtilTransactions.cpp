/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include "UtilTransactions.hpp"
#include <NdbSleep.h>
#include <NdbScanFilter.hpp>

#define VERBOSE 0

UtilTransactions::UtilTransactions(const NdbDictionary::Table& _tab,
				   const NdbDictionary::Index* _idx):
  tab(_tab), idx(_idx)
{
}

UtilTransactions::UtilTransactions(Ndb* ndb, 
				   const char * name,
				   const char * index) :
  tab(* ndb->getDictionary()->getTable(name)),
  idx(index ? ndb->getDictionary()->getIndex(index, name) : 0)
{
}

#define RESTART_SCAN 99

#define RETURN_FAIL(err) return (err.code != 0 ? err.code : NDBT_FAILED) 

int 
UtilTransactions::clearTable(Ndb* pNdb, 
                             NdbScanOperation::ScanFlag flags,
                             int records,
                             int parallelism){
  // Scan all records exclusive and delete 
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int deletedRows = 0;
  int check;
  NdbScanOperation *pOp;
  NdbError err;

  int par = parallelism;
  while (true){
  restart:
    if (retryAttempt++ >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    
    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      err = pNdb->getNdbError();
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	continue;
      }
      NDB_ERR(err);
      goto failed;
    }

    pOp = getScanOperation(pTrans);
    if (pOp == NULL) {
      err = pTrans->getNdbError();
      if(err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	par = 1;
	goto restart;
      }
      NDB_ERR(err);
      goto failed;
    }
    
    if( pOp->readTuples(NdbOperation::LM_Exclusive, flags, par) ) {
      err = pTrans->getNdbError();
      NDB_ERR(err);
      goto failed;
    }
    
    if(pTrans->execute(NoCommit, AbortOnError) != 0){
      err = pTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	continue;
      }
      NDB_ERR(err);
      goto failed;
    }
    
    while((check = pOp->nextResult(true)) == 0){
      do {
	if (pOp->deleteCurrentTuple() != 0){
          NDB_ERR(err);
	  goto failed;
	}
	deletedRows++;
      } while((check = pOp->nextResult(false)) == 0);
      
      if(check != -1){
	check = pTrans->execute(Commit, AbortOnError);
        if (check != -1)
          pTrans->getGCI(&m_util_latest_gci);
	pTrans->restart();
      }
      
      err = pTrans->getNdbError();    
      if(check == -1){
	if(err.status == NdbError::TemporaryError){
	  NDB_ERR(err);
	  closeTransaction(pNdb);
          if (err.code == 410 || err.code == 1501)
	    NdbSleep_MilliSleep(2000);
          else
	    NdbSleep_MilliSleep(50);
	  par = 1;
	  goto restart;
	}
        NDB_ERR(err);
	goto failed;
      }
    }
    if(check == -1){
      err = pTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	par = 1;
	goto restart;
      }
      NDB_ERR(err);
      goto failed;
    }
    closeTransaction(pNdb);
    return NDBT_OK;
  }
  abort(); /* Should never happen */
  return NDBT_FAILED;
  
 failed:
  if(pTrans != 0) closeTransaction(pNdb);
  return (err.code != 0 ? err.code : NDBT_FAILED);
}

int 
UtilTransactions::clearTable(Ndb* pNdb, 
			     int records,
			     int parallelism){

  return clearTable(pNdb, (NdbScanOperation::ScanFlag)0,
                    records, parallelism);
}


int 
UtilTransactions::clearTable1(Ndb* pNdb, 
			     int records,
			     int parallelism)
{
  return clearTable(pNdb, (NdbScanOperation::ScanFlag)0,
                    records, 1);
}

int 
UtilTransactions::clearTable2(Ndb* pNdb, 
			      int records,
			      int parallelism)
{
  return clearTable(pNdb, (NdbScanOperation::ScanFlag)0,
                    records, parallelism);
}

int 
UtilTransactions::clearTable3(Ndb* pNdb, 
			      int records,
			      int parallelism)
{
  return clearTable(pNdb, (NdbScanOperation::ScanFlag)0,
                    records, parallelism);
}

int 
UtilTransactions::copyTableData(Ndb* pNdb,
			    const char* destName){
  // Scan all records and copy 
  // them to destName table
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int insertedRows = 0;
  int parallelism = 240;
  int check;
  NdbScanOperation		*pOp;
  NDBT_ResultRow       row(tab);
  
  while (true){
    
    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }


    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if( pOp->readTuples(NdbScanOperation::LM_Read, parallelism) ) {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // Read all attributes
    for (int a = 0; a < tab.getNoOfColumns(); a++){
      if ((row.attributeStore(a) =  
	   pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	NDB_ERR(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }
    
    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
  
    int eof;
    while((eof = pOp->nextResult(true)) == 0){
      do {
	insertedRows++;
	if (addRowToInsert(pNdb, pTrans, row, destName) != 0){
	  closeTransaction(pNdb);
          g_err << "Line: " << __LINE__ << " failed to add row" << endl;
	  return NDBT_FAILED;
	}
      } while((eof = pOp->nextResult(false)) == 0);

      if (eof == -1) break;
      
      check = pTrans->execute(Commit, AbortOnError);   
      if( check == -1 ) {
        const NdbError err = pTrans->getNdbError();
        NDB_ERR(err);
        closeTransaction(pNdb);
        return NDBT_FAILED;
      }
      pTrans->getGCI(&m_util_latest_gci);
      pTrans->restart();
    }  
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	// If error = 488 there should be no limit on number of retry attempts
	if (err.code != 488) 
	  retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    closeTransaction(pNdb);
    
    g_info << insertedRows << " rows copied" << endl;
    
    return NDBT_OK;
  }
  abort(); /* Should never happen */
  return NDBT_FAILED;
}

int 
UtilTransactions::addRowToInsert(Ndb* pNdb,
				 NdbConnection* pInsTrans,
				 NDBT_ResultRow & row,
				 const char *insertTabName){

  int check;
  NdbOperation* pInsOp;

  pInsOp = pInsTrans->getNdbOperation(insertTabName);	
  if (pInsOp == NULL) {
    NDB_ERR(pInsTrans->getNdbError());
    return NDBT_FAILED;
  }
  
  check = pInsOp->insertTuple();
  if( check == -1 ) {
    NDB_ERR(pInsTrans->getNdbError());
    return NDBT_FAILED;
  }

  // Set all attributes
  for (int a = 0; a < tab.getNoOfColumns(); a++){
    NdbRecAttr* r =  row.attributeStore(a);
    int	 sz = r->get_size_in_bytes();
    if (pInsOp->setValue(tab.getColumn(a)->getName(),
			 r->aRef(),
			 sz) != 0) {
      NDB_ERR(pInsTrans->getNdbError());
      return NDBT_FAILED;
    }
  }
  
  return NDBT_OK;
}


int 
UtilTransactions::scanReadRecords(Ndb* pNdb,
				  int parallelism,
				  NdbOperation::LockMode lm,
				  int records,
				  int noAttribs,
				  int *attrib_list,
				  ReadCallBackFn* fn){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbScanOperation	       *pOp;
  NDBT_ResultRow       row(tab);

  while (true){

    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    pOp = getScanOperation(pTrans);
    if (pOp == NULL) {
      const NdbError err = pNdb->getNdbError();
      closeTransaction(pNdb);

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    if( pOp->readTuples(lm, 0, parallelism) ) {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // Call getValue for all the attributes supplied in attrib_list
    // ************************************************
    for (int a = 0; a < noAttribs; a++){
      if (attrib_list[a] < tab.getNoOfColumns()){ 
	g_info << "getValue(" << attrib_list[a] << ")" << endl;
	if ((row.attributeStore(attrib_list[a]) =  
	     pOp->getValue(tab.getColumn(attrib_list[a])->getName())) == 0) {
	  NDB_ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
    }
    // *************************************************
    
    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    int eof;
    int rows = 0;
    
    
    while((eof = pOp->nextResult()) == 0){
      rows++;
      
      // Call callback for each record returned
      if(fn != NULL)
	fn(&row);
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    closeTransaction(pNdb);
    g_info << rows << " rows have been read" << endl;
    if (records != 0 && rows != records){
      g_err << "Check expected number of records failed" << endl 
	     << "  expected=" << records <<", " << endl
	     << "  read=" << rows << endl;
      return NDBT_FAILED;
    }
    
    return NDBT_OK;
  }
  abort(); /* Should never happen */
  return NDBT_FAILED;
}

int 
UtilTransactions::selectCount(Ndb* pNdb, 
			      int parallelism,
			      int* count_rows,
			      NdbOperation::LockMode lm)
{
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;

  while (true){

    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL)
    {
      if (pNdb->getNdbError().status == NdbError::TemporaryError)
      {
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(pNdb->getNdbError());
      return NDBT_FAILED;
    }


    NdbScanOperation *pOp = getScanOperation(pTrans);
    if (pOp == NULL)
    {
      NdbError err = pTrans->getNdbError();
      closeTransaction(pNdb);
      if (err.status == NdbError::TemporaryError)
      {
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    if( pOp->readTuples(lm) )
    {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if(0){
      NdbScanFilter sf(pOp);
      sf.begin(NdbScanFilter::OR);
      sf.eq(2, (Uint32)30);
      sf.end();
    }
    
    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 )
    {
      NdbError err = pTrans->getNdbError();
      closeTransaction(pNdb);
      if (err.status == NdbError::TemporaryError)
      {
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    int eof;
    int rows = 0;
    

    while((eof = pOp->nextResult()) == 0){
      rows++;
    }

    if (eof == -1) 
    {
      const NdbError err = pTrans->getNdbError();
      closeTransaction(pNdb);
      
      if (err.status == NdbError::TemporaryError)
      {
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    closeTransaction(pNdb);
    
    if (count_rows != NULL){
      *count_rows = rows;
    }
    
    return NDBT_OK;
  }
  abort(); /* Should never happen */
  return NDBT_FAILED;
}

int 
UtilTransactions::verifyIndex(Ndb* pNdb,
			      const char* indexName,
			      int parallelism,
			      bool transactional) {
  

  const NdbDictionary::Index* pIndex
    = pNdb->getDictionary()->getIndex(indexName, tab.getName());
  if (pIndex == 0){
    ndbout << " Index " << indexName << " does not exist!" << endl;
    return NDBT_FAILED;
  }

  /* Scan from table, check pks, check index without finding nulls */
  return verifyIndex(pNdb, pIndex, false, false);
}

int UtilTransactions::verifyIndex(Ndb* pNdb,
                                  const NdbDictionary::Index* targetIndex,
                                  bool checkFromIndex,
                                  bool findNulls)
{
  if (m_verbosity > 0)
  {
    ndbout << "|- Checking index " << targetIndex->getName()
           << " options (checkFromIndex " << checkFromIndex
           << " findNulls " << findNulls
           << ")" << endl;
  }
  if (targetIndex)
  {
    if (!checkFromIndex)
    {
      /* Table scan drives check of index */
      switch (targetIndex->getType()){
      case NdbDictionary::Index::UniqueHashIndex:
        return verifyUniqueIndex(pNdb, targetIndex, 1, true);
      case NdbDictionary::Index::OrderedIndex:
        return verifyOrderedIndex(pNdb, NULL, targetIndex, 1, true, findNulls);
      default:
        ndbout << "Unknown index type" << endl;
        return NDBT_FAILED;
      }
    }
    else
    {
      /* Index scan drives check of table */
      switch (targetIndex->getType()){
      case NdbDictionary::Index::UniqueHashIndex:
        /* TODO : UI table scan not implemented yet */
        return NDBT_OK;
      case NdbDictionary::Index::OrderedIndex:
        return verifyOrderedIndex(pNdb, targetIndex, NULL, 1, true, findNulls);
      default:
        ndbout << "Unknown index type" << endl;
        return NDBT_FAILED;
      }
    }
  }
  else
  {
    /* NULL index - just check table */
    return verifyOrderedIndex(pNdb, NULL, NULL, 1, true, findNulls);
  }
}



int 
UtilTransactions::verifyUniqueIndex(Ndb* pNdb,
				    const NdbDictionary::Index * pIndex,
				    int parallelism,
				    bool transactional){
  
  /**
   * Scan all rows in TABLE and for each found row make one read in
   * TABLE and one using INDEX_TABLE. Then compare the two returned 
   * rows. They should be equal!
   *
   */

  if (scanAndCompareUniqueIndex(pNdb, 
				pIndex,
				parallelism,
				transactional) != NDBT_OK){
    return NDBT_FAILED;
  }


  return NDBT_OK;
  
}


int 
UtilTransactions::scanAndCompareUniqueIndex(Ndb* pNdb,
					    const NdbDictionary::Index* pIndex,
					    int parallelism,
					    bool transactional){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbScanOperation       *pOp;
  NDBT_ResultRow       row(tab);

  parallelism = 1;

  while (true){
restart:
    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbScanOperation(tab.getName());
    if (pOp == NULL) {
      const NdbError err = pNdb->getNdbError();
      closeTransaction(pNdb);
      NDB_ERR(err);
      
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      return NDBT_FAILED;
    }

    int rs;
    if(transactional){
      rs = pOp->readTuples(NdbScanOperation::LM_Read, 0, parallelism);
    } else {
      rs = pOp->readTuples(NdbScanOperation::LM_CommittedRead, 0, parallelism);
    }
    
    if( rs != 0 ) {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // Read all attributes
    for (int a = 0; a < tab.getNoOfColumns(); a++){
      if ((row.attributeStore(a) =  
	   pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	NDB_ERR(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }

    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    int eof;
    int rows = 0;
    
    
    while((eof = pOp->nextResult()) == 0){
      rows++;
      
      // ndbout << row.c_str().c_str() << endl;
      
      if (readRowFromTableAndIndex(pNdb,
				   pTrans,
				   pIndex,
				   row) != NDBT_OK){	
	
	while((eof= pOp->nextResult(false)) == 0);
	if(eof == 2)
	  eof = pOp->nextResult(true); // this should give -1
	if(eof == -1)
	{
	  const NdbError err = pTrans->getNdbError();
	  
	  if (err.status == NdbError::TemporaryError){
	    NDB_ERR(err);
	    closeTransaction(pNdb);
	    NdbSleep_MilliSleep(50);
	    retryAttempt++;
	    goto restart;
	  }
	}
	closeTransaction(pNdb);
        g_err << "Line: " << __LINE__ << " next result failed" << endl;
	return NDBT_FAILED;
      }
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    closeTransaction(pNdb);
    
    return NDBT_OK;
  }
  abort(); /* Should never happen */
  return NDBT_FAILED;
}
int 
UtilTransactions::readRowFromTableAndIndex(Ndb* pNdb,
					   NdbConnection* scanTrans,
					   const NdbDictionary::Index* pIndex,
					   NDBT_ResultRow& row ){


  NdbDictionary::Index::Type indexType= pIndex->getType();
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check, a;
  NdbConnection	       *pTrans1=NULL;
  NdbOperation	       *pOp;

  int return_code= NDBT_FAILED;

  // Allocate place to store the result
  NDBT_ResultRow       tabRow(tab);
  NDBT_ResultRow       indexRow(tab);
  const char * indexName = pIndex->getName();

  while (true){
    if(retryAttempt)
      ndbout_c("retryAttempt %d", retryAttempt);
    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      goto close_all;
    }

    pTrans1 = pNdb->hupp(scanTrans); //startTransaction();
    if (pTrans1 == NULL) {
      const NdbError err = pNdb->getNdbError();
      
      if (err.code == 4006)
      {
        g_err << "Line: " << __LINE__ << " err: 4006" << endl;
        goto close_all;
      }

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }

      if(err.code == 0){
	return_code = NDBT_OK;
	goto close_all;
      }
      NDB_ERR(err);
      goto close_all;
    }

    /**
     * Read the record from TABLE
     */
    pOp = pTrans1->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      NDB_ERR(pTrans1->getNdbError());
      goto close_all;
    }
    
    check = pOp->readTuple();
    if( check == -1 ) {
      NDB_ERR(pTrans1->getNdbError());
      goto close_all;
    }
    
    // Define primary keys
#if VERBOSE
    printf("PK: ");
#endif
    for(a = 0; a<tab.getNoOfColumns(); a++){
      const NdbDictionary::Column* attr = tab.getColumn(a);
      if (attr->getPrimaryKey() == true){
	if (pOp->equal(attr->getName(), row.attributeStore(a)->aRef()) != 0){
	  NDB_ERR(pTrans1->getNdbError());
	  goto close_all;
	}
#if VERBOSE
	printf("%s = %d: ", attr->getName(), row.attributeStore(a)->aRef());
#endif
      }
    }
#if VERBOSE
    printf("\n");
#endif
    // Read all attributes
#if VERBOSE
    printf("Reading %u attributes: ", tab.getNoOfColumns());
#endif
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if((tabRow.attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	NDB_ERR(pTrans1->getNdbError());
	goto close_all;
      }
#if VERBOSE
      printf("%s ", tab.getColumn(a)->getName());
#endif
    }
#if VERBOSE
    printf("\n");
#endif

    /**
     * Read the record from INDEX_TABLE
     */    
    NdbIndexOperation* pIndexOp= NULL;
    NdbIndexScanOperation *pScanOp= NULL;
    NdbOperation *pIOp= 0;

    bool null_found= false;
    for(a = 0; a<(int)pIndex->getNoOfColumns(); a++){
      const NdbDictionary::Column *  col = pIndex->getColumn(a);
      
      if (row.attributeStore(col->getName())->isNULL())
      {
	null_found= true;
	break;
      }
    }
    
    const char * tabName= tab.getName();
    if(!null_found)
    {
      if (indexType == NdbDictionary::Index::UniqueHashIndex) {
	pIOp= pIndexOp= pTrans1->getNdbIndexOperation(indexName, tabName);
      } else {
	pIOp= pScanOp= pTrans1->getNdbIndexScanOperation(indexName, tabName);
      }
      
      if (pIOp == NULL) {
	NDB_ERR(pTrans1->getNdbError());
	goto close_all;
      }
    
      {
	bool not_ok;
	if (pIndexOp) {
	  not_ok = pIndexOp->readTuple() == -1;
	} else {
	  not_ok = pScanOp->readTuples();
	}
	
	if( not_ok ) {
	  NDB_ERR(pTrans1->getNdbError());
	  goto close_all;
	}
      }
    
    // Define primary keys for index
#if VERBOSE
      printf("SI: ");
#endif
      for(a = 0; a<(int)pIndex->getNoOfColumns(); a++){
	const NdbDictionary::Column *  col = pIndex->getColumn(a);

	if ( !row.attributeStore(col->getName())->isNULL() ) {
	  if(pIOp->equal(col->getName(), 
			 row.attributeStore(col->getName())->aRef()) != 0){
	    NDB_ERR(pTrans1->getNdbError());
	    goto close_all;
	  }
	}
#if VERBOSE
	printf("%s = %d: ", col->getName(), row.attributeStore(a)->aRef());
#endif
      }
#if VERBOSE
      printf("\n");
#endif
      
      // Read all attributes
#if VERBOSE
      printf("Reading %u attributes: ", tab.getNoOfColumns());
#endif
      for(a = 0; a<tab.getNoOfColumns(); a++){
	void* pCheck;
	
	pCheck= indexRow.attributeStore(a)= 
	  pIOp->getValue(tab.getColumn(a)->getName());
	
	if(pCheck == NULL) {
	  NDB_ERR(pTrans1->getNdbError());
	  goto close_all;
	}
#if VERBOSE
	printf("%s ", tab.getColumn(a)->getName());
#endif
      }
    }
#if VERBOSE
    printf("\n");
#endif
    scanTrans->refresh();
    check = pTrans1->execute(Commit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = pTrans1->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	pNdb->closeTransaction(pTrans1);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ndbout << "Error when comparing records - normal op" << endl;
      NDB_ERR(err);
      ndbout << "row: " << row.c_str().c_str() << endl;
      goto close_all;
    } 
    
    /** 
     * Compare the two rows
     */ 
    if(!null_found){
      if (pScanOp) {
	if (pScanOp->nextResult() != 0){
	  const NdbError err = pTrans1->getNdbError();
	  NDB_ERR(err);
	  ndbout << "Error when comparing records - index op next_result missing" << endl;
	  ndbout << "row: " << row.c_str().c_str() << endl;
	  goto close_all;
	}
      }
      if (!(tabRow.c_str() == indexRow.c_str())){
	ndbout << "Error when comparing records" << endl;
	ndbout << " tabRow: \n" << tabRow.c_str().c_str() << endl;
	ndbout << " indexRow: \n" << indexRow.c_str().c_str() << endl;
	goto close_all;
      }
      if (pScanOp) {
	if (pScanOp->nextResult() == 0){
	  ndbout << "Error when comparing records - index op next_result to many" << endl;
	  ndbout << "row: " << row.c_str().c_str() << endl;
	  goto close_all;
	}
      }
    }
    return_code= NDBT_OK;
    goto close_all;
  }
  
close_all:
  if (pTrans1)
    pNdb->closeTransaction(pTrans1);
  
  return return_code;
}

int 
UtilTransactions::verifyOrderedIndex(Ndb* pNdb,
                                     const NdbDictionary::Index* sourceIndex,
				     const NdbDictionary::Index* destIndex,
				     int parallelism,
				     bool transactional,
                                     bool findNulls){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbScanOperation      *pOp = NULL;
  NdbIndexScanOperation *iop = NULL;

  NDBT_ResultRow       scanRow(tab);
  NDBT_ResultRow       pkRow(tab);
  NDBT_ResultRow       indexRow(tab);

  parallelism = 1;
  
  while (true){

    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    if (sourceIndex == NULL)
    {
      /* Scan table */
      pOp = pTrans->getNdbScanOperation(tab.getName());
    }
    else
    {
      /* Scan ordered index */
      pOp = pTrans->getNdbIndexScanOperation(sourceIndex->getName(),
                                             tab.getName());
    }
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if( pOp->readTuples(NdbScanOperation::LM_Read, 0, parallelism) ) {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if(get_values(pOp, scanRow))
    {
      abort();
    }

    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
        
    int eof = 0;
    int rows = 0;
    while(check == 0 && (eof = pOp->nextResult()) == 0){
      rows++;

      bool checkDestIndex = (destIndex != NULL);
      if (checkDestIndex &&
          !findNulls)
      {
        /* Check for NULLs */
        /* If we are checking the dest index, but not for null
         * values, then we need to check now whether this row has
         * null values or not to decide whether to check the dest index.
         * Otherwise (not checking, or checking nulls) no need.
         */
        for(int a = 0; a<(int)destIndex->getNoOfColumns(); a++){
          const NdbDictionary::Column *  col = destIndex->getColumn(a);
          if (scanRow.attributeStore(col->getName())->isNULL())
          {
            /* This row has a null, no check of dest index this time */
            checkDestIndex = false;
            break;
          }
        }
      }
      
      // Do pk lookup to check that the row is reachable by pk
      // in the base table
      NdbOperation * pk = pTrans->getNdbOperation(tab.getName());
      if (!pk || pk->readTuple(NdbOperation::LM_CommittedRead))
	goto error;
      if (equal(&tab, pk, scanRow) || get_values(pk, pkRow))
	goto error;

      if (checkDestIndex)
      {
        /* Check that the row can be found via the dest index */
        /* We set bounds on the dest index, but these may be loose
         * so we may have to check through a number of non equal 
         * candidate rows to find our row.
         */
        if ((iop= pTrans->getNdbIndexScanOperation(destIndex->getName(),
                                                   tab.getName())) != NULL)
        {
          if (iop->readTuples(NdbScanOperation::LM_CommittedRead,
                              parallelism))
            goto error;
          if (get_values(iop, indexRow))
            goto error;
          if (equal(destIndex, iop, scanRow, true))
            goto error;
        }
        else
        {
          goto error;
        }
      }

      check = pTrans->execute(NoCommit, AbortOnError);
      if(check)
	goto error;

      if (scanRow.c_str() != pkRow.c_str()){
	g_err << "Error when comparing records "
              << " source (" << (sourceIndex?sourceIndex->getName():"Table")
              << ") dest (" << (destIndex?destIndex->getName():"Table")
              << endl;
	g_err << " source scanRow: \n" << scanRow.c_str().c_str() << endl;
	g_err << " lookup pkRow: \n" << pkRow.c_str().c_str() << endl;
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      else
      {
        //g_err << "Pk ok" << endl;
      }

      if (checkDestIndex)
      {
        Uint32 candidate_row_count = 0;
        const BaseString scanRowString = scanRow.c_str();
        while (true)
        {
          if (iop->nextResult() != 0){
            g_err << "Failed to find row using index: "
                  << destIndex->getName() << endl;
            g_err << " source index : " << (sourceIndex?sourceIndex->getName():"Table") << endl;
            g_err << " source scanRow: \n" << scanRow.c_str().c_str() << endl;
            g_err << " index candidate rows : " << candidate_row_count << endl;
            NDB_ERR(pTrans->getNdbError());
            closeTransaction(pNdb);
            return NDBT_FAILED;
          }

          candidate_row_count++;
	
          if (scanRowString == indexRow.c_str()){
            //g_err << "Match found" << endl;
            // Found row, exit
            break;
          }
          //g_err << "No match between :" << endl;
          //g_err << "  scanRow  : \n" << scanRow.c_str().c_str() << endl;
          //g_err << "  indexRow : \n" << indexRow.c_str().c_str() << endl;
        }
        iop->close(false,true);  // Close, and release 'iop'
        iop = NULL;
      }
    } // while 'pOp->nextResult()'
    
    pOp->close();
    pOp = NULL;
    if (eof == -1 || check == -1) {
  error:
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	iop = 0;
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	rows--;
	continue;
      }
      NDB_ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
      
    closeTransaction(pNdb);
    
    return NDBT_OK;
  }
  abort(); /* Should never happen */
  return NDBT_FAILED;
}

int
UtilTransactions::verifyTableAndAllIndexes(Ndb* pNdb,
                                           bool findNulls,
                                           bool bidirectional,
                                           bool views,
                                           bool allSources)
{
  if (verifyTableReplicas(pNdb, allSources) != NDBT_OK)
  {
    return NDBT_FAILED;
  }

  return verifyAllIndexes(pNdb, findNulls, bidirectional, views);
}


int
UtilTransactions::verifyTableReplicas(Ndb* pNdb, bool allSources)
{
  Uint32 result = NDBT_OK;
  if (!allSources)
  {
    /* Any source */
    if (verifyTableReplicasWithSource(pNdb, 0) != NDBT_OK)
    {
      result = NDBT_FAILED;
    }
  }
  else
  {
    Ndb_cluster_connection* ncc = &pNdb->get_ndb_cluster_connection();
    Ndb_cluster_connection_node_iter nodeIter;
    ncc->init_get_next_node(nodeIter);
    unsigned int nodeId = 0;

    while ((nodeId = ncc->get_next_alive_node(nodeIter)) != 0)
    {
      if (verifyTableReplicasWithSource(pNdb, nodeId) != NDBT_OK)
      {
        result = NDBT_FAILED;
      }
    }
  }

  return result;
}

int
UtilTransactions::verifyTableReplicasPkCompareRow(Ndb* pNdb, Uint32 nodeId,
                                               const NDBT_ResultRow& scanRow) {
  NDBT_ResultRow       pkRow(tab);

  NdbTransaction* nodeTrans = pNdb->startTransaction(nodeId, 0);
  if (nodeTrans->getConnectedNodeId() != nodeId)
  {
    g_err << "Tried to start transaction on node " << nodeId
          << " but started on node " << nodeTrans->getConnectedNodeId() << endl;
    nodeTrans->close();
    return NDBT_FAILED;
  }

  // Do pk lookup using simpleRead
  NdbOperation * pk = nodeTrans->getNdbOperation(tab.getName());
  if(!pk || pk->simpleRead()) {
    NDB_ERR(nodeTrans->getNdbError());
    nodeTrans->close();
    return NDBT_FAILED;
  }

  if(equal(&tab, pk, scanRow) || get_values(pk, pkRow)){
    NDB_ERR(nodeTrans->getNdbError());
    nodeTrans->close();
    return NDBT_FAILED;
  }

  if (nodeTrans->execute(Commit, AbortOnError) != 0){
    const NdbError err = nodeTrans->getNdbError();
    NDB_ERR(err);
    nodeTrans->close();
    if (err.status == NdbError::TemporaryError)
      return NDBT_TEMPORARY;
    return NDBT_FAILED;
  }

  if(scanRow.c_str() != pkRow.c_str()){
    g_err << "Error when comparing records" << endl;
    g_err << " scanRow (from node  "
          << pTrans->getConnectedNodeId()
          << ") : \n" << scanRow.c_str().c_str() << endl;
    g_err << " pkRow from node " << nodeId << " : \n"
          << pkRow.c_str().c_str() << endl;
    return -1;
  }
  else
  {
     //g_err << "Pk ok" << endl;
  }

  nodeTrans->close();
  return NDBT_OK;
}


int
UtilTransactions::verifyTableReplicasScanAndCompareNodes(Ndb* pNdb,
                                                         Uint32 sourceNodeId,
                                                         Uint32 numDataNodes,
                                                         Uint32 dataNodes[256])
{
  pTrans = pNdb->startTransaction(sourceNodeId, 0);
  if (pTrans == NULL) {
    const NdbError err = pNdb->getNdbError();
    NDB_ERR(err);
    if (err.status == NdbError::TemporaryError)
      return NDBT_TEMPORARY;
    return NDBT_FAILED;
  }
  if (sourceNodeId && pTrans->getConnectedNodeId() != sourceNodeId)
  {
    g_err << "Transaction requested on node " << sourceNodeId
          << " but running on node " << pTrans->getConnectedNodeId()
          << ", failing..." << endl;
    return NDBT_FAILED;
  }

  /* Scan table */
  NdbScanOperation      *pScan = pTrans->getNdbScanOperation(tab.getName());

  if (pScan == NULL) {
    const NdbError err = pNdb->getNdbError();
    NDB_ERR(err);
    if (err.status == NdbError::TemporaryError)
      return NDBT_TEMPORARY;
    return NDBT_FAILED;
  }

  if( pScan->readTuples(NdbScanOperation::LM_Read) ) {
    const NdbError err = pNdb->getNdbError();
    NDB_ERR(err);
    if (err.status == NdbError::TemporaryError)
      return NDBT_TEMPORARY;
    return NDBT_FAILED;
  }

  NDBT_ResultRow       scanRow(tab);
  if(get_values(pScan, scanRow) != 0)
  {
    return NDBT_FAILED;
  }

  if( pTrans->execute(NoCommit, AbortOnError) != 0) {
    const NdbError err = pTrans->getNdbError();
    NDB_ERR(err);
    if (err.status == NdbError::TemporaryError){
      return NDBT_TEMPORARY;
    }
    return NDBT_FAILED;
  }

  int eof;
  int rows = 0;
  int checks = 0;
  int mismatchRows = 0;
  int mismatchReplicas = 0;
  while((eof = pScan->nextResult()) == 0){
    rows++;

    int mismatches = 0;
    for (Uint32 n=0; n < numDataNodes; n++)
    {
      const int result = verifyTableReplicasPkCompareRow(pNdb,
                                                         dataNodes[n],
                                                         scanRow);
      if (result == NDBT_OK)
        continue;

      if (result == -1) {
        // pk read detected mismatch
        mismatches++;
        continue;
      }

      // Error when reading row by pk
      pScan->close();
      return result;
    }

    checks += numDataNodes;
    mismatchReplicas += mismatches;
    if (mismatches)
      mismatchRows++;
  }

  pScan->close();

  // Check scan failure
  if (eof == -1) {
    const NdbError err = pTrans->getNdbError();
    NDB_ERR(err);
    if (err.status == NdbError::TemporaryError){
      return NDBT_TEMPORARY;
    }
    return NDBT_FAILED;
  }

  if (mismatchRows)
  {
    g_err << "|- Checked "
          << rows << " rows with "
          << checks << " checks across "
          << numDataNodes << " data nodes." << endl;
    g_err << "  Found "
          << mismatchReplicas << " mismatches in "
          << mismatchRows << " rows" << endl;
    return NDBT_FAILED;
  }

  if (m_verbosity > 0)
  {
    ndbout << "|- Checked "
           << rows
           << " rows with " << checks
           << " checks, no mismatches found." << endl;
  }

  return NDBT_OK;
}

int
UtilTransactions::verifyTableReplicasWithSource(Ndb* pNdb, Uint32 sourceNodeId)
{
  int                  retryAttempt = 0;
  const int            retryMax = 100;

  Uint32 numDataNodes = 0;
  Uint32 dataNodes[256];
  {
    Ndb_cluster_connection* ncc = &pNdb->get_ndb_cluster_connection();
    Ndb_cluster_connection_node_iter node_iter;
    ncc->init_get_next_node(node_iter);
    unsigned int node_id = ncc->get_next_alive_node(node_iter);
    while (node_id != 0)
    {
      dataNodes[numDataNodes++] = node_id;
      //ndbout_c("Found node %u", node_id);
      node_id = ncc->get_next_alive_node(node_iter);
    }
    //ndbout_c("Found %u data nodes", numDataNodes);
  }

  if (m_verbosity > 0)
  {
    ndbout_c("|- Checking replicas of table %s with source node %u from %u data nodes",
             tab.getName(),
             sourceNodeId,
             numDataNodes);
  }

  while (true){

    const int result = verifyTableReplicasScanAndCompareNodes(pNdb,
                                                              sourceNodeId,
                                                              numDataNodes,
                                                              dataNodes);
    closeTransaction(pNdb);

    if (result != NDBT_TEMPORARY) {
      return result;
    }

    retryAttempt++;
    if (retryAttempt >= retryMax) {
      g_err << "ERROR: has retried this operation " << retryAttempt
            << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    NdbSleep_MilliSleep(50);
  }

  abort(); /* Should never happen */
  return NDBT_FAILED;

}


int
UtilTransactions::verifyAllIndexes(Ndb* pNdb,
                                   bool findNulls,
                                   bool bidirectional,
                                   bool views)
{
  NdbDictionary::Dictionary::List indexList;

  if (pNdb->getDictionary()->listIndexes(indexList,
                                         tab.getName()) != 0)
  {
    ndbout << " Failed to list indexes on table " << tab.getName()
           << " Error " << pNdb->getDictionary()->getNdbError() << endl;
    return NDBT_FAILED;
  }

  for (unsigned i = 0; i < indexList.count; i++)
  {
    const char* indexName = indexList.elements[i].name;
//    ndbout << "Verifying table " << tab.getName()
//           << " index " << indexName
//           << endl;

    const NdbDictionary::Index* index = pNdb->getDictionary()->getIndex(indexName,
                                                                        tab);
    if (index == NULL)
    {
      g_err << "Failed to find index " << indexName << " on table "
            << tab.getName() << endl;
      return NDBT_FAILED;
    }

    /* Scan table, finding rows in index struct */
    if (verifyIndex(pNdb,
                    index,
                    false, /* Scan table, check index */
                    findNulls) != NDBT_OK)
    {
      return NDBT_FAILED;
    }

    if (bidirectional)
    {
      /* Scan index struct, finding rows in table */
      if (verifyIndex(pNdb,
                      index,
                      true, /* Scan table, check index */
                      findNulls) != NDBT_OK)
      {
        return NDBT_FAILED;
      }
    }


    if (views)
    {
      /**
       * Check that all data node's views of this index are
       * aligned
       */
      if (verifyIndexViews(pNdb,
                           index) != NDBT_OK)
      {
        return NDBT_FAILED;
      }
    }
  }

  return NDBT_OK;
}

int
UtilTransactions::verifyIndexViews(Ndb* pNdb,
                                   const NdbDictionary::Index* pIndex)
{
  switch (pIndex->getType()){
  case NdbDictionary::Index::UniqueHashIndex:
    /* Not yet implemented unique index view verification */
    return NDBT_OK;
  case NdbDictionary::Index::OrderedIndex:
    return verifyOrderedIndexViews(pNdb,
                                   pIndex);
  default:
    ndbout << "Unknown index type" << endl;
    break;
  }

  return NDBT_FAILED;
}

/**
 * verifyOrderedIndexViews
 * Verify views of an ordered index are the same from all
 * nodes
 */
int
UtilTransactions::verifyOrderedIndexViews(Ndb* pNdb,
                                          const NdbDictionary::Index* index)
{
  Uint32 numDataNodes = 0;
  Uint32 dataNodes[256];
  {
    Ndb_cluster_connection* ncc = &pNdb->get_ndb_cluster_connection();
    Ndb_cluster_connection_node_iter node_iter;
    ncc->init_get_next_node(node_iter);
    unsigned int node_id = ncc->get_next_alive_node(node_iter);
    while (node_id != 0)
    {
      dataNodes[numDataNodes++] = node_id;
      //ndbout_c("Found node %u", node_id);
      node_id = ncc->get_next_alive_node(node_iter);
    }
    //ndbout_c("Found %u data nodes", numDataNodes);
  }

  if (numDataNodes == 0)
  {
    /* No alive nodes */
    return NDBT_FAILED;
  }

  if (numDataNodes == 1)
  {
    /* No replicas */
    return NDBT_OK;
  }

  int result = NDBT_OK;

  /* Compare overlapping pairs of replicas */
  for (Uint32 comparison=0; comparison < numDataNodes -1; comparison++)
  {
    if (verifyTwoOrderedIndexViews(pNdb,
                                   index,
                                   dataNodes[comparison],
                                   dataNodes[comparison+1]) != NDBT_OK)
    {
      result = NDBT_FAILED;
    }
  }

  return result;
}


/**
 * verifyTwoOrderedIndexViews
 * Use an (ordered) zipper comparison to check that two
 * views of an ordered index (from different nodes) are
 * the same
 */
int
UtilTransactions::verifyTwoOrderedIndexViews(Ndb* pNdb,
                                             const NdbDictionary::Index* index,
                                             Uint32 node1,
                                             Uint32 node2)
{
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  NdbTransaction* scan1Trans = NULL;
  NdbTransaction* scan2Trans = NULL;
  NdbIndexScanOperation *scan1Op = NULL;
  NdbIndexScanOperation *scan2Op = NULL;

  NDBT_ResultRow       scan1row(tab);
  NDBT_ResultRow       scan2row(tab);

  if (m_verbosity > 0)
  {
    ndbout << "|- Checking views of ordered index "
           << index->getName()
           << " on table " << tab.getName()
           << " from two data nodes : "
           << node1 << ", " << node2
           << endl;
  }

  while (true){

    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    if (defineOrderedScan(pNdb, index, node1, scan1Trans, scan1Op, scan1row) != 0)
    {
      return NDBT_FAILED;
    }

    if (defineOrderedScan(pNdb, index, node2, scan2Trans, scan2Op, scan2row) != 0)
    {
      scan1Trans->close();
      return NDBT_FAILED;
    }

    int result = NDBT_OK;

    while (true)
    {
      /* Merge compare of ordered scan results */
      int eof1 = scan1Op->nextResult();
      int eof2 = scan2Op->nextResult();

      if (eof1 == -1 || eof2 == -1)
      {
        /* Error */
        const NdbError err = (eof1 == -1)?
          scan1Op->getNdbError():
          scan2Op->getNdbError();

        if (err.status == NdbError::TemporaryError)
        {
          NDB_ERR(err);
          goto temp_error;
        }
        NDB_ERR(err);
        scan1Trans->close();
        scan2Trans->close();
        return NDBT_FAILED;
      }

      if (eof1 || eof2)
      {
        if ((eof1 == 1) && (eof2 == 1))
        {
          /* Finished */
          break;
        }

        /* One scan finished before the other */
        g_err << "Error : Scan on node "
              << (eof1? node1 : node2)
              << " returned fewer rows." << endl;
        result = NDBT_FAILED;
        break;
      }

      if(scan1row.c_str() != scan2row.c_str()){
	g_err << "Error when comparing entries for index "
              << index->getName() << endl;
	g_err << " row from node " << node1 << " : \n"
              << scan1row.c_str().c_str() << endl;
	g_err << " row from node " << node2 << " : \n"
              << scan2row.c_str().c_str() << endl;
        result = NDBT_FAILED;
      }
    } // while 'pOp->nextResult()'

    scan1Trans->close();
    scan2Trans->close();

    return result;
temp_error:
    scan1Trans->close();
    scan2Trans->close();
    NdbSleep_MilliSleep(50);
    retryAttempt++;
  }
  abort(); /* Should never happen */
  return NDBT_FAILED;
}

int
UtilTransactions::defineOrderedScan(Ndb* pNdb,
                                    const NdbDictionary::Index* index,
                                    Uint32 nodeId,
                                    NdbTransaction*& scanTrans,
                                    NdbIndexScanOperation*& scanOp,
                                    NDBT_ResultRow& row)
{
  Uint32 retryAttempt = 0;
  Uint32 retryMax = 10;
  while (true){

    if (retryAttempt >= retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    NdbTransaction* trans = pNdb->startTransaction(nodeId, 0);
    if (trans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
        NDB_ERR(err);
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      NDB_ERR(err);
      return NDBT_FAILED;
    }

    if (trans->getConnectedNodeId() != nodeId)
    {
      g_err << "Failed to start transaction on node " << nodeId << endl;
      trans->close();
      return NDBT_FAILED;
    }

    NdbIndexScanOperation* op = trans->getNdbIndexScanOperation(index->getName(),
                                                                tab.getName());

    if (op == NULL) {
      NDB_ERR(trans->getNdbError());
      trans->close();
      return NDBT_FAILED;
    }

    if( op->readTuples(NdbScanOperation::LM_Read,
                       NdbScanOperation::SF_OrderBy) ) {
      NDB_ERR(trans->getNdbError());
      trans->close();
      return NDBT_FAILED;
    }

    if(get_values(op, row))
    {
      abort();
    }

    int check = trans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = trans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	trans->close();
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      trans->close();
      return NDBT_FAILED;
    }

    scanTrans = trans;
    scanOp = op;
    return NDBT_OK;
  }
}

int
UtilTransactions::get_values(NdbOperation* op, NDBT_ResultRow& dst)
{
  for (int a = 0; a < tab.getNoOfColumns(); a++){
    NdbRecAttr*& ref= dst.attributeStore(a);
    if ((ref= op->getValue(a)) == 0)
    {
      g_err << "Line: " << __LINE__ << " getValue failed" << endl;
      return NDBT_FAILED;
    }
  }
  return 0;
}

int
UtilTransactions::equal(const NdbDictionary::Index* pIndex, 
			NdbOperation* op,
                        const NDBT_ResultRow& src,
                        bool skipNull)
{
  for(Uint32 a = 0; a<pIndex->getNoOfColumns(); a++){
    const NdbDictionary::Column *  col = pIndex->getColumn(a);
    if (skipNull &&
        src.attributeStore(col->getName())->isNULL())
    {
      //ndbout_c("equal() exits before col %u", a);
      /* Have defined as many bounds as we can */
      return 0;
    }
    if(op->equal(col->getName(), 
		 src.attributeStore(col->getName())->aRef()) != 0){
      g_err << "Line: " << __LINE__ << " equal failed" << endl;
      return NDBT_FAILED;
    }
  }
  return 0;
}

int
UtilTransactions::equal(const NdbDictionary::Table* pTable, 
			NdbOperation* op, const NDBT_ResultRow& src)
{
  for(Uint32 a = 0; (int)a<tab.getNoOfColumns(); a++){
    const NdbDictionary::Column* attr = tab.getColumn(a);
    if (attr->getPrimaryKey() == true){
      if (op->equal(attr->getName(), src.attributeStore(a)->aRef()) != 0){
        g_err << "Line: " << __LINE__ << " equal failed" << endl;
	return NDBT_FAILED;
      }
    }
  }
  return 0;
}

NdbScanOperation*
UtilTransactions::getScanOperation(NdbConnection* pTrans)
{
  return (NdbScanOperation*)
    getOperation(pTrans, NdbOperation::OpenScanRequest);
}

NdbOperation*
UtilTransactions::getOperation(NdbConnection* pTrans,
			       NdbOperation::OperationType type)
{
  switch(type){
  case NdbOperation::ReadRequest:
  case NdbOperation::ReadExclusive:
    if(idx)
    {
      switch(idx->getType()){
      case NdbDictionary::Index::UniqueHashIndex:
	return pTrans->getNdbIndexOperation(idx->getName(), tab.getName());
      case NdbDictionary::Index::OrderedIndex:
	return pTrans->getNdbIndexScanOperation(idx->getName(), tab.getName());
      default:
        abort();
      }
    }
    return pTrans->getNdbOperation(tab.getName());
  case NdbOperation::InsertRequest:
  case NdbOperation::WriteRequest:
    return pTrans->getNdbOperation(tab.getName());
  case NdbOperation::UpdateRequest:
  case NdbOperation::DeleteRequest:
    if(idx)
    {
      switch(idx->getType()){
      case NdbDictionary::Index::UniqueHashIndex:
	return pTrans->getNdbIndexOperation(idx->getName(), tab.getName());
      default:
        break;
      }
    }
    return pTrans->getNdbOperation(tab.getName());
  case NdbOperation::OpenScanRequest:
    if(idx)
    {
      switch(idx->getType()){
      case NdbDictionary::Index::OrderedIndex:
	return pTrans->getNdbIndexScanOperation(idx->getName(), tab.getName());
      default:
        break;
      }
    }
    return pTrans->getNdbScanOperation(tab.getName());
  case NdbOperation::OpenRangeScanRequest:
    if(idx)
    {
      switch(idx->getType()){
      case NdbDictionary::Index::OrderedIndex:
	return pTrans->getNdbIndexScanOperation(idx->getName(), tab.getName());
      default:
        break;
      }
    }
    return 0;
  default:
    abort();
  }
  return 0;
}

#include <HugoOperations.hpp>

int
UtilTransactions::closeTransaction(Ndb* pNdb)
{
  if (pTrans != NULL){
    pNdb->closeTransaction(pTrans);
    pTrans = NULL;
  }
  return 0;
}

int 
UtilTransactions::compare(Ndb* pNdb, const char* tab_name2, int flags){


  NdbError err;
  int return_code= 0, row_count= 0;
  int retryAttempt = 0, retryMax = 10;

  HugoCalculator calc(tab);
  NDBT_ResultRow row(tab);
  const NdbDictionary::Table* tmp= pNdb->getDictionary()->getTable(tab_name2);
  if(tmp == 0)
  {
    g_err << "Unable to lookup table: " << tab_name2
	  << endl << pNdb->getDictionary()->getNdbError() << endl;
    return -1;
  }
  const NdbDictionary::Table& tab2= *tmp;

  HugoOperations cmp(tab2);
  UtilTransactions count(tab2);

  while (true){
loop:    
    if (retryAttempt++ >= retryMax){
      g_err << "ERROR: compare has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return -1;
    }

    NdbScanOperation *pOp= 0;
    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      err = pNdb->getNdbError();
      goto error;
    }

    pOp= pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      NDB_ERR(err= pTrans->getNdbError());
      goto error;
    }

    if( pOp->readTuples(NdbScanOperation::LM_Read) ) {
      NDB_ERR(err= pTrans->getNdbError());
      goto error;
    }

    // Read all attributes
    {
      for (int a = 0; a < tab.getNoOfColumns(); a++){
	if ((row.attributeStore(a) = 
	     pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	  NDB_ERR(err= pTrans->getNdbError());
	  goto error;
	}
      }
    }
    
    if( pTrans->execute(NoCommit, AbortOnError) == -1 ) {
      NDB_ERR(err= pTrans->getNdbError());
      goto error;
    }
  
    row_count= 0;
    {
      int eof;
      while((eof = pOp->nextResult(true)) == 0)
      {
	do {
	  row_count++;
	  if(cmp.startTransaction(pNdb) != NDBT_OK)
	  {
	    NDB_ERR(err= pNdb->getNdbError());
	    goto error;
	  }
	  int rowNo= calc.getIdValue(&row);
	  if(cmp.pkReadRecord(pNdb, rowNo, 1) != NDBT_OK)
	  {
	    NDB_ERR(err= cmp.getTransaction()->getNdbError());
	    goto error;
	  }
	  if(cmp.execute_Commit(pNdb) != NDBT_OK ||
	     cmp.getTransaction()->getNdbError().code)
	  {
	    NDB_ERR(err= cmp.getTransaction()->getNdbError());
	    goto error;
	  }
	  if(row != cmp.get_row(0))
	  {
	    g_err << "COMPARE FAILED" << endl;
	    g_err << row << endl;
	    g_err << cmp.get_row(0) << endl;
	    return_code++;
	  }
	  retryAttempt= 0;
	  cmp.closeTransaction(pNdb);
	} while((eof = pOp->nextResult(false)) == 0);

        if (eof == -1) break;
      }
      if (eof == -1) 
      {
	err = pTrans->getNdbError();
	goto error;
      }
    }
    
    closeTransaction(pNdb);
    
    g_info << row_count << " rows compared" << endl;
    {
      int row_count2;
      if(count.selectCount(pNdb, 0, &row_count2) != NDBT_OK)
      {
	g_err << "Failed to count rows in tab_name2" << endl;
	return -1;
      }
      
      g_info << row_count2 << " rows in tab_name2 - failed " << return_code
	     << endl;
      return (row_count == row_count2 ? return_code : 1);
    }
error:
    if(err.status == NdbError::TemporaryError)
    {
      g_err << err << endl;
      NdbSleep_MilliSleep(50);
      closeTransaction(pNdb);
      if(cmp.getTransaction())
	cmp.closeTransaction(pNdb);
      
      goto loop;
    }
    g_err << "ERROR" << endl;
    g_err << err << endl;
    
    break;
  }

  closeTransaction(pNdb);
  
  return return_code;
}

void
UtilTransactions::setVerbosity(Uint32 v)
{
  m_verbosity = v;
}

Uint32
UtilTransactions::getVerbosity() const
{
  return m_verbosity;
}
