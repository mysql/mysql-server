/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "API.hpp"

void
Ndb::checkFailedNode()
{
  DBUG_ENTER("Ndb::checkFailedNode");
  Uint32 *the_release_ind= theImpl->the_release_ind;
  if (the_release_ind[0] == 0)
  {
    DBUG_VOID_RETURN;
  }
  Uint32 tNoOfDbNodes = theImpl->theNoOfDBnodes;
  Uint8 *theDBnodes= theImpl->theDBnodes;

  DBUG_PRINT("enter", ("theNoOfDBnodes: %d", tNoOfDbNodes));

  DBUG_ASSERT(tNoOfDbNodes < MAX_NDB_NODES);
  for (Uint32 i = 0; i < tNoOfDbNodes; i++){
    const NodeId node_id = theDBnodes[i];
    DBUG_PRINT("info", ("i: %d, node_id: %d", i, node_id));
    
    DBUG_ASSERT(node_id < MAX_NDB_NODES);    
    if (the_release_ind[node_id] == 1){

      /**
       * Release all connections in idle list (for node)
       */
      NdbTransaction * tNdbCon = theConnectionArray[node_id];
      theConnectionArray[node_id] = NULL;
      theConnectionArrayLast[node_id] = NULL;
      while (tNdbCon != NULL) {
        NdbTransaction* tempNdbCon = tNdbCon;
        tNdbCon = tNdbCon->next();
        releaseNdbCon(tempNdbCon);
      }
      the_release_ind[node_id] = 0;
    }
  }
  DBUG_VOID_RETURN;
}

/***************************************************************************
 * int createConIdleList(int aNrOfCon);
 * 
 * Return Value:   Return the number of created connection object 
 *                 if createConIdleList was succesful
 *                 Return -1: In all other case.  
 * Parameters:     aNrOfCon : Number of connections offered to the application.
 * Remark:         Create connection idlelist with NdbTransaction objects.
 ***************************************************************************/ 
int 
Ndb::createConIdleList(int aNrOfCon)
{
  if (theImpl->theConIdleList.fill(this, aNrOfCon))
  {
    return -1;
  }
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
  if (theImpl->theOpIdleList.fill(this, aNrOfOp))
  {
    return -1;
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
  return theImpl->theBranchList.seize(this);
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
  return theImpl->theCallList.seize(this);
}

/***************************************************************************
 * NdbTransaction* getNdbCon();
 *
 * Return Value:   Return a connection if the  getNdbCon was successful.
 *                Return NULL : In all other case.  
 * Remark:         Get a connection from theConIdleList and return the object .
 ***************************************************************************/ 
NdbTransaction*
Ndb::getNdbCon()
{
  NdbTransaction* tNdbCon = theImpl->theConIdleList.seize(this);
  tNdbCon->theMagicNumber = tNdbCon->getMagicNumber();
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
  return theImpl->theLabelList.seize(this);
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
  return theImpl->theScanList.seize(this);
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
  return theImpl->theSubroutineList.seize(this);
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
  return theImpl->theOpIdleList.seize(this);
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
  return theImpl->theScanOpIdleList.seize(this);
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
  return theImpl->theIndexOpIdleList.seize(this);
}

/***************************************************************************
NdbRecAttr* getRecAttr();

Return Value:   Return a reference to a receive attribute object.
                Return NULL if it's not possible to get a receive attribute object.
***************************************************************************/
NdbRecAttr*
Ndb::getRecAttr()
{
  NdbRecAttr* tRecAttr = theImpl->theRecAttrIdleList.seize(this);
  if (tRecAttr != NULL) 
  {
    tRecAttr->init();
    return tRecAttr;
  }

  return NULL;
}

/***************************************************************************
NdbApiSignal* getSignal();

Return Value:   Return a reference to a signal object.
                Return NULL if not possible to get a signal object.
***************************************************************************/
NdbApiSignal*
Ndb::getSignal()
{
  return theImpl->theSignalIdleList.seize(this);
}

NdbBlob*
Ndb::getNdbBlob()
{
  NdbBlob* tBlob = theImpl->theNdbBlobIdleList.seize(this);
  if(tBlob)
  {
    tBlob->init();
  }
  return tBlob;
}

NdbLockHandle*
Ndb::getLockHandle()
{
  NdbLockHandle* lh = theImpl->theLockHandleList.seize(this);
  if (lh)
  {
    lh->init();
  }

  return lh;
} 

/***************************************************************************
void releaseNdbBranch(NdbBranch* aNdbBranch);

Parameters:     NdbBranch: The NdbBranch object.
Remark:         Add a NdbBranch object into the Branch idlelist.
***************************************************************************/
void
Ndb::releaseNdbBranch(NdbBranch* aNdbBranch)
{
  theImpl->theBranchList.release(aNdbBranch);
}

/***************************************************************************
void releaseNdbCall(NdbCall* aNdbCall);

Parameters:     NdbBranch: The NdbBranch object.
Remark:         Add a NdbBranch object into the Branch idlelist.
***************************************************************************/
void
Ndb::releaseNdbCall(NdbCall* aNdbCall)
{
  theImpl->theCallList.release(aNdbCall);
}

/***************************************************************************
void releaseNdbCon(NdbTransaction* aNdbCon);

Parameters:     aNdbCon: The NdbTransaction object.
Remark:         Add a Connection object into the connection idlelist.
***************************************************************************/
void
Ndb::releaseNdbCon(NdbTransaction* aNdbCon)
{
  aNdbCon->theMagicNumber = 0xFE11DD;
  theImpl->theConIdleList.release(aNdbCon);
}

/***************************************************************************
void releaseNdbLabel(NdbLabel* aNdbLabel);

Parameters:     NdbLabel: The NdbLabel object.
Remark:         Add a NdbLabel object into the Label idlelist.
***************************************************************************/
void
Ndb::releaseNdbLabel(NdbLabel* aNdbLabel)
{
  theImpl->theLabelList.release(aNdbLabel);
}

/***************************************************************************
void releaseNdbScanRec(NdbScanReceiver* aNdbScanRec);

Parameters:     aNdbScanRec: The NdbScanReceiver object.
Remark:         Add a NdbScanReceiver object into the Scan idlelist.
***************************************************************************/
void
Ndb::releaseNdbScanRec(NdbReceiver* aNdbScanRec)
{
  theImpl->theScanList.release(aNdbScanRec);
}

/***************************************************************************
void releaseNdbSubroutine(NdbSubroutine* aNdbSubroutine);

Parameters:     NdbSubroutine: The NdbSubroutine object.
Remark:         Add a NdbSubroutine object into theSubroutine idlelist.
***************************************************************************/
void
Ndb::releaseNdbSubroutine(NdbSubroutine* aNdbSubroutine)
{
  theImpl->theSubroutineList.release(aNdbSubroutine);
}

/***************************************************************************
void releaseOperation(NdbOperation* anOperation);

Parameters:     anOperation : The released NdbOperation object.
Remark:         Add a NdbOperation object into the operation idlelist.
***************************************************************************/
void
Ndb::releaseOperation(NdbOperation* anOperation)
{
  if(anOperation->m_tcReqGSN == GSN_TCKEYREQ){
    anOperation->theNdbCon = NULL;
    anOperation->theMagicNumber = 0xFE11D0;
    theImpl->theOpIdleList.release(anOperation);
  } else {
    assert(anOperation->m_tcReqGSN == GSN_TCINDXREQ);
    anOperation->theNdbCon = NULL;
    anOperation->theMagicNumber = 0xFE11D1;
    theImpl->theIndexOpIdleList.release((NdbIndexOperation*)anOperation);
  }
}

/***************************************************************************
void releaseScanOperation(NdbScanOperation* aScanOperation);

Parameters:     aScanOperation : The released NdbScanOperation object.
Remark:         Add a NdbScanOperation object into the scan idlelist.
***************************************************************************/
void
Ndb::releaseScanOperation(NdbIndexScanOperation* aScanOperation)
{
  DBUG_ENTER("Ndb::releaseScanOperation");
  DBUG_PRINT("enter", ("op: 0x%lx", (long) aScanOperation));
#ifdef ndb_release_check_dup
  { NdbIndexScanOperation* tOp = theScanOpIdleList;
    while (tOp != NULL) {
      assert(tOp != aScanOperation);
      tOp = (NdbIndexScanOperation*)tOp->theNext;
    }
  }
#endif
  aScanOperation->theNdbCon = NULL;
  aScanOperation->theMagicNumber = 0xFE11D2;
  theImpl->theScanOpIdleList.release(aScanOperation);
  DBUG_VOID_RETURN;
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
  theImpl->theRecAttrIdleList.release(aRecAttr);
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
  theImpl->theSignalIdleList.release(aSignal);
}

void
Ndb::releaseSignals(Uint32 cnt, NdbApiSignal* head, NdbApiSignal* tail)
{
#ifdef POORMANSPURIFY
  creleaseSignals += cnt;
#endif
  theImpl->theSignalIdleList.release(cnt, head, tail);
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
  theImpl->theNdbBlobIdleList.release(aBlob);
}

void
Ndb::releaseLockHandle(NdbLockHandle* lh)
{
  lh->release(this);
  theImpl->theLockHandleList.release(lh);
}

/****************************************************************************
int releaseConnectToNdb(NdbTransaction* aConnectConnection);

Return Value:   -1 if error 
Parameters:     aConnectConnection : Seized schema connection to DBTC
Remark:         Release and disconnect from DBTC a connection and seize it to theConIdleList.
*****************************************************************************/
void
Ndb::releaseConnectToNdb(NdbTransaction* a_con)     
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
  tSignal.setSignal(GSN_TCRELEASEREQ, refToBlock(a_con->m_tcRef));
  tSignal.setData((tConPtr = a_con->getTC_ConnectPtr()), 1);
  tSignal.setData(theMyRef, 2);
  tSignal.setData(a_con->ptr2int(), 3); 
  a_con->Status(NdbTransaction::DisConnecting);
  a_con->theMagicNumber = a_con->getMagicNumber();
  int ret_code = sendRecSignal(node_id,
                               WAIT_TC_RELEASE,
                               &tSignal,
                               conn_seq);
  if (likely(ret_code == 0)) {
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

template<class T>
static
Ndb::Free_list_usage*
update(Ndb::Free_list_usage* curr, 
       Ndb_free_list_t<T> & list, 
       const char * name)
{
  curr->m_name = name;
  curr->m_created = list.m_used_cnt+list.m_free_cnt;
  curr->m_free = list.m_free_cnt;
  curr->m_sizeof = sizeof(T);
  return curr;
}

Ndb::Free_list_usage*
Ndb::get_free_list_usage(Ndb::Free_list_usage* curr)
{
  if (curr == 0)
  {
    return 0;
  } 

  if(curr->m_name == 0)
  {
    update(curr, theImpl->theConIdleList, "NdbTransaction");
  }
  else if(!strcmp(curr->m_name, "NdbTransaction"))
  {
    update(curr, theImpl->theOpIdleList, "NdbOperation");
  }
  else if(!strcmp(curr->m_name, "NdbOperation"))
  {
    update(curr, theImpl->theScanOpIdleList, "NdbIndexScanOperation");
  }
  else if(!strcmp(curr->m_name, "NdbIndexScanOperation"))
  {
    update(curr, theImpl->theIndexOpIdleList, "NdbIndexOperation");
  }
  else if(!strcmp(curr->m_name, "NdbIndexOperation"))
  {
    update(curr, theImpl->theRecAttrIdleList, "NdbRecAttr");
  }
  else if(!strcmp(curr->m_name, "NdbRecAttr"))
  {
    update(curr, theImpl->theSignalIdleList, "NdbApiSignal");
  }
  else if(!strcmp(curr->m_name, "NdbApiSignal"))
  {
    update(curr, theImpl->theLabelList, "NdbLabel");
  }
  else if(!strcmp(curr->m_name, "NdbLabel"))
  {
    update(curr, theImpl->theBranchList, "NdbBranch");
  }
  else if(!strcmp(curr->m_name, "NdbBranch"))
  {
    update(curr, theImpl->theSubroutineList, "NdbSubroutine");
  }
  else if(!strcmp(curr->m_name, "NdbSubroutine"))
  {
    update(curr, theImpl->theCallList, "NdbCall");
  }
  else if(!strcmp(curr->m_name, "NdbCall"))
  {
    update(curr, theImpl->theNdbBlobIdleList, "NdbBlob");
  }
  else if(!strcmp(curr->m_name, "NdbBlob"))
  {
    update(curr, theImpl->theScanList, "NdbReceiver");
  }
  else if(!strcmp(curr->m_name, "NdbReceiver"))
  {
    update(curr, theImpl->theLockHandleList, "NdbLockHandle");
  }
  else if(!strcmp(curr->m_name, "NdbLockHandle"))
  {
    return 0;
  }
  else
  {
    update(curr, theImpl->theConIdleList, "NdbTransaction");
  }

  return curr;
}

#define TI(T) \
 template Ndb::Free_list_usage* \
 update(Ndb::Free_list_usage*, Ndb_free_list_t<T> &, const char * name);\
 template struct Ndb_free_list_t<T>

TI(NdbBlob);
TI(NdbCall);
TI(NdbLabel);
TI(NdbBranch);
TI(NdbSubroutine);
TI(NdbApiSignal);
TI(NdbRecAttr);
TI(NdbOperation);
TI(NdbReceiver);
TI(NdbConnection);
TI(NdbIndexOperation);
TI(NdbIndexScanOperation);
TI(NdbLockHandle);
