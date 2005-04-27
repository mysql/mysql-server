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

#define ljam() { jamLine(9000 + __LINE__); }
#define ljamEntry() { jamEntryLine(9000 + __LINE__); }

void Dbtup::freeAllAttrBuffers(Operationrec*  const regOperPtr)
{
  if (regOperPtr->storedProcedureId == ZNIL) {
    ljam();
    freeAttrinbufrec(regOperPtr->firstAttrinbufrec);
  } else {
    StoredProcPtr storedPtr;
    c_storedProcPool.getPtr(storedPtr, (Uint32)regOperPtr->storedProcedureId);
    ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);
    ljam();
    storedPtr.p->storedCounter--;
    regOperPtr->storedProcedureId = ZNIL;
  }//if
  regOperPtr->firstAttrinbufrec = RNIL;
  regOperPtr->lastAttrinbufrec = RNIL;
}//Dbtup::freeAllAttrBuffers()

void Dbtup::freeAttrinbufrec(Uint32 anAttrBuf) 
{
  Uint32 Ttemp;
  AttrbufrecPtr localAttrBufPtr;
  Uint32 RnoFree = cnoFreeAttrbufrec;
  localAttrBufPtr.i = anAttrBuf;
  while (localAttrBufPtr.i != RNIL) {
    ljam();
    ptrCheckGuard(localAttrBufPtr, cnoOfAttrbufrec, attrbufrec);
    Ttemp = localAttrBufPtr.p->attrbuf[ZBUF_NEXT];
    localAttrBufPtr.p->attrbuf[ZBUF_NEXT] = cfirstfreeAttrbufrec;
    cfirstfreeAttrbufrec = localAttrBufPtr.i;
    localAttrBufPtr.i = Ttemp;
    RnoFree++;
  }//if
  cnoFreeAttrbufrec = RnoFree;
}//Dbtup::freeAttrinbufrec()

/* ----------------------------------------------------------------- */
/* ----------- ABORT THIS PART OF THE TRANSACTION ------------------ */
/* ----------------------------------------------------------------- */
void Dbtup::execTUP_ABORTREQ(Signal* signal) 
{
  OperationrecPtr regOperPtr;
  FragrecordPtr regFragPtr;
  TablerecPtr regTabPtr;

  ljamEntry();
  regOperPtr.i = signal->theData[0];
  ptrCheckGuard(regOperPtr, cnoOfOprec, operationrec);
  ndbrequire((regOperPtr.p->transstate == STARTED) ||
             (regOperPtr.p->transstate == TOO_MUCH_AI) ||
             (regOperPtr.p->transstate == ERROR_WAIT_TUPKEYREQ) ||
             (regOperPtr.p->transstate == IDLE));
  if (regOperPtr.p->optype == ZREAD) {
    ljam();
    freeAllAttrBuffers(regOperPtr.p);
    initOpConnection(regOperPtr.p, 0);
    return;
  }//if

  regTabPtr.i = regOperPtr.p->tableRef;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  regFragPtr.i = regOperPtr.p->fragmentPtr;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);

  // XXX should be integrated into the code that comes after
  if (!regTabPtr.p->tuxCustomTriggers.isEmpty() &&
      regOperPtr.p->tupleState == NO_OTHER_OP) {
    ljam();
    executeTuxAbortTriggers(signal,
                            regOperPtr.p,
                            regTabPtr.p);
    OperationrecPtr loopOpPtr;
    loopOpPtr.i = regOperPtr.p->prevActiveOp;
    while (loopOpPtr.i != RNIL) {
      ljam();
      ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
      if (loopOpPtr.p->tupleState != ALREADY_ABORTED) {
        ljam();
        executeTuxAbortTriggers(signal,
                                loopOpPtr.p,
                                regTabPtr.p);
      }
      loopOpPtr.i = loopOpPtr.p->prevActiveOp;
    }
  }

  Uint32 prevActiveOp = regOperPtr.p->prevActiveOp;
  removeActiveOpList(regOperPtr.p);
  if (regOperPtr.p->tupleState == NO_OTHER_OP) {
    if (prevActiveOp == RNIL) {
      ljam();
      abortUpdate(signal, regOperPtr.p, regFragPtr.p, regTabPtr.p);
    } else { //prevActiveOp != RNIL
      setTupleStateOnPreviousOps(prevActiveOp);
      if (regOperPtr.p->optype == ZDELETE) {
        ljam();
        OperationrecPtr prevOpPtr;
        prevOpPtr.i = prevActiveOp;
        ptrCheckGuard(prevOpPtr, cnoOfOprec, operationrec);
        ndbrequire(prevOpPtr.p->realPageIdC != RNIL);
        ndbrequire(prevOpPtr.p->optype == ZINSERT);
        abortUpdate(signal, prevOpPtr.p, regFragPtr.p, regTabPtr.p);
      } else {
        jam();
        abortUpdate(signal, regOperPtr.p, regFragPtr.p, regTabPtr.p);
      }//if
    }//if
  } else {
    ndbrequire(regOperPtr.p->tupleState == ALREADY_ABORTED);
    commitUpdate(signal, regOperPtr.p, regFragPtr.p, regTabPtr.p);
  }//if
  initOpConnection(regOperPtr.p, regFragPtr.p);
}//execTUP_ABORTREQ()

void Dbtup::setTupleStateOnPreviousOps(Uint32 prevOpIndex)
{
  OperationrecPtr loopOpPtr;
  loopOpPtr.i = prevOpIndex;
  do {
    ljam();
    ptrCheckGuard(loopOpPtr, cnoOfOprec, operationrec);
    loopOpPtr.p->tupleState = ALREADY_ABORTED;
    loopOpPtr.i = loopOpPtr.p->prevActiveOp;
  } while (loopOpPtr.i != RNIL);
}//Dbtup::setTupleStateOnPreviousOps()

/* ---------------------------------------------------------------- */
/* ------------ PERFORM AN ABORT OF AN UPDATE OPERATION ----------- */
/* ---------------------------------------------------------------- */
void Dbtup::abortUpdate(Signal* signal,
                        Operationrec*  const regOperPtr,
                        Fragrecord* const regFragPtr,
                        Tablerec* const regTabPtr)
{
   /* RESTORE THE ORIGINAL DATA */
   /* THE OPER_PTR ALREADY CONTAINS BOTH THE PAGE AND THE COPY PAGE */
  if (regOperPtr->realPageIdC != RNIL) {
    ljam();
         /***********************/
         /* CHECKPOINT SPECIFIC */
         /***********************/
    if (isUndoLoggingNeeded(regFragPtr, regOperPtr->fragPageIdC)) {
      if (regOperPtr->undoLogged) {
        ljam();
/* ---------------------------------------------------------------- */
/*       THE UPDATE WAS MADE AFTER THE LOCAL CHECKPOINT STARTED.    */
/*       THUS THE ORIGINAL TUPLE WILL BE RESTORED BY A LOG RECORD   */
/*       CREATED WHEN UPDATING. THUS IT IS ENOUGH TO LOG THE UNDO   */
/*       OF THE COPY RELEASE == INSERT THE COPY TUPLE HEADER WITH   */
/*       NO DATA.                                                   */
/* ---------------------------------------------------------------- */
        cprAddUndoLogRecord(signal,
                            ZLCPR_TYPE_INSERT_TH_NO_DATA,
                            regOperPtr->fragPageIdC,
                            regOperPtr->pageIndexC,
                            regOperPtr->tableRef,
                            regOperPtr->fragId,
                            regFragPtr->checkpointVersion);
      } else {
        ljam();
/* ---------------------------------------------------------------- */
/*       THE UPDATE WAS MADE BEFORE THE LOCAL CHECKPOINT STARTED.   */
/*       THE TUPLE WILL THUS BE RESTORED BY COPYING FROM THE COPY.  */
/*       THUS WE DO NOT NEED TO RESTORE THE DATA IN THE ORIGINAL.   */
/*       WE DO HOWEVER NEED TO ENSURE THAT THE COPY CONTAINS THE    */
/*       CORRECT DATA.                                              */
/* ---------------------------------------------------------------- */
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
    Uint32 rpid = regOperPtr->realPageId;
    Uint32 rpid_copy = regOperPtr->realPageIdC;
    Uint32 offset = regOperPtr->pageOffset;
    Uint32 offset_copy = regOperPtr->pageOffsetC;
    Uint32 tuple_size = regTabPtr->tupheadsize;
    Uint32 end = offset + tuple_size;
    Uint32 end_copy = offset_copy + tuple_size;
    ndbrequire(rpid < cnoOfPage &&
               rpid_copy < cnoOfPage &&
               end <= ZWORDS_ON_PAGE &&
               end_copy <= ZWORDS_ON_PAGE);
    void* Tdestination = (void*)&page[rpid].pageWord[offset + 1];
    const void* Tsource = (void*)&page[rpid_copy].pageWord[offset_copy + 1];
    MEMCOPY_NO_WORDS(Tdestination, Tsource, (tuple_size - 1));
    {
      PagePtr pagePtr;

      pagePtr.i = rpid_copy;
      ptrAss(pagePtr, page);
      freeTh(regFragPtr,
             regTabPtr,
             signal,
             pagePtr.p,
             offset_copy);
    }
    regOperPtr->realPageIdC = RNIL;
    regOperPtr->fragPageIdC = RNIL;
    regOperPtr->pageOffsetC = ZNIL;
    regOperPtr->pageIndexC = ZNIL;
  }//if
}//Dbtup::abortUpdate()

/* **************************************************************** */
/* ********************** TRANSACTION ERROR MODULE **************** */
/* **************************************************************** */
int Dbtup::TUPKEY_abort(Signal* signal, int error_type)
{
  switch(error_type) {
  case 0:
    ndbrequire(false);
    break;
// Not used currently

  case 1:
//tmupdate_alloc_error:
    ljam();
    break;

  case 2:
    ndbrequire(false);
    break;
// Not used currently

    break;

  case 3:
//tmupdate_alloc_error:
    ljam();
    break;

  case 4:
//Trying to read non-existing attribute identity
    ljam();
    terrorCode = ZATTRIBUTE_ID_ERROR;
    break;

  case 6:
    ljam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    break;

  case 7:
    ljam();
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    break;

  case 8:
    ljam();
    terrorCode = ZATTR_INTERPRETER_ERROR;
    break;

  case 9:
    ljam();
//Trying to read non-existing attribute identity
    ljam();
    terrorCode = ZATTRIBUTE_ID_ERROR;
    break;

  case 11:
    ljam();
    terrorCode = ZATTR_INTERPRETER_ERROR;
    break;

  case 12:
    ljam();
    ndbrequire(false);
    break;

  case 13:
    ljam();
    ndbrequire(false);
    break;

  case 14:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 15:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 16:
    ljam();
    terrorCode = ZTRY_TO_UPDATE_ERROR;
    break;

  case 17:
    ljam();
    terrorCode = ZNO_ILLEGAL_NULL_ATTR;
    break;

  case 18:
    ljam();
    terrorCode = ZNOT_NULL_ATTR;
    break;

  case 19:
    ljam();
    terrorCode = ZTRY_TO_UPDATE_ERROR;
    break;

  case 20:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 21:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 22:
    ljam();
    terrorCode = ZTOTAL_LEN_ERROR;
    break;

  case 23:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 24:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 25:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 26:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 27:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 28:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 29:
    ljam();
    break;

  case 30:
    ljam();
    terrorCode = ZCALL_ERROR;
    break;

  case 31:
    ljam();
    terrorCode = ZSTACK_OVERFLOW_ERROR;
    break;

  case 32:
    ljam();
    terrorCode = ZSTACK_UNDERFLOW_ERROR;
    break;

  case 33:
    ljam();
    terrorCode = ZNO_INSTRUCTION_ERROR;
    break;

  case 34:
    ljam();
    terrorCode = ZOUTSIDE_OF_PROGRAM_ERROR;
    break;

  case 35:
    ljam();
    terrorCode = ZTOO_MANY_INSTRUCTIONS_ERROR;
    break;

  case 36:
    ljam();
    terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
    break;

  case 37:
    ljam();
    terrorCode = ZTEMPORARY_RESOURCE_FAILURE;
    break;

  case 38:
    ljam();
    terrorCode = ZTEMPORARY_RESOURCE_FAILURE;
    break;

  case 39:
    ljam();
    if (operPtr.p->transstate == TOO_MUCH_AI) {
      ljam();
      terrorCode = ZTOO_MUCH_ATTRINFO_ERROR;
    } else if (operPtr.p->transstate == ERROR_WAIT_TUPKEYREQ) {
      ljam();
      terrorCode = ZSEIZE_ATTRINBUFREC_ERROR;
    } else {
      ndbrequire(false);
    }//if
    break;

  default:
    ndbrequire(false);
    break;
  }//switch
  tupkeyErrorLab(signal);
  return -1;
}//Dbtup::TUPKEY_abort()

void Dbtup::tupkeyErrorLab(Signal* signal) 
{
  Operationrec * const regOperPtr = operPtr.p;

  freeAllAttrBuffers(regOperPtr);
  abortUpdate(signal, regOperPtr, fragptr.p, tabptr.p);
  removeActiveOpList(regOperPtr);
  initOpConnection(regOperPtr, fragptr.p);
  regOperPtr->transstate = IDLE;
  regOperPtr->tupleState = NO_OTHER_OP;
  TupKeyRef * const tupKeyRef = (TupKeyRef *)signal->getDataPtrSend();  

  tupKeyRef->userRef = regOperPtr->userpointer;
  tupKeyRef->errorCode = terrorCode;
  sendSignal(regOperPtr->userblockref, GSN_TUPKEYREF, signal, 
             TupKeyRef::SignalLength, JBB);
  return;
}//Dbtup::tupkeyErrorLab()

