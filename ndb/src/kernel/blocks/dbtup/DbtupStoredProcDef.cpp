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

#define ljam() { jamLine(18000 + __LINE__); }
#define ljamEntry() { jamEntryLine(18000 + __LINE__); }

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ------------ADD/DROP STORED PROCEDURE MODULE ------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execSTORED_PROCREQ(Signal* signal) 
{
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;
  ljamEntry();
  regOperPtr.i = signal->theData[0];
  ptrCheckGuard(regOperPtr, cnoOfOprec, operationrec);
  regTabPtr.i = signal->theData[1];
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  Uint32 requestInfo = signal->theData[3];

  ndbrequire(regOperPtr.p->transstate == IDLE ||
             ((regOperPtr.p->transstate == ERROR_WAIT_STORED_PROCREQ) &&
             (requestInfo == ZSTORED_PROCEDURE_DELETE)));
  ndbrequire(regTabPtr.p->tableStatus == DEFINED);
  switch (requestInfo) {
  case ZSCAN_PROCEDURE:
    ljam();
    scanProcedure(signal,
                  regOperPtr.p,
                  signal->theData[4]);
    break;
  case ZCOPY_PROCEDURE:
    ljam();
    copyProcedure(signal, regTabPtr, regOperPtr.p);
    break;
  case ZSTORED_PROCEDURE_DELETE:
    ljam();
    deleteScanProcedure(signal, regOperPtr.p);
    break;
  default:
    ndbrequire(false);
  }//switch
}//Dbtup::execSTORED_PROCREQ()

void Dbtup::deleteScanProcedure(Signal* signal,
                                Operationrec* regOperPtr) 
{
  StoredProcPtr storedPtr;
  Uint32 storedProcId = signal->theData[4];
  c_storedProcPool.getPtr(storedPtr, storedProcId);
  ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);
  ndbrequire(storedPtr.p->storedCounter == 0);
  Uint32 firstAttrinbuf = storedPtr.p->storedLinkFirst;
  storedPtr.p->storedCode = ZSTORED_PROCEDURE_FREE;
  storedPtr.p->storedLinkFirst = RNIL;
  storedPtr.p->storedLinkLast = RNIL;
  storedPtr.p->storedProcLength = 0;
  c_storedProcPool.release(storedPtr);
  freeAttrinbufrec(firstAttrinbuf);
  regOperPtr->currentAttrinbufLen = 0;
  regOperPtr->transstate = IDLE;
  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = storedProcId;
  sendSignal(regOperPtr->userblockref, GSN_STORED_PROCCONF, signal, 2, JBB);
}//Dbtup::deleteScanProcedure()

void Dbtup::scanProcedure(Signal* signal,
                          Operationrec* regOperPtr,
                          Uint32 lenAttrInfo)
{
//--------------------------------------------------------
// We introduce the maxCheck so that there is always one
// stored procedure entry free for copy procedures. Thus
// no amount of scanning can cause problems for the node
// recovery functionality.
//--------------------------------------------------------
  StoredProcPtr storedPtr;
  c_storedProcPool.seize(storedPtr);
  ndbrequire(storedPtr.i != RNIL);
  storedPtr.p->storedCode = ZSCAN_PROCEDURE;
  storedPtr.p->storedCounter = 0;
  storedPtr.p->storedProcLength = lenAttrInfo;
  storedPtr.p->storedLinkFirst = RNIL;
  storedPtr.p->storedLinkLast = RNIL;
  regOperPtr->transstate = WAIT_STORED_PROCEDURE_ATTR_INFO;
  regOperPtr->attrinbufLen = lenAttrInfo;
  regOperPtr->currentAttrinbufLen = 0;
  regOperPtr->pageOffset = storedPtr.i;
}//Dbtup::scanProcedure()

void Dbtup::copyProcedure(Signal* signal,
                          TablerecPtr regTabPtr,
                          Operationrec* regOperPtr) 
{
  Uint32 TnoOfAttributes = regTabPtr.p->noOfAttr;
  scanProcedure(signal,
                regOperPtr,
                TnoOfAttributes);

  Uint32 length = 0;
  for (Uint32 Ti = 0; Ti < TnoOfAttributes; Ti++) {
    AttributeHeader::init(&signal->theData[length + 1], Ti, 0);
    length++;
    if (length == 24) {
      ljam();
      ndbrequire(storedProcedureAttrInfo(signal, regOperPtr, length, 1, true));
      length = 0;
    }//if
  }//for
  if (length != 0) {
    ljam();
    ndbrequire(storedProcedureAttrInfo(signal, regOperPtr, length, 1, true));
  }//if
  ndbrequire(regOperPtr->currentAttrinbufLen == 0);
}//Dbtup::copyProcedure()

bool Dbtup::storedProcedureAttrInfo(Signal* signal,
                                    Operationrec* regOperPtr,
                                    Uint32 length,
                                    Uint32 firstWord,
                                    bool copyProcedure) 
{
  AttrbufrecPtr regAttrPtr;
  Uint32 RnoFree = cnoFreeAttrbufrec;
  if (ERROR_INSERTED(4004) && !copyProcedure) {
    CLEAR_ERROR_INSERT_VALUE;
    storedSeizeAttrinbufrecErrorLab(signal, regOperPtr);
    return false;
  }//if
  regOperPtr->currentAttrinbufLen += length;
  ndbrequire(regOperPtr->currentAttrinbufLen <= regOperPtr->attrinbufLen);
  if ((RnoFree > MIN_ATTRBUF) ||
      (copyProcedure)) {
    ljam();
    regAttrPtr.i = cfirstfreeAttrbufrec;
    ptrCheckGuard(regAttrPtr, cnoOfAttrbufrec, attrbufrec);
    regAttrPtr.p->attrbuf[ZBUF_DATA_LEN] = 0;
    cfirstfreeAttrbufrec = regAttrPtr.p->attrbuf[ZBUF_NEXT];
    cnoFreeAttrbufrec = RnoFree - 1;
    regAttrPtr.p->attrbuf[ZBUF_NEXT] = RNIL;
  } else {
    ljam();
    storedSeizeAttrinbufrecErrorLab(signal, regOperPtr);
    return false;
  }//if
  if (regOperPtr->firstAttrinbufrec == RNIL) {
    ljam();
    regOperPtr->firstAttrinbufrec = regAttrPtr.i;
  }//if
  regAttrPtr.p->attrbuf[ZBUF_NEXT] = RNIL;
  if (regOperPtr->lastAttrinbufrec != RNIL) {
    AttrbufrecPtr tempAttrinbufptr;
    ljam();
    tempAttrinbufptr.i = regOperPtr->lastAttrinbufrec;  
    ptrCheckGuard(tempAttrinbufptr, cnoOfAttrbufrec, attrbufrec);
    tempAttrinbufptr.p->attrbuf[ZBUF_NEXT] = regAttrPtr.i;
  }//if
  regOperPtr->lastAttrinbufrec = regAttrPtr.i;

  regAttrPtr.p->attrbuf[ZBUF_DATA_LEN] = length;
  MEMCOPY_NO_WORDS(&regAttrPtr.p->attrbuf[0],
                   &signal->theData[firstWord],
                   length);

  if (regOperPtr->currentAttrinbufLen < regOperPtr->attrinbufLen) {
    ljam();
    return true;
  }//if
  if (ERROR_INSERTED(4005) && !copyProcedure) {
    CLEAR_ERROR_INSERT_VALUE;
    storedSeizeAttrinbufrecErrorLab(signal, regOperPtr);
    return false;
  }//if

  StoredProcPtr storedPtr;
  c_storedProcPool.getPtr(storedPtr, (Uint32)regOperPtr->pageOffset);
  ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);

  regOperPtr->currentAttrinbufLen = 0;
  storedPtr.p->storedLinkFirst = regOperPtr->firstAttrinbufrec;
  storedPtr.p->storedLinkLast = regOperPtr->lastAttrinbufrec;
  regOperPtr->firstAttrinbufrec = RNIL;
  regOperPtr->lastAttrinbufrec = RNIL;
  regOperPtr->transstate = IDLE;
  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = storedPtr.i;
  sendSignal(regOperPtr->userblockref, GSN_STORED_PROCCONF, signal, 2, JBB);
  return true;
}//Dbtup::storedProcedureAttrInfo()

void Dbtup::storedSeizeAttrinbufrecErrorLab(Signal* signal,
                                            Operationrec* regOperPtr)
{
  StoredProcPtr storedPtr;
  c_storedProcPool.getPtr(storedPtr, (Uint32)regOperPtr->pageOffset);
  ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);

  storedPtr.p->storedLinkFirst = regOperPtr->firstAttrinbufrec;
  regOperPtr->firstAttrinbufrec = RNIL;
  regOperPtr->lastAttrinbufrec = RNIL;
  regOperPtr->transstate = ERROR_WAIT_STORED_PROCREQ;
  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = ZSTORED_SEIZE_ATTRINBUFREC_ERROR;
  signal->theData[2] = regOperPtr->pageOffset;
  sendSignal(regOperPtr->userblockref, GSN_STORED_PROCREF, signal, 3, JBB);
}//Dbtup::storedSeizeAttrinbufrecErrorLab()

