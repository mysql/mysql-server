/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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
  m_retryMaxReached = false;
  m_stats_latency = 0;

  m_thr_count = 0;
  m_thr_no = -1;

  m_empty_update = false;
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
                                  int scan_flags,
                                  int force_check_flag)
{
  
  int                  retryAttempt = 0;
  int                  check, a;
  NdbScanOperation	       *pOp;

  while (true){

    if (retryAttempt >= m_retryMax){
      g_err << __LINE__ << " ERROR: has retried this operation " 
            << retryAttempt << " times, failing!, line: " << __LINE__ << endl;
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
      setNdbError(err);
      return NDBT_FAILED;
    }

    pOp = getScanOperation(pTrans);
    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if( pOp ->readTuples(lm, scan_flags, parallelism) ) {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if((row.attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	NDB_ERR(pTrans->getNdbError());
	setNdbError(pTrans->getNdbError());
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
      setNdbError(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // Abort after 1-100 or 1-records rows
    int ranVal = rand();
    int abortCount = ranVal % (records == 0 ? 100 : records); 
    bool abortTrans = false;
    if (abortPercent > 0){
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
        g_err << "Line: " << __LINE__ << " verify row failed" << endl;
	return NDBT_FAILED;
      }

      if (abortCount == rows && abortTrans == true){
	ndbout << "Scan is aborted" << endl;
	g_info << "Scan is aborted" << endl;
	pOp->close();
	if( check == -1 ) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
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
	NDB_ERR_INFO(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	switch (err.code){
	case 488:
	case 245:
	case 490:
	  // Too many active scans, no limit on number of retry attempts
	  break;
	default:
          if (err.classification == NdbError::TimeoutExpired)
          {
            if (retryAttempt >= (m_retryMax / 10) && 
                (parallelism == 0 || parallelism > 1))
            {
              /**
               * decrease parallelism
               */
              parallelism = 1;
              ndbout_c("decrease parallelism");
            }
          }
	  retryAttempt++;
	}
	continue;
      }
      NDB_ERR(err);
      setNdbError(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    closeTransaction(pNdb);

    g_info << rows << " rows have been read" << endl;
    if ((force_check_flag != 0 || records != 0) && rows != records)
    {
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
HugoTransactions::scanReadRecords(Ndb* pNdb, 
				  const NdbDictionary::Index * pIdx,
				  int records,
				  int abortPercent,
				  int parallelism, 
				  NdbOperation::LockMode lm,
                                  int scan_flags,
                                  int bound_cnt, const HugoBound* bound_arr)
{
  
  int                  retryAttempt = 0;
  int                  check, a;
  NdbScanOperation     *pOp;
  NdbIndexScanOperation  *pIxOp;

  while (true){

    if (retryAttempt >= m_retryMax){
      g_err << __LINE__ << " ERROR: has retried this operation " 
            << retryAttempt  << " times, failing!" << endl;
      g_err << "lm: " << Uint32(lm) << " flags: H'" << hex << scan_flags
            << endl;
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
      setNdbError(err);
      return NDBT_FAILED;
    }

    if (pIdx != NULL) {
      pOp = pIxOp = pTrans->getNdbIndexScanOperation(pIdx->getName(), tab.getName());
    } else {
      pOp = pTrans->getNdbScanOperation(tab.getName());
      pIxOp = NULL;
    }

    if (pOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if( pOp ->readTuples(lm, scan_flags, parallelism) ) {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    for (int i = 0; i < bound_cnt; i++) {
      const HugoBound& b = bound_arr[i];
      if (pIxOp->setBound(b.attr, b.type, b.value) != 0) {
        NDB_ERR(pIxOp->getNdbError());
        setNdbError(pIxOp->getNdbError());
        return NDBT_FAILED;
      }
    }
    
    for(a = 0; a<tab.getNoOfColumns(); a++){
      if((row.attributeStore(a) = 
	  pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	NDB_ERR(pTrans->getNdbError());
	setNdbError(pTrans->getNdbError());
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
      setNdbError(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // Abort after 1-100 or 1-records rows
    int ranVal = rand();
    int abortCount = ranVal % (records == 0 ? 100 : records); 
    bool abortTrans = false;
    if (abortPercent > 0){
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
        g_err << "Line: " << __LINE__ << " verify row failed" << endl;
	return NDBT_FAILED;
      }

      if (abortCount == rows && abortTrans == true){
	ndbout << "Scan is aborted" << endl;
	g_info << "Scan is aborted" << endl;
	pOp->close();
	if( check == -1 ) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
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
	NDB_ERR_INFO(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	switch (err.code){
	case 488:
	case 245:
	case 490:
	  // Too many active scans, no limit on number of retry attempts
	  break;
	default:
          if (err.classification == NdbError::TimeoutExpired)
          {
            if (retryAttempt >= (m_retryMax / 10) && 
                (parallelism == 0 || parallelism > 1))
            {
              /**
               * decrease parallelism
               */
              parallelism = 1;
              ndbout_c("decrease parallelism");
            }
            else if (retryAttempt >= (m_retryMax / 5) &&
                     (lm != NdbOperation::LM_CommittedRead))
            {
              lm = NdbOperation::LM_CommittedRead;
              ndbout_c("switch to LM_CommittedRead");
            }
            else if (retryAttempt >= (m_retryMax / 4) &&
                     (pIdx != 0))
            {
              pIdx = NULL;
              bound_cnt = 0;
              scan_flags |= NdbScanOperation::SF_TupScan;
              ndbout_c("switch to table-scan (SF_TupScan) from index-scan");
            }
          }
	  retryAttempt++;
	}
	continue;
      }
      NDB_ERR(err);
      setNdbError(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    closeTransaction(pNdb);

    g_info << rows << " rows have been read"
           << ", number of index bounds " << bound_cnt << endl;
    // TODO verify expected number of records with index bounds
    if (records != 0 && rows != records && bound_cnt == 0){
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
  m_retryMaxReached = false;

  while (true){
restart:
    if (retryAttempt++ >= m_retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      m_retryMaxReached = true;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();
      NDB_ERR(err);
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	continue;
      }
      setNdbError(err);
      return NDBT_FAILED;
    }

    pOp = getScanOperation(pTrans);
    if (pOp == NULL)
    {
      const NdbError err = pTrans->getNdbError();
      NDB_ERR(err);
      closeTransaction(pNdb);
      if (err.status == NdbError::TemporaryError)
      {
        NdbSleep_MilliSleep(50);
        continue;
      }
      setNdbError(err);
      return NDBT_FAILED;
    }

    if( pOp->readTuples(NdbOperation::LM_Exclusive, flags,
                        parallelism))
    {
      NDB_ERR(pOp->getNdbError());
      setNdbError(pOp->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    // Read all attributes from this table    
    for(a=0; a<tab.getNoOfColumns(); a++){
      if((row.attributeStore(a) = pOp->getValue(tab.getColumn(a)->getName())) == NULL){
	NDB_ERR(pTrans->getNdbError());
	setNdbError(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }
    
    check = pTrans->execute(NoCommit, AbortOnError);
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      NDB_ERR(err);
      closeTransaction(pNdb);
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	continue;
      }
      setNdbError(err);
      return NDBT_FAILED;
    }

    // Abort after 1-100 or 1-records rows
    int ranVal = rand();
    int abortCount = ranVal % (records == 0 ? 100 : records); 
    bool abortTrans = false;
    if (abortPercent > 0){
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
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	const int updates = calc.getUpdatesValue(&row) + (m_empty_update? 0 : 1);
	const int r = calc.getIdValue(&row);
        
  	for(a = 0; a<tab.getNoOfColumns(); a++){
	  if (tab.getColumn(a)->getPrimaryKey() == false){
	    if(setValueForAttr(pUp, a, r, updates ) != 0){
	      NDB_ERR(pTrans->getNdbError());
	      setNdbError(pTrans->getNdbError());
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
	NDB_ERR(err);
        if (err.code == 499 || err.code == 631 ||   // Scan lock take over errors
            err.status == NdbError::TemporaryError) // Other temporary errors
        {
          if (err.code == 410 || err.code == 1501)
          {
	    NdbSleep_MilliSleep(2000);
          }
          else
          {
	    NdbSleep_MilliSleep(300);
          }
	  goto restart;
	}
	setNdbError(err);
	return NDBT_FAILED;
      }
    }
    
    const NdbError err = pTrans->getNdbError();    
    if( check == -1 ) {
      closeTransaction(pNdb);
      NDB_ERR(err);
      if (err.code == 499 || err.code == 631 ||   // Scan lock take over errors
          err.status == NdbError::TemporaryError) // Other temporary errors
      {
	NdbSleep_MilliSleep(50);
	goto restart;
      }
      setNdbError(err);
      return NDBT_FAILED;
    }
    
    closeTransaction(pNdb);
    
    g_info << rows << " rows have been updated" << endl;
    return NDBT_OK;
  }
  abort(); /* Should never happen */
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
			    bool abort,
                            bool abort_on_first_error,
                            int row_step)
{
  return loadTableStartFrom(pNdb, 0, records, batch, allowConstraintViolation,
                            doSleep, oneTrans, value, abort,
                            abort_on_first_error, row_step);
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
                                     bool abort,
                                     bool abort_on_first_error,
                                     int row_step)
{
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
  for (int c=0 ; c < records; )
  {
    int key;
    bool closeTrans = true;

    if(c + batch > records)
      batch = records - c;
    key = c * row_step;
    if (retryAttempt >= retryMax){
      g_info << "Record " << c << " could not be inserted, has retried "
	     << retryAttempt << " times " << endl;
      // Reset retry counters and continue with next record
      retryAttempt = 0;
      if (row_step == 1)
      {
        c+= row_step;
      }
    }
    if (doSleep > 0)
      NdbSleep_MilliSleep(doSleep);

    //    if (first_batch || !oneTrans)
    if (first_batch || !pTrans) {
      first_batch = false;
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
        setNdbError(err);
        return NDBT_FAILED;
      }
    }
    if(pkInsertRecord(pNdb, key + startFrom, batch, value, row_step) != NDBT_OK)
    { 
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
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
	NDB_ERR(err);
	g_info << "ERROR: NdbError reports success when transcaction failed"
	       << endl;
	setNdbError(err);
	return NDBT_FAILED;
	break;
	
      case NdbError::TemporaryError:      
        if (abort_on_first_error || row_step != 1)
        {
          return err.code;
        }
	NDB_ERR(err);
        if (err.code == 410 || err.code == 1501)
	  NdbSleep_MilliSleep(2000);
        else
	  NdbSleep_MilliSleep(50);
	retryAttempt++;
        batch = 1;
	continue;
	break;
	
      case NdbError::UnknownResult:
	NDB_ERR(err);
	setNdbError(err);
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
	NDB_ERR(err);
	setNdbError(err);
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
  int             retryFull = 0;
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
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      setNdbError(err);
      return NDBT_FAILED;
    }

    if(pkInsertRecord(pNdb, c, batch) != NDBT_OK)
    {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    // Execute the transaction and insert the record
    check = pTrans->execute(Commit, CommitAsMuchAsPossible); 
    const NdbError err = pTrans->getNdbError();
    if(check == -1 || err.code != 0) {
      closeTransaction(pNdb);
      
      switch(err.status){
      case NdbError::Success:
	NDB_ERR(err);
	setNdbError(err);
	g_info << "ERROR: NdbError reports success when transcaction failed"
	       << endl;
	return NDBT_FAILED;
	break;
	
      case NdbError::TemporaryError:      
	NDB_ERR(err);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
	break;
	
      case NdbError::UnknownResult:
	NDB_ERR(err);
	setNdbError(err);
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
          // Datamemory might have been released by abort of
          // batch insert. Retry fill with a smaller batch
          // in order to ensure table is filled to last row.
          if (batch > 1){
            c = c+batch; 
            batch = batch/2;
            continue;
          }
          // Only some datanodes might be full. Retry with
          // another record until we are *really sure* that
          // all datanodes are full.
          if (retryFull < 64) {
            retryFull++;
            c++;
	     continue;
          }

	  NDB_ERR(err);
	  return NDBT_OK;
	}

	if (err.classification == NdbError::ConstraintViolation){
	  NDB_ERR(err);
	  break;
	}
	NDB_ERR(err);
	setNdbError(err);
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
    retryFull = 0;
  }
  return NDBT_OK;
}

int 
HugoTransactions::pkReadRecords(Ndb* pNdb, 
				int records,
				int batch,
				NdbOperation::LockMode lm,
                                int _rand){
  int                  reads = 0;
  int                  r = 0;
  int                  retryAttempt = 0;
  int                  check;

  if (batch == 0) {
    g_err << "ERROR: Argument batch == 0 in pkReadRecords(). Not allowed.";
    g_err << "Line: " << __LINE__ << endl;
    return NDBT_FAILED;
  }

  while (r < records){
    if(r + batch > records)
      batch = records - r;

    if (retryAttempt >= m_retryMax){
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    
    pTrans = pNdb->startTransaction();
    if (pTrans == NULL) {
      const NdbError err = pNdb->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	NdbSleep_MilliSleep(500);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      setNdbError(err);
      return NDBT_FAILED;
    }
    retryAttempt = 0;

    NDB_TICKS timer_start;
    NDB_TICKS timer_stop;
    bool timer_active =
      m_stats_latency != 0 &&
      r >= batch &&             // first batch is "warmup"
      r + batch != records;     // last batch is usually partial

    if (timer_active)
      timer_start = NdbTick_getCurrentTicks();

    NdbOperation::LockMode lmused;
    if (_rand == 0)
    {
      if(pkReadRecord(pNdb, r, batch, lm, &lmused) != NDBT_OK)
      {
        NDB_ERR(pTrans->getNdbError());
        setNdbError(pTrans->getNdbError());
        closeTransaction(pNdb);
        return NDBT_FAILED;
      }
    }
    else
    {
      if(pkReadRandRecord(pNdb, records, batch, lm, &lmused) != NDBT_OK)
      {
        NDB_ERR(pTrans->getNdbError());
        setNdbError(pTrans->getNdbError());
        closeTransaction(pNdb);
        return NDBT_FAILED;
      }
    }
    
    check = pTrans->execute(Commit, AbortOnError);

    if (check != -1 && lmused == NdbOperation::LM_CommittedRead)
    {
      /**
       * LM_CommittedRead will not abort transaction
       *   even if doing execute(AbortOnError);
       *   so also check pTrans->getNdbError() in this case
       */
      if (pTrans->getNdbError().status != NdbError::Success)
      {
        check = -1;
      }
    }      

    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
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
	NDB_ERR(err);
	setNdbError(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    } else {

      /**
       * Extra debug aid:
       * We do not (yet) expect any transaction or operation
       * errors if ::execute() does not return with error.
       */
      const NdbError err1 = pTrans->getNdbError();
      if (err1.code)
      {
        ndbout << "BEWARE: HugoTransactions::pkReadRecords"
               << ", execute succeeded with Trans error: " << err1.code
               << endl;

      }
      const NdbOperation* pOp = pTrans->getNdbErrorOperation();
      if (pOp != NULL)
      {
        const NdbError err2 = pOp->getNdbError();
        ndbout << "BEWARE HugoTransactions::pkReadRecords"
             << ", NdbOperation error: " << err2.code
             << endl;
      }

      retryAttempt = 0;
      if(indexScans.size() > 0)
      {
        /* Index scan used to read records....*/
	int rows_found = 0;
        for (Uint32 scanOp=0; scanOp < indexScans.size(); scanOp++)
        {
          while((check = indexScans[scanOp]->nextResult()) == 0)
          {
            rows_found++;
            if (calc.verifyRowValues(rows[0]) != 0){
              closeTransaction(pNdb);
              g_err << "Line: " << __LINE__ << " verify row failed" << endl;
              return NDBT_FAILED;
            }
          }
        }
	if(check != 1 || rows_found > batch)
	{
	  closeTransaction(pNdb);
          g_err << "Line: " << __LINE__ << " check rows failed" << endl;
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
            g_err << "Line: " << __LINE__ 
                  << " verify row failed"
                  << ", record: " << r << " of: " << records 
                  << ", row: " << b << " in a batch of: " << batch 
                  << endl;
	    return NDBT_FAILED;
	  }
	  reads++;
	  r++;
	}
      }
    }
    
    closeTransaction(pNdb);

    if (timer_active) {
      timer_stop = NdbTick_getCurrentTicks();
      Uint64 elapsed = NdbTick_Elapsed(timer_start, timer_stop).microSec();
      m_stats_latency->addObservation((double)elapsed);
    }
  }
  deallocRows();
  indexScans.clear();
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
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }
    
    if (doSleep > 0)
      NdbSleep_MilliSleep(doSleep);

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
      setNdbError(err);
      return NDBT_FAILED;
    }

    if(pkReadRecord(pNdb, r, batch, NdbOperation::LM_Exclusive) != NDBT_OK)
    {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
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
      setNdbError(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    NDB_TICKS timer_start;
    NDB_TICKS timer_stop;
    bool timer_active =
      m_stats_latency != 0 &&
      r >= batch &&             // first batch is "warmup"
      r + batch != records;     // last batch is usually partial

    if (timer_active)
      timer_start = NdbTick_getCurrentTicks();

    int rows_found = 0;

    if(indexScans.size() > 0)
    {
      /* Index scans used to read records */
      for (Uint32 scanOp=0; scanOp < indexScans.size(); scanOp++)
      {
        while((check = indexScans[scanOp]->nextResult(true)) == 0)
        {
          do {
            
            if (calc.verifyRowValues(rows[0]) != 0){
              g_err << "Row validation failure, line: " << __LINE__ << endl;
              closeTransaction(pNdb);
              return NDBT_FAILED;
            }
            
            int updates = calc.getUpdatesValue(rows[0]) + (m_empty_update? 0 : 1);
            
            /* Rows may not arrive in the order they were requested
             * (When multiple partitions scanned without ORDERBY)
             * therefore we use the id from the row to update it
             */
            const Uint32 rowId= calc.getIdValue(rows[0]);
            if(pkUpdateRecord(pNdb, rowId, 1, updates) != NDBT_OK)
            {
              NDB_ERR(pTrans->getNdbError());
              setNdbError(pTrans->getNdbError());
              closeTransaction(pNdb);
              return NDBT_FAILED;
            }
            rows_found++;
          } while((check = indexScans[scanOp]->nextResult(false)) == 0);
          
          if(check != 2)
            break;
          if((check = pTrans->execute(NoCommit, AbortOnError)) != 0)
            break;
        } // Next fetch on this scan op...

        if(check != 1)
        {
          g_err << "Check failed, line: " << __LINE__ << endl;
          closeTransaction(pNdb);
          return NDBT_FAILED;
        } 
      } // Next scan op...

      if (rows_found != batch)
      {
        g_err << "Incorrect num of rows found.  Expected "
               << batch << ". Found " << rows_found << endl;
        g_err << "Line: " << __LINE__ << endl;
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
          g_err << "Line: " << __LINE__ << " verify row failed" << endl;
	  return NDBT_FAILED;
	}
	
	int updates = calc.getUpdatesValue(rows[b]) + (m_empty_update? 0 : 1);
	
	if(pkUpdateRecord(pNdb, r+b, 1, updates) != NDBT_OK)
	{
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
      check = pTrans->execute(Commit, AbortOnError);   
    }
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();

      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
        if (err.code == 410 || err.code == 1501)
	  NdbSleep_MilliSleep(2000);
        else
	  NdbSleep_MilliSleep(300);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      setNdbError(err);
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
      timer_stop = NdbTick_getCurrentTicks();
      Uint64 elapsed = NdbTick_Elapsed(timer_start, timer_stop).microSec();
      m_stats_latency->addObservation((double)elapsed);
    }

    r += batch; // Read next record
    batch_no++;
  }
  
  deallocRows();
  indexScans.clear();
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
      setNdbError(err);
      return NDBT_FAILED;
    }

   NdbOperation* pOp = pTrans->getNdbOperation(tab.getName());	
   if (pOp == NULL) {
     NDB_ERR(pTrans->getNdbError());
     setNdbError(pTrans->getNdbError());
     closeTransaction(pNdb);
     return NDBT_FAILED;
   }
   
   check = pOp->readTupleExclusive();
   if( check == -1 ) {
     NDB_ERR(pTrans->getNdbError());
     setNdbError(pTrans->getNdbError());
     closeTransaction(pNdb);
     return NDBT_FAILED;
   }
   
   // Define primary keys
   if (equalForRow(pOp, r) != 0)
   {
     closeTransaction(pNdb);
     g_err << "Line: " << __LINE__ << " equal for row failed" << endl;
     return NDBT_FAILED;
   }
   
   // Read update value
   for(a = 0; a<tab.getNoOfColumns(); a++){
     if (calc.isUpdateCol(a) == true){
       if((row.attributeStore(a) = 
	   pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	 NDB_ERR(pTrans->getNdbError());
	 setNdbError(pTrans->getNdbError());
	 closeTransaction(pNdb);
	 return NDBT_FAILED;
       }
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
      setNdbError(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    int updates = calc.getUpdatesValue(&row) + (m_empty_update? 0 : 1);

    NdbOperation* pUpdOp;
    pUpdOp = pTrans->getNdbOperation(tab.getName());	
    if (pUpdOp == NULL) {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    check = pUpdOp->interpretedUpdateTuple();
    if( check == -1 ) {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    // PKs
    if (equalForRow(pUpdOp, r) != 0)
    {
      closeTransaction(pNdb);
       g_err << "Line: " << __LINE__ << " equal for row failed" << endl;
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
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
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
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
    }


    
    check = pTrans->execute(Commit, AbortOnError);   
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
      setNdbError(err);
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
			       int doSleep,
                               int start_record,
                               int step)
{
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
      g_err << "ERROR: has retried this operation " << retryAttempt 
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    if (doSleep > 0)
      NdbSleep_MilliSleep(doSleep);

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
      setNdbError(err);
      g_err << r << ": " << err.code << " " << err.message << endl;
      return NDBT_FAILED;
    }

    NDB_TICKS timer_start;
    NDB_TICKS timer_stop;
    bool timer_active =
      m_stats_latency != 0 &&
      r >= batch &&             // first batch is "warmup"
      r + batch != records;     // last batch is usually partial

    if (timer_active)
      timer_start = NdbTick_getCurrentTicks();

    int row = start_record + (r * step);
    if(pkDeleteRecord(pNdb,
                      row,
                      batch,
                      step) != NDBT_OK)
    {
      const NdbError err = pTrans->getNdbError();
      NDB_ERR(err);
      setNdbError(err);
      g_err << r << ": " << err.code << " " << err.message << endl;
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    check = pTrans->execute(Commit, AbortOnError);   
    if( check == -1) {
      const NdbError err = pTrans->getNdbError();
      
      switch(err.status){
      case NdbError::TemporaryError:
	NDB_ERR(err);
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
        g_err << "r = " << r << endl;
	NDB_ERR(err);
	setNdbError(err);
	g_err << r << ": " << err.code << " " << err.message << endl;
	closeTransaction(pNdb);
	return NDBT_FAILED;
	break;
	
      default:
	NDB_ERR(err);
	setNdbError(err);
	g_err << r << ": " << err.code << " " << err.message << endl;
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
      timer_stop = NdbTick_getCurrentTicks();
      Uint64 elapsed = NdbTick_Elapsed(timer_start, timer_stop).microSec();
      m_stats_latency->addObservation((double)elapsed);
    }

    r += batch; // Read next record
    batch_no++;
  }

  g_info << "|- " << deleted << " records deleted" << endl;
  return NDBT_OK;
}

int 
HugoTransactions::pkRefreshRecords(Ndb* pNdb,
                                   int startFrom,
                                   int count,
                                   int batch)
{
  int r = 0;
  int retryAttempt = 0;

  g_info << "|- Refreshing records..." << startFrom << "-" << (startFrom+count)
         << " (batch=" << batch << ")" << endl;

  while (r < count)
  {
    if(r + batch > count)
      batch = count - r;

    if (retryAttempt >= m_retryMax)
    {
      g_err << "ERROR: has retried this operation " << retryAttempt
	     << " times, failing!, line: " << __LINE__ << endl;
      return NDBT_FAILED;
    }

    pTrans = pNdb->startTransaction();
    if (pTrans == NULL)
    {
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

    if (pkRefreshRecord(pNdb, r, batch) != NDBT_OK)
    {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }

    if (pTrans->execute(Commit, AbortOnError) == -1)
    {
      const NdbError err = pTrans->getNdbError();

      switch(err.status){
      case NdbError::TemporaryError:
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
	break;

      default:
	NDB_ERR(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    }

    closeTransaction(pNdb);
    r += batch; // Read next record
  }

  return NDBT_OK;
}

int
HugoTransactions::pkReadUnlockRecords(Ndb* pNdb, 
                                      int records,
                                      int batch,
                                      NdbOperation::LockMode lm)
{
  int                  reads = 0;
  int                  r = 0;
  int                  retryAttempt = 0;
  int                  check;
  
  if (batch == 0) {
    g_err << "ERROR: Argument batch == 0 in pkReadRecords(). Not allowed.";
    g_err << " line: " << __LINE__ << endl;
    return NDBT_FAILED;
  }

  if (idx != NULL) {
    g_err << "ERROR: Cannot call pkReadUnlockRecords for index";
    g_err << " line: " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  
  while (r < records){
    if(r + batch > records)
      batch = records - r;
    
    if (retryAttempt >= m_retryMax){
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

    NDB_TICKS timer_start;
    NDB_TICKS timer_stop;
    bool timer_active =
      m_stats_latency != 0 &&
      r >= batch &&             // first batch is "warmup"
      r + batch != records;     // last batch is usually partial

    if (timer_active)
      timer_start = NdbTick_getCurrentTicks();

    Vector<const NdbLockHandle*> lockHandles;

    NdbOperation::LockMode lmused;
    if(pkReadRecordLockHandle(pNdb, lockHandles, r, batch, lm, &lmused) != NDBT_OK)
    {
      NDB_ERR(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
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
      switch(err.code){
      case 626: // Tuple did not exist
	g_info << r << ": " << err.code << " " << err.message << endl;
	r++;
	break;

      default:
	NDB_ERR(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    } else {
      /* Execute succeeded ok */
      for (int b=0; (b<batch) && (r+b<records); b++){ 
        if (calc.verifyRowValues(rows[b]) != 0){
          closeTransaction(pNdb);
          g_err << "Line: " << __LINE__ << " verify row failed" << endl;
          return NDBT_FAILED;
        }
        reads++;
        r++;
      }
      
      if (pkUnlockRecord(pNdb,
                         lockHandles) != NDBT_OK)
      {
        closeTransaction(pNdb);
        g_err << "Line: " << __LINE__ << " unlock row failed" << endl;
        return NDBT_FAILED;
      }
      
      check = pTrans->execute(Commit, AbortOnError);
      
      if (check == -1 ) 
      {
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
    }
    
    closeTransaction(pNdb);
    
    if (timer_active) {
      timer_stop = NdbTick_getCurrentTicks();
      Uint64 elapsed = NdbTick_Elapsed(timer_start, timer_stop).microSec();
      m_stats_latency->addObservation((double)elapsed);
    }
  }
  deallocRows();
  g_info << reads << " records read" << endl;
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
      setNdbError(err);
      return NDBT_FAILED;
    }

    if(pkReadRecord(pNdb, r, lockBatch, lm) != NDBT_OK)
    {
      NDB_ERR(pTrans->getNdbError());
      setNdbError(pTrans->getNdbError());
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    
    // NoCommit lockTime times with 100 millis interval
    int sleepInterval = 50;
    int lockCount = lockTime / sleepInterval;
    int commitCount = 0;
    bool tempErr = false;
    do {
      check = pTrans->execute(NoCommit, AbortOnError);   
      if( check == -1) {
	const NdbError err = pTrans->getNdbError();
	
	if (err.status == NdbError::TemporaryError){
	  NDB_ERR(err);
	  closeTransaction(pNdb);
	  NdbSleep_MilliSleep(50);
	  tempErr = true;
          retryAttempt++;
          break;
	}
	NDB_ERR(err);
	setNdbError(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      for (int b=0; (b<lockBatch) && (r+b<records); b++){ 
	if (calc.verifyRowValues(rows[b]) != 0){
	  closeTransaction(pNdb);
          g_err << "Line: " << __LINE__ << " verify row failed" << endl;
	  return NDBT_FAILED;
	}
      }
      commitCount++;
      NdbSleep_MilliSleep(sleepInterval);
    } while (commitCount < lockCount);

    if (tempErr)
      continue; /* Retry lock attempt */
    
    // Really commit the trans, puuh!
    check = pTrans->execute(Commit, AbortOnError);   
    if( check == -1) {
      const NdbError err = pTrans->getNdbError();
      
      if (err.status == NdbError::TemporaryError){
	NDB_ERR(err);
	closeTransaction(pNdb);
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      NDB_ERR(err);
      setNdbError(err);
      closeTransaction(pNdb);
      return NDBT_FAILED;
    }
    else{
      for (int b=0; (b<lockBatch) && (r<records); b++){ 
	if (calc.verifyRowValues(rows[b]) != 0){
	  closeTransaction(pNdb);
          g_err << "Line: " << __LINE__ << " verify row failed" << endl;
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
 
  if (pIndex == NULL)
  {
    g_info << "Index " << idxName << " not existing" << endl;
    return NDBT_FAILED;
  }

  const bool ordered = (pIndex->getType()==NdbDictionary::Index::OrderedIndex);

  if (batch == 0) {
    g_err << "ERROR: Argument batch == 0 in indexReadRecords(). "
	   << "Not allowed, line: " << __LINE__ << endl;
    return NDBT_FAILED;
  }
  
  if (ordered) {
    batch = 1;
  }

  allocRows(batch);
  
  while (r < records){
    if (retryAttempt >= m_retryMax){
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
      setNdbError(err);
      return NDBT_FAILED;
    }
    
    for(int b=0; (b<batch) && (r+b < records); b++){
      if(!ordered){
	pOp = pTrans->getNdbIndexOperation(idxName, tab.getName());	
	if (pOp == NULL) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	check = pOp->readTuple();
      } else {
	pOp = sOp = pTrans->getNdbIndexScanOperation(idxName, tab.getName());
	if (sOp == NULL) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	check = sOp->readTuples();
      }
      
      if( check == -1 ) {
	NDB_ERR(pTrans->getNdbError());
	setNdbError(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      
      // Define primary keys
      if (equalForRow(pOp, r+b) != 0)
      {
        closeTransaction(pNdb);
        g_err << "Line: " << __LINE__ << " equal for row failed" << endl;
        return NDBT_FAILED;
      }
      
      // Define attributes to read  
      for(a = 0; a<tab.getNoOfColumns(); a++){
	if((rows[b]->attributeStore(a) = 
	    pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
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
	NDB_ERR(err);
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
	NDB_ERR(err);
	setNdbError(err);
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
    } else{
      for (int b=0; (b<batch) && (r+b<records); b++){ 
	if (calc.verifyRowValues(rows[b]) != 0){
	  closeTransaction(pNdb);
          g_err << "Line: " << __LINE__ 
                << " verify row failed"
                << ", record: " << r << " of: " << records 
                << ", row: " << b << " in a batch of: " << batch 
                << endl;
	  return NDBT_FAILED;
	}
	reads++;
	r++;
      }
      if(ordered && sOp->nextResult(true) == 0){
	ndbout << "Error when comparing records "
	       << " - index op next_result to many" << endl;
        ndbout << "Line: " << __LINE__ << endl;
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
      setNdbError(err);
      return NDBT_FAILED;
    }

    for(b = 0; b<batch && (b+r)<records; b++){
      if(!ordered){
	pOp = pTrans->getNdbIndexOperation(idxName, tab.getName());	
	if (pOp == NULL) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
	
	check = pOp->readTupleExclusive();
	if( check == -1 ) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      } else {
	pOp = sOp = pTrans->getNdbIndexScanOperation(idxName, tab.getName());
	if (pOp == NULL) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
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
        g_err << "Line: " << __LINE__ << " equal for row failed" << endl;
        return NDBT_FAILED;
      }
      
      // Define attributes to read  
      for(a = 0; a<tab.getNoOfColumns(); a++){
	if((rows[b]->attributeStore(a) = 
	    pOp->getValue(tab.getColumn(a)->getName())) == 0) {
	  NDB_ERR(pTrans->getNdbError());
	  setNdbError(pTrans->getNdbError());
	  closeTransaction(pNdb);
	  return NDBT_FAILED;
	}
      }
    }
     
    check = pTrans->execute(NoCommit, AbortOnError);   
    check = (check == -1 ? -1 : !ordered ? check : sOp->nextResult(true));
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      NDB_ERR(err);
      closeTransaction(pNdb);
      
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      setNdbError(err);
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
        g_err << "Line: " << __LINE__ << " verify row failed" << endl;
	return NDBT_FAILED;
      }
      
      int updates = calc.getUpdatesValue(rows[b]) + (m_empty_update? 0 : 1);
      
      NdbOperation* pUpdOp;
      if(!ordered){
	pUpdOp = pTrans->getNdbIndexOperation(idxName, tab.getName());
	check = (pUpdOp == 0 ? -1 : pUpdOp->updateTuple());
      } else {
	pUpdOp = sOp->updateCurrentTuple();
      }

      if (pUpdOp == NULL) {
	NDB_ERR(pTrans->getNdbError());
	setNdbError(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      
      if( check == -1 ) {
	NDB_ERR(pTrans->getNdbError());
	setNdbError(pTrans->getNdbError());
	closeTransaction(pNdb);
	return NDBT_FAILED;
      }
      
      if(!ordered)
      {
        if (equalForRow(pUpdOp, r+b) != 0)
        {
          closeTransaction(pNdb);
          g_err << "Line: " << __LINE__ << " equal for row failed" << endl;
          return NDBT_FAILED;
        }
      }
      
      for(a = 0; a<tab.getNoOfColumns(); a++){
	if (tab.getColumn(a)->getPrimaryKey() == false){
	  if(setValueForAttr(pUpdOp, a, r+b, updates ) != 0){
	    NDB_ERR(pTrans->getNdbError());
	    setNdbError(pTrans->getNdbError());
	    closeTransaction(pNdb);
	    return NDBT_FAILED;
	  }
	}
      }
    }
    
    check = pTrans->execute(Commit, AbortOnError);   
    if( check == -1 ) {
      const NdbError err = pTrans->getNdbError();
      NDB_ERR(err);
      closeTransaction(pNdb);
      
      if (err.status == NdbError::TemporaryError){
	NdbSleep_MilliSleep(50);
	retryAttempt++;
	continue;
      }
      ndbout << "r = " << r << endl;
      setNdbError(err);
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
template class Vector<NdbIndexScanOperation*>;
