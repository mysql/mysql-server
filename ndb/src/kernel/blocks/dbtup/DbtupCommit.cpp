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

#define DBTUP_C
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/TupCommit.hpp>

#define ljam() { jamLine(5000 + __LINE__); }
#define ljamEntry() { jamEntryLine(5000 + __LINE__); }

void Dbtup::execTUP_WRITELOG_REQ(Signal* signal)
{
  jamEntry();
  OperationrecPtr loopOpPtr;
  loopOpPtr.i = signal->theData[0];
  Uint32 gci = signal->theData[1];
  ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
  while (loopOpPtr.p->nextActiveOp != RNIL) {
    ljam();
    loopOpPtr.i = loopOpPtr.p->nextActiveOp;
    ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
  }//while
  do {
    Uint32 blockNo = refToBlock(loopOpPtr.p->userblockref);
    ndbrequire(loopOpPtr.p->transstate == STARTED);
    signal->theData[0] = loopOpPtr.p->userpointer;
    signal->theData[1] = gci;
    if (loopOpPtr.p->prevActiveOp == RNIL) {
      ljam();
      EXECUTE_DIRECT(blockNo, GSN_LQH_WRITELOG_REQ, signal, 2);
      return;
    }//if
    ljam();
    EXECUTE_DIRECT(blockNo, GSN_LQH_WRITELOG_REQ, signal, 2);
    jamEntry();
    loopOpPtr.i = loopOpPtr.p->prevActiveOp;
    ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
  } while (true);
}//Dbtup::execTUP_WRITELOG_REQ()

void Dbtup::execTUP_DEALLOCREQ(Signal* signal)
{
  TablerecPtr regTabPtr;
  FragrecordPtr regFragPtr;

  jamEntry();

  Uint32 fragId = signal->theData[0];
  regTabPtr.i = signal->theData[1];
  Uint32 fragPageId = signal->theData[2];
  Uint32 pageIndex = signal->theData[3];

  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  ndbrequire(regFragPtr.p != NULL);

  PagePtr pagePtr;
  pagePtr.i = getRealpid(regFragPtr.p, fragPageId);
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 pageIndexScaled = pageIndex >> 1;
  ndbrequire((pageIndex & 1) == 0);
  Uint32 pageOffset = ZPAGE_HEADER_SIZE + 
                     (regTabPtr.p->tupheadsize * pageIndexScaled);
//---------------------------------------------------
/* --- Deallocate a tuple as requested by ACC  --- */
//---------------------------------------------------
  if (isUndoLoggingNeeded(regFragPtr.p, fragPageId)) {
    ljam();
    cprAddUndoLogRecord(signal,
                        ZLCPR_TYPE_INSERT_TH,
                        fragPageId,
                        pageIndex,
                        regTabPtr.i,
                        fragId,
                        regFragPtr.p->checkpointVersion);
    cprAddData(signal,
               regFragPtr.p,
               pagePtr.i,
               regTabPtr.p->tupheadsize,
               pageOffset);
  }//if
  {
    freeTh(regFragPtr.p,
           regTabPtr.p,
           signal,
           pagePtr.p,
           pageOffset);
  }
}

/* ---------------------------------------------------------------- */
/* ------------ PERFORM A COMMIT ON AN UPDATE OPERATION  ---------- */
/* ---------------------------------------------------------------- */
void Dbtup::commitUpdate(Signal* signal,
                         Operationrec*  const regOperPtr,
                         Fragrecord* const regFragPtr,
                         Tablerec* const regTabPtr)
{
  if (regOperPtr->realPageIdC != RNIL) {
    if (isUndoLoggingNeeded(regFragPtr, regOperPtr->fragPageIdC)) {
/* ------------------------------------------------------------------------ */
/* IF THE COPY WAS CREATED WITHIN THIS CHECKPOINT WE ONLY HAVE              */
/* TO LOG THE CREATION OF THE COPY. IF HOWEVER IT WAS CREATED BEFORE  SAVE  */
/* THIS CHECKPOINT, WE HAVE TO THE DATA AS WELL.                            */
/* ------------------------------------------------------------------------ */
      if (regOperPtr->undoLogged) {
        ljam();
        cprAddUndoLogRecord(signal,
                            ZLCPR_TYPE_INSERT_TH_NO_DATA,
                            regOperPtr->fragPageIdC,
                            regOperPtr->pageIndexC,
                            regOperPtr->tableRef,
                            regOperPtr->fragId,
                            regFragPtr->checkpointVersion);
      } else {
        ljam();
        cprAddUndoLogRecord(signal,
                            ZLCPR_TYPE_INSERT_TH,
                            regOperPtr->fragPageIdC,
                            regOperPtr->pageIndexC,
                            regOperPtr->tableRef,
                            regOperPtr->fragId,
                            regFragPtr->checkpointVersion);
        cprAddData(signal,
                   regFragPtr,
                   regOperPtr->realPageIdC,
                   regTabPtr->tupheadsize,
                   regOperPtr->pageOffsetC);
      }//if
    }//if

    PagePtr copyPagePtr;
    copyPagePtr.i = regOperPtr->realPageIdC;
    ptrCheckGuard(copyPagePtr, cnoOfPage, page);
    freeTh(regFragPtr,
           regTabPtr,
           signal,
           copyPagePtr.p,
           (Uint32)regOperPtr->pageOffsetC);
    regOperPtr->realPageIdC = RNIL;
    regOperPtr->fragPageIdC = RNIL;
    regOperPtr->pageOffsetC = ZNIL;
    regOperPtr->pageIndexC = ZNIL;
  }//if
}//Dbtup::commitUpdate()

void
Dbtup::commitSimple(Signal* signal,
                    Operationrec* const regOperPtr,
                    Fragrecord* const regFragPtr,
                    Tablerec* const regTabPtr)
{
  operPtr.p = regOperPtr;
  fragptr.p = regFragPtr;
  tabptr.p = regTabPtr;

  // Checking detached triggers
  checkDetachedTriggers(signal,
                        regOperPtr,
                        regTabPtr);

  removeActiveOpList(regOperPtr);
  if (regOperPtr->optype == ZUPDATE) {
    ljam();
    commitUpdate(signal, regOperPtr, regFragPtr, regTabPtr);
    if (regTabPtr->GCPIndicator) {
      updateGcpId(signal, regOperPtr, regFragPtr, regTabPtr);
    }//if
  } else if (regOperPtr->optype == ZINSERT) {
    ljam();
    if (regTabPtr->GCPIndicator) {
      updateGcpId(signal, regOperPtr, regFragPtr, regTabPtr);
    }//if
  } else {
    ndbrequire(regOperPtr->optype == ZDELETE);
  }//if
}//Dbtup::commitSimple()

void Dbtup::removeActiveOpList(Operationrec*  const regOperPtr)
{
  if (regOperPtr->inActiveOpList == ZTRUE) {
    OperationrecPtr raoOperPtr;
    regOperPtr->inActiveOpList = ZFALSE;
    if (regOperPtr->prevActiveOp != RNIL) {
      ljam();
      raoOperPtr.i = regOperPtr->prevActiveOp;
      ptrCheckGuard(raoOperPtr, cnoOfOprec, operationrec);
      raoOperPtr.p->nextActiveOp = regOperPtr->nextActiveOp;
    } else {
      ljam();
      PagePtr pagePtr;
      pagePtr.i = regOperPtr->realPageId;
      ptrCheckGuard(pagePtr, cnoOfPage, page);
      ndbrequire(regOperPtr->pageOffset < ZWORDS_ON_PAGE);
      pagePtr.p->pageWord[regOperPtr->pageOffset] = regOperPtr->nextActiveOp;
    }//if
    if (regOperPtr->nextActiveOp != RNIL) {
      ljam();
      raoOperPtr.i = regOperPtr->nextActiveOp;
      ptrCheckGuard(raoOperPtr, cnoOfOprec, operationrec);
      raoOperPtr.p->prevActiveOp = regOperPtr->prevActiveOp;
    }//if
    regOperPtr->prevActiveOp = RNIL;
    regOperPtr->nextActiveOp = RNIL;
  }//if
}//Dbtup::removeActiveOpList()

/* ---------------------------------------------------------------- */
/* INITIALIZATION OF ONE CONNECTION RECORD TO PREPARE FOR NEXT OP.  */
/* ---------------------------------------------------------------- */
void Dbtup::initOpConnection(Operationrec* regOperPtr,
			     Fragrecord * fragPtrP)
{
  Uint32 RinFragList = regOperPtr->inFragList;
  regOperPtr->transstate = IDLE;
  regOperPtr->currentAttrinbufLen = 0;
  regOperPtr->optype = ZREAD;
  if (RinFragList == ZTRUE) {
    OperationrecPtr tropNextLinkPtr;
    OperationrecPtr tropPrevLinkPtr;
/*----------------------------------------------------------------- */
/*       TO ENSURE THAT WE HAVE SUCCESSFUL ABORTS OF FOLLOWING      */
/*       OPERATIONS WHICH NEVER STARTED WE SET THE OPTYPE TO READ.  */
/*----------------------------------------------------------------- */
/*       REMOVE IT FROM THE DOUBLY LINKED LIST ON THE FRAGMENT      */
/*----------------------------------------------------------------- */
    tropPrevLinkPtr.i = regOperPtr->prevOprecInList;
    tropNextLinkPtr.i = regOperPtr->nextOprecInList;
    regOperPtr->inFragList = ZFALSE;
    if (tropPrevLinkPtr.i == RNIL) {
      ljam();
      fragPtrP->firstusedOprec = tropNextLinkPtr.i;
    } else {
      ljam();
      ptrCheckGuard(tropPrevLinkPtr, cnoOfOprec, operationrec);
      tropPrevLinkPtr.p->nextOprecInList = tropNextLinkPtr.i;
    }//if
    if (tropNextLinkPtr.i == RNIL) {
      fragPtrP->lastusedOprec = tropPrevLinkPtr.i;
    } else {
      ptrCheckGuard(tropNextLinkPtr, cnoOfOprec, operationrec);
      tropNextLinkPtr.p->prevOprecInList = tropPrevLinkPtr.i;
    }
    regOperPtr->prevOprecInList = RNIL;
    regOperPtr->nextOprecInList = RNIL;
  }//if
}//Dbtup::initOpConnection()

/* ----------------------------------------------------------------- */
/* --------------- COMMIT THIS PART OF A TRANSACTION --------------- */
/* ----------------------------------------------------------------- */
void Dbtup::execTUP_COMMITREQ(Signal* signal) 
{
  FragrecordPtr regFragPtr;
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;

  TupCommitReq * const tupCommitReq = (TupCommitReq *)signal->getDataPtr();

  ljamEntry();
  regOperPtr.i = tupCommitReq->opPtr;
  ptrCheckGuard(regOperPtr, cnoOfOprec, operationrec);

  ndbrequire(regOperPtr.p->transstate == STARTED);
  regOperPtr.p->gci = tupCommitReq->gci;
  regOperPtr.p->hashValue = tupCommitReq->hashValue;

  regFragPtr.i = regOperPtr.p->fragmentPtr;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);

  regTabPtr.i = regOperPtr.p->tableRef;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  if (!regTabPtr.p->tuxCustomTriggers.isEmpty()) {
    ljam();
    executeTuxCommitTriggers(signal,
                             regOperPtr.p,
                             regTabPtr.p);
  }

  if (regOperPtr.p->tupleState == NO_OTHER_OP) {
    if ((regOperPtr.p->prevActiveOp == RNIL) &&
        (regOperPtr.p->nextActiveOp == RNIL)) {
      ljam();
/* ---------------------------------------------------------- */
// We handle the simple case separately as an optimisation
/* ---------------------------------------------------------- */
      commitSimple(signal,
                   regOperPtr.p,
                   regFragPtr.p,
                   regTabPtr.p);
    } else {
/* ---------------------------------------------------------- */
// This is the first commit message of this record in this
// transaction. We will commit this record completely for this
// transaction. If there are other operations they will be
// responsible to release their own resources. Also commit of
// a delete is postponed until the last operation is committed
// on the tuple.
//
// As part of this commitRecord we will also handle detached
// triggers and release of resources for this operation.
/* ---------------------------------------------------------- */
      ljam();
      commitRecord(signal,
                   regOperPtr.p,
                   regFragPtr.p,
                   regTabPtr.p);
      removeActiveOpList(regOperPtr.p);
    }//if
  } else {
    ljam();
/* ---------------------------------------------------------- */
// Release any copy tuples
/* ---------------------------------------------------------- */
    ndbrequire(regOperPtr.p->tupleState == TO_BE_COMMITTED);
    commitUpdate(signal, regOperPtr.p, regFragPtr.p, regTabPtr.p);
    removeActiveOpList(regOperPtr.p);
  }//if
  initOpConnection(regOperPtr.p, regFragPtr.p);
}//execTUP_COMMITREQ()

void
Dbtup::updateGcpId(Signal* signal,
                   Operationrec* const regOperPtr,
                   Fragrecord* const regFragPtr,
                   Tablerec* const regTabPtr)
{
  PagePtr pagePtr;
  ljam();
//--------------------------------------------------------------------
// Is this code safe for UNDO logging. Not sure currently. RONM
//--------------------------------------------------------------------
  pagePtr.i = regOperPtr->realPageId;
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 temp = regOperPtr->pageOffset + regTabPtr->tupGCPIndex;
  ndbrequire((temp < ZWORDS_ON_PAGE) &&
             (regTabPtr->tupGCPIndex < regTabPtr->tupheadsize));
  if (isUndoLoggingNeeded(regFragPtr, regOperPtr->fragPageId)) {
    Uint32 prevGCI = pagePtr.p->pageWord[temp];
    ljam();
    cprAddUndoLogRecord(signal,
                        ZLCPR_TYPE_UPDATE_GCI,
                        regOperPtr->fragPageId,
                        regOperPtr->pageIndex,
                        regOperPtr->tableRef,
                        regOperPtr->fragId,
                        regFragPtr->checkpointVersion);
    cprAddGCIUpdate(signal,
                    prevGCI,
                    regFragPtr);
  }//if
  pagePtr.p->pageWord[temp] = regOperPtr->gci;
  if (regTabPtr->checksumIndicator) {
    ljam();
    setChecksum(pagePtr.p, regOperPtr->pageOffset, regTabPtr->tupheadsize);
  }//if
}//Dbtup::updateGcpId()

void
Dbtup::commitRecord(Signal* signal,
                    Operationrec* const regOperPtr,
                    Fragrecord* const regFragPtr,
                    Tablerec* const regTabPtr)
{
  Uint32 opType;
  OperationrecPtr firstOpPtr;
  PagePtr pagePtr;

  pagePtr.i = regOperPtr->realPageId;
  ptrCheckGuard(pagePtr, cnoOfPage, page);

  setTupleStatesSetOpType(regOperPtr, pagePtr.p, opType, firstOpPtr);

  fragptr.p = regFragPtr;
  tabptr.p = regTabPtr;

  if (opType == ZINSERT_DELETE) {
    ljam();
//--------------------------------------------------------------------
// We started by inserting the tuple and ended by deleting. Seen from
// transactions point of view no changes were made.
//--------------------------------------------------------------------
    commitUpdate(signal, regOperPtr, regFragPtr, regTabPtr);
    return;
  } else if (opType == ZINSERT) {
    ljam();
//--------------------------------------------------------------------
// We started by inserting whereafter we made several changes to the
// tuple that could include updates, deletes and new inserts. The final
// state of the tuple is the original tuple. This is reached from this
// operation. We change the optype on this operation to ZINSERT to
// ensure proper operation of the detached trigger.
// We restore the optype after executing triggers although not really
// needed.
//--------------------------------------------------------------------
    Uint32 saveOpType = regOperPtr->optype;
    regOperPtr->optype = ZINSERT;
    operPtr.p = regOperPtr;

    checkDetachedTriggers(signal,
                          regOperPtr,
                          regTabPtr);

    regOperPtr->optype = saveOpType;
  } else if (opType == ZUPDATE) {
    ljam();
//--------------------------------------------------------------------
// We want to use the first operation which contains a copy tuple
// reference. This operation contains the before value of this record
// for this transaction. Then this operation is used for executing
// triggers with optype set to update.
//--------------------------------------------------------------------
    OperationrecPtr befOpPtr;
    findBeforeValueOperation(befOpPtr, firstOpPtr);

    Uint32 saveOpType = befOpPtr.p->optype;
    Bitmask<MAXNROFATTRIBUTESINWORDS> attributeMask;
    Bitmask<MAXNROFATTRIBUTESINWORDS> saveAttributeMask;

    calculateChangeMask(pagePtr.p,
                        regTabPtr,
                        befOpPtr.p->pageOffset,
                        attributeMask);

    saveAttributeMask.clear();
    saveAttributeMask.bitOR(befOpPtr.p->changeMask);
    befOpPtr.p->changeMask.clear();
    befOpPtr.p->changeMask.bitOR(attributeMask);
    
    operPtr.p = befOpPtr.p;
    checkDetachedTriggers(signal,
                          befOpPtr.p,
                          regTabPtr);

    befOpPtr.p->changeMask.clear();
    befOpPtr.p->changeMask.bitOR(saveAttributeMask);

    befOpPtr.p->optype = saveOpType;
  } else if (opType == ZDELETE) {
    ljam();
//--------------------------------------------------------------------
// We want to use the first operation which contains a copy tuple.
// We benefit from the fact that we know that it cannot be a simple
// delete and it cannot be an insert followed by a delete. Thus there
// must either be an update or a insert following a delete. In both
// cases we will find a before value in a copy tuple.
//
// An added complexity is that the trigger handling assumes that the
// before value is located in the original tuple so we have to move the
// copy tuple reference to the original tuple reference and afterwards
// restore it again.
//--------------------------------------------------------------------
    OperationrecPtr befOpPtr;
    findBeforeValueOperation(befOpPtr, firstOpPtr);
    Uint32 saveOpType = befOpPtr.p->optype;

    Uint32 realPageId = befOpPtr.p->realPageId;
    Uint32 pageOffset = befOpPtr.p->pageOffset;
    Uint32 fragPageId = befOpPtr.p->fragPageId;
    Uint32 pageIndex  = befOpPtr.p->pageIndex;

    befOpPtr.p->realPageId = befOpPtr.p->realPageIdC;
    befOpPtr.p->pageOffset = befOpPtr.p->pageOffsetC;
    befOpPtr.p->fragPageId = befOpPtr.p->fragPageIdC;
    befOpPtr.p->pageIndex  = befOpPtr.p->pageIndexC;

    operPtr.p = befOpPtr.p;
    checkDetachedTriggers(signal,
                          befOpPtr.p,
                          regTabPtr);

    befOpPtr.p->realPageId = realPageId;
    befOpPtr.p->pageOffset = pageOffset;
    befOpPtr.p->fragPageId = fragPageId;
    befOpPtr.p->pageIndex  = pageIndex;
    befOpPtr.p->optype     = saveOpType;
  } else {
    ndbrequire(false);
  }//if

  commitUpdate(signal, regOperPtr, regFragPtr, regTabPtr);
  if (regTabPtr->GCPIndicator) {
    updateGcpId(signal, regOperPtr, regFragPtr, regTabPtr);
  }//if
}//Dbtup::commitRecord()

void
Dbtup::setTupleStatesSetOpType(Operationrec* const regOperPtr,
                               Page* const pagePtr,
                               Uint32& opType,
                               OperationrecPtr& firstOpPtr)
{
  OperationrecPtr loopOpPtr;
  OperationrecPtr lastOpPtr;

  ndbrequire(regOperPtr->pageOffset < ZWORDS_ON_PAGE);
  loopOpPtr.i = pagePtr->pageWord[regOperPtr->pageOffset];
  ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
  lastOpPtr = loopOpPtr;
  if (loopOpPtr.p->optype == ZDELETE) {
    ljam();
    opType = ZDELETE;
  } else {
    ljam();
    opType = ZUPDATE;
  }//if
  do {
    ljam();
    ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
    firstOpPtr = loopOpPtr;
    loopOpPtr.p->tupleState = TO_BE_COMMITTED;
    loopOpPtr.i = loopOpPtr.p->nextActiveOp;
  } while (loopOpPtr.i != RNIL);
  if (opType == ZDELETE) {
    ljam();
    if (firstOpPtr.p->optype == ZINSERT) {
      ljam();
      opType = ZINSERT_DELETE;
    }//if
  } else {
    ljam();
    if (firstOpPtr.p->optype == ZINSERT) {
      ljam();
      opType = ZINSERT;
    }//if
  }///if
}//Dbtup::setTupleStatesSetOpType()

void Dbtup::findBeforeValueOperation(OperationrecPtr& befOpPtr,
                                     OperationrecPtr firstOpPtr)
{
  befOpPtr = firstOpPtr;
  if (befOpPtr.p->realPageIdC != RNIL) {
    ljam();
    return;
  } else {
    ljam();
    befOpPtr.i = befOpPtr.p->prevActiveOp;
    ptrCheckGuard(befOpPtr, cnoOfOprec, operationrec);
    ndbrequire(befOpPtr.p->realPageIdC != RNIL);
  }//if
}//Dbtup::findBeforeValueOperation()

void
Dbtup::calculateChangeMask(Page* const pagePtr,
                           Tablerec* const regTabPtr,
                           Uint32 pageOffset,
                           Bitmask<MAXNROFATTRIBUTESINWORDS>& attributeMask)
{
  OperationrecPtr loopOpPtr;

  attributeMask.clear();
  ndbrequire(pageOffset < ZWORDS_ON_PAGE);
  loopOpPtr.i = pagePtr->pageWord[pageOffset];
  do {
    ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
    if (loopOpPtr.p->optype == ZUPDATE) {
      ljam();
      attributeMask.bitOR(loopOpPtr.p->changeMask);
    } else if (loopOpPtr.p->optype == ZINSERT) {
      ljam();
      attributeMask.set();
      return;
    } else {
      ndbrequire(loopOpPtr.p->optype == ZDELETE);
    }//if
    loopOpPtr.i = loopOpPtr.p->nextActiveOp;
  } while (loopOpPtr.i != RNIL);
}//Dbtup::calculateChangeMask()
