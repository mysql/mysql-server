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
}

int 
UtilTransactions::clearTable2(Ndb* pNdb, 
			     int records,
			     int parallelism){
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
      goto failed;
    }
    
    NdbResultSet * rs = pOp->readTuplesExclusive(par);
    if( rs == 0 ) {
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
	pTrans->releaseCompletedOperations();
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
  NdbOperation		*pOp;
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

    pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    check = pOp->openScanRead(parallelism);
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
    
    check = pTrans->executeScan();   
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }
  
    int eof;
    NdbConnection* pInsTrans;

    while((eof = pTrans->nextScanResult(true)) == 0){
      pInsTrans = pNdb->startTransaction();
      if (pInsTrans == NULL) {
	const NdbError err = pNdb->getNdbError();
	ERR(err);
	pNdb->closeTransaction(pInsTrans);
	return NDBT_FAILED;
      }
      do {
	insertedRows++;
	if (addRowToInsert(pNdb, pInsTrans, row, destName) != 0){
	  pNdb->closeTransaction(pInsTrans);
	  pNdb->closeTransaction(pTrans);
	  return NDBT_FAILED;
	}
      } while((eof = pTrans->nextScanResult(false)) == 0);

      check = pInsTrans->execute(Commit);   
      if( check == -1 ) {
	const NdbError err = pInsTrans->getNdbError();    
	ERR(err);
	pNdb->closeTransaction(pInsTrans);
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }
      pNdb->closeTransaction(pInsTrans);

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
UtilTransactions::addRowToDelete(Ndb* pNdb,
				 NdbConnection* pDelTrans,
				 NdbOperation* pOrgOp){

  NdbOperation* pDelOp = pOrgOp->takeOverForDelete(pDelTrans);
  if (pDelOp == NULL){
    ERR(pNdb->getNdbError());
    return NDBT_FAILED;
  }
  return NDBT_OK;
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

// Take over one record from pOrgOp and delete it
int 
UtilTransactions::takeOverAndDeleteRecord(Ndb* pNdb, 
					  NdbOperation* pOrgOp){

  int                  retryAttempt = 0;
  const int            retryMax = 10;
  int check;
  NdbConnection      	*pDelTrans;
  NdbOperation       	*pDelOp;

  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pDelTrans = pNdb->startTransaction();
    if (pDelTrans == NULL) {
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

    if ((pDelOp = pOrgOp->takeOverForDelete(pDelTrans)) == NULL){
      ERR(pNdb->getNdbError());
      return NDBT_FAILED;
    }

#if 0
    // It should not be necessary to call deleteTuple HERE!!!
    check = pDelOp->deleteTuple();
    if( check == -1 ) {
      ERR(pDelTrans->getNdbError());
      pNdb->closeTransaction(pDelTrans);
      return NDBT_FAILED;
    }
#endif

    check = pDelTrans->execute( Commit );
    if(check == -1 ) {
      const NdbError err = pDelTrans->getNdbError();
      pNdb->closeTransaction(pDelTrans);

      ERR(err);
      if(err.code == 250 || err.code == 499)
	return RESTART_SCAN;
      
      switch(err.status){
      case NdbError::Success:
	g_info << "ERROR: NdbError reports success when transcaction failed"
	       << endl;
	RETURN_FAIL(err);
	break;

      case NdbError::TemporaryError:      
	NdbSleep_MilliSleep(50+50*retryAttempt);
	retryAttempt++;
	continue;
	break;

      case NdbError::UnknownResult:
	RETURN_FAIL(err);
	break;

      default:
      case NdbError::PermanentError:
	switch (err.classification){
	default:
	  RETURN_FAIL(err);
	  break;
	}
	break;
      }
    }
    else{
      pNdb->closeTransaction(pDelTrans);
    }

    return NDBT_OK;
  }
  return NDBT_FAILED;
}


 

int 
UtilTransactions::scanReadRecords(Ndb* pNdb,
				  int parallelism,
				  bool exclusive,
				  int records,
				  int noAttribs,
				  int *attrib_list,
				  ReadCallBackFn* fn){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbConnection	       *pTrans;
  NdbOperation	       *pOp;
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

    pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if (exclusive == true)
      check = pOp->openScanExclusive(parallelism);
    else
      check = pOp->openScanRead(parallelism);
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

    check = pTrans->executeScan();   
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
    eof = pTrans->nextScanResult();
    
    while(eof == 0){
      rows++;
      
      // Call callback for each record returned
      if(fn != NULL)
	fn(&row);
      eof = pTrans->nextScanResult();
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
			      ScanLock lock,
			      NdbConnection* pBuddyTrans){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbConnection	       *pTrans;
  NdbOperation	       *pOp;

  while (true){

    if (retryAttempt >= retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->hupp(pBuddyTrans);
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
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

    switch(lock){
    case SL_ReadHold:
      check = pOp->openScanReadHoldLock(parallelism);
      break;
    case SL_Exclusive:
      check = pOp->openScanExclusive(parallelism);
      break;
    case SL_Read:
    default:
      check = pOp->openScanRead(parallelism);
    }
    
    if( check == -1 ) {
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

  
    check = pTrans->executeScan();   
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    int eof;
    int rows = 0;
    eof = pTrans->nextScanResult();

    while(eof == 0){
      rows++;
      eof = pTrans->nextScanResult();
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
  case NdbDictionary::Index::OrderedIndex:
    return verifyUniqueIndex(pNdb, indexName, parallelism, transactional);
    break;
  default:
    ndbout << "Unknown index type" << endl;
    break;
  }
  
  return NDBT_FAILED;
}

int 
UtilTransactions::verifyUniqueIndex(Ndb* pNdb,
				    const char* indexName,
				    int parallelism,
				    bool transactional){
  
  /**
   * Scan all rows in TABLE and for each found row make one read in
   * TABLE and one using INDEX_TABLE. Then compare the two returned 
   * rows. They should be equal!
   *
   */

  if (scanAndCompareUniqueIndex(pNdb, 
				indexName,
				parallelism,
				transactional) != NDBT_OK){
    return NDBT_FAILED;
  }


  return NDBT_OK;
  
}


int 
UtilTransactions::scanAndCompareUniqueIndex(Ndb* pNdb,
					    const char * indexName,
					    int parallelism,
					    bool transactional){
  
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbConnection	       *pTrans;
  NdbOperation	       *pOp;
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

    pOp = pTrans->getNdbOperation(tab.getName());	
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      pNdb->closeTransaction(pTrans);
      return NDBT_FAILED;
    }

    if(transactional){
      check = pOp->openScanReadHoldLock(parallelism);
    } else {
      check = pOp->openScanRead(parallelism);
    }

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

    check = pTrans->executeScan();   
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
    eof = pTrans->nextScanResult();
    
    while(eof == 0){
      rows++;

      // ndbout << row.c_str().c_str() << endl;


      if (readRowFromTableAndIndex(pNdb,
				   pTrans,
				   indexName, 
				   row) != NDBT_OK){	
	pNdb->closeTransaction(pTrans);
	return NDBT_FAILED;
      }



      
      eof = pTrans->nextScanResult();
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
					   const char * indexName,
					   NDBT_ResultRow& row ){
  const NdbDictionary::Index* pIndex
    = pNdb->getDictionary()->getIndex(indexName, tab.getName());

  if (pIndex == 0){
    ndbout << " Index " << indexName << " does not exist!" << endl;
    return NDBT_FAILED;
  }

  NdbDictionary::Index::Type indexType= pIndex->getType();
  int                  retryAttempt = 0;
  const int            retryMax = 100;
  int                  check;
  NdbConnection	       *pTrans1=NULL;
  NdbResultSet         *cursor= NULL;
  NdbOperation	       *pOp;

  int return_code= NDBT_FAILED;

  // Allocate place to store the result
  NDBT_ResultRow       tabRow(tab);
  NDBT_ResultRow       indexRow(tab);


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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
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
    NdbScanOperation *pScanOp= NULL;
    {
      void* pOpCheck= NULL;
      if (indexType == NdbDictionary::Index::UniqueHashIndex) {
	pOpCheck= pIndexOp= pTrans1->getNdbIndexOperation(indexName, tab.getName());
      } else {
	pOpCheck= pScanOp= pTrans1->getNdbScanOperation(indexName, tab.getName());
      }

      if (pOpCheck == NULL) {
	ERR(pTrans1->getNdbError());
	goto close_all;
      }
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
    for(int a = 0; a<(int)pIndex->getNoOfColumns(); a++){
      const NdbDictionary::Column *  col = pIndex->getColumn(a);

      int r;
      if (pIndexOp)
	r = pIndexOp->equal(col->getName(), row.attributeStore(col->getName())->aRef());
      else {
	// setBound not possible for null attributes
	if ( !row.attributeStore(col->getName())->isNULL() ) {
	  r = pScanOp->setBound(col->getName(),
				NdbOperation::BoundEQ,
				row.attributeStore(col->getName())->aRef());
	}
      }
      if (r != 0){
	ERR(pTrans1->getNdbError());
	goto close_all;
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
    for(int a = 0; a<tab.getNoOfColumns(); a++){
      void* pCheck;

      if (pIndexOp)
	pCheck= indexRow.attributeStore(a)=
	  pIndexOp->getValue(tab.getColumn(a)->getName());
      else
	pCheck= indexRow.attributeStore(a)=
	  pScanOp->getValue(tab.getColumn(a)->getName());

      if(pCheck == NULL) {
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
