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

#include "NdbScanReceiver.hpp"
#include <NdbRecAttr.hpp>

#include <signaldata/ScanFrag.hpp>

#include <NdbOut.hpp>


/***************************************************************************
 * int receiveKEYINFO20( NdbApiSignal* aSignal)
 *
 * Remark:  Handles the reception of the KEYINFO20 signal. 
 *          Save a copy of the signal in list
 *
 ***************************************************************************/
int
NdbScanReceiver::receiveKEYINFO20( NdbApiSignal* aSignal){
  const KeyInfo20 * const keyInfo = CAST_CONSTPTR(KeyInfo20, aSignal->getDataPtr());
  if (theStatus !=  Waiting){
    //ndbout << "Dropping KEYINFO20, theStatus="<<theStatus << endl;
    return -1;
  }
  if (aSignal->getLength() < 5){
    //ndbout << "Dropping KEYINFO20, length="<<aSignal->getLength() << endl;
  }
  Uint64 tCurrTransId = theNdbOp->theNdbCon->getTransactionId();
  Uint64 tRecTransId = (Uint64)keyInfo->transId1 + ((Uint64)keyInfo->transId2 << 32);
  if ((tRecTransId - tCurrTransId) != (Uint64)0){
    //ndbout << "Dropping KEYINFO20 wrong transid" << endl;
    return -1;
  }

  NdbApiSignal * tCopy = new NdbApiSignal(0);//getSignal();
  if (tCopy == NULL) {
    theNdbOp->setErrorCode(4000);
    return 2; // theWaitState = NO_WAIT
  } 
  // Put copy last in list of KEYINFO20 signals
  tCopy->copyFrom(aSignal);
  tCopy->next(NULL);
  if (theFirstKEYINFO20_Recv == NULL)
    theFirstKEYINFO20_Recv = tCopy;
  else
    theLastKEYINFO20_Recv->next(tCopy);
  theLastKEYINFO20_Recv = tCopy;

  theTotalKI_Len = keyInfo->keyLen; // This is the total length of all signals
  theTotalRecKI_Len += aSignal->getLength() - 5;    
  return theNdbOp->theNdbCon->checkNextScanResultComplete();
}

/***************************************************************************
 * int receiveTRANSID_AI_SCAN( NdbApiSignal* aSignal)
 *
 * Remark:  Handles the reception of the TRANSID_AI_signal with 
 *          22 signal data words.
 *          Save a copy of the signal in list and check if all 
 *          signals belonging to this resultset is receieved.
 *
 ***************************************************************************/
int
NdbScanReceiver::receiveTRANSID_AI_SCAN( NdbApiSignal* aSignal)
{
  const Uint32* aDataPtr = aSignal->getDataPtr();
  if (theStatus !=  Waiting){
    //ndbout << "Dropping TRANSID_AI, theStatus="<<theStatus << endl;
    return -1;
  }
  if (aSignal->getLength() < 3){
    //ndbout << "Dropping TRANSID_AI, length="<<aSignal->getLength() << endl;
    return -1;
  }
  if (theNdbOp == NULL){
    //ndbout << "Dropping TRANSID_AI, theNdbOp == NULL" << endl;
    return -1;
  }
  if (theNdbOp->theNdbCon == NULL){
    //ndbout << "Dropping TRANSID_AI, theNdbOp->theNdbCon == NULL" << endl;
    return -1;
  }
  Uint64 tCurrTransId = theNdbOp->theNdbCon->getTransactionId();
  Uint64 tRecTransId = (Uint64)aDataPtr[1] + ((Uint64)aDataPtr[2] << 32);
  if ((tRecTransId - tCurrTransId) != (Uint64)0){
    //ndbout << "Dropping TRANSID_AI wrong transid" << endl;
    return -1;
  }

  NdbApiSignal * tCopy = new NdbApiSignal(0);//getSignal();
  if (tCopy == NULL){
    theNdbOp->setErrorCode(4000);
    return 2; // theWaitState = NO_WAIT
  }
  tCopy->copyFrom(aSignal);
  tCopy->next(NULL);
  if (theFirstTRANSID_AI_Recv == NULL)
      theFirstTRANSID_AI_Recv = tCopy;
  else
      theLastTRANSID_AI_Recv->next(tCopy);
  theLastTRANSID_AI_Recv = tCopy;
  theTotalRecAI_Len += aSignal->getLength() - 3;    

  return theNdbOp->theNdbCon->checkNextScanResultComplete();
}

/***************************************************************************
 * int executeSavedSignals()
 *
 * Remark:  Execute all saved TRANSID_AI signals into the parent NdbOperation
 *          
 *
 ***************************************************************************/
int
NdbScanReceiver::executeSavedSignals(){

  NdbApiSignal* tSignal = theFirstTRANSID_AI_Recv;
  while (tSignal != NULL) {
    const Uint32* tDataPtr = tSignal->getDataPtr();
    
    int tRet = theNdbOp->receiveREAD_AI((Uint32*)&tDataPtr[3], 
					tSignal->getLength() - 3);
    if (tRet != -1){
      // -1 means that more signals are wanted ?
      // Make sure there are no more signals in the list
      assert(tSignal->next() == NULL);
    }
    tSignal = tSignal->next();
  }
  // receiveREAD_AI may not copy to application buffers
  NdbRecAttr* tRecAttr = theNdbOp->theFirstRecAttr;
  while (tRecAttr != NULL) {
    if (tRecAttr->copyoutRequired())    // copy to application buffer
      tRecAttr->copyout();
    tRecAttr = tRecAttr->next();
  }
  // Release TRANSID_AI signals for this receiver
  while(theFirstTRANSID_AI_Recv != NULL){
    NdbApiSignal* tmp = theFirstTRANSID_AI_Recv;
    theFirstTRANSID_AI_Recv = tmp->next();
    delete tmp;
  }

  //  theNdbOp->theNdb->releaseSignalsInList(&theFirstTRANSID_AI_Recv);
  theFirstTRANSID_AI_Recv = NULL;
  theLastTRANSID_AI_Recv = NULL;
  theStatus = Executed;

  return 0;
}


void 
NdbScanReceiver::prepareNextScanResult(){
  if(theStatus == Executed){

    // theNdbOp->theNdb->releaseSignalsInList(&theFirstKEYINFO20_Recv);
    while(theFirstKEYINFO20_Recv != NULL){
      NdbApiSignal* tmp = theFirstKEYINFO20_Recv;
      theFirstKEYINFO20_Recv = tmp->next();
      delete tmp;
    }
    theFirstKEYINFO20_Recv = NULL;
    theLastKEYINFO20_Recv = NULL;
    theTotalRecAI_Len = 0;
    theTotalRecKI_Len = 0;
    if (theLockMode == true)
      theTotalKI_Len = 0xFFFFFFFF;
    else
      theTotalKI_Len = 0;
    theStatus = Waiting;
  }
}
