/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "HugoTransactions.hpp"
#include <NDBT_Stats.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>

HugoTransactions::HugoTransactions(const NdbDictionary::Table& _tab,
				   const NdbDictionary::Index* idx):
  HugoOperations(_tab, idx),
  row(_tab){

  m_defaultScanUpdateMethod = 3;
  setRetryMax();
  m_stats_latency = 0;

  m_thr_count = 0;
  m_thr_no = -1;
}

HugoTransactions::~HugoTransactions(){
  deallocRows();
}

int
HugoTransactions::scanReadRecords(Ndb* pNdb, 
				  int records,
				  int abortPercent,
				  int parallelism, 
				  NdbOperation::LockMode lm,
                                  int scan_flags)
{
  
  int                  retryAttempt = 0;
  int                  check, a;
  NdbScanOperation	       *pOp;

  while (true){

    if (retryAttempt >= m_retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
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

    pOp = getScanOperation(pTrans);
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if( pOp ->readTuples(lm, scan_flags, parallelism) ) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
  
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if((row.attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }

    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // Abort after 1-100 or 1-records rows
    int ranVal = rand();
    int abortCount = ranVal % (records == 0 ? 100 : records); 
    bool abortTrans = false;
    if (abort > 0){
      // Abort if abortCount is less then abortPercent 
      if (abortCount < abortPercent) 
	abortTrans = true;
    }
    
    int eof;
    int rows = 0;
    while((eof = pOp->nextResult(true)) == 0){
      rows++;
      if (calc.verifyRowValues(&row) != 0){
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }

      if (abortCount == rows && abortTrans == true){
	ndbout << "Scan is aborted" << endl;
	g_info << "Scan is aborted" << endl;
	pOp->close();
	if( check == -1 ) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	
	closeTransaction(pNdb);
	return NDBT_OK;
      }
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR_INFO(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	switch (err.code){
	case 488:
	case 245:
	case 490:
	  // Too many active scans, no limit on number of retry attempts
	  break;
	default:
	  retryAttempt++;
	}
	continue;
      }
      ERR(err);
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
  return NDBT_FAILED;
}

int
HugoTransactions::scanReadRecords(Ndb* pNdb, 
				  const NdbDictionary::Index * pIdx,
				  int records,
				  int abortPercent,
				  int parallelism, 
				  NdbOperation::LockMode lm,
                                  int scan_flags)
{
  
  int                  retryAttempt = 0;
  int                  check, a;
  NdbIndexScanOperation	       *pOp;

  while (true){

    if (retryAttempt >= m_retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
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

    pOp = pTrans->getNdbIndexScanOperation(pIdx->getName(), tab.getName());
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if( pOp ->readTuples(lm, scan_flags, parallelism) ) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    check = pOp->interpret_exit_ok();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
  
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if((row.attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	ERR(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }

    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // Abort after 1-100 or 1-records rows
    int ranVal = rand();
    int abortCount = ranVal % (records == 0 ? 100 : records); 
    bool abortTrans = false;
    if (abort > 0){
      // Abort if abortCount is less then abortPercent 
      if (abortCount < abortPercent) 
	abortTrans = true;
    }
    
    int eof;
    int rows = 0;
    while((eof = pOp->nextResult(true)) == 0){
      rows++;
      if (calc.verifyRowValues(&row) != 0){
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }

      if (abortCount == rows && abortTrans == true){
	ndbout << "Scan is aborted" << endl;
	g_info << "Scan is aborted" << endl;
	pOp->close();
	if( check == -1 ) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	
	closeTransaction(pNdb);
	return NDBT_OK;
      }
    }
    if (eof == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR_INFO(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	switch (err.code){
	case 488:
	case 245:
	case 490:
	  // Too many active scans, no limit on number of retry attempts
	  break;
	default:
	  retryAttempt++;
	}
	continue;
      }
      ERR(err);
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
  return NDBT_FAILED;
}


#define RESTART_SCAN 99

int
HugoTransactions::scanUpdateRecords(Ndb* pNdb, 
                                    NdbScanOperation::ScanFlag flags,
                                    int records,
                                    int abortPercent,
                                    int parallelism){
  int retryAttempt = 0;
  int check, a;
  NdbScanOperation *pOp;

  while (true){
restart:
    if (retryAttempt++ >= m_retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();
      ERR(err);
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	continue;
      }
      return NDBT_FAILED;
    }

    pOp = getScanOperation(pTrans);
    if (pOp == NULL) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    if( pOp->readTuples(NdbOperation::LM_Exclusive, flags,
                        parallelism))
    {
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    // Read all attributes from this table    
    for(a=0; a<tab.getNoOfColumns(); a++){
      if((row.attributeStore(a) = pOp->getValue(tab.getColumn(a)->getName())) == NULL){
	ERR(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }
    
    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      ERR(err);
      closeTransaction(pNdb);
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	continue;
      }
      return NDBT_FAILED;
    }

    // Abort after 1-100 or 1-records rows
    int ranVal = rand();
    int abortCount = ranVal % (records == 0 ? 100 : records); 
    bool abortTrans = false;
    if (abort > 0){
      // Abort if abortCount is less then abortPercent 
      if (abortCount < abortPercent) 
	abortTrans = true;
    }
    
    int rows = 0;
    while((check = pOp->nextResult(true)) == 0){
      do {
	rows++;
	NdbOperation* pUp = pOp->updateCurrentTuple();
	if(pUp == 0){
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	const int updates = calc.getUpdatesValue(&row) + 1;
	const int r = calc.getIdValue(&row);
        
  	for(a = 0; a<tab.getNoOfColumns(); a++){
	  if (tab.getColumn(a)->getPrimaryKey() == false){
	    if(setValueForAttr(pUp, a, r, updates ) != 0){
	      ERR(pTrans->getNdbError());
	      closeTransaction(pNdb);
	      return NDBT_FAILED;
	    }
	  }
	}
        
	if (rows == abortCount && abortTrans == true){
	  g_info << "Scan is aborted" << endl;
	  // This scan should be aborted
	  closeTransaction(pNdb);
	  return NDBT_OK;
	}
      } while((check = pOp->nextResult(false)) == 0);

      if(check != -1){
	check = pTrans->execute(Commit, AbortOnError);   
	if(check != -1)
	  m_latest_gci = pTrans->getGCI();
	pTrans->restart();
      }

      const NdbError err = pTrans->getNdbError();    
      if( check == -1 ) {
	closeTransaction(pNdb);
	ERR(err);
	if (err.status == NdbError::TemporaryError){
	  NdbSleep_MilliSleep(50);
	  goto restart;
	}
	return NDBT_FAILED;
      }
    }
    
    const NdbError err = pTrans->getNdbError();    
    if( check == -1 ) {
      closeTransaction(pNdb);
      ERR(err);
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	goto restart;
      }
      return NDBT_FAILED;
    }
    
    closeTransaction(pNdb);
    
    g_info << rows << " rows have been updated" << endl;
    return NDBT_OK;
  }
  return NDBT_FAILED;
}

int
HugoTransactions::scanUpdateRecords(Ndb* pNdb, 
				    int records,
				    int abortPercent,
				    int parallelism){

  return scanUpdateRecords(pNdb, 
                           (NdbScanOperation::ScanFlag)0, 
                           records, abortPercent, parallelism);
}

// Scan all records exclusive and update 
// them one by one
int
HugoTransactions::scanUpdateRecords1(Ndb* pNdb, 
				     int records,
				     int abortPercent,
				     int parallelism){
  return scanUpdateRecords(pNdb, 
                           (NdbScanOperation::ScanFlag)0, 
                           records, abortPercent, 1);
}

// Scan all records exclusive and update 
// them batched by asking nextScanResult to
// give us all cached records before fetching new 
// records from db
int
HugoTransactions::scanUpdateRecords2(Ndb* pNdb, 
				     int records,
				     int abortPercent,
				     int parallelism){
  return scanUpdateRecords(pNdb, (NdbScanOperation::ScanFlag)0, 
                           records, abortPercent, parallelism);
}

int
HugoTransactions::scanUpdateRecords3(Ndb* pNdb, 
				     int records,
				     int abortPercent,
				     int parallelism)
{
  return scanUpdateRecords(pNdb, (NdbScanOperation::ScanFlag)0, 
                           records, abortPercent, parallelism);
}

int
HugoTransactions::loadTable(Ndb* pNdb, 
			    int records,
			    int batch,
			    bool allowConstraintViolation,
			    int doSleep,
                            bool oneTrans,
			    int value,
			    bool abort)
{
  return loadTableStartFrom(pNdb, 0, records, batch, allowConstraintViolation,
                            doSleep, oneTrans, value, abort);
}

int
HugoTransactions::loadTableStartFrom(Ndb* pNdb, 
                                     int startFrom,
                                     int records,
                                     int batch,
                                     bool allowConstraintViolation,
                                     int doSleep,
                                     bool oneTrans,
                                     int value,
                                     bool abort){
  int             check;
  int             retryAttempt = 0;
  int             retryMax = 5;
  bool            first_batch = true;

  const int org = batch;
  const int cols = tab.getNoOfColumns();
  const int brow = tab.getRowSizeInBytes();
  const int bytes = 12 + brow + 4 * cols;
  batch = (batch * 256); // -> 512 -> 65536k per commit
  batch = batch/bytes;   // 
  batch = batch == 0 ? 1 : batch;
 
  if(batch != org){
    g_info << "batch = " << org << " rowsize = " << bytes
	   << " -> rows/commit = " << batch << endl;
  }
  
  //Uint32 orgbatch = batch;
  g_info << "|- Inserting records..." << endl;
  for (int c=0 ; c<records; ){
    bool closeTrans = true;

    if(c + batch > records)
      batch = records - c;
    
    if (retryAttempt >= retryMax){
      g_info << "Record " << c << " could not be inserted, has retried "
	     << retryAttempt << " times " << endl;
      // Reset retry counters and continue with next record
      retryAttempt = 0;
      c++;
    }
    if (doSleep > 0)
      NdbSleep_MilliSleep(doSleep);

    //    if (first_batch || !oneTrans) {
    if (first_batch || !pTrans) {
      first_batch = false;
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
    }

    if(pkInsertRecord(pNdb, c + startFrom, batch, value) != NDBT_OK)
    { 
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    // Execute the transaction and insert the record
    if (!oneTrans || (c + batch) >= records) {
      //      closeTrans = true;
      closeTrans = false;
      if (!abort)
      {
	check = pTrans->execute(Commit, AbortOnError);
	if(check != -1)
	  pTrans->getGCI(&m_latest_gci);
	pTrans->restart();
      }
      else
      {
	check = pTrans->execute(NoCommit, AbortOnError);
	if (check != -1)
	{
	  check = pTrans->execute( Rollback );	
	  closeTransaction(pNdb);
	}
      }
    } else {
      closeTrans = false;
      check = pTrans->execute(NoCommit, AbortOnError);
    }
    if(check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      closeTransaction(pNdb);
      pTrans= 0;
      switch(err.status){
      case NdbError::Success:
	ERR(err);
	g_info << "ERROR: NdbError reports success when transcaction failed"
	       << endl;
	return NDBT_FAILED;
	break;
	
      case NdbError::TemporaryError:      
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
        batch = 1;
	continue;
	break;
	
      case NdbError::UnknownResult:
	ERR(err);
	return NDBT_FAILED;
	break;
	
      case NdbError::PermanentError:
	if (allowConstraintViolation == true){
	  switch (err.classification){
	  case NdbError::ConstraintViolation:
	    // Tuple already existed, OK but should be reported
	    g_info << c << ": " << err.code << " " << err.message << endl;
	    c++;
	    continue;
	    break;
	  default:	    
	    break;
	  }
	}
	ERR(err);
	return err.code;
	break;
      }
    }
    else{
      if (closeTrans) {
        closeTransaction(pNdb);
	pTrans= 0;
      }
    }
    
    // Step to next record
    c = c+batch; 
    retryAttempt = 0;
  }

  if(pTrans)
    closeTransaction(pNdb);
  return NDBT_OK;
}

int
HugoTransactions::fillTable(Ndb* pNdb, 
                                     int batch){
  return fillTableStartFrom(pNdb, 0, batch);
}

int
HugoTransactions::fillTableStartFrom(Ndb* pNdb, 
                                     int startFrom,
                                     int batch){
  int             check;
  int             retryAttempt = 0;
  int             retryMax = 5;

  const int org = batch;
  const int cols = tab.getNoOfColumns();
  const int brow = tab.getRowSizeInBytes();
  const int bytes = 12 + brow + 4 * cols;
  batch = (batch * 256); // -> 512 -> 65536k per commit
  batch = batch/bytes;   // 
  batch = batch == 0 ? 1 : batch;
 
  if(batch != org){
    g_info << "batch = " << org << " rowsize = " << bytes
	   << " -> rows/commit = " << batch << endl;
  }
  
  for (int c=startFrom ; ; ){

    if (retryAttempt >= retryMax){
      g_info << "Record " << c << " could not be inserted, has retried "
	     << retryAttempt << " times " << endl;
      // Reset retry counters and continue with next record
      retryAttempt = 0;
      c++;
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

    if(pkInsertRecord(pNdb, c, batch) != NDBT_OK)
    {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    // Execute the transaction and insert the record
    check = pTrans->execute(Commit, CommitAsMuchAsPossible); 
    if(check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      closeTransaction(pNdb);
      
      switch(err.status){
      case NdbError::Success:
	ERR(err);
	g_info << "ERROR: NdbError reports success when transcaction failed"
	       << endl;
	return NDBT_FAILED;
	break;
	
      case NdbError::TemporaryError:      
	ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
	break;
	
      case NdbError::UnknownResult:
	ERR(err);
	return NDBT_FAILED;
	break;
	
      case NdbError::PermanentError:
	//  if (allowConstraintViolation == true){
	//    switch (err.classification){
	//    case NdbError::ConstraintViolation:
	//      // Tuple already existed, OK but should be reported
	//      g_info << c << ": " << err.code << " " << err.message << endl;
	//      c++;
	//      continue;
	//      break;
	//    default:	    
	//      break;es
	//     }
	//   }

	// Check if this is the "db full" error 
	if (err.classification==NdbError::InsufficientSpace){
	  ERR(err);
	  return NDBT_OK;
	}

	if (err.classification == NdbError::ConstraintViolation){
	  ERR(err);
	  break;
	}
	ERR(err);
	return NDBT_FAILED;
	break;
      }
    }
    else{      
      pTrans->getGCI(&m_latest_gci);
      closeTransaction(pNdb);
    }
    
    // Step to next record
    c = c+batch; 
    retryAttempt = 0;
  }
  return NDBT_OK;
}

int 
HugoTransactions::pkReadRecords(Ndb* pNdb, 
				int records,
				int batch,
				NdbOperation::LockMode lm){
  int                  reads = 0;
  int                  r = 0;
  int                  retryAttempt = 0;
  int                  check;

  if (batch == 0) {
    g_info << "ERROR: Argument batch == 0 in pkReadRecords(). Not allowed." << endl;
    return NDBT_FAILED;
  }

  while (r < records){
    if(r + batch > records)
      batch = records - r;

    if (retryAttempt >= m_retryMax){
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

    MicroSecondTimer timer_start;
    MicroSecondTimer timer_stop;
    bool timer_active =
      m_stats_latency != 0 &&
      r >= batch &&             // first batch is "warmup"
      r + batch != records;     // last batch is usually partial

    if (timer_active)
      NdbTick_getMicroTimer(&timer_start);

    if(pkReadRecord(pNdb, r, batch, lm) != NDBT_OK)
    {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    check = pTrans->execute(Commit, AbortOnError);   
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      switch(err.code){
      case 626: // Tuple did not exist
	g_info << r << ": " << err.code << " " << err.message << endl;
	r++;
	break;

      default:
	ERR(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    } else {

      if(pIndexScanOp)
      {
	int rows_found = 0;
	while((check = pIndexScanOp->nextResult()) == 0)
	{
	  rows_found++;
	  if (calc.verifyRowValues(rows[0]) != 0){
	    closeTransaction(pNdb);
	    return NDBT_FAILED;
	  }
	}
	if(check != 1 || rows_found > batch)
	{
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	else if(rows_found < batch)
	{
	  if(batch == 1){
	    g_info << r << ": not found" << endl; abort(); }
	  else
	    g_info << "Found " << rows_found << " of " 
		   << batch << " rows" << endl;
	}
	r += batch;
	reads += rows_found;
      }
      else 
      {
	for (int b=0; (b<batch) && (r+b<records); b++){ 
	  if (calc.verifyRowValues(rows[b]) != 0){
	    closeTransaction(pNdb);
	    return NDBT_FAILED;
	  }
	  reads++;
	  r++;
	}
      }
    }
    
    closeTransaction(pNdb);

    if (timer_active) {
      NdbTick_getMicroTimer(&timer_stop);
      NDB_TICKS ticks = NdbTick_getMicrosPassed(timer_start, timer_stop);
      m_stats_latency->addObservation((double)ticks);
    }
  }
  deallocRows();
  g_info << reads << " records read" << endl;
  return NDBT_OK;
}



int 
HugoTransactions::pkUpdateRecords(Ndb* pNdb, 
				  int records,
				  int batch,
				  int doSleep){
  int updated = 0;
  int                  r = 0;
  int                  retryAttempt = 0;
  int                  check, b;

  allocRows(batch);

  g_info << "|- Updating records (batch=" << batch << ")..." << endl;
  int batch_no = 0;
  while (r < records){
    if(r + batch > records)
      batch = records - r;

    if (m_thr_count != 0 && m_thr_no != batch_no % m_thr_count)
    {
      r += batch;
      batch_no++;
      continue;
    }
    
    if (retryAttempt >= m_retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }
    
    if (doSleep > 0)
      NdbSleep_MilliSleep(doSleep);

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

    if(pkReadRecord(pNdb, r, batch, NdbOperation::LM_Exclusive) != NDBT_OK)
    {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    check = pTrans->execute(NoCommit, AbortOnError);   
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    MicroSecondTimer timer_start;
    MicroSecondTimer timer_stop;
    bool timer_active =
      m_stats_latency != 0 &&
      r >= batch &&             // first batch is "warmup"
      r + batch != records;     // last batch is usually partial

    if (timer_active)
      NdbTick_getMicroTimer(&timer_start);

    if(pIndexScanOp)
    {
      int rows_found = 0;
      while((check = pIndexScanOp->nextResult(true)) == 0)
      {
	do {
	  
	  if (calc.verifyRowValues(rows[0]) != 0){
	    closeTransaction(pNdb);
	    return NDBT_FAILED;
	  }
	  
	  int updates = calc.getUpdatesValue(rows[0]) + 1;
	  
	  if(pkUpdateRecord(pNdb, r+rows_found, 1, updates) != NDBT_OK)
	  {
	    ERR(pTrans->getNdbError());
	    closeTransaction(pNdb);
	    return NDBT_FAILED;
	  }
	  rows_found++;
	} while((check = pIndexScanOp->nextResult(false)) == 0);
	
	if(check != 2)
	  break;
	if((check = pTrans->execute(NoCommit, AbortOnError)) != 0)
	  break;
      }
      if(check != 1 || rows_found != batch)
      {
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }
    else
    {
      for(b = 0; b<batch && (b+r)<records; b++)
      {
	if (calc.verifyRowValues(rows[b]) != 0)
	{
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	
	int updates = calc.getUpdatesValue(rows[b]) + 1;
	
	if(pkUpdateRecord(pNdb, r+b, 1, updates) != NDBT_OK)
	{
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
      check = pTrans->execute(Commit, AbortOnError);   
    }
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      ndbout << "r = " << r << endl;
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    else{
      updated += batch;
      pTrans->getGCI(&m_latest_gci);
    }
    
    closeTransaction(pNdb);

    if (timer_active) {
      NdbTick_getMicroTimer(&timer_stop);
      NDB_TICKS ticks = NdbTick_getMicrosPassed(timer_start, timer_stop);
      m_stats_latency->addObservation((double)ticks);
    }

    r += batch; // Read next record
    batch_no++;
  }
  
  deallocRows();
  g_info << "|- " << updated << " records updated" << endl;
  return NDBT_OK;
}

int 
HugoTransactions::pkInterpretedUpdateRecords(Ndb* pNdb, 
					     int records,
					     int batch){
  int updated = 0;
  int r = 0;
  int retryAttempt = 0;
  int check, a;

  while (r < records){
    
    if (retryAttempt >= m_retryMax){
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

   NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
   if (pOp == NULL) {
     ERR(pTrans->getNdbError());
     closeTransaction(pNdb);
     return NDBT_FAILED;
   }
   
   check = pOp->readTupleExclusive();
   if( check == -1 ) {
     ERR(pTrans->getNdbError());
     closeTransaction(pNdb);
     return NDBT_FAILED;
   }
   
   // Define primary keys
   if (equalForRow(pOp, r) != 0)
   {
     closeTransaction(pNdb);
     return NDBT_FAILED;
   }
   
   // Read update value
   for(a = 0; a<tab.getNoOfColumns(); a++){
     if (calc.isUpdateCol(a) == true){
       if((row.attributeStore(a) = 
	   pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	 ERR(pTrans->getNdbError());
	 closeTransaction(pNdb);
	 return NDBT_FAILED;
       }
     }
   }
   
    check = pTrans->execute(NoCommit, AbortOnError);   
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    int updates = calc.getUpdatesValue(&row) + 1;

    NdbOperation* pUpdOp;
    pUpdOp = pTrans->getNdbOperation(tab.getName());	
    if (pUpdOp == NULL) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    check = pUpdOp->interpretedUpdateTuple();
    if( check == -1 ) {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // PKs
    if (equalForRow(pUpdOp, r) != 0)
    {
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // Update col
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if ((tab.getColumn(a)->getPrimaryKey() == false) && 
	  (calc.isUpdateCol(a) == true)){
	
	// TODO switch for 32/64 bit
	const NdbDictionary::Column* attr = tab.getColumn(a);
	Uint32 valToIncWith = 1;
	check = pUpdOp->incValue(attr->getName(), valToIncWith);
	if( check == -1 ) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
    }

    // Remaining attributes
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if ((tab.getColumn(a)->getPrimaryKey() == false) && 
	  (calc.isUpdateCol(a) == false)){
	if(setValueForAttr(pUpdOp, a, r, updates ) != 0){
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
    }


    
    check = pTrans->execute(Commit, AbortOnError);   
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      ndbout << "r = " << r << endl;
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    else{
      updated++;
      pTrans->getGCI(&m_latest_gci);
    }
    

    closeTransaction(pNdb);

    r++; // Read next record

  }

  g_info << "|- " << updated << " records updated" << endl;
  return NDBT_OK;
}

int 
HugoTransactions::pkDelRecords(Ndb* pNdb, 
			       int records,
			       int batch,
			       bool allowConstraintViolation,
			       int doSleep){
  // TODO Batch is not implemented
  int deleted = 0;
  int                  r = 0;
  int                  retryAttempt = 0;
  int                  check;

  g_info << "|- Deleting records..." << endl;
  int batch_no = 0;
  while (r < records){
    if(r + batch > records)
      batch = records - r;

    if (m_thr_count != 0 && m_thr_no != batch_no % m_thr_count)
    {
      r += batch;
      batch_no++;
      continue;
    }

    if (retryAttempt >= m_retryMax){
      g_info << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    if (doSleep > 0)
      NdbSleep_MilliSleep(doSleep);

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

    MicroSecondTimer timer_start;
    MicroSecondTimer timer_stop;
    bool timer_active =
      m_stats_latency != 0 &&
      r >= batch &&             // first batch is "warmup"
      r + batch != records;     // last batch is usually partial

    if (timer_active)
      NdbTick_getMicroTimer(&timer_start);

    if(pkDeleteRecord(pNdb, r, batch) != NDBT_OK)
    {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    check = pTrans->execute(Commit, AbortOnError);   
    if( check == -1) {
      const NdbError err = pTrans->getNdbError();
      
      switch(err.status){
      case NdbError::TemporaryError:
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
	break;

      case NdbError::PermanentError:
	if (allowConstraintViolation == true){
	  switch (err.classification){
	  case NdbError::ConstraintViolation:
	    // Tuple did not exist, OK but should be reported
	    g_info << r << ": " << err.code << " " << err.message << endl;
	    continue;
	    break;
	  default:	    
	    break;
	  }
	}
	ERR(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
	break;
	
      default:
	ERR(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }
    else {
      deleted += batch;
      pTrans->getGCI(&m_latest_gci);
    }
    closeTransaction(pNdb);

    if (timer_active) {
      NdbTick_getMicroTimer(&timer_stop);
      NDB_TICKS ticks = NdbTick_getMicrosPassed(timer_start, timer_stop);
      m_stats_latency->addObservation((double)ticks);
    }

    r += batch; // Read next record
    batch_no++;
  }

  g_info << "|- " << deleted << " records deleted" << endl;
  return NDBT_OK;
}


int 
HugoTransactions::lockRecords(Ndb* pNdb, 
			      int records,
			      int percentToLock,
			      int lockTime){
  // Place a lock on percentToLock% of the records in the Db
  // Keep the locks for lockTime ms, commit operation
  // and lock som other records
  int                  r = 0;
  int                  retryAttempt = 0;
  int                  check;
  NdbOperation::LockMode lm = NdbOperation::LM_Exclusive;

  // Calculate how many records to lock in each batch
  if (percentToLock <= 0)
    percentToLock = 1;
  double percentVal = (double)percentToLock / 100;
  int lockBatch = (int)(records * percentVal);
  if (lockBatch <= 0)
    lockBatch = 1;

  allocRows(lockBatch);
  
  while (r < records){
    if(r + lockBatch > records)
      lockBatch = records - r;
    
    g_info << "|- Locking " << lockBatch << " records..." << endl;

    if (retryAttempt >= m_retryMax){
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

    if(pkReadRecord(pNdb, r, lockBatch, lm) != NDBT_OK)
    {
      ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    // NoCommit lockTime times with 100 millis interval
    int sleepInterval = 50;
    int lockCount = lockTime / sleepInterval;
    int commitCount = 0;
    do {
      check = pTrans->execute(NoCommit, AbortOnError);   
      if( check == -1) {
	const NdbError err = pTrans->getNdbError();
	
	if (err.status == NdbError::TemporaryError){
	  ERR(err);
	  closeTransaction(pNdb);
	  NdbSleep_MilliSleep(50);
	  retryAttempt++;
	  continue;
	}
	ERR(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      for (int b=0; (b<lockBatch) && (r+b<records); b++){ 
	if (calc.verifyRowValues(rows[b]) != 0){
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
      commitCount++;
      NdbSleep_MilliSleep(sleepInterval);
    } while (commitCount < lockCount);
    
    // Really commit the trans, puuh!
    check = pTrans->execute(Commit, AbortOnError);   
    if( check == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ERR(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    else{
      for (int b=0; (b<lockBatch) && (r<records); b++){ 
	if (calc.verifyRowValues(rows[b]) != 0){
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	r++; // Read next record
      }
    }
    
    closeTransaction(pNdb);
    
  }
  deallocRows();
  g_info << "|- Record locking completed" << endl;
  return NDBT_OK;
}

int 
HugoTransactions::indexReadRecords(Ndb* pNdb, 
				   const char * idxName,
				   int records,
				   int batch){
  int                  reads = 0;
  int                  r = 0;
  int                  retryAttempt = 0;
  int                  check, a;
  NdbOperation *pOp;
  NdbIndexScanOperation *sOp;

  const NdbDictionary::Index* pIndex
    = pNdb->getDictionary()->getIndex(idxName, tab.getName());
  
  const bool ordered = (pIndex->getType()==NdbDictionary::Index::OrderedIndex);

  if (batch == 0) {
    g_info << "ERROR: Argument batch == 0 in indexReadRecords(). "
	   << "Not allowed." << endl;
    return NDBT_FAILED;
  }
  
  if (ordered) {
    batch = 1;
  }

  allocRows(batch);
  
  while (r < records){
    if (retryAttempt >= m_retryMax){
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
    
    for(int b=0; (b<batch) && (r+b < records); b++){
      if(!ordered){
	pOp = pTrans->getNdbIndexOperation(idxName, tab.getName());	
	if (pOp == NULL) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	check = pOp->readTuple();
      } else {
	pOp = sOp = pTrans->getNdbIndexScanOperation(idxName, tab.getName());
	if (sOp == NULL) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	check = sOp->readTuples();
      }
      
      if( check == -1 ) {
	ERR(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      
      // Define primary keys
      if (equalForRow(pOp, r+b) != 0)
      {
        closeTransaction(pNdb);
        return NDBT_FAILED;
      }
      
      // Define attributes to read  
      for(a = 0; a<tab.getNoOfColumns(); a++){
	if((rows[b]->attributeStore(a) = 
	    pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
    }

    check = pTrans->execute(Commit, AbortOnError);   
    check = (check == -1 ? -1 : !ordered ? check : sOp->nextResult(true));
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      switch(err.code){
      case 626: // Tuple did not exist
	  g_info << r << ": " << err.code << " " << err.message << endl;
	  r++;
	  break;
	  
      default:
	ERR(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    } else{
      for (int b=0; (b<batch) && (r+b<records); b++){ 
	if (calc.verifyRowValues(rows[b]) != 0){
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	reads++;
	r++;
      }
      if(ordered && sOp->nextResult(true) == 0){
	ndbout << "Error when comparing records "
	       << " - index op next_result to many" << endl;
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }
    closeTransaction(pNdb);
  }
  deallocRows();
  g_info << reads << " records read" << endl;
  return NDBT_OK;
}



int 
HugoTransactions::indexUpdateRecords(Ndb* pNdb, 
				     const char * idxName,
				     int records,
				     int batch){

  int updated = 0;
  int                  r = 0;
  int                  retryAttempt = 0;
  int                  check, a, b;
  NdbOperation *pOp;
  NdbScanOperation * sOp;

  const NdbDictionary::Index* pIndex
    = pNdb->getDictionary()->getIndex(idxName, tab.getName());
  
  const bool ordered = (pIndex->getType()==NdbDictionary::Index::OrderedIndex);
  if (ordered){
    batch = 1;
  }

  allocRows(batch);
  
  while (r < records){
    if (retryAttempt >= m_retryMax){
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

    for(b = 0; b<batch && (b+r)<records; b++){
      if(!ordered){
	pOp = pTrans->getNdbIndexOperation(idxName, tab.getName());	
	if (pOp == NULL) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	
	check = pOp->readTupleExclusive();
	if( check == -1 ) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      } else {
	pOp = sOp = pTrans->getNdbIndexScanOperation(idxName, tab.getName());
	if (pOp == NULL) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	
	check = 0;
	sOp->readTuplesExclusive();
      }	
      
      // Define primary keys
      if (equalForRow(pOp, r+b) != 0)
      {
        closeTransaction(pNdb);
        return NDBT_FAILED;
      }
      
      // Define attributes to read  
      for(a = 0; a<tab.getNoOfColumns(); a++){
	if((rows[b]->attributeStore(a) = 
	    pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	  ERR(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
    }
     
    check = pTrans->execute(NoCommit, AbortOnError);   
    check = (check == -1 ? -1 : !ordered ? check : sOp->nextResult(true));
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      ERR(err);
      closeTransaction(pNdb);
      
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      return NDBT_FAILED;
    }

    if(ordered && check != 0){
      g_err << check << " - Row: " << r << " not found!!" << endl;
      closeTransaction(pNdb);
      return NDBT_FAILED;    
    }
    
    for(b = 0; b<batch && (b+r)<records; b++){
      if (calc.verifyRowValues(rows[b]) != 0){
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      
      int updates = calc.getUpdatesValue(rows[b]) + 1;
      
      NdbOperation* pUpdOp;
      if(!ordered){
	pUpdOp = pTrans->getNdbIndexOperation(idxName, tab.getName());
	check = (pUpdOp == 0 ? -1 : pUpdOp->updateTuple());
      } else {
	pUpdOp = sOp->updateCurrentTuple();
      }

      if (pUpdOp == NULL) {
	ERR(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      
      if( check == -1 ) {
	ERR(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      
      if(!ordered)
      {
        if (equalForRow(pUpdOp, r+b) != 0)
        {
          closeTransaction(pNdb);
          return NDBT_FAILED;
        }
      }
      
      for(a = 0; a<tab.getNoOfColumns(); a++){
	if (tab.getColumn(a)->getPrimaryKey() == false){
	  if(setValueForAttr(pUpdOp, a, r+b, updates ) != 0){
	    ERR(pTrans->getNdbError());
	    closeTransaction(pNdb);
	    return NDBT_FAILED;
	  }
	}
      }
    }
    
    check = pTrans->execute(Commit, AbortOnError);   
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      ERR(err);
      closeTransaction(pNdb);
      
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ndbout << "r = " << r << endl;
      return NDBT_FAILED;
    } else {
      updated += batch;
      pTrans->getGCI(&m_latest_gci);
    }
    
    closeTransaction(pNdb);
    
    r+= batch; // Read next record
  }
  
  g_info << "|- " << updated << " records updated" << endl;
  return NDBT_OK;
}

template class Vector<NDBT_ResultRow*>;
