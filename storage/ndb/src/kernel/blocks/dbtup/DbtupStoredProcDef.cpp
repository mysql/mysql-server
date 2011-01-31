/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#define DBTUP_C
#define DBTUP_STORE_PROC_DEF_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ------------ADD/DROP STORED PROCEDURE MODULE ------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execSTORED_PROCREQ(Signal* signal) 
{
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;
  jamEntry();
  regOperPtr.i = signal->theData[0];
  c_operation_pool.getPtr(regOperPtr);
  regTabPtr.i = signal->theData[1];
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  Uint32 requestInfo = signal->theData[3];
  TransState trans_state= get_trans_state(regOperPtr.p);
  ndbrequire(trans_state == TRANS_IDLE ||
             ((trans_state == TRANS_ERROR_WAIT_STORED_PROCREQ) &&
             (requestInfo == ZSTORED_PROCEDURE_DELETE)));
  ndbrequire(regTabPtr.p->tableStatus == DEFINED);
  /*
   * Also store count of procs called from non-API scans.
   * It can be done here since seize/release always succeeds.
   * The count is only used under -DERROR_INSERT via DUMP.
   */
  BlockReference apiBlockref = signal->theData[5];
  switch (requestInfo) {
  case ZSCAN_PROCEDURE:
    jam();
#if defined VM_TRACE || defined ERROR_INSERT
    storedProcCountNonAPI(apiBlockref, +1);
#endif
    scanProcedure(signal,
                  regOperPtr.p,
                  signal->theData[4]);
    break;
  case ZCOPY_PROCEDURE:
    jam();
#if defined VM_TRACE || defined ERROR_INSERT
    storedProcCountNonAPI(apiBlockref, +1);
#endif
    copyProcedure(signal, regTabPtr, regOperPtr.p);
    break;
  case ZSTORED_PROCEDURE_DELETE:
    jam();
#if defined VM_TRACE || defined ERROR_INSERT
    storedProcCountNonAPI(apiBlockref, -1);
#endif
    deleteScanProcedure(signal, regOperPtr.p);
    break;
  default:
    ndbrequire(false);
  }//switch
}//Dbtup::execSTORED_PROCREQ()

void Dbtup::storedProcCountNonAPI(BlockReference apiBlockref, int add_del)
{
  BlockNumber apiBlockno = refToBlock(apiBlockref);
  if (apiBlockno < MIN_API_BLOCK_NO) {
    ndbassert(apiBlockno == BACKUP ||
              apiBlockno == SUMA ||
              apiBlockno == DBLQH);
    if (add_del == +1) {
      jam();
      c_storedProcCountNonAPI++;
    } else if (add_del == -1) {
      jam();
      ndbassert(c_storedProcCountNonAPI > 0);
      c_storedProcCountNonAPI--;
    } else {
      ndbassert(false);
    }
  }
}

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
  set_trans_state(regOperPtr, TRANS_IDLE);
  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = storedProcId;
  sendSignal(DBLQH_REF, GSN_STORED_PROCCONF, signal, 2, JBB);
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
  set_trans_state(regOperPtr, TRANS_WAIT_STORED_PROCEDURE_ATTR_INFO);
  regOperPtr->attrinbufLen = lenAttrInfo;
  regOperPtr->currentAttrinbufLen = 0;
  regOperPtr->storedProcPtr = storedPtr.i;
  if (lenAttrInfo >= ZATTR_BUFFER_SIZE) { // yes ">="
    jam();
    // send REF and change state to ignore the ATTRINFO to come
    storedSeizeAttrinbufrecErrorLab(signal, regOperPtr, ZSTORED_TOO_MUCH_ATTRINFO_ERROR);
  }
}//Dbtup::scanProcedure()

void Dbtup::copyProcedure(Signal* signal,
                          TablerecPtr regTabPtr,
                          Operationrec* regOperPtr) 
{
  Uint32 TnoOfAttributes = regTabPtr.p->m_no_of_attributes;
  scanProcedure(signal,
                regOperPtr,
                TnoOfAttributes);

  Uint32 length = 0;
  for (Uint32 Ti = 0; Ti < TnoOfAttributes; Ti++) {
    AttributeHeader::init(&signal->theData[length + 1], Ti, 0);
    length++;
    if (length == 24) {
      jam();
      ndbrequire(storedProcedureAttrInfo(signal, regOperPtr, 
					 signal->theData+1, length, true));
      length = 0;
    }//if
  }//for
  if (length != 0) {
    jam();
    ndbrequire(storedProcedureAttrInfo(signal, regOperPtr, 
				       signal->theData+1, length, true));
  }//if
  ndbrequire(regOperPtr->currentAttrinbufLen == 0);
}//Dbtup::copyProcedure()

bool Dbtup::storedProcedureAttrInfo(Signal* signal,
                                    Operationrec* regOperPtr,
				    const Uint32 *data,
                                    Uint32 length,
                                    bool copyProcedure) 
{
  AttrbufrecPtr regAttrPtr;
  Uint32 RnoFree = cnoFreeAttrbufrec;
  if (ERROR_INSERTED(4004) && !copyProcedure) {
    CLEAR_ERROR_INSERT_VALUE;
    storedSeizeAttrinbufrecErrorLab(signal, regOperPtr, ZSTORED_SEIZE_ATTRINBUFREC_ERROR);
    return false;
  }//if
  regOperPtr->currentAttrinbufLen += length;
  ndbrequire(regOperPtr->currentAttrinbufLen <= regOperPtr->attrinbufLen);
  if ((RnoFree > MIN_ATTRBUF) ||
      (copyProcedure)) {
    jam();
    regAttrPtr.i = cfirstfreeAttrbufrec;
    ptrCheckGuard(regAttrPtr, cnoOfAttrbufrec, attrbufrec);
    regAttrPtr.p->attrbuf[ZBUF_DATA_LEN] = 0;
    cfirstfreeAttrbufrec = regAttrPtr.p->attrbuf[ZBUF_NEXT];
    cnoFreeAttrbufrec = RnoFree - 1;
    regAttrPtr.p->attrbuf[ZBUF_NEXT] = RNIL;
  } else {
    jam();
    storedSeizeAttrinbufrecErrorLab(signal, regOperPtr, ZSTORED_SEIZE_ATTRINBUFREC_ERROR);
    return false;
  }//if
  if (regOperPtr->firstAttrinbufrec == RNIL) {
    jam();
    regOperPtr->firstAttrinbufrec = regAttrPtr.i;
  }//if
  regAttrPtr.p->attrbuf[ZBUF_NEXT] = RNIL;
  if (regOperPtr->lastAttrinbufrec != RNIL) {
    AttrbufrecPtr tempAttrinbufptr;
    jam();
    tempAttrinbufptr.i = regOperPtr->lastAttrinbufrec;  
    ptrCheckGuard(tempAttrinbufptr, cnoOfAttrbufrec, attrbufrec);
    tempAttrinbufptr.p->attrbuf[ZBUF_NEXT] = regAttrPtr.i;
  }//if
  regOperPtr->lastAttrinbufrec = regAttrPtr.i;

  regAttrPtr.p->attrbuf[ZBUF_DATA_LEN] = length;
  MEMCOPY_NO_WORDS(&regAttrPtr.p->attrbuf[0],
                   data,
                   length);

  if (regOperPtr->currentAttrinbufLen < regOperPtr->attrinbufLen) {
    jam();
    return true;
  }//if
  if (ERROR_INSERTED(4005) && !copyProcedure) {
    CLEAR_ERROR_INSERT_VALUE;
    storedSeizeAttrinbufrecErrorLab(signal, regOperPtr, ZSTORED_SEIZE_ATTRINBUFREC_ERROR);
    return false;
  }//if

  StoredProcPtr storedPtr;
  c_storedProcPool.getPtr(storedPtr, (Uint32)regOperPtr->storedProcPtr);
  ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);

  regOperPtr->currentAttrinbufLen = 0;
  storedPtr.p->storedLinkFirst = regOperPtr->firstAttrinbufrec;
  storedPtr.p->storedLinkLast = regOperPtr->lastAttrinbufrec;
  regOperPtr->firstAttrinbufrec = RNIL;
  regOperPtr->lastAttrinbufrec = RNIL;
  regOperPtr->m_any_value = 0;
  set_trans_state(regOperPtr, TRANS_IDLE);
  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = storedPtr.i;
  sendSignal(DBLQH_REF, GSN_STORED_PROCCONF, signal, 2, JBB);
  return true;
}//Dbtup::storedProcedureAttrInfo()

void Dbtup::storedSeizeAttrinbufrecErrorLab(Signal* signal,
                                            Operationrec* regOperPtr,
                                            Uint32 errorCode)
{
  StoredProcPtr storedPtr;
  c_storedProcPool.getPtr(storedPtr, regOperPtr->storedProcPtr);
  ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);

  storedPtr.p->storedLinkFirst = regOperPtr->firstAttrinbufrec;
  regOperPtr->firstAttrinbufrec = RNIL;
  regOperPtr->lastAttrinbufrec = RNIL;
  regOperPtr->m_any_value = 0;
  set_trans_state(regOperPtr, TRANS_ERROR_WAIT_STORED_PROCREQ);
  signal->theData[0] = regOperPtr->userpointer;
  signal->theData[1] = errorCode;
  signal->theData[2] = regOperPtr->storedProcPtr;
  sendSignal(DBLQH_REF, GSN_STORED_PROCREF, signal, 3, JBB);
}//Dbtup::storedSeizeAttrinbufrecErrorLab()

