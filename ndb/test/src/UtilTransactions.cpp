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

#include "UtilTransactions.hpp"
#include <NdbSleep.h>
#include <NdbScanFilter.hpp>

#define VERBOSE 0

UtilTransactions::UtilTransactions(const NdbDictionary::Table& _tab):
  tab(_tab){
  m_defaultClearMethod = 3;
}

UtilTransactions::UtilTransactions(Ndb* ndb, const char * name) :
  tab(* ndb->getDictionary()->getTable(name)){
  m_defaultClearMethod = 3;
}

#define RESTART_SCAN 99

#define RETURN_FAIL(err) return (err.code != 0 ? err.code : NDBT_FAILED) 

int 
UtilTransactions::clearTable(Ndb* pNdb, 
			     int records,
			     int parallelism){
  if(m_defaultClearMethod == 1){
    return clearTable1(pNdb, records, parallelism);
  } else if(m_defaultClearMethod == 2){
    return clearTable2(pNdb, records, parallelism);
  } else {
    return clearTable3(pNdb, records, parallelism);
  }
}


int 
UtilTransactions::clearTable1(Ndb* pNdb, 
			     int records,
			     int parallelism){
#if 1
  return clearTable3(pNdb, records, 1);
#else
  // Scan all records exclusive and delete 
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int check;
  NdbConnection		*pTrans;
  NdbOperation		*pOp;

  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: Has retried this operation " << retryAttempt
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }


    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      RETURN_FAIL(err);
    }

    pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      NdbError err = pNdb->getNdbError();
      ERR(err);
      pNdb->closeTransaction(pTrans);
      RETURN_FAIL(err);
    }

    check = pOp->openScanExclusive(parallelism);
    if( check == -1 ) {
      NdbError err = pNdb->getNdbError();
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      RETURN_FAIL(err);
    }

    check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      NdbError err = pNdb->getNdbError();
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      RETURN_FAIL(err);
    }  
#if 0
    // It's not necessary to read and PK's
    // Information about the PK's are sent in
    // KEYINFO20 signals anyway and used by takeOverScan

    // Read the primary keys from this table    
    for(int a=0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey()){
	if(pOp->getValue(tab.getColumn(a)->getName()) == NULL){
	  ERR(pTrans->getNdbError());
	  pNdb->closeTransaction(pTrans);
	  RETURN_FAIL(err);
	}
      }
    }
#endif

    check = pTrans->executeScan();   
    if( check == -1 ) {
      NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      RETURN_FAIL(err);
    }
  
    int eof;
    int rows = 0;

    eof = pTrans->nextScanResult();
    while(eof == 0){
      rows++;

      int res = takeOverAndDeleteRecord(pNdb, pOp);
      if(res == RESTART_SCAN){
	eof = -2;
	continue;
      }

      if (res != 0){      
	NdbError err = pNdb->getNdbError(res);
	pNdb->closeTransaction(pTrans);
	RETURN_FAIL(err);
      }
      
      eof = pTrans->nextScanResult();
    }
  
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	// If error = 488 there should be no limit on number of retry attempts
	if (err.code != 488) 
	  retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      RETURN_FAIL(err);
    }

    if(eof == -2){
      pNdb->closeTransaction(pTrans);
      NdbSleep_MilliSleep(50);
      retryAttempt++;
      continue;
    }
    
    pNdb->closeTransaction(pTrans);

    g_info << rows << " deleted" << endl;

    return NDBT_OK;
  }
  return NDBT_FAILED;
#endif
}

int 
UtilTransactions::clearTable2(Ndb* pNdb, 
			     int records,
			     int parallelism){
#if 1
  return clearTable3(pNdb, records, parallelism);
#else
  // Scan all records exclusive and delete 
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int deletedRows = 0;
  int check;
  NdbConnection		*pTrans;
  NdbOperation		*pOp;

  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }


    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    check = pOp->openScanExclusive(parallelism);
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }  
#if 0
    // It's not necessary to read any PK's
    // Information about the PK's are sent in
    // KEYINFO20 signals anyway and used by takeOverScan

    // Read the primary keys from this table    
    for(int a=0; a<tab.getNoOfColumns(); a++){
      if (tab.getColumn(a)->getPrimaryKey()){
	if(pOp->getValue(tab.getColumn(a)->getName()) == NULL){
	  ERR(pTrans->getNdbError());
	  pNdb->closeTransaction(pTrans);
	  return -1;
	}
      }
    }
#endif

    check = pTrans->executeScan();   
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
  
    int eof;
    NdbConnection* pDelTrans;

    while((eof = pTrans->nextScanResult(true)) == 0){
      pDelTrans = pNdb->startTransaction();
      if (pDelTrans == NULL) {
	const NdbError err = pNdb->getNdbError();
#if 0
	if (err.status == NdbError::TemporaryError){
	  ERR(err);
	  NdbSleep_MilliSleep(50);
	  retryAttempt++;
	  continue;
	}
#endif
	ERR(err);
	pNdb->closeTransaction(pDelTrans);
	return NDBT_FAILED;
      }
      do {
	deletedRows++;
	if (addRowToDelete(pNdb, pDelTrans, pOp) != 0){
	  pNdb->closeTransaction(pDelTrans);
	  pNdb->closeTransaction(pTrans);
	  return NDBT_FAILED;
	}
      } while((eof = pTrans->nextScanResult(false)) == 0);

      check = pDelTrans->execute(Commit);   
      if( check == -1 ) {
	const NdbError err = pDelTrans->getNdbError();    
	ERR(err);
	pNdb->closeTransaction(pDelTrans);
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
      pNdb->closeTransaction(pDelTrans);

    }  
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	// If error = 488 there should be no limit on number of retry attempts
	if (err.code != 488) 
	  retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    pNdb->closeTransaction(pTrans);

    g_info << deletedRows << " rows deleted" << endl;

    return NDBT_OK;
  }
  return NDBT_FAILED;
#endif
}

int 
UtilTransactions::clearTable3(Ndb* pNdb, 
			     int records,
			     int parallelism){
  // Scan all records exclusive and delete 
  // them one by one
  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int deletedRows = 0;
  int check;
  NdbConnection *pTrans;
  NdbScanOperation *pOp;
  NdbError err;

  int par = parallelism;
  while (true){
  restart:
    if (retryAttempt++ >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }
    
    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      err = pNdb->getNdbError();
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	continue;
      }
      goto failed;
    }
    
    pOp = pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      err = pTrans->getNdbError();
      if(err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	par = 1;
	goto restart;
      }
      goto failed;
    }
    
    NdbResultSet * rs = pOp->readTuplesExclusive(par);
    if( rs == 0 ) {
      err = pTrans->getNdbError();
      goto failed;
    }
    
    if(pTrans->execute(NoCommit) != 0){
      err = pTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	continue;
      }
      goto failed;
    }
    
    while((check = rs->nextResult(true)) == 0){
      do {
	if (rs->deleteTuple() != 0){
	  goto failed;
	}
	deletedRows++;
      } while((check = rs->nextResult(false)) == 0);
      
      if(check != -1){
	check = pTrans->execute(Commit);   
	pTrans->restart();
      }
      
      err = pTrans->getNdbError();    
      if(check == -1){
	if(err.status == NdbError::TemporaryError){
	  ERR(err);
	  pNdb->closeTransaction(pTrans);
	  NdbSleep_MilliSleep(50);
	  par = 1;
	  goto restart;
	}
	goto failed;
      }
    }
    if(check == -1){
      err = pTrans->getNdbError();    
      if(err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	par = 1;
	goto restart;
      }
      goto failed;
    }
    pNdb->closeTransaction(pTrans);
    return NDBT_OK;
  }
  return NDBT_FAILED;
  
 failed:
  if(pTrans != 0) pNdb->closeTransaction(pTrans);
  ERR(err);
  return (err.code != 0 ? err.code : NDBT_FAILED);
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
  NdbConnection		*pTrans;
  NdbScanOperation		*pOp;
  NDBT_ResultRow       row(tab);
  
  while (true){
    
    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }


    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    NdbResultSet* rs = pOp->readTuples(NdbScanOperation::LM_Read, 
				       parallelism);
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }  

    // Read all attributes
    for (int a = 0; a < tab.getNoOfColumns(); a++){
      if ((row.attributeStore(a) =  
	   pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
    }
    
    check = pTrans->execute(NoCommit);
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
  
    int eof;
    while((eof = rs->nextResult(true)) == 0){
      do {
	insertedRows++;
	if (addRowToInsert(pNdb, pTrans, row, destName) != 0){
	  pNdb->closeTransaction(pTrans);
	  return NDBT_FAILED;
	}
      } while((eof = rs->nextResult(false)) == 0);
      
      check = pTrans->execute(Commit);   
      pTrans->restart();
      if( check == -1 ) {
	const NdbError err = pTrans->getNdbError();    
	ERR(err);
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
    }  
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	// If error = 488 there should be no limit on number of retry attempts
	if (err.code != 488) 
	  retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    pNdb->closeTransaction(pTrans);
    
    g_info << insertedRows << " rows copied" << endl;
    
    return NDBT_OK;
  }
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
    ERR(pInsTrans->getNdbError());
    return NDBT_FAILED;
  }
  
  check = pInsOp->insertTuple();
  if( check == -1 ) {
    ERR(pInsTrans->getNdbError());
    return NDBT_FAILED;
  }

  // Set all attributes
  for (int a = 0; a < tab.getNoOfColumns(); a++){
    NdbRecAttr* r =  row.attributeStore(a);
    int	 sz = r->attrSize() * r->arraySize();
    if (pInsOp->setValue(tab.getColumn(a)->getName(),
			 r->aRef(),
			 sz) != 0) {
      ERR(pInsTrans->getNdbError());
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
  NdbConnection	       *pTrans;
  NdbScanOperation	       *pOp;
  NDBT_ResultRow       row(tab);

  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      const NdbError err = pNdb->getNdbError();
      pNdb->closeTransaction(pTrans);

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      return NDBT_FAILED;
    }

    NdbResultSet * rs = pOp->readTuples(lm, 0, parallelism);
    if( rs == 0 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    // Call getValue for all the attributes supplied in attrib_list
    // ************************************************
    for (int a = 0; a < noAttribs; a++){
      if (attrib_list[a] < tab.getNoOfColumns()){ 
	g_info << "getValue(" << attrib_list[a] << ")" << endl;
	if ((row.attributeStore(attrib_list[a]) =  
	     pOp->getValue(tab.getColumn(attrib_list[a])->getName())) == 0) {
	  ERR(pTrans->getNdbError());
	  pNdb->closeTransaction(pTrans);
	  return NDBT_FAILED;
	}
      }
    }
    // *************************************************
    
    check = pTrans->execute(NoCommit);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    int eof;
    int rows = 0;
    
    
    while((eof = rs->nextResult()) == 0){
      rows++;
      
      // Call callback for each record returned
      if(fn != NULL)
	fn(&row);
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    pNdb->closeTransaction(pTrans);
    g_info << rows << " rows have been read" << endl;
    if (records != 0 && rows != records){
      g_info << "Check expected number of records failed" << endl 
	     << "  expected=" << records <<", " << endl
	     << "  read=" << rows << endl;
      return NDBT_FAILED;
    }
    
    return NDBT_OK;
  }
  return NDBT_FAILED;
}

int 
UtilTransactions::selectCount(Ndb* pNdb, 
			      int parallelism,
			      int* count_rows,
			      NdbOperation::LockMode lm,
			      NdbConnection* pTrans){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbScanOperation     *pOp;

  if(!pTrans)
    pTrans = pNdb->startTransaction();
  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }
    pOp = pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    NdbResultSet * rs = pOp->readTuples(lm);
    if( rs == 0) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if(0){
      NdbScanFilter sf(pOp);
      sf.begin(NdbScanFilter::OR);
      sf.eq(2, (Uint32)30);
      sf.end();
    } else {
      check = pOp->interpret_exit_ok();
      if( check == -1 ) {
	ERR(pTrans->getNdbError());
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
    }
    
    
    check = pTrans->execute(NoCommit);
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    int eof;
    int rows = 0;
    

    while((eof = rs->nextResult()) == 0){
      rows++;
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    pNdb->closeTransaction(pTrans);
    
    if (count_rows != NULL){
      *count_rows = rows;
    }
    
    return NDBT_OK;
  }
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
  NdbConnection	       *pTrans;
  NdbScanOperation       *pOp;
  NDBT_ResultRow       row(tab);

  parallelism = 1;

  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      const NdbError err = pNdb->getNdbError();
      pNdb->closeTransaction(pTrans);
      ERR(err);
      
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      return NDBT_FAILED;
    }

    NdbResultSet* rs;
    if(transactional){
      rs = pOp->readTuples(NdbScanOperation::LM_Read, 0, parallelism);
    } else {
      rs = pOp->readTuples(NdbScanOperation::LM_CommittedRead, 0, parallelism);
    }
    
    if( rs == 0 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    // Read all attributes
    for (int a = 0; a < tab.getNoOfColumns(); a++){
      if ((row.attributeStore(a) =  
	   pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
    }

    check = pTrans->execute(NoCommit);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    int eof;
    int rows = 0;
    
    
    while((eof = rs->nextResult()) == 0){
      rows++;
      
      // ndbout << row.c_str().c_str() << endl;
      
      
      if (readRowFromTableAndIndex(pNdb,
				   pTrans,
				   pIndex,
				   row) != NDBT_OK){	
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	rows--;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
    
    pNdb->closeTransaction(pTrans);
    
    return NDBT_OK;
  }
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
  NdbResultSet         *cursor= NULL;
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
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
	goto close_all;
    }

    pTrans1 = pNdb->hupp(scanTrans); //startTransaction();
    if (pTrans1 == NULL) {
      const NdbError err = pNdb->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }

      if(err.code == 0){
	return_code = NDBT_OK;
	goto close_all;
      }
      ERR(err);
      goto close_all;
    }

    /**
     * Read the record from TABLE
     */
    pOp = pTrans1->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans1->getNdbError());
      goto close_all;
    }
    
    check = pOp->readTuple();
    if( check == -1 ) {
      ERR(pTrans1->getNdbError());
      pNdb->closeTransaction(pTrans1);
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
	  ERR(pTrans1->getNdbError());
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
	ERR(pTrans1->getNdbError());
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
	ERR(pTrans1->getNdbError());
	goto close_all;
      }
    
      {
	bool not_ok;
	if (pIndexOp) {
	  not_ok = pIndexOp->readTuple() == -1;
	} else {
	  not_ok = (cursor= pScanOp->readTuples()) == 0;
	}
	
	if( not_ok ) {
	  ERR(pTrans1->getNdbError());
	  goto close_all;
	}
      }
    
    // Define primary keys for index
#if VERBOSE
      printf("SI: ");
#endif
      for(a = 0; a<(int)pIndex->getNoOfColumns(); a++){
	const NdbDictionary::Column *  col = pIndex->getColumn(a);
	
	int r;
	if ( !row.attributeStore(col->getName())->isNULL() ) {
	  if(pIOp->equal(col->getName(), 
			 row.attributeStore(col->getName())->aRef()) != 0){
	    ERR(pTrans1->getNdbError());
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
	  ERR(pTrans1->getNdbError());
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
    
    check = pTrans1->execute(Commit);
    if( check == -1 ) {
      const NdbError err = pTrans1->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans1);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ndbout << "Error when comparing records - normal op" << endl;
      ERR(err);
      ndbout << "row: " << row.c_str().c_str() << endl;
      goto close_all;
    } 
    
    /** 
     * Compare the two rows
     */ 
    if(!null_found){
      if (pScanOp) {
	if (cursor->nextResult() != 0){
	  const NdbError err = pTrans1->getNdbError();
	  ERR(err);
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
	if (cursor->nextResult() == 0){
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
  if (cursor)
    cursor->close();
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
  NdbConnection	       *pTrans;
  NdbScanOperation     *pOp;
  NdbIndexScanOperation * iop = 0;
  NdbResultSet* cursor= 0;

  NDBT_ResultRow       scanRow(tab);
  NDBT_ResultRow       pkRow(tab);
  NDBT_ResultRow       indexRow(tab);
  const char * indexName = pIndex->getName();

  int res;
  parallelism = 1;
  
  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      return NDBT_FAILED;
    }

    pOp = pTrans->getNdbScanOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    NdbResultSet* 
      rs = pOp->readTuples(NdbScanOperation::LM_Read, 0, parallelism);
    
    if( rs == 0 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if(get_values(pOp, scanRow))
    {
      abort();
    }

    check = pTrans->execute(NoCommit);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
        
    int eof;
    int rows = 0;
    while(check == 0 && (eof = rs->nextResult()) == 0){
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
	if(!iop && (iop= pTrans->getNdbIndexScanOperation(indexName, 
							  tab.getName())))
	{
	  cursor= iop->readTuples(NdbScanOperation::LM_CommittedRead, 
				  parallelism);
	  iop->interpret_exit_ok();
	  if(!cursor || get_values(iop, indexRow))
	    goto error;
	}
	else if(!iop || iop->reset_bounds())
	{
	  goto error;
	}
	
	if(equal(pIndex, iop, scanRow))
	  goto error;
      }     

      check = pTrans->execute(NoCommit);
      if(check)
	goto error;

      if(scanRow.c_str() != pkRow.c_str()){
	g_err << "Error when comapring records" << endl;
	g_err << " scanRow: \n" << scanRow.c_str().c_str() << endl;
	g_err << " pkRow: \n" << pkRow.c_str().c_str() << endl;
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }

      if(!null_found)
      {
	
	if((res= cursor->nextResult()) != 0){
	  g_err << "Failed to find row using index: " << res << endl;
	  ERR(pTrans->getNdbError());
	  pNdb->closeTransaction(pTrans);
	  return NDBT_FAILED;
	}
	
	if(scanRow.c_str() != indexRow.c_str()){
	  g_err << "Error when comapring records" << endl;
	  g_err << " scanRow: \n" << scanRow.c_str().c_str() << endl;
	  g_err << " indexRow: \n" << indexRow.c_str().c_str() << endl;
	  pNdb->closeTransaction(pTrans);
	  return NDBT_FAILED;
	}
	
	if(cursor->nextResult() == 0){
	  g_err << "Found extra row!!" << endl;
	  g_err << " indexRow: \n" << indexRow.c_str().c_str() << endl;
	  pNdb->closeTransaction(pTrans);
	  return NDBT_FAILED;
	}
      }
    }
    
    if (eof == -1 || check == -1) {
  error:
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	iop = 0;
	pNdb->closeTransaction(pTrans);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	rows--;
	continue;
      }
      ERR(err);
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
      
    pNdb->closeTransaction(pTrans);
    
    return NDBT_OK;
  }
  return NDBT_FAILED;
}

int
UtilTransactions::get_values(NdbOperation* op, NDBT_ResultRow& dst)
{
  for (int a = 0; a < tab.getNoOfColumns(); a++){
    NdbRecAttr*& ref= dst.attributeStore(a);
    if ((ref= op->getValue(a)) == 0)
    {
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
      return NDBT_FAILED;
    }
  }
  return 0;
}

int
UtilTransactions::equal(const NdbDictionary::Table* pTable, 
			NdbOperation* op, const NDBT_ResultRow& src)
{
  for(Uint32 a = 0; a<tab.getNoOfColumns(); a++){
    const NdbDictionary::Column* attr = tab.getColumn(a);
    if (attr->getPrimaryKey() == true){
      if (op->equal(attr->getName(), src.attributeStore(a)->aRef()) != 0){
	return NDBT_FAILED;
      }
    }
  }
  return 0;
}
