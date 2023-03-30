/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#define DBTUP_C
#define DBTUP_STORE_PROC_DEF_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

#define JAM_FILE_ID 406


/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ------------ADD/DROP STORED PROCEDURE MODULE ------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execSTORED_PROCREQ(Signal* signal) 
{
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;
  jamEntryDebug();
  regOperPtr.i = signal->theData[0];
  ndbrequire(c_operation_pool.getValidPtr(regOperPtr));
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
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  BlockReference apiBlockref = signal->theData[5];
#endif
  switch (requestInfo) {
  case ZSCAN_PROCEDURE:
  {
    jamDebug();
#if defined(VM_TRACE) || defined(ERROR_INSERT)
    storedProcCountNonAPI(apiBlockref, +1);
#endif
    SectionHandle handle(this);
    handle.m_ptr[0].i = signal->theData[6];
    handle.m_cnt = 1;
    getSections(handle.m_cnt, handle.m_ptr);

    scanProcedure(signal,
                  regOperPtr.p,
                  &handle,
                  false); // Not copy
    break;
  }
  case ZCOPY_PROCEDURE:
    jamDebug();
#if defined(VM_TRACE) || defined(ERROR_INSERT)
    storedProcCountNonAPI(apiBlockref, +1);
#endif
    copyProcedure(signal, regTabPtr, regOperPtr.p);
    break;
  case ZSTORED_PROCEDURE_DELETE:
    jamDebug();
#if defined(VM_TRACE) || defined(ERROR_INSERT)
    storedProcCountNonAPI(apiBlockref, -1);
#endif
    deleteScanProcedure(signal, regOperPtr.p);
    break;
  default:
    ndbabort();
  }//switch
}//Dbtup::execSTORED_PROCREQ()

void Dbtup::storedProcCountNonAPI(BlockReference apiBlockref, int add_del)
{
  BlockNumber apiBlockno = refToBlock(apiBlockref);
  if (apiBlockno < MIN_API_BLOCK_NO) {
    ndbassert(blockToMain(apiBlockno) >= MIN_BLOCK_NO &&
              blockToMain(apiBlockno) <= MAX_BLOCK_NO);
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
  storedPtr.i = storedProcId;
  if (storedPtr.i != RNIL)
  {
    jam();
    ndbrequire(c_storedProcPool.getValidPtr(storedPtr));
    ndbrequire(storedPtr.p->storedCode != ZSTORED_PROCEDURE_FREE);
    if (unlikely(storedPtr.p->storedCode == ZCOPY_PROCEDURE))
    {
      releaseCopyProcedure();
    }
    else
    {
      /* ZSCAN_PROCEDURE */
      releaseSection(storedPtr.p->storedProcIVal);
    }
    storedPtr.p->storedCode = ZSTORED_PROCEDURE_FREE;
    storedPtr.p->storedProcIVal= RNIL;
    c_storedProcPool.release(storedPtr);
    checkPoolShrinkNeed(DBTUP_STORED_PROCEDURE_TRANSIENT_POOL_INDEX,
                        c_storedProcPool);
  }
  set_trans_state(regOperPtr, TRANS_IDLE);
  signal->theData[0] = 0; /* Success */
  signal->theData[1] = storedProcId;
}//Dbtup::deleteScanProcedure()

void Dbtup::scanProcedure(Signal* signal,
                          Operationrec* regOperPtr,
                          SectionHandle* handle,
                          bool isCopy)
{
  /* Size a stored procedure record, and link the
   * stored procedure AttrInfo section from it
   */
  ndbrequire( handle->m_cnt == 1 );
  ndbrequire( handle->m_ptr[0].p->m_sz > 0 );

  StoredProcPtr storedPtr;
  if (unlikely(!c_storedProcPool.seize(storedPtr)))
  {
    jam();
    handle->clear();
    storedProcBufferSeizeErrorLab(signal, 
                                  regOperPtr,
                                  RNIL,
                                  ZOUT_OF_STORED_PROC_MEMORY_ERROR);
    return;
  }
  Uint32 lenAttrInfo= handle->m_ptr[0].p->m_sz;
  handle->clear();
  storedPtr.p->storedCode = (isCopy)? ZCOPY_PROCEDURE : ZSCAN_PROCEDURE;
  storedPtr.p->storedProcIVal= handle->m_ptr[0].i;
  storedPtr.p->storedParamNo = 0;

  set_trans_state(regOperPtr, TRANS_IDLE);
  
  if (lenAttrInfo >= ZATTR_BUFFER_SIZE) { // yes ">="
    jam();
    // send REF and change state
    storedProcBufferSeizeErrorLab(signal, 
                                  regOperPtr,
                                  storedPtr.i,
                                  ZSTORED_TOO_MUCH_ATTRINFO_ERROR);
    return;
  }

  signal->theData[0] = 0; /* Success */
  signal->theData[1] = storedPtr.i;
}//Dbtup::scanProcedure()

void Dbtup::allocCopyProcedure()
{
  /* We allocate some segments and initialise them with
   * Attribute Ids for the 'worst case' table.
   * At run time we can use prefixes of this data.
   * 
   * TODO : Consider using read packed 'read all columns' word once
   * updatePacked supported.
   */
  Uint32 iVal= RNIL;
  Uint32 ahWord;

  for (Uint32 attrNum=0; attrNum < MAX_ATTRIBUTES_IN_TABLE; attrNum++)
  {
    AttributeHeader::init(&ahWord, attrNum, 0);
    ndbrequire(appendToSection(iVal, &ahWord, 1));
  }

  /* Add space for extra attrs */
  ahWord = 0;
  for (Uint32 extra=0; extra < EXTRA_COPY_PROC_WORDS; extra++)
    ndbrequire(appendToSection(iVal, &ahWord, 1));

  cCopyProcedure= iVal;
  cCopyLastSeg= RNIL;
  cCopyOverwrite= 0;
  cCopyOverwriteLen= 0;
}

void Dbtup::freeCopyProcedure()
{
  /* Should only be called when shutting down node.
   */
  releaseSection(cCopyProcedure);
  cCopyProcedure=RNIL;
}

void Dbtup::prepareCopyProcedure(Uint32 numAttrs,
                                 Uint16 tableBits)
{
  /* Set length of copy procedure section to the
   * number of attributes supplied
   */
  ndbassert(numAttrs <= MAX_ATTRIBUTES_IN_TABLE);
  ndbassert(cCopyProcedure != RNIL);
  ndbassert(cCopyLastSeg == RNIL);
  ndbassert(cCopyOverwrite == 0);
  ndbassert(cCopyOverwriteLen == 0);
  Ptr<SectionSegment> first;
  ndbrequire(g_sectionSegmentPool.getPtr(first, cCopyProcedure));

  /* Record original 'last segment' of section */
  cCopyLastSeg= first.p->m_lastSegment;

  /* Check table bits to see if we need to do extra reads */
  Uint32 extraAttrIds[EXTRA_COPY_PROC_WORDS];
  Uint32 extraReads = 0;

  if (tableBits & Tablerec::TR_ExtraRowGCIBits)
  {
    AttributeHeader ah(AttributeHeader::ROW_GCI64,0);
    extraAttrIds[extraReads++] = ah.m_value;
  }
  if (tableBits & Tablerec::TR_ExtraRowAuthorBits)
  {
    AttributeHeader ah(AttributeHeader::ROW_AUTHOR,0);
    extraAttrIds[extraReads++] = ah.m_value;
  }

  /* Modify section to represent relevant prefix 
   * of code by modifying size and lastSegment
   */
  Uint32 newSize = numAttrs + extraReads;
  first.p->m_sz= newSize;

  if (extraReads)
  {
    cCopyOverwrite= numAttrs;
    cCopyOverwriteLen = extraReads;

    ndbrequire(writeToSection(first.i, numAttrs, extraAttrIds, extraReads));
  }

   /* Trim section size and lastSegment */
  Ptr<SectionSegment> curr= first;  
  while(newSize > SectionSegment::DataLength)
  {
    ndbrequire(g_sectionSegmentPool.getPtr(curr, curr.p->m_nextSegment));
    newSize-= SectionSegment::DataLength;
  }
  first.p->m_lastSegment= curr.i;
}

void Dbtup::releaseCopyProcedure()
{
  /* Return Copy Procedure section to original length */
  ndbassert(cCopyProcedure != RNIL);
  ndbassert(cCopyLastSeg != RNIL);
  
  Ptr<SectionSegment> first;
  ndbrequire(g_sectionSegmentPool.getPtr(first, cCopyProcedure));
  
  ndbassert(first.p->m_sz <= MAX_COPY_PROC_LEN);
  first.p->m_sz= MAX_COPY_PROC_LEN;
  first.p->m_lastSegment= cCopyLastSeg;
  
  if (cCopyOverwriteLen)
  {
    ndbassert(cCopyOverwriteLen <= EXTRA_COPY_PROC_WORDS);
    Uint32 attrids[EXTRA_COPY_PROC_WORDS];
    for (Uint32 i=0; i < cCopyOverwriteLen; i++)
    {
      AttributeHeader ah(cCopyOverwrite + i, 0);
      attrids[i] = ah.m_value;
    }
    ndbrequire(writeToSection(first.i, cCopyOverwrite, attrids, cCopyOverwriteLen));
    cCopyOverwriteLen= 0;
    cCopyOverwrite= 0;
  }

  cCopyLastSeg= RNIL;
}
  

void Dbtup::copyProcedure(Signal* signal,
                          TablerecPtr regTabPtr,
                          Operationrec* regOperPtr) 
{
  /* We create a stored procedure for the fragment copy scan
   * This is done by trimming a 'read all columns in order'
   * program to the correct length for this table and
   * using that to create the procedure
   * This assumes that there is only one fragment copy going
   * on at any time, which is verified by checking 
   * cCopyLastSeg == RNIL before starting each copy
   *
   * If the table has extra per-row metainformation that
   * needs copied then we add that to the copy procedure
   * as well.
   */
  prepareCopyProcedure(regTabPtr.p->m_no_of_attributes,
                       regTabPtr.p->m_bits);

  SectionHandle handle(this);
  handle.m_cnt=1;
  handle.m_ptr[0].i= cCopyProcedure;
  getSections(handle.m_cnt, handle.m_ptr);

  scanProcedure(signal,
                regOperPtr,
                &handle,
                true); // isCopy
  Ptr<SectionSegment> first;
  ndbrequire(g_sectionSegmentPool.getPtr(first, cCopyProcedure));
  signal->theData[2] = first.p->m_sz;
}//Dbtup::copyProcedure()

void Dbtup::storedProcBufferSeizeErrorLab(Signal* signal,
                                          Operationrec* regOperPtr,
                                          Uint32 storedProcPtr,
                                          Uint32 errorCode)
{
  regOperPtr->m_any_value = 0;
  set_trans_state(regOperPtr, TRANS_ERROR_WAIT_STORED_PROCREQ);
  signal->theData[0] = 1; /* Failure */
  signal->theData[1] = errorCode;
  signal->theData[2] = storedProcPtr;
}//Dbtup::storedSeizeAttrinbufrecErrorLab()

