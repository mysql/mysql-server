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
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>
#include <Interpreter.hpp>
#include <signaldata/TupCommit.hpp>
#include <signaldata/TupKey.hpp>
#include <NdbSqlUtil.hpp>

/* ----------------------------------------------------------------- */
/* -----------       INIT_STORED_OPERATIONREC         -------------- */
/* ----------------------------------------------------------------- */
int Dbtup::initStoredOperationrec(Operationrec* const regOperPtr,
                                  Uint32 storedId) 
{
  jam();
  StoredProcPtr storedPtr;
  c_storedProcPool.getPtr(storedPtr, storedId);
  if (storedPtr.i != RNIL) {
    if (storedPtr.p->storedCode == ZSCAN_PROCEDURE) {
      storedPtr.p->storedCounter++;
      regOperPtr->firstAttrinbufrec = storedPtr.p->storedLinkFirst;
      regOperPtr->lastAttrinbufrec = storedPtr.p->storedLinkLast;
      regOperPtr->attrinbufLen = storedPtr.p->storedProcLength;
      regOperPtr->currentAttrinbufLen = storedPtr.p->storedProcLength;
      return ZOK;
    }//if
  }//if
  terrorCode = ZSTORED_PROC_ID_ERROR;
  return terrorCode;
}//Dbtup::initStoredOperationrec()

void Dbtup::copyAttrinfo(Signal* signal,
                         Operationrec * const regOperPtr,
                         Uint32* inBuffer)
{
  AttrbufrecPtr copyAttrBufPtr;
  Uint32 RnoOfAttrBufrec = cnoOfAttrbufrec;
  int RbufLen;
  Uint32 RinBufIndex = 0;
  Uint32 Rnext;
  Uint32 Rfirst;
  Uint32 TstoredProcedure = (regOperPtr->storedProcedureId != ZNIL);
  Uint32 RnoFree = cnoFreeAttrbufrec;

//-------------------------------------------------------------------------
// As a prelude to the execution of the TUPKEYREQ we will copy the program
// into the inBuffer to enable easy execution without any complex jumping
// between the buffers. In particular this will make the interpreter less
// complex. Hopefully it does also improve performance.
//-------------------------------------------------------------------------
  copyAttrBufPtr.i = regOperPtr->firstAttrinbufrec;
  while (copyAttrBufPtr.i != RNIL) {
    jam();
    ndbrequire(copyAttrBufPtr.i < RnoOfAttrBufrec);
    ptrAss(copyAttrBufPtr, attrbufrec);
    RbufLen = copyAttrBufPtr.p->attrbuf[ZBUF_DATA_LEN];
    Rnext = copyAttrBufPtr.p->attrbuf[ZBUF_NEXT];
    Rfirst = cfirstfreeAttrbufrec;
    MEMCOPY_NO_WORDS(&inBuffer[RinBufIndex],
                     &copyAttrBufPtr.p->attrbuf[0],
                     RbufLen);
    RinBufIndex += RbufLen;
    if (!TstoredProcedure) {
      copyAttrBufPtr.p->attrbuf[ZBUF_NEXT] = Rfirst;
      cfirstfreeAttrbufrec = copyAttrBufPtr.i;
      RnoFree++;
    }//if
    copyAttrBufPtr.i = Rnext;
  }//while
  cnoFreeAttrbufrec = RnoFree;
  if (TstoredProcedure) {
    jam();
    StoredProcPtr storedPtr;
    c_storedProcPool.getPtr(storedPtr, (Uint32)regOperPtr->storedProcedureId);
    ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);
    storedPtr.p->storedCounter--;
    regOperPtr->storedProcedureId = ZNIL;
  }//if
  // Release the ATTRINFO buffers
  regOperPtr->firstAttrinbufrec = RNIL;
  regOperPtr->lastAttrinbufrec = RNIL;
}//Dbtup::copyAttrinfo()

void Dbtup::handleATTRINFOforTUPKEYREQ(Signal* signal,
                                       Uint32 length,
                                       Operationrec * const regOperPtr) 
{
  AttrbufrecPtr TAttrinbufptr;
  TAttrinbufptr.i = cfirstfreeAttrbufrec;
  if ((cfirstfreeAttrbufrec < cnoOfAttrbufrec) &&
      (cnoFreeAttrbufrec > MIN_ATTRBUF)) {
    ptrAss(TAttrinbufptr, attrbufrec);
    MEMCOPY_NO_WORDS(&TAttrinbufptr.p->attrbuf[0],
                     &signal->theData[3],
                     length);
    Uint32 RnoFree = cnoFreeAttrbufrec;
    Uint32 Rnext = TAttrinbufptr.p->attrbuf[ZBUF_NEXT];
    TAttrinbufptr.p->attrbuf[ZBUF_DATA_LEN] = length;
    TAttrinbufptr.p->attrbuf[ZBUF_NEXT] = RNIL;

    AttrbufrecPtr locAttrinbufptr;
    Uint32 RnewLen = regOperPtr->currentAttrinbufLen;

    locAttrinbufptr.i = regOperPtr->lastAttrinbufrec;
    cfirstfreeAttrbufrec = Rnext;
    cnoFreeAttrbufrec = RnoFree - 1;
    RnewLen += length;
    regOperPtr->lastAttrinbufrec = TAttrinbufptr.i;
    regOperPtr->currentAttrinbufLen = RnewLen;
    if (locAttrinbufptr.i == RNIL) {
      regOperPtr->firstAttrinbufrec = TAttrinbufptr.i;
      return;
    } else {
      jam();
      ptrCheckGuard(locAttrinbufptr, cnoOfAttrbufrec, attrbufrec);
      locAttrinbufptr.p->attrbuf[ZBUF_NEXT] = TAttrinbufptr.i;
    }//if
    if (RnewLen < ZATTR_BUFFER_SIZE) {
      return;
    } else {
      jam();
      regOperPtr->transstate = TOO_MUCH_AI;
      return;
    }//if
  } else if (cnoFreeAttrbufrec <= MIN_ATTRBUF) {
    jam();
    regOperPtr->transstate = ERROR_WAIT_TUPKEYREQ;
  } else {
    ndbrequire(false);
  }//if
}//Dbtup::handleATTRINFOforTUPKEYREQ()

void Dbtup::execATTRINFO(Signal* signal) 
{
  OperationrecPtr regOpPtr;
  Uint32 Rsig0 = signal->theData[0];
  Uint32 Rlen = signal->length();
  regOpPtr.i = Rsig0;

  jamEntry();

  ptrCheckGuard(regOpPtr, cnoOfOprec, operationrec);
  if (regOpPtr.p->transstate == IDLE) {
    handleATTRINFOforTUPKEYREQ(signal, Rlen - 3, regOpPtr.p);
    return;
  } else if (regOpPtr.p->transstate == WAIT_STORED_PROCEDURE_ATTR_INFO) {
    storedProcedureAttrInfo(signal, regOpPtr.p, Rlen - 3, 3, false);
    return;
  }//if
  switch (regOpPtr.p->transstate) {
  case ERROR_WAIT_STORED_PROCREQ:
    jam();
  case TOO_MUCH_AI:
    jam();
  case ERROR_WAIT_TUPKEYREQ:
    jam();
    return;	/* IGNORE ATTRINFO IN THOSE STATES, WAITING FOR ABORT SIGNAL */
    break;
  case DISCONNECTED:
    jam();
  case STARTED:
    jam();
  default:
    ndbrequire(false);
  }//switch
}//Dbtup::execATTRINFO()

void Dbtup::execTUP_ALLOCREQ(Signal* signal)
{
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;
  FragrecordPtr regFragPtr;

  jamEntry();

  regOperPtr.i = signal->theData[0];
  regFragPtr.i = signal->theData[1];
  regTabPtr.i = signal->theData[2];

  if (!((regOperPtr.i < cnoOfOprec) &&
        (regFragPtr.i < cnoOfFragrec) &&
        (regTabPtr.i < cnoOfTablerec))) {
    ndbrequire(false);
  }//if
  ptrAss(regOperPtr, operationrec);
  ptrAss(regFragPtr, fragrecord);
  ptrAss(regTabPtr, tablerec);

//---------------------------------------------------
/* --- Allocate a tuple as requested by ACC    --- */
//---------------------------------------------------
  PagePtr pagePtr;
  Uint32 pageOffset;
  if (!allocTh(regFragPtr.p,
               regTabPtr.p,
               NORMAL_PAGE,
               signal,
               pageOffset,
               pagePtr)) {
    signal->theData[0] = terrorCode; // Indicate failure
    return;
  }//if
  Uint32 fragPageId = pagePtr.p->pageWord[ZPAGE_FRAG_PAGE_ID_POS];
  Uint32 pageIndex = ((pageOffset - ZPAGE_HEADER_SIZE) /
                       regTabPtr.p->tupheadsize) << 1;
  regOperPtr.p->tableRef = regTabPtr.i;
  regOperPtr.p->fragId = regFragPtr.p->fragmentId;
  regOperPtr.p->realPageId = pagePtr.i;
  regOperPtr.p->fragPageId = fragPageId;
  regOperPtr.p->pageOffset = pageOffset;
  regOperPtr.p->pageIndex  = pageIndex;
  /* -------------------------------------------------------------- */
  /* AN INSERT IS UNDONE BY FREEING THE DATA OCCUPIED BY THE INSERT */
  /* THE ONLY DATA WE HAVE TO LOG EXCEPT THE TYPE, PAGE AND INDEX   */
  /* IS THE AMOUNT OF DATA TO FREE                                  */
  /* -------------------------------------------------------------- */
  if (isUndoLoggingNeeded(regFragPtr.p, fragPageId)) {
    jam();
    cprAddUndoLogRecord(signal,
                        ZLCPR_TYPE_DELETE_TH,
                        fragPageId,
                        pageIndex,
                        regTabPtr.i,
                        regFragPtr.p->fragmentId,
                        regFragPtr.p->checkpointVersion);
  }//if

  //---------------------------------------------------------------
  // Initialise Active operation list by setting the list to empty
  //---------------------------------------------------------------
  ndbrequire(pageOffset < ZWORDS_ON_PAGE);
  pagePtr.p->pageWord[pageOffset] = RNIL;

  signal->theData[0] = 0;
  signal->theData[1] = fragPageId;
  signal->theData[2] = pageIndex;
}//Dbtup::execTUP_ALLOCREQ()

void
Dbtup::setChecksum(Page* const pagePtr, Uint32 tupHeadOffset, Uint32 tupHeadSize)
{
  // 2 == regTabPtr.p->tupChecksumIndex
  pagePtr->pageWord[tupHeadOffset + 2] = 0;
  Uint32 checksum = calculateChecksum(pagePtr, tupHeadOffset, tupHeadSize);
  pagePtr->pageWord[tupHeadOffset + 2] = checksum;
}//Dbtup::setChecksum()

Uint32
Dbtup::calculateChecksum(Page* pagePtr,
                         Uint32 tupHeadOffset,
                         Uint32 tupHeadSize)
{
  Uint32 checksum = 0;
  Uint32 loopStop = tupHeadOffset + tupHeadSize;
  ndbrequire(loopStop <= ZWORDS_ON_PAGE);
  // includes tupVersion
  for (Uint32 i = tupHeadOffset + 1; i < loopStop; i++) {
    checksum ^= pagePtr->pageWord[i];
  }//if
  return checksum;
}//Dbtup::calculateChecksum()

/* ----------------------------------------------------------------- */
/* -----------       INSERT_ACTIVE_OP_LIST            -------------- */
/* ----------------------------------------------------------------- */
void Dbtup::insertActiveOpList(Signal* signal, 
                               OperationrecPtr regOperPtr,
                               Page*  const pagePtr,
                               Uint32 pageOffset) 
{
  OperationrecPtr iaoPrevOpPtr;
  ndbrequire(regOperPtr.p->inActiveOpList == ZFALSE);
  regOperPtr.p->inActiveOpList = ZTRUE;
  ndbrequire(pageOffset < ZWORDS_ON_PAGE);
  iaoPrevOpPtr.i = pagePtr->pageWord[pageOffset];
  pagePtr->pageWord[pageOffset] = regOperPtr.i;
  regOperPtr.p->prevActiveOp = RNIL;
  regOperPtr.p->nextActiveOp = iaoPrevOpPtr.i;
  if (iaoPrevOpPtr.i == RNIL) {
    return;
  } else {
    jam();
    ptrCheckGuard(iaoPrevOpPtr, cnoOfOprec, operationrec);
    iaoPrevOpPtr.p->prevActiveOp = regOperPtr.i;
    if (iaoPrevOpPtr.p->optype == ZDELETE &&
        regOperPtr.p->optype == ZINSERT) {
      jam();
      // mark both
      iaoPrevOpPtr.p->deleteInsertFlag = 1;
      regOperPtr.p->deleteInsertFlag = 1;
    }
    return;
  }//if
}//Dbtup::insertActiveOpList()

void Dbtup::linkOpIntoFragList(OperationrecPtr regOperPtr,
                               Fragrecord* const regFragPtr) 
{
  OperationrecPtr sopTmpOperPtr;
  Uint32 tail = regFragPtr->lastusedOprec;
  ndbrequire(regOperPtr.p->inFragList == ZFALSE);
  regOperPtr.p->inFragList = ZTRUE;
  regOperPtr.p->prevOprecInList = tail;
  regOperPtr.p->nextOprecInList = RNIL;
  sopTmpOperPtr.i = tail;
  if (tail == RNIL) {
    regFragPtr->firstusedOprec = regOperPtr.i;
  } else {
    jam();
    ptrCheckGuard(sopTmpOperPtr, cnoOfOprec, operationrec);
    sopTmpOperPtr.p->nextOprecInList = regOperPtr.i;
  }//if
  regFragPtr->lastusedOprec = regOperPtr.i;
}//Dbtup::linkOpIntoFragList()

/*
This routine is optimised for use from TUPKEYREQ.
This means that a lot of input data is stored in the operation record.
The routine expects the following data in the operation record to be
set-up properly.
Transaction data
1) transid1
2) transid2
3) savePointId

Operation data
4) optype
5) dirtyOp

Tuple address
6) fragPageId
7) pageIndex

regFragPtr and regTabPtr are references to the table and fragment data and
is read-only.

The routine will set up the following data in the operation record if
returned with success.

Tuple address data
1) realPageId
2) fragPageId
3) pageOffset
4) pageIndex

Also the pagePtr is an output variable if the routine returns with success.
It's input value can be undefined.
*/
bool
Dbtup::getPage(PagePtr& pagePtr,
               Operationrec* const regOperPtr,
               Fragrecord* const regFragPtr,
               Tablerec* const regTabPtr)
{
/* ------------------------------------------------------------------------- */
// GET THE REFERENCE TO THE TUPLE HEADER BY TRANSLATING THE FRAGMENT PAGE ID
// INTO A REAL PAGE ID AND BY USING THE PAGE INDEX TO DERIVE THE PROPER INDEX
// IN THE REAL PAGE.
/* ------------------------------------------------------------------------- */
  pagePtr.i = getRealpid(regFragPtr, regOperPtr->fragPageId);
  regOperPtr->realPageId = pagePtr.i;
  Uint32 RpageIndex = regOperPtr->pageIndex;
  Uint32 Rtupheadsize = regTabPtr->tupheadsize;
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 RpageIndexScaled = RpageIndex >> 1;
  ndbrequire((RpageIndex & 1) == 0);
  regOperPtr->pageOffset = ZPAGE_HEADER_SIZE + 
                           (Rtupheadsize * RpageIndexScaled);

  OperationrecPtr leaderOpPtr;
  ndbrequire(regOperPtr->pageOffset < ZWORDS_ON_PAGE);
  leaderOpPtr.i = pagePtr.p->pageWord[regOperPtr->pageOffset];
  if (leaderOpPtr.i == RNIL) {
    return true;
  }//if
  ptrCheckGuard(leaderOpPtr, cnoOfOprec, operationrec);
  bool dirtyRead = ((regOperPtr->optype == ZREAD) &&
                    (regOperPtr->dirtyOp == 1));
  if (dirtyRead) {
    bool sameTrans = ((regOperPtr->transid1 == leaderOpPtr.p->transid1) &&
                      (regOperPtr->transid2 == leaderOpPtr.p->transid2));
    if (!sameTrans) {
      if (!getPageLastCommitted(regOperPtr, leaderOpPtr.p)) {
        return false;
      }//if
      pagePtr.i = regOperPtr->realPageId;
      ptrCheckGuard(pagePtr, cnoOfPage, page);
      return true;
    }//if
  }//if
  if (regOperPtr->optype == ZREAD) {
    /*
    Read uses savepoint id's to find the correct tuple version.
    */
    if (getPageThroughSavePoint(regOperPtr, leaderOpPtr.p)) {
      jam();
      pagePtr.i = regOperPtr->realPageId;
      ptrCheckGuard(pagePtr, cnoOfPage, page);
      return true;
    }
    return false;
  }
//----------------------------------------------------------------------
// Check that no other operation is already active on the tuple. Also
// that abort or commit is not ongoing.
//----------------------------------------------------------------------
  if (leaderOpPtr.p->tupleState == NO_OTHER_OP) {
    jam();
    if ((leaderOpPtr.p->optype == ZDELETE) &&
        (regOperPtr->optype != ZINSERT)) {
      jam();
      terrorCode = ZTUPLE_DELETED_ERROR;
      return false;
    }//if
    return true;
  } else if (leaderOpPtr.p->tupleState == ALREADY_ABORTED) {
    jam();
    terrorCode = ZMUST_BE_ABORTED_ERROR;
    return false;
  } else {
    ndbrequire(false);
  }//if
  return true;
}//Dbtup::getPage()

bool
Dbtup::getPageThroughSavePoint(Operationrec* regOperPtr,
                               Operationrec* leaderOpPtr)
{
  bool found = false;
  OperationrecPtr loopOpPtr;
  loopOpPtr.p = leaderOpPtr;
  while(true) {
    if (regOperPtr->savePointId > loopOpPtr.p->savePointId) {
      jam();
      found = true;
      break;
    }
    if (loopOpPtr.p->nextActiveOp == RNIL) {
      break;
    }
    loopOpPtr.i = loopOpPtr.p->nextActiveOp;
    ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
    jam();
  }
  if (!found) {
    return getPageLastCommitted(regOperPtr, loopOpPtr.p);
  } else {
    if (loopOpPtr.p->optype == ZDELETE) {
      jam();
      terrorCode = ZTUPLE_DELETED_ERROR;
      return false;
    }
    if (loopOpPtr.p->tupleState == ALREADY_ABORTED) {
      /*
      Requested tuple version has already been aborted
      */
      jam();
      terrorCode = ZMUST_BE_ABORTED_ERROR;
      return false;
    }
    bool use_copy;
    if (loopOpPtr.p->prevActiveOp == RNIL) {
      jam();
      /*
      Use original tuple since we are reading from the last written tuple.
      We are the 
      */
      use_copy = false;
    } else {
      /*
      Go forward in time to find a copy of the tuple which this operation
      produced
      */
      loopOpPtr.i = loopOpPtr.p->prevActiveOp;
      ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
      if (loopOpPtr.p->optype == ZDELETE) {
        /*
        This operation was a Delete and thus have no copy tuple attached to
        it. We will move forward to the next that either doesn't exist in
        which case we will return the original tuple of any operation and
        otherwise it must be an insert which contains a copy record.
        */
        if (loopOpPtr.p->prevActiveOp == RNIL) {
          jam();
          use_copy = false;
        } else {
          jam();
          loopOpPtr.i = loopOpPtr.p->prevActiveOp;
          ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
          ndbrequire(loopOpPtr.p->optype == ZINSERT);
          use_copy = true;
        }
      } else if (loopOpPtr.p->optype == ZUPDATE) {
        jam();
        /*
        This operation which was the next in time have a copy which was the
        result of the previous operation which we want to use. Thus use
        the copy tuple of this operation.
        */
        use_copy = true;
      } else {
        /*
        This operation was an insert that happened after an insert or update.
        This is not a possible case.
        */
        ndbrequire(false);
        return false;
      }
    }
    if (use_copy) {
      regOperPtr->realPageId = loopOpPtr.p->realPageIdC;
      regOperPtr->fragPageId = loopOpPtr.p->fragPageIdC;
      regOperPtr->pageIndex = loopOpPtr.p->pageIndexC;
      regOperPtr->pageOffset = loopOpPtr.p->pageOffsetC;
    } else {
      regOperPtr->realPageId = loopOpPtr.p->realPageId;
      regOperPtr->fragPageId = loopOpPtr.p->fragPageId;
      regOperPtr->pageIndex = loopOpPtr.p->pageIndex;
      regOperPtr->pageOffset = loopOpPtr.p->pageOffset;
    }
    return true;
  }
}

bool
Dbtup::getPageLastCommitted(Operationrec* const regOperPtr,
                            Operationrec* const leaderOpPtr)
{
//----------------------------------------------------------------------
// Dirty reads wants to read the latest committed tuple. The latest
// tuple value could be not existing or else we have to find the copy
// tuple. Start by finding the end of the list to find the first operation
// on the record in the ongoing transaction.
//----------------------------------------------------------------------
  jam();
  OperationrecPtr loopOpPtr;
  loopOpPtr.p = leaderOpPtr;
  while (loopOpPtr.p->nextActiveOp != RNIL) {
    jam();
    loopOpPtr.i = loopOpPtr.p->nextActiveOp;
    ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
  }//while
  if (loopOpPtr.p->optype == ZINSERT) {
    jam();
//----------------------------------------------------------------------
// With an insert in the start of the list we know that the tuple did not
// exist before this transaction was started. We don't care if the current
// transaction is in the commit phase since the commit is not really
// completed until the operation is gone from TUP.
//----------------------------------------------------------------------
    terrorCode = ZTUPLE_DELETED_ERROR;
    return false;
  } else {
//----------------------------------------------------------------------
// A successful update and delete as first in the queue means that a tuple
// exist in the committed world. We need to find it.
//----------------------------------------------------------------------
    if (loopOpPtr.p->optype == ZUPDATE) {
      jam();
//----------------------------------------------------------------------
// The first operation was a delete we set our tuple reference to the
// copy tuple of this operation.
//----------------------------------------------------------------------
      regOperPtr->realPageId = loopOpPtr.p->realPageIdC;
      regOperPtr->fragPageId = loopOpPtr.p->fragPageIdC;
      regOperPtr->pageIndex  = loopOpPtr.p->pageIndexC;
      regOperPtr->pageOffset = loopOpPtr.p->pageOffsetC;
    } else if ((loopOpPtr.p->optype == ZDELETE) &&
               (loopOpPtr.p->prevActiveOp == RNIL)) {
      jam();
//----------------------------------------------------------------------
// There was only a delete. The original tuple still is ok.
//----------------------------------------------------------------------
    } else {
      jam();
//----------------------------------------------------------------------
// There was another operation after the delete, this must be an insert
// and we have found our copy tuple there.
//----------------------------------------------------------------------
      loopOpPtr.i = loopOpPtr.p->prevActiveOp;
      ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
      ndbrequire(loopOpPtr.p->optype == ZINSERT);
      regOperPtr->realPageId = loopOpPtr.p->realPageIdC;
      regOperPtr->fragPageId = loopOpPtr.p->fragPageIdC;
      regOperPtr->pageIndex  = loopOpPtr.p->pageIndexC;
      regOperPtr->pageOffset = loopOpPtr.p->pageOffsetC;
    }//if
  }//if
  return true;
}//Dbtup::getPageLastCommitted()

void Dbtup::execTUPKEYREQ(Signal* signal) 
{
  TupKeyReq * const tupKeyReq = (TupKeyReq *)signal->getDataPtr();
  Uint32 RoperPtr = tupKeyReq->connectPtr;
  Uint32 Rtabptr = tupKeyReq->tableRef;
  Uint32 RfragId = tupKeyReq->fragId;
  Uint32 Rstoredid = tupKeyReq->storedProcedure;
  Uint32 Rfragptr = tupKeyReq->fragPtr;

  Uint32 RnoOfOprec = cnoOfOprec;
  Uint32 RnoOfTablerec = cnoOfTablerec;
  Uint32 RnoOfFragrec = cnoOfFragrec;

  operPtr.i = RoperPtr;
  fragptr.i = Rfragptr;
  tabptr.i = Rtabptr;
  jamEntry();

  ndbrequire(((RoperPtr < RnoOfOprec) &&
        (Rtabptr < RnoOfTablerec) &&
        (Rfragptr < RnoOfFragrec)));
  ptrAss(operPtr, operationrec);
  Operationrec * const regOperPtr = operPtr.p;
  ptrAss(fragptr, fragrecord);
  Fragrecord * const regFragPtr = fragptr.p;
  ptrAss(tabptr, tablerec);
  Tablerec* const regTabPtr = tabptr.p;

  Uint32 TrequestInfo = tupKeyReq->request;

  if (regOperPtr->transstate != IDLE) {
    TUPKEY_abort(signal, 39);
    return;
  }//if
/* ----------------------------------------------------------------- */
// Operation is ZREAD when we arrive here so no need to worry about the
// abort process.
/* ----------------------------------------------------------------- */
/* -----------    INITIATE THE OPERATION RECORD       -------------- */
/* ----------------------------------------------------------------- */
  regOperPtr->fragmentPtr = Rfragptr;
  regOperPtr->dirtyOp = TrequestInfo & 1;
  regOperPtr->opSimple = (TrequestInfo >> 1) & 1;
  regOperPtr->interpretedExec = (TrequestInfo >> 10) & 1;
  regOperPtr->optype = (TrequestInfo >> 6) & 0xf;

  // Attributes needed by trigger execution
  regOperPtr->noFiredTriggers = 0;
  regOperPtr->tableRef = Rtabptr;
  regOperPtr->tcOperationPtr = tupKeyReq->opRef;
  regOperPtr->primaryReplica = tupKeyReq->primaryReplica;
  regOperPtr->coordinatorTC = tupKeyReq->coordinatorTC;
  regOperPtr->tcOpIndex = tupKeyReq->tcOpIndex;
  regOperPtr->savePointId = tupKeyReq->savePointId;

  regOperPtr->fragId = RfragId;

  regOperPtr->fragPageId = tupKeyReq->keyRef1;
  regOperPtr->pageIndex = tupKeyReq->keyRef2;
  regOperPtr->attrinbufLen = regOperPtr->logSize = tupKeyReq->attrBufLen;
  regOperPtr->recBlockref = tupKeyReq->applRef;

// Schema Version in tupKeyReq->schemaVersion not used in this version
  regOperPtr->storedProcedureId = Rstoredid;
  regOperPtr->transid1 = tupKeyReq->transId1;
  regOperPtr->transid2 = tupKeyReq->transId2;

  regOperPtr->attroutbufLen = 0;
/* ----------------------------------------------------------------------- */
// INITIALISE TO DEFAULT VALUE
// INIT THE COPY REFERENCE RECORDS TO RNIL TO ENSURE THAT THEIR VALUES
// ARE VALID IF THEY EXISTS
// NO PENDING CHECKPOINT WHEN COPY CREATED (DEFAULT)
// NO TUPLE HAS BEEN ALLOCATED YET
// NO COPY HAS BEEN CREATED YET
/* ----------------------------------------------------------------------- */
  regOperPtr->undoLogged = false;
  regOperPtr->realPageId = RNIL;
  regOperPtr->realPageIdC = RNIL;
  regOperPtr->fragPageIdC = RNIL;

  regOperPtr->pageOffset = ZNIL;
  regOperPtr->pageOffsetC = ZNIL;

  regOperPtr->pageIndexC = ZNIL;

  // version not yet known
  regOperPtr->tupVersion = ZNIL;
  regOperPtr->deleteInsertFlag = 0;

  regOperPtr->tupleState = TUPLE_BLOCKED;
  regOperPtr->changeMask.clear();
  
  if (Rstoredid != ZNIL) {
    ndbrequire(initStoredOperationrec(regOperPtr, Rstoredid) == ZOK);
  }//if
  copyAttrinfo(signal, regOperPtr, &cinBuffer[0]);

  PagePtr pagePtr;
  if (!getPage(pagePtr, regOperPtr, regFragPtr, regTabPtr)) {
    tupkeyErrorLab(signal);
    return;
  }//if

  Uint32 Roptype = regOperPtr->optype;
  if (Roptype == ZREAD) {
    jam();
    if (handleReadReq(signal, regOperPtr, regTabPtr, pagePtr.p) != -1) {
      sendTUPKEYCONF(signal, regOperPtr, 0);
/* ------------------------------------------------------------------------- */
// Read Operations need not to be taken out of any lists. We also do not
// need to wait for commit since there is no changes to commit. Thus we
// prepare the operation record already now for the next operation.
// Write operations have set the state to STARTED above indicating that
// they are waiting for the Commit or Abort decision.
/* ------------------------------------------------------------------------- */
      regOperPtr->transstate = IDLE;
      regOperPtr->currentAttrinbufLen = 0;
    }//if
    return;
  }//if
  linkOpIntoFragList(operPtr, regFragPtr);
  insertActiveOpList(signal,
                     operPtr,
                     pagePtr.p,
                     regOperPtr->pageOffset);
  if (isUndoLoggingBlocked(regFragPtr)) {
    TUPKEY_abort(signal, 38);
    return;
  }//if
/* ---------------------------------------------------------------------- */
// WE SET THE CURRENT ACTIVE OPERATION IN THE TUPLE TO POINT TO OUR
//OPERATION RECORD. IF SEVERAL OPERATIONS WORK ON THIS TUPLE THEY ARE
// LINKED TO OUR OPERATION RECORD. DIRTY READS CAN ACCESS THE COPY
// TUPLE THROUGH OUR OPERATION RECORD.
/* ---------------------------------------------------------------------- */
  if (Roptype == ZINSERT) {
    jam();
    if (handleInsertReq(signal, regOperPtr,
                        regFragPtr, regTabPtr, pagePtr.p) == -1) {
      return;
    }//if
    if (!regTabPtr->tuxCustomTriggers.isEmpty()) {
      jam();
      if (executeTuxInsertTriggers(signal, regOperPtr, regTabPtr) != 0) {
        jam();
        tupkeyErrorLab(signal);
        return;
      }
    }
    checkImmediateTriggersAfterInsert(signal,
                                      regOperPtr,
                                      regTabPtr);
    sendTUPKEYCONF(signal, regOperPtr, regOperPtr->logSize);
    return;
  }//if
  if (regTabPtr->checksumIndicator &&
      (calculateChecksum(pagePtr.p,
                         regOperPtr->pageOffset,
                         regTabPtr->tupheadsize) != 0)) {
    jam();
    terrorCode = ZTUPLE_CORRUPTED_ERROR;
    tupkeyErrorLab(signal);
    return;
  }//if
  if (Roptype == ZUPDATE) {
    jam();
    if (handleUpdateReq(signal, regOperPtr,
                        regFragPtr, regTabPtr, pagePtr.p) == -1) {
      return;
    }//if
    // If update operation is done on primary, 
    // check any after op triggers
    terrorCode = 0;
    if (!regTabPtr->tuxCustomTriggers.isEmpty()) {
      jam();
      if (executeTuxUpdateTriggers(signal, regOperPtr, regTabPtr) != 0) {
        jam();
        tupkeyErrorLab(signal);
        return;
      }
    }
    checkImmediateTriggersAfterUpdate(signal,
                                      regOperPtr,
                                      regTabPtr);
    // XXX use terrorCode for now since all methods are void
    if (terrorCode != 0) {
      tupkeyErrorLab(signal);
      return;
    }
    sendTUPKEYCONF(signal, regOperPtr, regOperPtr->logSize);
    return;
  } else if (Roptype == ZDELETE) {
    jam();
    if (handleDeleteReq(signal, regOperPtr,
                        regFragPtr, regTabPtr, pagePtr.p) == -1) {
      return;
    }//if
    // If delete operation is done on primary, 
    // check any after op triggers
    if (!regTabPtr->tuxCustomTriggers.isEmpty()) {
      jam();
      if (executeTuxDeleteTriggers(signal, regOperPtr, regTabPtr) != 0) {
        jam();
        tupkeyErrorLab(signal);
        return;
      }
    }
    checkImmediateTriggersAfterDelete(signal,
                                      regOperPtr, 
                                      regTabPtr);
    sendTUPKEYCONF(signal, regOperPtr, 0);
    return;
  } else {
    ndbrequire(false);
  }//if
}//Dbtup::execTUPKEYREQ()

/* ---------------------------------------------------------------- */
/* ------------------------ CONFIRM REQUEST ----------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::sendTUPKEYCONF(Signal* signal, 
                           Operationrec * const regOperPtr, 
                           Uint32 TlogSize) 
{
  TupKeyConf * const tupKeyConf = (TupKeyConf *)signal->getDataPtrSend();  

  Uint32 RuserPointer = regOperPtr->userpointer;
  Uint32 RattroutbufLen = regOperPtr->attroutbufLen;
  Uint32 RnoFiredTriggers = regOperPtr->noFiredTriggers;
  BlockReference Ruserblockref = regOperPtr->userblockref;
  Uint32 lastRow = regOperPtr->lastRow;

  regOperPtr->transstate = STARTED;
  regOperPtr->tupleState = NO_OTHER_OP;
  tupKeyConf->userPtr = RuserPointer;
  tupKeyConf->readLength = RattroutbufLen;
  tupKeyConf->writeLength = TlogSize;
  tupKeyConf->noFiredTriggers = RnoFiredTriggers;
  tupKeyConf->lastRow = lastRow;

  EXECUTE_DIRECT(refToBlock(Ruserblockref), GSN_TUPKEYCONF, signal,
		 TupKeyConf::SignalLength);
  return;
}//Dbtup::sendTUPKEYCONF()

/* ---------------------------------------------------------------- */
/* ----------------------------- READ  ---------------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::handleReadReq(Signal* signal,
                         Operationrec* const regOperPtr,
                         Tablerec* const regTabPtr,
                         Page* pagePtr)
{
  Uint32 Ttupheadoffset = regOperPtr->pageOffset;
  const BlockReference sendBref = regOperPtr->recBlockref;
  if (regTabPtr->checksumIndicator &&
      (calculateChecksum(pagePtr, Ttupheadoffset,
                         regTabPtr->tupheadsize) != 0)) {
    jam();
    terrorCode = ZTUPLE_CORRUPTED_ERROR;
    tupkeyErrorLab(signal);
    return -1;
  }//if

  Uint32 * dst = &signal->theData[25];
  Uint32 dstLen = (sizeof(signal->theData) / 4) - 25;
  const Uint32 node = refToNode(sendBref);
  if(node != 0 && node != getOwnNodeId()) {
    ;
  } else {
    jam();
    /**
     * execute direct
     */
    dst = &signal->theData[3];
    dstLen = (sizeof(signal->theData) / 4) - 3;
  }
  
  if (regOperPtr->interpretedExec != 1) {
    jam();
    int ret = readAttributes(pagePtr,
			     Ttupheadoffset,
			     &cinBuffer[0],
			     regOperPtr->attrinbufLen,
			     dst,
			     dstLen,
			     false);
    if (ret != -1) {
/* ------------------------------------------------------------------------- */
// We have read all data into coutBuffer. Now send it to the API.
/* ------------------------------------------------------------------------- */
      jam();
      Uint32 TnoOfDataRead= (Uint32) ret;
      regOperPtr->attroutbufLen = TnoOfDataRead;
      sendReadAttrinfo(signal, TnoOfDataRead, regOperPtr);
      return 0;
    }//if
    jam();
    tupkeyErrorLab(signal);
    return -1;
  } else {
    jam();
    regOperPtr->lastRow = 0;
    if (interpreterStartLab(signal, pagePtr, Ttupheadoffset) != -1) {
      return 0;
    }//if
    return -1;
  }//if
}//Dbtup::handleReadReq()

/* ---------------------------------------------------------------- */
/* ---------------------------- UPDATE ---------------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::handleUpdateReq(Signal* signal,
                           Operationrec* const regOperPtr,
                           Fragrecord* const regFragPtr,
                           Tablerec* const regTabPtr,
                           Page* const pagePtr) 
{
  PagePtr copyPagePtr;
  Uint32 tuple_size = regTabPtr->tupheadsize;

//---------------------------------------------------
/* --- MAKE A COPY OF THIS TUPLE ON A COPY PAGE --- */
//---------------------------------------------------
  Uint32 RpageOffsetC;
  if (!allocTh(regFragPtr,
               regTabPtr,
               COPY_PAGE,
               signal,
               RpageOffsetC,
               copyPagePtr)) {
    TUPKEY_abort(signal, 1);
    return -1;
  }//if
  Uint32 RpageIdC = copyPagePtr.i;
  Uint32 RfragPageIdC = copyPagePtr.p->pageWord[ZPAGE_FRAG_PAGE_ID_POS];
  Uint32 indexC = ((RpageOffsetC - ZPAGE_HEADER_SIZE) / tuple_size) << 1;
  regOperPtr->pageIndexC = indexC;
  regOperPtr->fragPageIdC = RfragPageIdC;
  regOperPtr->realPageIdC = RpageIdC;
  regOperPtr->pageOffsetC = RpageOffsetC;
  /* -------------------------------------------------------------- */
  /* IF WE HAVE AN ONGING CHECKPOINT WE HAVE TO LOG THE ALLOCATION  */
  /* OF THE TUPLE HEADER TO BE ABLE TO DELETE IT UPON RESTART       */
  /* THE ONLY DATA EXCEPT THE TYPE, PAGE, INDEX IS THE SIZE TO FREE */
  /* -------------------------------------------------------------- */
  if (isUndoLoggingActive(regFragPtr)) {
    if (isPageUndoLogged(regFragPtr, RfragPageIdC)) {
      jam();
      regOperPtr->undoLogged = true;
      cprAddUndoLogRecord(signal,
                          ZLCPR_TYPE_DELETE_TH,
                          RfragPageIdC,
                          indexC,
                          regOperPtr->tableRef,
                          regOperPtr->fragId,
                          regFragPtr->checkpointVersion);
    }//if
    if (isPageUndoLogged(regFragPtr, regOperPtr->fragPageId)) {
      jam();
      cprAddUndoLogRecord(signal,
                          ZLCPR_TYPE_UPDATE_TH,
                          regOperPtr->fragPageId,
                          regOperPtr->pageIndex,
                          regOperPtr->tableRef,
                          regOperPtr->fragId,
                          regFragPtr->checkpointVersion);
      cprAddData(signal,
                 regFragPtr,
                 regOperPtr->realPageId,
                 tuple_size,
                 regOperPtr->pageOffset);
    }//if
  }//if
  Uint32 RwordCount = tuple_size - 1;
  Uint32 end_dest = RpageOffsetC + tuple_size;
  Uint32 offset = regOperPtr->pageOffset;
  Uint32 end_source = offset + tuple_size;
  ndbrequire(end_dest <= ZWORDS_ON_PAGE && end_source <= ZWORDS_ON_PAGE);
  void* Tdestination = (void*)&copyPagePtr.p->pageWord[RpageOffsetC + 1];
  const void* Tsource = (void*)&pagePtr->pageWord[offset + 1];
  MEMCOPY_NO_WORDS(Tdestination, Tsource, RwordCount);

  Uint32 prev_tup_version;
  // nextActiveOp is before this op in event order
  if (regOperPtr->nextActiveOp == RNIL) {
    jam();
    prev_tup_version = ((const Uint32*)Tsource)[0];
  } else {
    OperationrecPtr prevOperPtr;
    jam();
    prevOperPtr.i = regOperPtr->nextActiveOp;
    ptrCheckGuard(prevOperPtr, cnoOfOprec, operationrec);
    prev_tup_version = prevOperPtr.p->tupVersion;
  }//if
  regOperPtr->tupVersion = (prev_tup_version + 1) &
                           ((1 << ZTUP_VERSION_BITS) - 1);
  // global variable alert
  ndbassert(operationrec + operPtr.i == regOperPtr);
  copyPagePtr.p->pageWord[RpageOffsetC] = operPtr.i;

  return updateStartLab(signal, regOperPtr, regTabPtr, pagePtr);
}//Dbtup::handleUpdateReq()

/* ---------------------------------------------------------------- */
/* ----------------------------- INSERT --------------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::handleInsertReq(Signal* signal,
                           Operationrec* const regOperPtr,
                           Fragrecord* const regFragPtr,
                           Tablerec* const regTabPtr,
                           Page* const pagePtr) 
{
  Uint32 ret_value;

  if (regOperPtr->nextActiveOp != RNIL) {
    jam();
    OperationrecPtr prevExecOpPtr;
    prevExecOpPtr.i = regOperPtr->nextActiveOp;
    ptrCheckGuard(prevExecOpPtr, cnoOfOprec, operationrec);
    if (prevExecOpPtr.p->optype != ZDELETE) {
      terrorCode = ZINSERT_ERROR;
      tupkeyErrorLab(signal);
      return -1;
    }//if
    ret_value = handleUpdateReq(signal, regOperPtr,
                                regFragPtr, regTabPtr, pagePtr);
  } else {
    jam();
    regOperPtr->tupVersion = 0;
    ret_value = updateStartLab(signal, regOperPtr, regTabPtr, pagePtr);
  }//if
  if (ret_value != (Uint32)-1) {
    if (checkNullAttributes(regOperPtr, regTabPtr)) {
      jam();
      return 0;
    }//if
    TUPKEY_abort(signal, 17);
  }//if
  return -1;
}//Dbtup::handleInsertReq()

/* ---------------------------------------------------------------- */
/* ---------------------------- DELETE ---------------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::handleDeleteReq(Signal* signal,
                           Operationrec* const regOperPtr,
                           Fragrecord* const regFragPtr,
                           Tablerec* const regTabPtr,
                           Page* const pagePtr)
{
  // delete must set but not increment tupVersion
  if (regOperPtr->nextActiveOp != RNIL) {
    OperationrecPtr prevExecOpPtr;
    prevExecOpPtr.i = regOperPtr->nextActiveOp;
    ptrCheckGuard(prevExecOpPtr, cnoOfOprec, operationrec);
    regOperPtr->tupVersion = prevExecOpPtr.p->tupVersion;
  } else {
    jam();
    regOperPtr->tupVersion = pagePtr->pageWord[regOperPtr->pageOffset + 1];
  }
  if (isUndoLoggingNeeded(regFragPtr, regOperPtr->fragPageId)) {
    jam();
    cprAddUndoLogRecord(signal,
                        ZINDICATE_NO_OP_ACTIVE,
                        regOperPtr->fragPageId,
                        regOperPtr->pageIndex,
                        regOperPtr->tableRef,
                        regOperPtr->fragId,
                        regFragPtr->checkpointVersion);
  }//if
  if (regOperPtr->attrinbufLen == 0) {
    return 0;
  }//if
/* ------------------------------------------------------------------------ */
/* THE APPLICATION WANTS TO READ THE TUPLE BEFORE IT IS DELETED.            */
/* ------------------------------------------------------------------------ */
  return handleReadReq(signal, regOperPtr, regTabPtr, pagePtr);
}//Dbtup::handleDeleteReq()

int
Dbtup::updateStartLab(Signal* signal,
                      Operationrec* const regOperPtr,
                      Tablerec* const regTabPtr,
                      Page* const pagePtr)
{
  int retValue;
  if (regOperPtr->optype == ZINSERT) {
    jam();
    setNullBits(pagePtr, regTabPtr, regOperPtr->pageOffset);
  }
  if (regOperPtr->interpretedExec != 1) {
    jam();
    retValue = updateAttributes(pagePtr,
                                regOperPtr->pageOffset,
                                &cinBuffer[0],
                                regOperPtr->attrinbufLen);
    if (retValue == -1) {
      tupkeyErrorLab(signal);
      return -1;
    }//if
  } else {
    jam();
    retValue = interpreterStartLab(signal, pagePtr, regOperPtr->pageOffset);
  }//if
  ndbrequire(regOperPtr->tupVersion != ZNIL);
  pagePtr->pageWord[regOperPtr->pageOffset + 1] = regOperPtr->tupVersion;
  if (regTabPtr->checksumIndicator) {
    jam();
    setChecksum(pagePtr, regOperPtr->pageOffset, regTabPtr->tupheadsize);
  }//if
  return retValue;
}//Dbtup::updateStartLab()

void
Dbtup::setNullBits(Page* const regPage, Tablerec* const regTabPtr, Uint32 pageOffset)
{
  Uint32 noOfExtraNullWords = regTabPtr->tupNullWords;
  Uint32 nullOffsetStart = regTabPtr->tupNullIndex + pageOffset;
  ndbrequire((noOfExtraNullWords + nullOffsetStart) < ZWORDS_ON_PAGE);
  for (Uint32 i = 0; i < noOfExtraNullWords; i++) {
    regPage->pageWord[nullOffsetStart + i] = 0xFFFFFFFF;
  }//for
}//Dbtup::setNullBits()

bool
Dbtup::checkNullAttributes(Operationrec* const regOperPtr,
                           Tablerec* const regTabPtr)
{
// Implement checking of updating all not null attributes in an insert here.
  Bitmask<MAXNROFATTRIBUTESINWORDS> attributeMask;  
  /* 
   * The idea here is maybe that changeMask is not-null attributes
   * and must contain notNullAttributeMask.  But:
   *
   * 1. changeMask has all bits set on insert
   * 2. not-null is checked in each UpdateFunction
   * 3. the code below does not work except trivially due to 1.
   *
   * XXX remove or fix
   */
  attributeMask.clear();
  attributeMask.bitOR(regOperPtr->changeMask);
  attributeMask.bitAND(regTabPtr->notNullAttributeMask);
  attributeMask.bitXOR(regTabPtr->notNullAttributeMask);
  if (!attributeMask.isclear()) {
    return false;
  }//if
  return true;
}//Dbtup::checkNullAttributes()

/* ---------------------------------------------------------------- */
/* THIS IS THE START OF THE INTERPRETED EXECUTION OF UPDATES. WE    */
/* START BY LINKING ALL ATTRINFO'S IN A DOUBLY LINKED LIST (THEY ARE*/
/* ALREADY IN A LINKED LIST). WE ALLOCATE A REGISTER MEMORY (EQUAL  */
/* TO AN ATTRINFO RECORD). THE INTERPRETER GOES THROUGH FOUR  PHASES*/
/* DURING THE FIRST PHASE IT IS ONLY ALLOWED TO READ ATTRIBUTES THAT*/
/* ARE SENT TO THE CLIENT APPLICATION. DURING THE SECOND PHASE IT IS*/
/* ALLOWED TO READ FROM ATTRIBUTES INTO REGISTERS, TO UPDATE        */
/* ATTRIBUTES BASED ON EITHER A CONSTANT VALUE OR A REGISTER VALUE, */
/* A DIVERSE SET OF OPERATIONS ON REGISTERS ARE AVAILABLE AS WELL.  */
/* IT IS ALSO POSSIBLE TO PERFORM JUMPS WITHIN THE INSTRUCTIONS THAT*/
/* BELONGS TO THE SECOND PHASE. ALSO SUBROUTINES CAN BE CALLED IN   */
/* THIS PHASE. THE THIRD PHASE IS TO AGAIN READ ATTRIBUTES AND      */
/* FINALLY THE FOURTH PHASE READS SELECTED REGISTERS AND SEND THEM  */
/* TO THE CLIENT APPLICATION.                                       */
/* THERE IS A FIFTH REGION WHICH CONTAINS SUBROUTINES CALLABLE FROM */
/* THE INTERPRETER EXECUTION REGION.                                */
/* THE FIRST FIVE WORDS WILL GIVE THE LENGTH OF THE FIVEE REGIONS   */
/*                                                                  */
/* THIS MEANS THAT FROM THE APPLICATIONS POINT OF VIEW THE DATABASE */
/* CAN HANDLE SUBROUTINE CALLS WHERE THE CODE IS SENT IN THE REQUEST*/
/* THE RETURN PARAMETERS ARE FIXED AND CAN EITHER BE GENERATED      */
/* BEFORE THE EXECUTION OF THE ROUTINE OR AFTER.                    */
/*                                                                  */
/* IN LATER VERSIONS WE WILL ADD MORE THINGS LIKE THE POSSIBILITY   */
/* TO ALLOCATE MEMORY AND USE THIS AS LOCAL STORAGE. IT IS ALSO     */
/* IMAGINABLE TO HAVE SPECIAL ROUTINES THAT CAN PERFORM CERTAIN     */
/* OPERATIONS ON BLOB'S DEPENDENT ON WHAT THE BLOB REPRESENTS.      */
/*                                                                  */
/*                                                                  */
/*       -----------------------------------------                  */
/*       +   INITIAL READ REGION                 +                  */
/*       -----------------------------------------                  */
/*       +   INTERPRETED EXECUTE  REGION         +                  */
/*       -----------------------------------------                  */
/*       +   FINAL UPDATE REGION                 +                  */
/*       -----------------------------------------                  */
/*       +   FINAL READ REGION                   +                  */
/*       -----------------------------------------                  */
/*       +   SUBROUTINE REGION                   +                  */
/*       -----------------------------------------                  */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ----------------- INTERPRETED EXECUTION  ----------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::interpreterStartLab(Signal* signal,
                               Page* const pagePtr,
                               Uint32 TupHeadOffset) 
{
  Operationrec *  const regOperPtr = operPtr.p;
  Uint32 RtotalLen;
  int TnoDataRW;

  Uint32 RinitReadLen = cinBuffer[0];
  Uint32 RexecRegionLen = cinBuffer[1];
  Uint32 RfinalUpdateLen = cinBuffer[2];
  Uint32 RfinalRLen = cinBuffer[3];
  Uint32 RsubLen = cinBuffer[4];

  Uint32 RattrinbufLen = regOperPtr->attrinbufLen;
  const BlockReference sendBref = regOperPtr->recBlockref;

  Uint32 * dst = &signal->theData[25];
  Uint32 dstLen = (sizeof(signal->theData) / 4) - 25;
  const Uint32 node = refToNode(sendBref);
  if(node != 0 && node != getOwnNodeId()) {
    ;
  } else {
    jam();
    /**
     * execute direct
     */
    dst = &signal->theData[3];
    dstLen = (sizeof(signal->theData) / 4) - 3;
  }
  
  RtotalLen = RinitReadLen;
  RtotalLen += RexecRegionLen;
  RtotalLen += RfinalUpdateLen;
  RtotalLen += RfinalRLen;
  RtotalLen += RsubLen;

  Uint32 RattroutCounter = 0;
  Uint32 RinstructionCounter = 5;
  Uint32 RlogSize = 0;

  if (((RtotalLen + 5) == RattrinbufLen) &&
      (RattrinbufLen >= 5) &&
      (RattrinbufLen < ZATTR_BUFFER_SIZE)) {
    /* ---------------------------------------------------------------- */
    // We start by checking consistency. We must have the first five
    // words of the ATTRINFO to give us the length of the regions. The
    // size of these regions must be the same as the total ATTRINFO
    // length and finally the total length must be within the limits.
    /* ---------------------------------------------------------------- */

    if (RinitReadLen > 0) {
      jam();
      /* ---------------------------------------------------------------- */
      // The first step that can be taken in the interpreter is to read
      // data of the tuple before any updates have been applied.
      /* ---------------------------------------------------------------- */
      TnoDataRW = readAttributes(pagePtr,
				 TupHeadOffset,
				 &cinBuffer[5],
				 RinitReadLen,
				 &dst[0],
				 dstLen,
                                 false);
      if (TnoDataRW != -1) {
	RattroutCounter = TnoDataRW;
	RinstructionCounter += RinitReadLen;
      } else {
	jam();
	tupkeyErrorLab(signal);
	return -1;
      }//if
    }//if
    if (RexecRegionLen > 0) {
      jam();
      /* ---------------------------------------------------------------- */
      // The next step is the actual interpreted execution. This executes
      // a register-based virtual machine which can read and write attributes
      // to and from registers.
      /* ---------------------------------------------------------------- */
      Uint32 RsubPC = RinstructionCounter + RfinalUpdateLen + RfinalRLen;     
      TnoDataRW = interpreterNextLab(signal,
				     pagePtr,
				     TupHeadOffset,
				     &clogMemBuffer[0],
				     &cinBuffer[RinstructionCounter],
				     RexecRegionLen,
				     &cinBuffer[RsubPC],
				     RsubLen,
				     &coutBuffer[0],
				     sizeof(coutBuffer) / 4);
      if (TnoDataRW != -1) {
	RinstructionCounter += RexecRegionLen;
	RlogSize = TnoDataRW;
      } else {
	jam();
	return -1;
      }//if
    }//if
    if (RfinalUpdateLen > 0) {
      jam();
      /* ---------------------------------------------------------------- */
      // We can also apply a set of updates without any conditions as part
      // of the interpreted execution.
      /* ---------------------------------------------------------------- */
      if (regOperPtr->optype == ZUPDATE) {
	TnoDataRW = updateAttributes(pagePtr,
				     TupHeadOffset,
				     &cinBuffer[RinstructionCounter],
				     RfinalUpdateLen);
	if (TnoDataRW != -1) {
	  MEMCOPY_NO_WORDS(&clogMemBuffer[RlogSize],
			   &cinBuffer[RinstructionCounter],
			   RfinalUpdateLen);
	  RinstructionCounter += RfinalUpdateLen;
	  RlogSize += RfinalUpdateLen;
	} else {
	  jam();
	  tupkeyErrorLab(signal);
	  return -1;
	}//if
      } else {
	return TUPKEY_abort(signal, 19);
      }//if
    }//if
    if (RfinalRLen > 0) {
      jam();
      /* ---------------------------------------------------------------- */
      // The final action is that we can also read the tuple after it has
      // been updated.
      /* ---------------------------------------------------------------- */
      TnoDataRW = readAttributes(pagePtr,
				 TupHeadOffset,
				 &cinBuffer[RinstructionCounter],
				 RfinalRLen,
				 &dst[RattroutCounter],
				 (dstLen - RattroutCounter),
                                 false);
      if (TnoDataRW != -1) {
	RattroutCounter += TnoDataRW;
      } else {
	jam();
	tupkeyErrorLab(signal);
	return -1;
      }//if
    }//if
    regOperPtr->logSize = RlogSize;
    regOperPtr->attroutbufLen = RattroutCounter;
    sendReadAttrinfo(signal, RattroutCounter, regOperPtr);
    if (RlogSize > 0) {
      sendLogAttrinfo(signal, RlogSize, regOperPtr);
    }//if
    return 0;
  } else {
    return TUPKEY_abort(signal, 22);
  }//if
}//Dbtup::interpreterStartLab()

/* ---------------------------------------------------------------- */
/*       WHEN EXECUTION IS INTERPRETED WE NEED TO SEND SOME ATTRINFO*/
/*       BACK TO LQH FOR LOGGING AND SENDING TO BACKUP AND STANDBY  */
/*       NODES.                                                     */
/*       INPUT:  LOG_ATTRINFOPTR         WHERE TO FETCH DATA FROM   */
/*               TLOG_START              FIRST INDEX TO LOG         */
/*               TLOG_END                LAST INDEX + 1 TO LOG      */
/* ---------------------------------------------------------------- */
void Dbtup::sendLogAttrinfo(Signal* signal,
                            Uint32 TlogSize,
                            Operationrec *  const regOperPtr)

{
  Uint32 TbufferIndex = 0;
  signal->theData[0] = regOperPtr->userpointer;
  while (TlogSize > 22) {
    MEMCOPY_NO_WORDS(&signal->theData[3],
                     &clogMemBuffer[TbufferIndex],
                     22);
    EXECUTE_DIRECT(refToBlock(regOperPtr->userblockref), 
                   GSN_TUP_ATTRINFO, signal, 25);
    TbufferIndex += 22;
    TlogSize -= 22;
  }//while
  MEMCOPY_NO_WORDS(&signal->theData[3],
                   &clogMemBuffer[TbufferIndex],
                   TlogSize);
  EXECUTE_DIRECT(refToBlock(regOperPtr->userblockref), 
                 GSN_TUP_ATTRINFO, signal, 3 + TlogSize);
}//Dbtup::sendLogAttrinfo()

inline
Uint32 
brancher(Uint32 TheInstruction, Uint32 TprogramCounter)
{         
  Uint32 TbranchDirection = TheInstruction >> 31;
  Uint32 TbranchLength = (TheInstruction >> 16) & 0x7fff;
  TprogramCounter--;
  if (TbranchDirection == 1) {
    jam();
    /* ---------------------------------------------------------------- */
    /*       WE JUMP BACKWARDS.                                         */
    /* ---------------------------------------------------------------- */
    return (TprogramCounter - TbranchLength);
  } else {
    jam();
    /* ---------------------------------------------------------------- */
    /*       WE JUMP FORWARD.                                           */
    /* ---------------------------------------------------------------- */
    return (TprogramCounter + TbranchLength);
  }//if
}//brancher()

int Dbtup::interpreterNextLab(Signal* signal,
                              Page* const pagePtr,
                              Uint32 TupHeadOffset,
                              Uint32* logMemory,
                              Uint32* mainProgram,
                              Uint32 TmainProgLen,
                              Uint32* subroutineProg,
                              Uint32 TsubroutineLen,
			      Uint32 * tmpArea,
			      Uint32 tmpAreaSz)
{
  register Uint32* TcurrentProgram = mainProgram;
  register Uint32 TcurrentSize = TmainProgLen;
  register Uint32 RnoOfInstructions = 0;
  register Uint32 TprogramCounter = 0;
  register Uint32 theInstruction;
  register Uint32 theRegister;
  Uint32 TdataWritten = 0;
  Uint32 RstackPtr = 0;
  union {
    Uint32 TregMemBuffer[32];
    Uint64 Tdummy[16];
  };
  Uint32 TstackMemBuffer[32];

  /* ---------------------------------------------------------------- */
  // Initialise all 8 registers to contain the NULL value.
  // In this version we can handle 32 and 64 bit unsigned integers.
  // They are handled as 64 bit values. Thus the 32 most significant
  // bits are zeroed for 32 bit values.
  /* ---------------------------------------------------------------- */
  TregMemBuffer[0] = 0;
  TregMemBuffer[4] = 0;
  TregMemBuffer[8] = 0;
  TregMemBuffer[12] = 0;
  TregMemBuffer[16] = 0;
  TregMemBuffer[20] = 0;
  TregMemBuffer[24] = 0;
  TregMemBuffer[28] = 0;
  Uint32 tmpHabitant = ~0;

  while (RnoOfInstructions < 8000) {
    /* ---------------------------------------------------------------- */
    /* EXECUTE THE NEXT INTERPRETER INSTRUCTION.                        */
    /* ---------------------------------------------------------------- */
    RnoOfInstructions++;
    theInstruction = TcurrentProgram[TprogramCounter];
    theRegister = Interpreter::getReg1(theInstruction) << 2;
    if (TprogramCounter < TcurrentSize) {
      TprogramCounter++;
      switch (Interpreter::getOpCode(theInstruction)) {
      case Interpreter::READ_ATTR_INTO_REG:
	jam();
	/* ---------------------------------------------------------------- */
	// Read an attribute from the tuple into a register.
	// While reading an attribute we allow the attribute to be an array
	// as long as it fits in the 64 bits of the register.
	/* ---------------------------------------------------------------- */
	{
	  Uint32 theAttrinfo = theInstruction;
	  int TnoDataRW= readAttributes(pagePtr,
					TupHeadOffset,
					&theAttrinfo,
					(Uint32)1,
					&TregMemBuffer[theRegister],
					(Uint32)3,
					false);
	  if (TnoDataRW == 2) {
	    /* ------------------------------------------------------------- */
	    // Two words read means that we get the instruction plus one 32 
	    // word read. Thus we set the register to be a 32 bit register.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister] = 0x50;
            * (Int64*)(TregMemBuffer+theRegister+2) = TregMemBuffer[theRegister+1];
	  } else if (TnoDataRW == 3) {
	    /* ------------------------------------------------------------- */
	    // Three words read means that we get the instruction plus two 
	    // 32 words read. Thus we set the register to be a 64 bit register.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister] = 0x60;
            TregMemBuffer[theRegister+3] = TregMemBuffer[theRegister+2];
            TregMemBuffer[theRegister+2] = TregMemBuffer[theRegister+1];
	  } else if (TnoDataRW == 1) {
	    /* ------------------------------------------------------------- */
	    // One word read means that we must have read a NULL value. We set
	    // the register to indicate a NULL value.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister] = 0;
	    TregMemBuffer[theRegister + 2] = 0;
	    TregMemBuffer[theRegister + 3] = 0;
	  } else if (TnoDataRW == -1) {
	    jam();
	    tupkeyErrorLab(signal);
	    return -1;
	  } else {
	    /* ------------------------------------------------------------- */
	    // Any other return value from the read attribute here is not 
	    // allowed and will lead to a system crash.
	    /* ------------------------------------------------------------- */
	    ndbrequire(false);
	  }//if
	  break;
	}

      case Interpreter::WRITE_ATTR_FROM_REG:
	jam();
	{
	  Uint32 TattrId = theInstruction >> 16;
	  Uint32 TattrDescrIndex = tabptr.p->tabDescriptor +
	    (TattrId << ZAD_LOG_SIZE);
	  Uint32 TattrDesc1 = tableDescriptor[TattrDescrIndex].tabDescr;
	  Uint32 TregType = TregMemBuffer[theRegister];

	  /* --------------------------------------------------------------- */
	  // Calculate the number of words of this attribute.
	  // We allow writes into arrays as long as they fit into the 64 bit
	  // register size.
	  //TEST_MR See to that TattrNoOfWords can be 
	  // read faster from attribute description.
	  /* --------------------------------------------------------------- */
	  Uint32 TarraySize = (TattrDesc1 >> 16);
	  Uint32 TattrLogLen = (TattrDesc1 >> 4) & 0xf;
	  Uint32 TattrNoOfBits = TarraySize << TattrLogLen;
	  Uint32 TattrNoOfWords = (TattrNoOfBits + 31) >> 5;
	  Uint32 Toptype = operPtr.p->optype;

	  Uint32 TdataForUpdate[3];
	  Uint32 Tlen;

	  AttributeHeader& ah = AttributeHeader::init(&TdataForUpdate[0], 
						      TattrId, TattrNoOfWords);
	  TdataForUpdate[1] = TregMemBuffer[theRegister + 2];
	  TdataForUpdate[2] = TregMemBuffer[theRegister + 3];
	  Tlen = TattrNoOfWords + 1;
	  if (Toptype == ZUPDATE) {
	    if (TattrNoOfWords <= 2) {
	      if (TregType == 0) {
		/* --------------------------------------------------------- */
		// Write a NULL value into the attribute
		/* --------------------------------------------------------- */
		ah.setNULL();
		Tlen = 1;
	      }//if
	      int TnoDataRW= updateAttributes(pagePtr,
					      TupHeadOffset,
					      &TdataForUpdate[0],
					      Tlen);
	      if (TnoDataRW != -1) {
		/* --------------------------------------------------------- */
		// Write the written data also into the log buffer so that it 
		// will be logged.
		/* --------------------------------------------------------- */
		logMemory[TdataWritten + 0] = TdataForUpdate[0];
		logMemory[TdataWritten + 1] = TdataForUpdate[1];
		logMemory[TdataWritten + 2] = TdataForUpdate[2];
		TdataWritten += Tlen;
	      } else {
		tupkeyErrorLab(signal);
		return -1;
	      }//if
	    } else {
	      return TUPKEY_abort(signal, 15);
	    }//if
	  } else {
	    return TUPKEY_abort(signal, 16);
	  }//if
	  break;
	}

      case Interpreter::LOAD_CONST_NULL:
	jam();
	TregMemBuffer[theRegister] = 0;	/* NULL INDICATOR */
	break;

      case Interpreter::LOAD_CONST16:
	jam();
	TregMemBuffer[theRegister] = 0x50;	/* 32 BIT UNSIGNED CONSTANT */
	* (Int64*)(TregMemBuffer+theRegister+2) = theInstruction >> 16;
	break;

      case Interpreter::LOAD_CONST32:
	jam();
	TregMemBuffer[theRegister] = 0x50;	/* 32 BIT UNSIGNED CONSTANT */
	* (Int64*)(TregMemBuffer+theRegister+2) = * 
	  (TcurrentProgram+TprogramCounter);
	TprogramCounter++;
	break;

      case Interpreter::LOAD_CONST64:
	jam();
	TregMemBuffer[theRegister] = 0x60;	/* 64 BIT UNSIGNED CONSTANT */
        TregMemBuffer[theRegister + 2 ] = * (TcurrentProgram + TprogramCounter++);
        TregMemBuffer[theRegister + 3 ] = * (TcurrentProgram + TprogramCounter++);
	break;

      case Interpreter::ADD_REG_REG:
	jam();
	{
	  Uint32 TrightRegister = Interpreter::getReg2(theInstruction) << 2;
	  Uint32 TdestRegister = Interpreter::getReg3(theInstruction) << 2;

	  Uint32 TrightType = TregMemBuffer[TrightRegister];
	  Int64 Tright0 = * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  

	  Uint32 TleftType = TregMemBuffer[theRegister];
	  Int64 Tleft0 = * (Int64*)(TregMemBuffer + theRegister + 2);
         
	  if ((TleftType | TrightType) != 0) {
	    Uint64 Tdest0 = Tleft0 + Tright0;
	    * (Int64*)(TregMemBuffer+TdestRegister+2) = Tdest0;
	    TregMemBuffer[TdestRegister] = 0x60;
	  } else {
	    return TUPKEY_abort(signal, 20);
	  }
	  break;
	}

      case Interpreter::SUB_REG_REG:
	jam();
	{
	  Uint32 TrightRegister = Interpreter::getReg2(theInstruction) << 2;
	  Uint32 TdestRegister = Interpreter::getReg3(theInstruction) << 2;

	  Uint32 TrightType = TregMemBuffer[TrightRegister];
	  Int64 Tright0 = * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType = TregMemBuffer[theRegister];
	  Int64 Tleft0 = * (Int64*)(TregMemBuffer + theRegister + 2);
         
	  if ((TleftType | TrightType) != 0) {
	    Int64 Tdest0 = Tleft0 - Tright0;
	    * (Int64*)(TregMemBuffer+TdestRegister+2) = Tdest0;
	    TregMemBuffer[TdestRegister] = 0x60;
	  } else {
	    return TUPKEY_abort(signal, 20);
	  }
	  break;
	}

      case Interpreter::BRANCH:
	TprogramCounter = brancher(theInstruction, TprogramCounter);
	break;

      case Interpreter::BRANCH_REG_EQ_NULL:
	if (TregMemBuffer[theRegister] != 0) {
	  jam();
	  continue;
	} else {
	  jam();
	  TprogramCounter = brancher(theInstruction, TprogramCounter);
	}//if
	break;

      case Interpreter::BRANCH_REG_NE_NULL:
	if (TregMemBuffer[theRegister] == 0) {
	  jam();
	  continue;
	} else {
	  jam();
	  TprogramCounter = brancher(theInstruction, TprogramCounter);
	}//if
	break;


      case Interpreter::BRANCH_EQ_REG_REG:
	{
	  Uint32 TrightRegister = Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TleftType = TregMemBuffer[theRegister];
	  Uint32 Tleft0    = TregMemBuffer[theRegister + 2];
	  Uint32 Tleft1    = TregMemBuffer[theRegister + 3];

	  Uint32 TrightType = TregMemBuffer[TrightRegister];
	  Uint32 Tright0 = TregMemBuffer[TrightRegister + 2];
	  Uint32 Tright1 = TregMemBuffer[TrightRegister + 3];
	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if ((Tleft0 == Tright0) && (Tleft1 == Tright1)) {
	      TprogramCounter = brancher(theInstruction, TprogramCounter);
	    }//if
	  } else {
	    return TUPKEY_abort(signal, 23);
	  }//if
	  break;
	}

      case Interpreter::BRANCH_NE_REG_REG:
	{
	  Uint32 TrightRegister = Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TleftType = TregMemBuffer[theRegister];
	  Uint32 Tleft0    = TregMemBuffer[theRegister + 2];
	  Uint32 Tleft1    = TregMemBuffer[theRegister + 3];

	  Uint32 TrightType = TregMemBuffer[TrightRegister];
	  Uint32 Tright0 = TregMemBuffer[TrightRegister + 2];
	  Uint32 Tright1 = TregMemBuffer[TrightRegister + 3];
	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if ((Tleft0 != Tright0) || (Tleft1 != Tright1)) {
	      TprogramCounter = brancher(theInstruction, TprogramCounter);
	    }//if
	  } else {
	    return TUPKEY_abort(signal, 24);
	  }//if
	  break;
	}

      case Interpreter::BRANCH_LT_REG_REG:
	{
	  Uint32 TrightRegister = Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TrightType = TregMemBuffer[TrightRegister];
	  Int64 Tright0 = * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType = TregMemBuffer[theRegister];
	  Int64 Tleft0 = * (Int64*)(TregMemBuffer + theRegister + 2);
         

	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if (Tleft0 < Tright0) {
	      TprogramCounter = brancher(theInstruction, TprogramCounter);
	    }//if
	  } else {
	    return TUPKEY_abort(signal, 24);
	  }//if
	  break;
	}

      case Interpreter::BRANCH_LE_REG_REG:
	{
	  Uint32 TrightRegister = Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TrightType = TregMemBuffer[TrightRegister];
	  Int64 Tright0 = * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType = TregMemBuffer[theRegister];
	  Int64 Tleft0 = * (Int64*)(TregMemBuffer + theRegister + 2);
	  

	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if (Tleft0 <= Tright0) {
	      TprogramCounter = brancher(theInstruction, TprogramCounter);
	    }//if
	  } else {
	    return TUPKEY_abort(signal, 26);
	  }//if
	  break;
	}

      case Interpreter::BRANCH_GT_REG_REG:
	{
	  Uint32 TrightRegister = Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TrightType = TregMemBuffer[TrightRegister];
	  Int64 Tright0 = * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType = TregMemBuffer[theRegister];
	  Int64 Tleft0 = * (Int64*)(TregMemBuffer + theRegister + 2);
	  

	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if (Tleft0 > Tright0){
	      TprogramCounter = brancher(theInstruction, TprogramCounter);
	    }//if
	  } else {
	    return TUPKEY_abort(signal, 27);
	  }//if
	  break;
	}

      case Interpreter::BRANCH_GE_REG_REG:
	{
	  Uint32 TrightRegister = Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TrightType = TregMemBuffer[TrightRegister];
	  Int64 Tright0 = * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType = TregMemBuffer[theRegister];
	  Int64 Tleft0 = * (Int64*)(TregMemBuffer + theRegister + 2);
	  

	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if (Tleft0 >= Tright0){
	      TprogramCounter = brancher(theInstruction, TprogramCounter);
	    }//if
	  } else {
	    return TUPKEY_abort(signal, 28);
	  }//if
	  break;
	}

      case Interpreter::BRANCH_ATTR_OP_ARG:{
	jam();
	Uint32 cond = Interpreter::getBinaryCondition(theInstruction);
	Uint32 diff = Interpreter::getArrayLengthDiff(theInstruction);
	Uint32 vchr = Interpreter::isVarchar(theInstruction);
        Uint32 nopad =Interpreter::isNopad(theInstruction);
	Uint32 ins2 = TcurrentProgram[TprogramCounter];
	Uint32 attrId = Interpreter::getBranchCol_AttrId(ins2) << 16;
	Uint32 argLen = Interpreter::getBranchCol_Len(ins2);

	if(tmpHabitant != attrId){
	  Int32 TnoDataR = readAttributes(pagePtr,
					  TupHeadOffset,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (TnoDataR == -1) {
	    jam();
	    tupkeyErrorLab(signal);
	    return -1;
	  }
	  tmpHabitant = attrId;
	}
	
	AttributeHeader ah(tmpArea[0]);

        const char* s1 = (char*)&tmpArea[1];
        const char* s2 = (char*)&TcurrentProgram[TprogramCounter+1];
	Uint32 attrLen = (4 * ah.getDataSize()) - diff;
        if (vchr) {
#if NDB_VERSION_MAJOR >= 3
          bool vok = false;
          if (attrLen >= 2) {
            Uint32 vlen = (s1[0] << 8) | s1[1]; // big-endian
            s1 += 2;
            attrLen -= 2;
            if (attrLen >= vlen) {
              attrLen = vlen;
              vok = true;
            }
          }
          if (!vok) {
            terrorCode = ZREGISTER_INIT_ERROR;
            tupkeyErrorLab(signal);
            return -1;
          }
#else
          Uint32 tmp;
          if (attrLen >= 2) {
            unsigned char* ss = (unsigned char*)&s1[attrLen - 2];
            tmp = (ss[0] << 8) | ss[1];
            if (tmp <= attrLen - 2)
              attrLen = tmp;
          }
          // XXX handle bad data
#endif
        }
        bool res = false;

        switch ((Interpreter::BinaryCondition)cond) {
        case Interpreter::EQ:
          res = NdbSqlUtil::char_compare(s1, attrLen, s2, argLen, !nopad) == 0;
          break;
        case Interpreter::NE:
          res = NdbSqlUtil::char_compare(s1, attrLen, s2, argLen, !nopad) != 0;
          break;
        // note the condition is backwards
        case Interpreter::LT:
          res = NdbSqlUtil::char_compare(s1, attrLen, s2, argLen, !nopad) > 0;
          break;
        case Interpreter::LE:
          res = NdbSqlUtil::char_compare(s1, attrLen, s2, argLen, !nopad) >= 0;
          break;
        case Interpreter::GT:
          res = NdbSqlUtil::char_compare(s1, attrLen, s2, argLen, !nopad) < 0;
          break;
        case Interpreter::GE:
          res = NdbSqlUtil::char_compare(s1, attrLen, s2, argLen, !nopad) <= 0;
          break;
        case Interpreter::LIKE:
          res = NdbSqlUtil::char_like(s1, attrLen, s2, argLen, !nopad);
          break;
        case Interpreter::NOT_LIKE:
          res = ! NdbSqlUtil::char_like(s1, attrLen, s2, argLen, !nopad);
          break;
        // XXX handle invalid value
        }
#ifdef TRACE_INTERPRETER
	  ndbout_c("cond=%u diff=%d vc=%d nopad=%d attr(%d) = >%.*s<(%d) str=>%.*s<(%d) -> res = %d",
		   cond, diff, vchr, nopad,
		   attrId >> 16, attrLen, s1, attrLen, argLen, s2, argLen, res);
#endif
        if (res)
          TprogramCounter = brancher(theInstruction, TprogramCounter);
        else {
          Uint32 tmp = (Interpreter::mod4(argLen) >> 2) + 1;
          TprogramCounter += tmp;
        }
	break;
      }

      case Interpreter::BRANCH_ATTR_EQ_NULL:{
	jam();
	Uint32 ins2 = TcurrentProgram[TprogramCounter];
	Uint32 attrId = Interpreter::getBranchCol_AttrId(ins2) << 16;
	
	if(tmpHabitant != attrId){
	  Int32 TnoDataR = readAttributes(pagePtr,
					  TupHeadOffset,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (TnoDataR == -1) {
	    jam();
	    tupkeyErrorLab(signal);
	    return -1;
	  }
	  tmpHabitant = attrId;
	}
	
	AttributeHeader ah(tmpArea[0]);
	if(ah.isNULL()){
	  TprogramCounter = brancher(theInstruction, TprogramCounter);
	} else {
	  TprogramCounter ++;
	}
	break;
      }

      case Interpreter::BRANCH_ATTR_NE_NULL:{
	jam();
	Uint32 ins2 = TcurrentProgram[TprogramCounter];
	Uint32 attrId = Interpreter::getBranchCol_AttrId(ins2) << 16;
	
	if(tmpHabitant != attrId){
	  Int32 TnoDataR = readAttributes(pagePtr,
					  TupHeadOffset,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (TnoDataR == -1) {
	    jam();
	    tupkeyErrorLab(signal);
	    return -1;
	  }
	  tmpHabitant = attrId;
	}
	
	AttributeHeader ah(tmpArea[0]);
	if(ah.isNULL()){
	  TprogramCounter ++;
	} else {
	  TprogramCounter = brancher(theInstruction, TprogramCounter);
	}
	break;
      }
	
      case Interpreter::EXIT_OK:
	jam();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_ok");
#endif
	return TdataWritten;

      case Interpreter::EXIT_OK_LAST:
	jam();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_ok_last");
#endif
	operPtr.p->lastRow = 1;
	return TdataWritten;
	
      case Interpreter::EXIT_REFUSE:
	jam();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_nok");
#endif
	terrorCode = theInstruction >> 16;
	return TUPKEY_abort(signal, 29);

      case Interpreter::CALL:
	jam();
	RstackPtr++;
	if (RstackPtr < 32) {
	  TstackMemBuffer[RstackPtr] = TprogramCounter + 1;
	  TprogramCounter = theInstruction >> 16;
	  if (TprogramCounter < TsubroutineLen) {
	    TcurrentProgram = subroutineProg;
	    TcurrentSize = TsubroutineLen;
	  } else {
	    return TUPKEY_abort(signal, 30);
	  }//if
	} else {
	  return TUPKEY_abort(signal, 31);
	}//if
	break;

      case Interpreter::RETURN:
	jam();
	if (RstackPtr > 0) {
	  TprogramCounter = TstackMemBuffer[RstackPtr];
	  RstackPtr--;
	  if (RstackPtr == 0) {
	    jam();
	    /* ------------------------------------------------------------- */
	    // We are back to the main program.
	    /* ------------------------------------------------------------- */
	    TcurrentProgram = mainProgram;
	    TcurrentSize = TmainProgLen;
	  }//if
	} else {
	  return TUPKEY_abort(signal, 32);
	}//if
	break;

      default:
	return TUPKEY_abort(signal, 33);
      }//switch
    } else {
      return TUPKEY_abort(signal, 34);
    }//if
  }//while
  return TUPKEY_abort(signal, 35);
}//Dbtup::interpreterNextLab()


