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


/*****************************************************************************
 * Name:          NdbConnectionScan.cpp
 * Include:
 * Link:
 * Author:        UABRONM MikaelRonström UAB/M/MT       
 *                QABJKAM Jonas Kamf UAB/M/MT                  
 * Date:          2000-06-12
 * Version:       0.1
 * Description:   Interface between Application and NDB
 * Documentation:
 * Adjust:  2000-06-12  UABRONM   First version.
 ****************************************************************************/
#include <ndb_global.h>

#include <Ndb.hpp>
#include <NdbConnection.hpp>
#include <NdbOperation.hpp>
#include <NdbScanOperation.hpp>
#include "NdbScanReceiver.hpp"
#include "NdbApiSignal.hpp"
#include "TransporterFacade.hpp"
#include "NdbUtil.hpp"
#include "API.hpp"
#include "NdbImpl.hpp"

#include <signaldata/ScanTab.hpp>

#include <NdbOut.hpp>

// time out for next scan result (-1 is infinite)
// XXX should change default only if non-trivial interpreted program is used
#define WAITFOR_SCAN_TIMEOUT    120000


/*****************************************************************************
 * int executeScan();
 *
 * 1. Check that the transaction is started and other important preconditions
 * 2. Tell the kernel to start scanning by sending one SCAN_TABREQ, if 
 *    parallelism is greater than 16 also send one SCAN_TABINFO for each 
 *    additional 16 
 *    Define which attributes to scan in ATTRINFO, this signal also holds the 
 *    interpreted program
 * 3. Wait for the answer of the SCAN_TABREQ. This is either a SCAN_TABCONF if
 *    the scan was correctly defined and a SCAN_TABREF if the scan couldn't 
 *    be started.
 * 4. Check the result, if scan was not started return -1
 *
 ****************************************************************************/
int
NdbConnection::executeScan(){
  if (theTransactionIsStarted == true){ // Transaction already started.
    setErrorCode(4600);
    return -1;
  }
  if (theStatus != Connected) {       // Lost connection 
    setErrorCode(4601); 
    return -1;
  }
  if (theScanningOp == NULL){
    setErrorCode(4602); // getNdbOperation must be called before executeScan
    return -1;
  }
  TransporterFacade* tp = TransporterFacade::instance();
  theNoOfOpCompleted = 0;
  theNoOfSCANTABCONFRecv = 0;
  tp->lock_mutex();
  if (tp->get_node_alive(theDBnode) &&
     (tp->getNodeSequence(theDBnode) == theNodeSequence)) {
    if (tp->check_send_size(theDBnode, get_send_size())) {
      theTransactionIsStarted = true;
      if (sendScanStart() == -1){
        tp->unlock_mutex();
        return -1;
      }//if
      theNdb->theWaiter.m_node = theDBnode;
      theNdb->theWaiter.m_state = WAIT_SCAN;
      int res = theNdb->receiveResponse(WAITFOR_SCAN_TIMEOUT);
      if (res == 0) {
        return 0;
      } else {
        if (res == -1) {
          setErrorCode(4008);
        } else if (res == -2) {
          theTransactionIsStarted = false;
          theReleaseOnClose = true;
          setErrorCode(4028);
        } else {
          ndbout << "Impossible return from receiveResponse in executeScan";
          ndbout << endl;
          abort();
        }//if
        theCommitStatus = Aborted;
        return -1;
      }//if
    } else {
      TRACE_DEBUG("Start a scan with send buffer full attempted");
      setErrorCode(4022);
      theCommitStatus = Aborted;
    }//if
  } else {
    if (!(tp->get_node_stopping(theDBnode) &&
         (tp->getNodeSequence(theDBnode) == theNodeSequence))) {
      TRACE_DEBUG("The node is hard dead when attempting to start a scan");
      setErrorCode(4029);
      theReleaseOnClose = true;
    } else {
      TRACE_DEBUG("The node is stopping when attempting to start a scan");
      setErrorCode(4030);
    }//if
    theCommitStatus = Aborted;
  }//if
  tp->unlock_mutex();
  return -1;
}

/******************************************************************************
 * int nextScanResult();
 * Remark:
 *  This method is used to distribute data received to the application.
 *  Iterate through the list and search for operations that haven't 
 *  been distributed yet (status != Finished).
 *  If there are no more operations/records still waiting to be exececuted
 *  we have to send SCAN_NEXTREQ to fetch next set of records.
 *
 *  TODO - This function should be able to return a value indicating if
 *  there are any more records already fetched from memory or if it has to
 *  ask the db for more. This would mean we could get better performance when
 * takeOver is used wince we can take over all ops already fetched, put them
 * in another trans and send them of to the db when there are no more records
 * already fetched. Maybe use a new argument to the function for this
******************************************************************************/
int
NdbConnection::nextScanResult(bool fetchAllowed){ 

  if (theTransactionIsStarted != true){ // Transaction not started.
    setErrorCode(4601);
    return -1;
  }
  // Scan has finished ok but no operations recived = empty recordset.
  if(theScanFinished == true){
    return 1; // No more records
  }
  if (theStatus != Connected){// Lost connection
    setErrorCode(4601); 
    return -1; 
  }
  // Something went wrong, probably we got a SCAN_TABREF earlier.
  if (theCompletionStatus == CompletedFailure) {
    return -1;
  }
  if (theNoOfOpCompleted == theNoOfOpFetched) {   
    // There are no more records cached in NdbApi 
    if (fetchAllowed == true){
      // Get some more records from db

      if (fetchNextScanResult() == -1){
	return -1;
      }
      if (theScanFinished == true) { // The scan has finished.
	return 1; // 1 = No more records
      }
      if (theCompletionStatus == CompletedFailure) {
	return -1; // Something went wrong, probably we got a SCAN_TABREF.
      }
    } else {
      // There where no more cached records in NdbApi
      // and we where not allowed to go to db and ask for 
      // more
      return 2;
    }
  }

  // It's not allowed to come here without any cached records
  if (theCurrentScanRec == NULL){
#ifdef VM_TRACE
    ndbout << "nextScanResult("<<fetchAllowed<<")"<<endl
	   << "  theTransactionIsStarted = " << theTransactionIsStarted << endl
	   << "  theScanFinished = " << theScanFinished << endl
	   << "  theCommitStatus = " << theCommitStatus << endl
	   << "  theStatus = " << theStatus << endl
	   << "  theCompletionStatus = " << theCompletionStatus << endl
	   << "  theNoOfOpCompleted = " << theNoOfOpCompleted << endl
	   << "  theNoOfOpFetched = " << theNoOfOpFetched << endl
	   << "  theScanningOp = " << theScanningOp << endl
	   << "  theNoOfSCANTABCONFRecv = "<< theNoOfSCANTABCONFRecv << endl
	   << "  theNdb->theWaiter.m_node = " <<theNdb->theWaiter.m_node<<endl
	   << "  theNdb->theWaiter.m_state = " << theNdb->theWaiter.m_state << endl;
    abort();
#endif
    return -1;
  }

  // Execute the saved signals for this operation.
  NdbScanReceiver* tScanRec = theCurrentScanRec;
  theScanningOp->theCurrRecAI_Len = 0;
  theScanningOp->theCurrentRecAttr = theScanningOp->theFirstRecAttr;
  if(tScanRec->executeSavedSignals() != 0)
    return -1;
  theNoOfOpCompleted++;
  // Remember for next iteration and takeOverScanOp
  thePreviousScanRec = tScanRec; 
  theCurrentScanRec = tScanRec->next(); 
  return 0;   // 0 = There are more rows to be fetched.
}

/******************************************************************************
 * int  stopScan()
 * Remark:  By sending SCAN_NEXTREQ with data word 2 set to TRUE we 
 *          abort the scan process.
 *****************************************************************************/
int
NdbConnection::stopScan()
{
  if(theScanFinished == true){
    return 0; 
  }
  if (theCompletionStatus == CompletedFailure){
    return 0;
  }

  if (theScanningOp == 0){
    return 0;
  }

  theNoOfOpCompleted = 0;
  theNoOfSCANTABCONFRecv = 0;
  theScanningOp->prepareNextScanResult(); 
  return sendScanNext(1);
}


/********************************************************************
 * int sendScanStart()
 *
 * Send the signals reuired to define and start the scan
 * 1. Send SCAN_TABREQ
 * 2. Send SCAN_TABINFO(if any, parallelism must be > 16)
 * 3. Send ATTRINFO signals
 *
 * Returns -1 if an error occurs otherwise 0.
 *
 ********************************************************************/
int
NdbConnection::sendScanStart(){

  /***** 0. Prepare signals ******************/
  // This might modify variables and signals
  if(theScanningOp->prepareSendScan(theTCConPtr, 
				    theTransactionId) == -1)
    return -1;

  /***** 1. Send SCAN_TABREQ **************/
  /***** 2. Send SCAN_TABINFO *************/    
  /***** 3. Send ATTRINFO signals *********/
  if (theScanningOp->doSendScan(theDBnode) == -1)
    return -1;
  return 0;
}


int 
NdbConnection::fetchNextScanResult(){
  theNoOfOpCompleted = 0;
  theNoOfSCANTABCONFRecv = 0;
  theScanningOp->prepareNextScanResult(); 
  return sendScanNext(0);
}



/***********************************************************
 * int sendScanNext(int stopScanFlag)
 *
 * ************************************************************/
int NdbConnection::sendScanNext(bool stopScanFlag){  
  NdbApiSignal tSignal(theNdb->theMyRef);
  Uint32 tTransId1, tTransId2;
  tSignal.setSignal(GSN_SCAN_NEXTREQ);
  tSignal.setData(theTCConPtr, 1);
  // Set the stop flag in word 2(1 = stop)
  Uint32 tStopValue;
  tStopValue = stopScanFlag == true ? 1 : 0;
  tSignal.setData(tStopValue, 2);
  tTransId1 = (Uint32) theTransactionId;
  tTransId2 = (Uint32) (theTransactionId >> 32);
  tSignal.setData(tTransId1, 3);
  tSignal.setData(tTransId2, 4);
  tSignal.setLength(4);
  Uint32 conn_seq = theNodeSequence;
  int return_code = theNdb->sendRecSignal(theDBnode,
                                          WAIT_SCAN,
                                          &tSignal,
                                          conn_seq);
  if (return_code == 0) {
    return 0;
  } else if (return_code == -1) { // Time-out
    TRACE_DEBUG("Time-out when sending sendScanNext");
    setErrorCode(4024);
    theTransactionIsStarted = false;
    theReleaseOnClose = true;
    theCommitStatus = Aborted;
  } else if (return_code == -2) { // Node failed
    TRACE_DEBUG("Node failed when sendScanNext");
    setErrorCode(4027);
    theTransactionIsStarted = false;
    theReleaseOnClose = true;
    theCommitStatus = Aborted;
  } else if (return_code == -3) {
    TRACE_DEBUG("Send failed when sendScanNext");
    setErrorCode(4033);
    theTransactionIsStarted = false;
    theReleaseOnClose = true;
    theCommitStatus = Aborted;
  } else if (return_code == -4) {
    TRACE_DEBUG("Send buffer full when sendScanNext");
    setErrorCode(4032);
  } else if (return_code == -5) {
    TRACE_DEBUG("Node stopping when sendScanNext");
    setErrorCode(4034);
  } else {
    ndbout << "Impossible return from sendRecSignal" << endl;
    abort();
  }//if
  return -1;
}


/***************************************************************************
 * int  receiveSCAN_TABREF(NdbApiSignal* aSignal)
 *
 *  This means the scan could not be started, set status(s) to indicate 
 *  the failure
 *
 ****************************************************************************/
int			
NdbConnection::receiveSCAN_TABREF(NdbApiSignal* aSignal){
  const ScanTabRef * const scanTabRef = CAST_CONSTPTR(ScanTabRef, aSignal->getDataPtr());
  if (theStatus != Connected){
#ifdef VM_TRACE
    ndbout << "SCAN_TABREF dropped, theStatus = " << theStatus << endl; 
#endif
    return -1;
  }
  if (aSignal->getLength() != ScanTabRef::SignalLength){
#ifdef VM_TRACE
    ndbout << "SCAN_TABREF dropped, signal length " << aSignal->getLength() << endl;
#endif
    return -1;
  }
  const Uint64 tCurrTransId = this->getTransactionId();
  const Uint64 tRecTransId = (Uint64)scanTabRef->transId1 + 
    ((Uint64)scanTabRef->transId2 << 32);
  if ((tRecTransId - tCurrTransId) != (Uint64)0){
#ifdef VM_TRACE
    ndbout << "SCAN_TABREF dropped, wrong transid" << endl;
#endif
    return -1;
  }
#if 0 
  ndbout << "SCAN_TABREF, "
	 <<"transid=("<<hex<<scanTabRef->transId1<<", "<<hex<<scanTabRef->transId2<<")"
	 <<", err="<<dec<<scanTabRef->errorCode << endl;
#endif
  setErrorCode(scanTabRef->errorCode);
  theCompletionStatus = CompletedFailure;
  theCommitStatus = Aborted; // Indicate that this "transaction" was aborted
  theTransactionIsStarted = false;
  theScanningOp->releaseSignals();
  return 0;
}

/*****************************************************************************
 * int  receiveSCAN_TABCONF(NdbApiSignal* aSignal)
 *
 * Receive SCAN_TABCONF
 * If scanStatus == 0 there is more records to read. Since signals may be 
 * received in any order we have to go through the lists with saved signals 
 * and check if all expected signals are there so that we can start to 
 * execute them.
 *
 * If scanStatus > 0 this indicates that the scan is finished and there are 
 * no more data to be read.
 * 
 *****************************************************************************/
int			
NdbConnection::receiveSCAN_TABCONF(NdbApiSignal* aSignal)
{
  const ScanTabConf * const conf = CAST_CONSTPTR(ScanTabConf, aSignal->getDataPtr());
  if (theStatus != Connected){
#ifdef VM_TRACE
    ndbout << "Dropping SCAN_TABCONF, theStatus = "<< theStatus << endl;
#endif
    return -1;
  }
  if(aSignal->getLength() != ScanTabConf::SignalLength){
#ifdef VM_TRACE
    ndbout << "Dropping SCAN_TABCONF, getLength = "<< aSignal->getLength() << endl;
#endif
    return -1;
  }
  const Uint64 tCurrTransId = this->getTransactionId();
  const Uint64 tRecTransId = 
    (Uint64)conf->transId1 + ((Uint64)conf->transId2 << 32);
  if ((tRecTransId - tCurrTransId) != (Uint64)0){
#ifdef VM_TRACE
    ndbout << "Dropping SCAN_TABCONF, wrong transid" << endl;
#endif
    return -1;
  }

  const Uint8 scanStatus = 
    ScanTabConf::getScanStatus(conf->requestInfo);

  if (scanStatus != 0) {
    theCompletionStatus = CompletedSuccess;
    theCommitStatus = Committed;
    theScanFinished = true;  
    return 0;
  }

  // There can only be one SCANTABCONF
  assert(theNoOfSCANTABCONFRecv == 0);
  theNoOfSCANTABCONFRecv++;

  // Save a copy of the signal  
  NdbApiSignal * tCopy = new NdbApiSignal(0);//getSignal();
  if (tCopy == NULL){
    setErrorCode(4000);
    return 2; // theWaiter.m_state = NO_WAIT
  }
  tCopy->copyFrom(aSignal);
  tCopy->next(NULL);
  theScanningOp->theSCAN_TABCONF_Recv = tCopy;

  return checkNextScanResultComplete();

}

/*****************************************************************************
 * int  receiveSCAN_TABINFO(NdbApiSignal* aSignal)
 *
 * Receive SCAN_TABINFO
 * 
 *****************************************************************************/
int			
NdbConnection::receiveSCAN_TABINFO(NdbApiSignal* aSignal)
{
  if (theStatus != Connected){
    //ndbout << "SCAN_TABINFO dropped, theStatus = " << theStatus << endl; 
    return -1;
  }
  if (aSignal->getLength() !=  ScanTabInfo::SignalLength){
    //ndbout << "SCAN_TABINFO dropped, length = " << aSignal->getLength() << endl; 
    return -1;
  }

  NdbApiSignal * tCopy = new NdbApiSignal(0);//getSignal();
  if (tCopy == NULL){
    setErrorCode(4000);
    return 2; // theWaiter.m_state = NO_WAIT
  }
  tCopy->copyFrom(aSignal);
  tCopy->next(NULL);

  // Put  the signal last in list
  if (theScanningOp->theFirstSCAN_TABINFO_Recv == NULL)
      theScanningOp->theFirstSCAN_TABINFO_Recv = tCopy;
  else
      theScanningOp->theLastSCAN_TABINFO_Recv->next(tCopy);
  theScanningOp->theLastSCAN_TABINFO_Recv = tCopy;

  return checkNextScanResultComplete();
}

/******************************************************************************
 * int   checkNextScanResultComplete(NdbApiSignal* aSignal)
 *
 * Remark      Traverses all the lists that are associated with 
 *             this resultset and checks if all signals are there.
 *             If all required signal are  received return 0
 *
 *
 *****************************************************************************/
int
NdbConnection::checkNextScanResultComplete(){

  if (theNoOfSCANTABCONFRecv != 1) {
    return -1;
  }
  
  Uint32 tNoOfOpFetched = 0;
  theCurrentScanRec = NULL; 
  thePreviousScanRec = NULL;

  const ScanTabConf * const conf = 
    CAST_CONSTPTR(ScanTabConf, theScanningOp->theSCAN_TABCONF_Recv->getDataPtr());
  const Uint32 numOperations = ScanTabConf::getOperations(conf->requestInfo);
  Uint32 sigIndex = 0;
  NdbApiSignal* tSignal = theScanningOp->theFirstSCAN_TABINFO_Recv;
  while(tSignal != NULL){
    const ScanTabInfo * const info = CAST_CONSTPTR(ScanTabInfo, tSignal->getDataPtr());
    // Loop through the operations for this SCAN_TABINFO
    // tOpAndLength is allowed to be zero, this means no 
    // TRANSID_AI signals where sent for this record
    // I.e getValue was called 0 times when defining scan

    // The max number of operations in each signal is 16
    Uint32 numOpsInSig = numOperations - sigIndex*16;
    if (numOpsInSig > 16)
      numOpsInSig = 16;
    for(Uint32 i = 0; i < numOpsInSig; i++){
      const Uint32 tOpAndLength = info->operLenAndIdx[i];
      const Uint32 tOpIndex = ScanTabInfo::getIdx(tOpAndLength);
      const Uint32 tOpLen = ScanTabInfo::getLen(tOpAndLength);

      assert(tOpIndex < 256);
      NdbScanReceiver* tScanRec = 
	theScanningOp->theScanReceiversArray[tOpIndex];
      assert(tScanRec != NULL);
      if(tScanRec->isCompleted(tOpLen))
	tScanRec->setCompleted();
      else{
	return -1; // At least one receiver was not ready
      }

	// Build list of scan receivers
      if (theCurrentScanRec == NULL) {
	theCurrentScanRec = tScanRec;
	thePreviousScanRec = tScanRec;
      } else {
	thePreviousScanRec->next(tScanRec);
	thePreviousScanRec = tScanRec;
      }
      tNoOfOpFetched++;      
    }
    tSignal = tSignal->next();
    sigIndex++;
  } 

  // Check number of operations fetched against value in SCANTAB_CONF
  if (tNoOfOpFetched != numOperations) {
    setErrorCode(4113);
    return 2; // theWaiter.m_state = NO_WAIT
  }

  // All signals for this resultset recieved
  // release SCAN_TAB signals
  theNoOfSCANTABCONFRecv = 0;
  theScanningOp->releaseSignals();
  
  // We have received all operations with correct lengths.
  thePreviousScanRec = NULL;
  theNoOfOpFetched = tNoOfOpFetched;
  return 0;
}
