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

#include <NdbOut.hpp>
#include <Ndb.hpp>
#include <NdbOperation.hpp>
#include <NdbIndexOperation.hpp>
#include <NdbIndexScanOperation.hpp>
#include <NdbConnection.hpp>
#include "NdbApiSignal.hpp"
#include <NdbRecAttr.hpp>
#include "NdbUtil.hpp"
#include "API.hpp"
#include "NdbBlob.hpp"

void
Ndb::checkFailedNode()
{
  DBUG_ENTER("Ndb::checkFailedNode");
  DBUG_PRINT("enter", ("theNoOfDBnodes: %d", theNoOfDBnodes));

  DBUG_ASSERT(theNoOfDBnodes < MAX_NDB_NODES);
  for (Uint32 i = 0; i < theNoOfDBnodes; i++){
    const NodeId node_id = theDBnodes[i];
    DBUG_PRINT("info", ("i: %d, node_id: %d", i, node_id));
    
    DBUG_ASSERT(node_id < MAX_NDB_NODES);    
    if (the_release_ind[node_id] == 1){

      /**
       * Release all connections in idle list (for node)
       */
      NdbConnection * tNdbCon = theConnectionArray[node_id];
      theConnectionArray[node_id] = NULL;
      while (tNdbCon != NULL) {
        NdbConnection* tempNdbCon = tNdbCon;
        tNdbCon = tNdbCon->next();
        releaseNdbCon(tempNdbCon);
      }
      the_release_ind[node_id] = 0;
    }
  }
  DBUG_VOID_RETURN;
}

#if 0
void
NdbImpl::checkInvalidTable(NdbDictionaryImpl * dict){
  Uint32 sz = m_invalidTables.size();
  for(Int32 i = sz - 1; i >= 0; i--){
    NdbTableImpl * tab = m_invalidTables[i];
    m_invalidTables.erase(i);
    dict->tableDropped(* tab);
  }
}

void
NdbImpl::checkErrorCode(Uint32 i, NdbTableImpl * tab){
  switch(i){
  case 241:
  case 283:
  case 284:
  case 285:
  case 1225:
  case 1226:
    
  }
}
#endif

/***************************************************************************
 * int createConIdleList(int aNrOfCon);
 * 
 * Return Value:   Return the number of created connection object 
 *                 if createConIdleList was succesful
 *                 Return -1: In all other case.  
 * Parameters:     aNrOfCon : Number of connections offered to the application.
 * Remark:         Create connection idlelist with NdbConnection objects.
 ***************************************************************************/ 
int 
Ndb::createConIdleList(int aNrOfCon)
{
  for (int i = 0; i < aNrOfCon; i++)
  {
    NdbConnection* tNdbCon = new NdbConnection(this);
    if (tNdbCon == NULL)
    {
      return -1;
    }
    if (theConIdleList == NULL)
    {
      theConIdleList = tNdbCon;
      theConIdleList->next(NULL);
    } else
    {
      tNdbCon->next(theConIdleList);
      theConIdleList = tNdbCon;
    }
    tNdbCon->Status(NdbConnection::NotConnected);
  }
  theNoOfAllocatedTransactions = aNrOfCon;
  return aNrOfCon; 
}

/***************************************************************************
 * int createOpIdleList(int aNrOfOp);
 *
 * Return Value:   Return the number of created operation object if 
 *                 createOpIdleList was succesful.
 *                 Return -1: In all other case.
 * Parameters:     aNrOfOp:  Number of operations offered to the application. 
 * Remark:         Create  operation idlelist with NdbOperation objects..
 ***************************************************************************/ 
int 
Ndb::createOpIdleList(int aNrOfOp)
{ 
  for (int i = 0; i < aNrOfOp; i++){
    NdbOperation* tOp = new NdbOperation(this);
    if ( tOp == NULL ){
      return -1;
    }
    if (theOpIdleList == NULL){
      theOpIdleList = tOp;
      theOpIdleList->next(NULL);
    } else{
      tOp->next(theOpIdleList);
      theOpIdleList = tOp;
    }
  }
  return aNrOfOp; 
}

/***************************************************************************
 * NdbBranch* NdbBranch();
 *
 * Return Value:   Return a NdbBranch if the  getNdbBranch was successful.
 *                Return NULL : In all other case.  
 * Remark:         Get a NdbBranch from theBranchList and return the object .
 ***************************************************************************/ 
NdbBranch*
Ndb::getNdbBranch()
{
  NdbBranch*    tNdbBranch;
  if ( theBranchList == NULL )
  {
    tNdbBranch = new NdbBranch;
    if (tNdbBranch == NULL)
    {
      return NULL;
    }
    tNdbBranch->theNext = NULL;
  } else
  {
    tNdbBranch = theBranchList;
    theBranchList = tNdbBranch->theNext;
    tNdbBranch->theNext = NULL;
  }
  return tNdbBranch;
}

/***************************************************************************
 * NdbCall* NdbCall();
 *
 * Return Value:   Return a NdbCall if the  getNdbCall was successful.
 *                Return NULL : In all other case.  
 * Remark:         Get a NdbCall from theCallList and return the object .
 ***************************************************************************/ 
NdbCall*
Ndb::getNdbCall()
{
  NdbCall*      tNdbCall;
  if ( theCallList == NULL )
  {
    tNdbCall = new NdbCall;
    if (tNdbCall == NULL)
    {
      return NULL;
    }
    tNdbCall->theNext = NULL;
  } else
  {
    tNdbCall = theCallList;
    theCallList = tNdbCall->theNext;
    tNdbCall->theNext = NULL;
  }
  return tNdbCall;
}

/***************************************************************************
 * NdbConnection* getNdbCon();
 *
 * Return Value:   Return a connection if the  getNdbCon was successful.
 *                Return NULL : In all other case.  
 * Remark:         Get a connection from theConIdleList and return the object .
 ***************************************************************************/ 
NdbConnection*
Ndb::getNdbCon()
{
  NdbConnection*        tNdbCon;
  if ( theConIdleList == NULL ) {
    if (theNoOfAllocatedTransactions < theMaxNoOfTransactions) {
      tNdbCon = new NdbConnection(this);
      if (tNdbCon == NULL) {
        return NULL;
      }//if
      theNoOfAllocatedTransactions++;
    } else {
      ndbout << "theNoOfAllocatedTransactions = " << theNoOfAllocatedTransactions << " theMaxNoOfTransactions = " << theMaxNoOfTransactions << endl;
      return NULL;
    }//if
    tNdbCon->next(NULL);
  } else
  {
    tNdbCon = theConIdleList;
    theConIdleList = tNdbCon->next();
    tNdbCon->next(NULL);
  }
  tNdbCon->theMagicNumber = 0x37412619;
  return tNdbCon;
}

/***************************************************************************
 * NdbLabel* getNdbLabel();
 *
 * Return Value:   Return a NdbLabel if the  getNdbLabel was successful.
 *                 Return NULL : In all other case.  
 * Remark:         Get a NdbLabel from theLabelList and return the object .
 ***************************************************************************/ 
NdbLabel*
Ndb::getNdbLabel()
{
  NdbLabel*     tNdbLabel;
  if ( theLabelList == NULL )
  {
    tNdbLabel = new NdbLabel;
    if (tNdbLabel == NULL)
    {
      return NULL;
    }
    tNdbLabel->theNext = NULL;
  } else
  {
    tNdbLabel = theLabelList;
    theLabelList = tNdbLabel->theNext;
    tNdbLabel->theNext = NULL;
  }
  return tNdbLabel;
}

/***************************************************************************
 * NdbScanReceiver* getNdbScanRec()
 * 
 * Return Value:  Return a NdbScanReceiver
 *                Return NULL : In all other case.  
 * Remark:        Get a NdbScanReceiver from theScanRecList and return the 
 *                object .
 ****************************************************************************/ 
NdbReceiver*
Ndb::getNdbScanRec()
{
  NdbReceiver*      tNdbScanRec;
  if ( theScanList == NULL )
  {
    tNdbScanRec = new NdbReceiver(this);
    if (tNdbScanRec == NULL)
    {
      return NULL;
    }
    tNdbScanRec->next(NULL);
  } else
  {
    tNdbScanRec = theScanList;
    theScanList = tNdbScanRec->next();
    tNdbScanRec->next(NULL);
  }

  return tNdbScanRec;
}

/***************************************************************************
 * NdbSubroutine* getNdbSubroutine();
 *
 * Return Value: Return a NdbSubroutine if the  getNdbSubroutine was successful.
 *                Return NULL : In all other case.  
 * Remark:    Get a NdbSubroutine from theSubroutineList and return the object .
 ***************************************************************************/ 
NdbSubroutine*
Ndb::getNdbSubroutine()
{
  NdbSubroutine*        tNdbSubroutine;
  if ( theSubroutineList == NULL )
  {
    tNdbSubroutine = new NdbSubroutine;
    if (tNdbSubroutine == NULL)
    {
      return NULL;
    }
    tNdbSubroutine->theNext = NULL;
  } else
  {
    tNdbSubroutine = theSubroutineList;
    theSubroutineList = tNdbSubroutine->theNext;
    tNdbSubroutine->theNext = NULL;
  }
  return tNdbSubroutine;
}

/***************************************************************************
NdbOperation* getOperation();

Return Value:   Return theOpList : if the  getOperation was succesful.
                Return NULL : In all other case.  
Remark:         Get an operation from theOpIdleList and return the object .
***************************************************************************/ 
NdbOperation*
Ndb::getOperation()
{
  NdbOperation* tOp = theOpIdleList;
  if (tOp != NULL ) {
    NdbOperation* tOpNext = tOp->next();
    tOp->next(NULL);
    theOpIdleList = tOpNext;
    return tOp;
  } else {
    tOp = new NdbOperation(this);
    if (tOp != NULL)
      tOp->next(NULL);
  }
  return tOp;
}

/***************************************************************************
NdbScanOperation* getScanOperation();

Return Value:   Return theOpList : if the  getScanOperation was succesful.
                Return NULL : In all other case.  
Remark:         Get an operation from theScanOpIdleList and return the object .
***************************************************************************/ 
NdbIndexScanOperation*
Ndb::getScanOperation()
{
  NdbIndexScanOperation* tOp = theScanOpIdleList;
  if (tOp != NULL ) {
    NdbIndexScanOperation* tOpNext = (NdbIndexScanOperation*)tOp->next();
    tOp->next(NULL);
    theScanOpIdleList = tOpNext;
    return tOp;
  } else {
    tOp = new NdbIndexScanOperation(this);
    if (tOp != NULL)
      tOp->next(NULL);
  }
  return tOp;
}

/***************************************************************************
NdbIndexOperation* getIndexOperation();

Return Value:   Return theOpList : if the  getIndexOperation was succesful.
                Return NULL : In all other case.  
Remark:         Get an operation from theIndexOpIdleList and return the object .
***************************************************************************/ 
NdbIndexOperation*
Ndb::getIndexOperation()
{
  NdbIndexOperation* tOp = theIndexOpIdleList;
  if (tOp != NULL ) {
    NdbIndexOperation* tOpNext = (NdbIndexOperation*) tOp->next();
    tOp->next(NULL);
    theIndexOpIdleList = tOpNext;
    return tOp;
  } else {
    tOp = new NdbIndexOperation(this);
    if (tOp != NULL)
      tOp->next(NULL);
  }
  return tOp;
}

/***************************************************************************
NdbRecAttr* getRecAttr();

Return Value:   Return a reference to a receive attribute object.
                Return NULL if it's not possible to get a receive attribute object.
***************************************************************************/
NdbRecAttr*
Ndb::getRecAttr()
{
  NdbRecAttr* tRecAttr;
  tRecAttr = theRecAttrIdleList;
  if (tRecAttr != NULL) {
    NdbRecAttr* tRecAttrNext = tRecAttr->next();
    tRecAttr->init();
    theRecAttrIdleList = tRecAttrNext;
    return tRecAttr;
  } else {
    tRecAttr = new NdbRecAttr;
    if (tRecAttr == NULL)
      return NULL;
    tRecAttr->next(NULL);
  }//if
  tRecAttr->init();
  return tRecAttr;
}

/***************************************************************************
NdbApiSignal* getSignal();

Return Value:   Return a reference to a signal object.
                Return NULL if not possible to get a signal object.
***************************************************************************/
NdbApiSignal*
Ndb::getSignal()
{
  NdbApiSignal* tSignal = theSignalIdleList;
  if (tSignal != NULL){
    NdbApiSignal* tSignalNext = tSignal->next();
    tSignal->next(NULL);
    theSignalIdleList = tSignalNext;
  } else {
    tSignal = new NdbApiSignal(theMyRef);
#ifdef POORMANSPURIFY
    cnewSignals++;
#endif
    if (tSignal != NULL)
      tSignal->next(NULL);
  }
#ifdef POORMANSPURIFY
  cgetSignals++;
#endif
  return tSignal;
}

NdbBlob*
Ndb::getNdbBlob()
{
  NdbBlob* tBlob = theNdbBlobIdleList;
  if (tBlob != NULL) {
    theNdbBlobIdleList = tBlob->theNext;
    tBlob->init();
  } else {
    tBlob = new NdbBlob;
  }
  return tBlob;
}

/***************************************************************************
void releaseNdbBranch(NdbBranch* aNdbBranch);

Parameters:     NdbBranch: The NdbBranch object.
Remark:         Add a NdbBranch object into the Branch idlelist.
***************************************************************************/
void
Ndb::releaseNdbBranch(NdbBranch* aNdbBranch)
{
  aNdbBranch->theNext = theBranchList;
  theBranchList = aNdbBranch;
}

/***************************************************************************
void releaseNdbCall(NdbCall* aNdbCall);

Parameters:     NdbBranch: The NdbBranch object.
Remark:         Add a NdbBranch object into the Branch idlelist.
***************************************************************************/
void
Ndb::releaseNdbCall(NdbCall* aNdbCall)
{
  aNdbCall->theNext = theCallList;
  theCallList = aNdbCall;
}

/***************************************************************************
void releaseNdbCon(NdbConnection* aNdbCon);

Parameters:     aNdbCon: The NdbConnection object.
Remark:         Add a Connection object into the signal idlelist.
***************************************************************************/
void
Ndb::releaseNdbCon(NdbConnection* aNdbCon)
{
  aNdbCon->next(theConIdleList);
  aNdbCon->theMagicNumber = 0xFE11DD;
  theConIdleList = aNdbCon;
}

/***************************************************************************
void releaseNdbLabel(NdbLabel* aNdbLabel);

Parameters:     NdbLabel: The NdbLabel object.
Remark:         Add a NdbLabel object into the Label idlelist.
***************************************************************************/
void
Ndb::releaseNdbLabel(NdbLabel* aNdbLabel)
{
  aNdbLabel->theNext = theLabelList;
  theLabelList = aNdbLabel;
}

/***************************************************************************
void releaseNdbScanRec(NdbScanReceiver* aNdbScanRec);

Parameters:     aNdbScanRec: The NdbScanReceiver object.
Remark:         Add a NdbScanReceiver object into the Scan idlelist.
***************************************************************************/
void
Ndb::releaseNdbScanRec(NdbReceiver* aNdbScanRec)
{
  aNdbScanRec->next(theScanList);
  theScanList = aNdbScanRec;
}

/***************************************************************************
void releaseNdbSubroutine(NdbSubroutine* aNdbSubroutine);

Parameters:     NdbSubroutine: The NdbSubroutine object.
Remark:         Add a NdbSubroutine object into theSubroutine idlelist.
***************************************************************************/
void
Ndb::releaseNdbSubroutine(NdbSubroutine* aNdbSubroutine)
{
  aNdbSubroutine->theNext = theSubroutineList;
  theSubroutineList = aNdbSubroutine;
}

/***************************************************************************
void releaseOperation(NdbOperation* anOperation);

Parameters:     anOperation : The released NdbOperation object.
Remark:         Add a NdbOperation object into the signal idlelist.
***************************************************************************/
void
Ndb::releaseOperation(NdbOperation* anOperation)
{
  if(anOperation->m_tcReqGSN == GSN_TCKEYREQ){
    anOperation->next(theOpIdleList);
    anOperation->theNdbCon = NULL;
    anOperation->theMagicNumber = 0xFE11D0;
    theOpIdleList = anOperation;
  } else {
    assert(anOperation->m_tcReqGSN == GSN_TCINDXREQ);
    anOperation->next(theIndexOpIdleList);
    anOperation->theNdbCon = NULL;
    anOperation->theMagicNumber = 0xFE11D1;
    theIndexOpIdleList = (NdbIndexOperation*)anOperation;
  }
}

/***************************************************************************
void releaseScanOperation(NdbScanOperation* aScanOperation);

Parameters:     aScanOperation : The released NdbScanOperation object.
Remark:         Add a NdbScanOperation object into the signal idlelist.
***************************************************************************/
void
Ndb::releaseScanOperation(NdbIndexScanOperation* aScanOperation)
{
  aScanOperation->next(theScanOpIdleList);
  aScanOperation->theNdbCon = NULL;
  aScanOperation->theMagicNumber = 0xFE11D2;
  theScanOpIdleList = aScanOperation;
}

/***************************************************************************
void releaseRecAttr(NdbRecAttr* aRecAttr);

Parameters:     aRecAttr : The released NdbRecAttr object.
Remark:         Add a NdbRecAttr object into the RecAtt idlelist.
***************************************************************************/
void
Ndb::releaseRecAttr(NdbRecAttr* aRecAttr)
{
  aRecAttr->release();
  aRecAttr->next(theRecAttrIdleList);
  theRecAttrIdleList = aRecAttr;
}

/***************************************************************************
void releaseSignal(NdbApiSignal* aSignal);

Parameters:     aSignal : The released NdbApiSignal object.
Remark:         Add a NdbApiSignal object into the signal idlelist.
***************************************************************************/
void
Ndb::releaseSignal(NdbApiSignal* aSignal)
{
#if defined VM_TRACE
  // Check that signal is not null
  assert(aSignal != NULL);
#if 0
  // Check that signal is not already in list
  NdbApiSignal* tmp = theSignalIdleList;
  while (tmp != NULL){
    assert(tmp != aSignal);
    tmp = tmp->next();
  }
#endif
#endif
#ifdef POORMANSPURIFY
  creleaseSignals++;
#endif
  aSignal->next(theSignalIdleList);
  theSignalIdleList = aSignal;
}

void
Ndb::releaseSignalsInList(NdbApiSignal** pList){
  NdbApiSignal* tmp;
  while (*pList != NULL){
    tmp = *pList;
    *pList = (*pList)->next();
    releaseSignal(tmp);
  }
}

void
Ndb::releaseNdbBlob(NdbBlob* aBlob)
{
  aBlob->release();
  aBlob->theNext = theNdbBlobIdleList;
  theNdbBlobIdleList = aBlob;
}

/***************************************************************************
void freeOperation();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeOperation()
{
  NdbOperation* tOp = theOpIdleList;
  theOpIdleList = theOpIdleList->next();
  delete tOp;
}

/***************************************************************************
void freeScanOperation();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeScanOperation()
{
  NdbIndexScanOperation* tOp = theScanOpIdleList;
  theScanOpIdleList = (NdbIndexScanOperation *)tOp->next();
  delete tOp;
}

/***************************************************************************
void freeIndexOperation();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeIndexOperation()
{
  NdbIndexOperation* tOp = theIndexOpIdleList;
  theIndexOpIdleList = (NdbIndexOperation *) theIndexOpIdleList->next();
  delete tOp;
}

/***************************************************************************
void freeNdbBranch();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeNdbBranch()
{
  NdbBranch* tNdbBranch = theBranchList;
  theBranchList = theBranchList->theNext;
  delete tNdbBranch;
}

/***************************************************************************
void freeNdbCall();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeNdbCall()
{
  NdbCall* tNdbCall = theCallList;
  theCallList = theCallList->theNext;
  delete tNdbCall;
}

/***************************************************************************
void freeNdbScanRec();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeNdbScanRec()
{
  NdbReceiver* tNdbScanRec = theScanList;
  theScanList = theScanList->next();
  delete tNdbScanRec;
}

/***************************************************************************
void freeNdbCon();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeNdbCon()
{
  NdbConnection* tNdbCon = theConIdleList;
  theConIdleList = theConIdleList->next();
  delete tNdbCon;
}

/***************************************************************************
void freeNdbLabel();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeNdbLabel()
{
  NdbLabel* tNdbLabel = theLabelList;
  theLabelList = theLabelList->theNext;
  delete tNdbLabel;
}

/***************************************************************************
void freeNdbSubroutine();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeNdbSubroutine()
{
  NdbSubroutine* tNdbSubroutine = theSubroutineList;
  theSubroutineList = theSubroutineList->theNext;
  delete tNdbSubroutine;
}

/***************************************************************************
void freeRecAttr();

Remark:         Always release the first item in the free list
***************************************************************************/
void
Ndb::freeRecAttr()
{
  NdbRecAttr* tRecAttr = theRecAttrIdleList;
  theRecAttrIdleList = theRecAttrIdleList->next();
  delete tRecAttr;
}

/***************************************************************************
void freeSignal();

Remark:         Delete  a signal object from the signal idlelist.
***************************************************************************/
void
Ndb::freeSignal()
{
  NdbApiSignal* tSignal = theSignalIdleList;
  theSignalIdleList = tSignal->next();
  delete tSignal;
#ifdef POORMANSPURIFY
  cfreeSignals++;
#endif
}

void
Ndb::freeNdbBlob()
{
  NdbBlob* tBlob = theNdbBlobIdleList;
  theNdbBlobIdleList = tBlob->theNext;
  delete tBlob;
}

/****************************************************************************
int releaseConnectToNdb(NdbConnection* aConnectConnection);

Return Value:   -1 if error 
Parameters:     aConnectConnection : Seized schema connection to DBTC
Remark:         Release and disconnect from DBTC a connection and seize it to theConIdleList.
*****************************************************************************/
void
Ndb::releaseConnectToNdb(NdbConnection* a_con)     
{
  DBUG_ENTER("Ndb::releaseConnectToNdb");
  NdbApiSignal          tSignal(theMyRef);
  int                   tConPtr;

// I need to close the connection irrespective of whether I
// manage to reach NDB or not.

  if (a_con == NULL)
    DBUG_VOID_RETURN;

  Uint32 node_id = a_con->getConnectedNodeId();
  Uint32 conn_seq = a_con->theNodeSequence;
  tSignal.setSignal(GSN_TCRELEASEREQ);
  tSignal.setData((tConPtr = a_con->getTC_ConnectPtr()), 1);
  tSignal.setData(theMyRef, 2);
  tSignal.setData(a_con->ptr2int(), 3); 
  a_con->Status(NdbConnection::DisConnecting);
  a_con->theMagicNumber = 0x37412619;
  int ret_code = sendRecSignal(node_id,
                               WAIT_TC_RELEASE,
                               &tSignal,
                               conn_seq);
  if (ret_code == 0) {
    ;
  } else if (ret_code == -1) {
    TRACE_DEBUG("Time-out when TCRELEASE sent");
  } else if (ret_code == -2) {
    TRACE_DEBUG("Node failed when TCRELEASE sent");
  } else if (ret_code == -3) {
    TRACE_DEBUG("Send failed when TCRELEASE sent");
  } else if (ret_code == -4) {
    TRACE_DEBUG("Send buffer full when TCRELEASE sent");
  } else if (ret_code == -5) {
    TRACE_DEBUG("Node stopping when TCRELEASE sent");
  } else {
    ndbout << "Impossible return from sendRecSignal when TCRELEASE" << endl;
    abort();
  }//if
  releaseNdbCon(a_con);
  DBUG_VOID_RETURN;
}

