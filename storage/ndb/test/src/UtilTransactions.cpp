/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "UtilTransactions.hpp"
#include <NdbSleep.h>
#include <NdbScanFilter.hpp>

#define VERBOSE 0

UtilTransactions::UtilTransactions(const NdbDictionary::Table& _tab,
				   const NdbDictionary::Index* _idx):
  tab(_tab), idx(_idx), pTrans(0)
{
  m_defaultClearMethod = 3;
}

UtilTransactions::UtilTransactions(Ndb* ndb, 
				   const char * name,
				   const char * index) :
  tab(* ndb->getDictionary()->getTable(name)),
  idx(index ? ndb->getDictionary()->getIndex(index, name) : 0),
  pTrans(0)
{
  m_defaultClearMethod = 3;
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
	pTrans->restart();
      }
      
      err = pTrans->getNdbError();    
      if(check == -1){
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
      
      check = pTrans->execute(Commit, AbortOnError);   
      pTrans->restart();
      if( check == -1 ) {
	const NdbError err = pTrans->getNdbError();    
	NDB_ERR(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
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
			      bool transactional){
  

  const NdbDictionary::Index* pIndex
    = pNdb->getDictionary()->getIndex(indexName, tab.getName());
  if (pIndex == 0){
    ndbout << " Index " << indexName << " does not exist!" << endl;
    return NDBT_FAILED;
  }
  
  switch (pIndex->getType()){
  case NdbDictionary::Index::UniqueHashIndex:
    return verifyUniqueIndex(pNdb, pIndex, parallelism, transactional);
  case NdbDictionary::Index::OrderedIndex:
    return verifyOrderedIndex(pNdb, pIndex, parallelism, transactional);
    break;
  default:
    ndbout << "Unknown index type" << endl;
    break;
  }
  
  return NDBT_FAILED;
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
	ndbout << "Error when comapring records" << endl;
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
				     const NdbDictionary::Index* pIndex,
				     int parallelism,
				     bool transactional){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbScanOperation     *pOp = NULL;
  NdbIndexScanOperation *iop = NULL;

  NDBT_ResultRow       scanRow(tab);
  NDBT_ResultRow       pkRow(tab);
  NDBT_ResultRow       indexRow(tab);
  const char * indexName = pIndex->getName();

  int res;
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

    pOp = pTrans->getNdbScanOperation(tab.getName());	
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
        
    int eof;
    int rows = 0;
    while(check == 0 && (eof = pOp->nextResult()) == 0){
      rows++;

      bool null_found= false;
      for(int a = 0; a<(int)pIndex->getNoOfColumns(); a++){
	const NdbDictionary::Column *  col = pIndex->getColumn(a);
	if (scanRow.attributeStore(col->getName())->isNULL())
	{
	  null_found= true;
	  break;
	}
      }
      
      // Do pk lookup
      NdbOperation * pk = pTrans->getNdbOperation(tab.getName());
      if(!pk || pk->readTuple())
	goto error;
      if(equal(&tab, pk, scanRow) || get_values(pk, pkRow))
	goto error;

      if(!null_found)
      {
	if((iop= pTrans->getNdbIndexScanOperation(indexName, 
                                                  tab.getName())) != NULL)
	{
	  if(iop->readTuples(NdbScanOperation::LM_CommittedRead, 
			     parallelism))
	    goto error;
	  if(get_values(iop, indexRow))
	    goto error;
          if(equal(pIndex, iop, scanRow))
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

      if(scanRow.c_str() != pkRow.c_str()){
	g_err << "Error when comapring records" << endl;
	g_err << " scanRow: \n" << scanRow.c_str().c_str() << endl;
	g_err << " pkRow: \n" << pkRow.c_str().c_str() << endl;
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }

      if(!null_found)
      {
	if((res= iop->nextResult()) != 0){
	  g_err << "Failed to find row using index: " << res << endl;
	  NDB_ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	
	if(scanRow.c_str() != indexRow.c_str()){
	  g_err << "Error when comapring records" << endl;
	  g_err << " scanRow: \n" << scanRow.c_str().c_str() << endl;
	  g_err << " indexRow: \n" << indexRow.c_str().c_str() << endl;
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	
	if(iop->nextResult() == 0){
	  g_err << "Found extra row!!" << endl;
	  g_err << " indexRow: \n" << indexRow.c_str().c_str() << endl;
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
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
			NdbOperation* op, const NDBT_ResultRow& src)
{
  for(Uint32 a = 0; a<pIndex->getNoOfColumns(); a++){
    const NdbDictionary::Column *  col = pIndex->getColumn(a);
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

