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

#define ljam() { jamLine(12000 + __LINE__); }
#define ljamEntry() { jamEntryLine(12000 + __LINE__); }

void Dbtup::cprAddData(Signal* signal,
                       Fragrecord* const regFragPtr,
                       Uint32 pageIndex,
                       Uint32 noOfWords,
                       Uint32 startOffset)
{
  UndoPagePtr undoPagePtr;
  PagePtr pagePtr;
  LocalLogInfoPtr regLliPtr;

  regLliPtr.i = regFragPtr->checkpointVersion;
  ptrCheckGuard(regLliPtr, cnoOfParallellUndoFiles, localLogInfo);

  pagePtr.i = pageIndex;
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  undoPagePtr.i = regLliPtr.p->lliUndoPage;
  ptrCheckGuard(undoPagePtr, cnoOfUndoPage, undoPage);

  startOffset++;
  noOfWords--;
  if ((regLliPtr.p->lliUndoWord + noOfWords) < ZWORDS_ON_PAGE) {
    ljam();
    MEMCOPY_NO_WORDS(&undoPagePtr.p->undoPageWord[regLliPtr.p->lliUndoWord],
                     &pagePtr.p->pageWord[startOffset],
                     noOfWords);
    regLliPtr.p->lliUndoWord += noOfWords;
  } else {
    for (Uint32 i = 0; i < noOfWords; i++) {
      ljam();
      Uint32 undoWord = pagePtr.p->pageWord[startOffset + i];
      cprAddUndoLogWord(signal, regLliPtr.p, undoWord);
    }//for
  }//if
}//Dbtup::cprAddData()

void Dbtup::cprAddLogHeader(Signal* signal,
                            LocalLogInfo* const lliPtr,
                            Uint32 recordType,
                            Uint32 tableId,
                            Uint32 fragId)
{
  Uint32 prevRecId = lliPtr->lliPrevRecordId;
  lliPtr->lliPrevRecordId = lliPtr->lliUndoWord + (lliPtr->lliLogFilePage << ZUNDO_RECORD_ID_PAGE_INDEX);
  cprAddUndoLogWord(signal, lliPtr, recordType);
  cprAddUndoLogWord(signal, lliPtr, prevRecId);
  cprAddUndoLogWord(signal, lliPtr, tableId);
  cprAddUndoLogWord(signal, lliPtr, fragId);
}//Dbtup::cprAddLogHeader()

void Dbtup::cprAddGCIUpdate(Signal* signal,
                            Uint32 prevGCI,
                            Fragrecord* const regFragPtr)
{
  LocalLogInfoPtr regLliPtr;
  regLliPtr.i = regFragPtr->checkpointVersion;
  ptrCheckGuard(regLliPtr, cnoOfParallellUndoFiles, localLogInfo);

  cprAddUndoLogWord(signal, regLliPtr.p, prevGCI);
}//Dbtup::cprAddLogHeader()

void Dbtup::cprAddUndoLogPageHeader(Signal* signal,
                                    Page* const regPagePtr,
                                    Fragrecord* const regFragPtr)
{
  UndoPagePtr regUndoPagePtr;
  LocalLogInfoPtr regLliPtr;

  regLliPtr.i = regFragPtr->checkpointVersion;
  ptrCheckGuard(regLliPtr, cnoOfParallellUndoFiles, localLogInfo);

  Uint32 prevRecId = regLliPtr.p->lliPrevRecordId;
  Uint32 lliWord = regLliPtr.p->lliUndoWord;
  regLliPtr.p->lliPrevRecordId = lliWord +
                                 (regLliPtr.p->lliLogFilePage << ZUNDO_RECORD_ID_PAGE_INDEX);
  if ((lliWord + 7) < ZWORDS_ON_PAGE) {
    ljam();
    regUndoPagePtr.i = regLliPtr.p->lliUndoPage;
    ptrCheckGuard(regUndoPagePtr, cnoOfUndoPage, undoPage);

    regUndoPagePtr.p->undoPageWord[lliWord] = ZLCPR_UNDO_LOG_PAGE_HEADER;
    regUndoPagePtr.p->undoPageWord[lliWord + 1] = prevRecId;
    regUndoPagePtr.p->undoPageWord[lliWord + 2] = regFragPtr->fragTableId;
    regUndoPagePtr.p->undoPageWord[lliWord + 3] = regFragPtr->fragmentId;
    regUndoPagePtr.p->undoPageWord[lliWord + 4] = regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS];
    regUndoPagePtr.p->undoPageWord[lliWord + 5] = regPagePtr->pageWord[ZPAGE_STATE_POS];
    regUndoPagePtr.p->undoPageWord[lliWord + 6] = regPagePtr->pageWord[ZPAGE_NEXT_POS];
    regLliPtr.p->lliUndoWord = lliWord + 7;
  } else {
    ljam();
    cprAddUndoLogWord(signal, regLliPtr.p, ZLCPR_UNDO_LOG_PAGE_HEADER);
    cprAddUndoLogWord(signal, regLliPtr.p, prevRecId);
    cprAddUndoLogWord(signal, regLliPtr.p, regFragPtr->fragTableId);
    cprAddUndoLogWord(signal, regLliPtr.p, regFragPtr->fragmentId);
    cprAddUndoLogWord(signal, regLliPtr.p, regPagePtr->pageWord[ZPAGE_FRAG_PAGE_ID_POS]);
    cprAddUndoLogWord(signal, regLliPtr.p, regPagePtr->pageWord[ZPAGE_STATE_POS]);
    cprAddUndoLogWord(signal, regLliPtr.p, regPagePtr->pageWord[ZPAGE_NEXT_POS]);
  }//if
}//Dbtup::cprAddUndoLogPageHeader()

void Dbtup::cprAddUndoLogRecord(Signal* signal,
                                Uint32 recordType,
                                Uint32 pageId,
                                Uint32 pageIndex,
                                Uint32 tableId,
                                Uint32 fragId,
                                Uint32 localLogIndex)
{
  LocalLogInfoPtr regLliPtr;
  UndoPagePtr regUndoPagePtr;

  regLliPtr.i = localLogIndex;
  ptrCheckGuard(regLliPtr, cnoOfParallellUndoFiles, localLogInfo);

  Uint32 prevRecId = regLliPtr.p->lliPrevRecordId;
  Uint32 lliWord = regLliPtr.p->lliUndoWord;

  regLliPtr.p->lliPrevRecordId = lliWord +
                                 (regLliPtr.p->lliLogFilePage << ZUNDO_RECORD_ID_PAGE_INDEX);
  if ((lliWord + 6) < ZWORDS_ON_PAGE) {
    ljam();
    regUndoPagePtr.i = regLliPtr.p->lliUndoPage;
    ptrCheckGuard(regUndoPagePtr, cnoOfUndoPage, undoPage);
    regUndoPagePtr.p->undoPageWord[lliWord] = recordType;
    regUndoPagePtr.p->undoPageWord[lliWord + 1] = prevRecId;
    regUndoPagePtr.p->undoPageWord[lliWord + 2] = tableId;
    regUndoPagePtr.p->undoPageWord[lliWord + 3] = fragId;
    regUndoPagePtr.p->undoPageWord[lliWord + 4] = pageId;
    regUndoPagePtr.p->undoPageWord[lliWord + 5] = pageIndex;

    regLliPtr.p->lliUndoWord = lliWord + 6;
  } else {
    ljam();
    cprAddUndoLogWord(signal, regLliPtr.p, recordType);
    cprAddUndoLogWord(signal, regLliPtr.p, prevRecId);
    cprAddUndoLogWord(signal, regLliPtr.p, tableId);
    cprAddUndoLogWord(signal, regLliPtr.p, fragId);
    cprAddUndoLogWord(signal, regLliPtr.p, pageId);
    cprAddUndoLogWord(signal, regLliPtr.p, pageIndex);
  }//if
}//Dbtup::cprAddUndoLogRecord()

void Dbtup::cprAddAbortUpdate(Signal* signal,
                              LocalLogInfo* const lliPtr,
                              Operationrec* const regOperPtr) 
{
  Uint32 lliWord = lliPtr->lliUndoWord;
  if ((lliWord + 4) < ZWORDS_ON_PAGE) {
    ljam();
    UndoPagePtr regUndoPagePtr;
    regUndoPagePtr.i = lliPtr->lliUndoPage;
    ptrCheckGuard(regUndoPagePtr, cnoOfUndoPage, undoPage);

    regUndoPagePtr.p->undoPageWord[lliWord] = regOperPtr->fragPageId;
    regUndoPagePtr.p->undoPageWord[lliWord + 1] = regOperPtr->pageIndex;
    regUndoPagePtr.p->undoPageWord[lliWord + 2] = regOperPtr->fragPageIdC;
    regUndoPagePtr.p->undoPageWord[lliWord + 3] = regOperPtr->pageIndexC;
    lliPtr->lliUndoWord = lliWord + 4;
  } else {
    ljam();
    cprAddUndoLogWord(signal, lliPtr, regOperPtr->fragPageId);
    cprAddUndoLogWord(signal, lliPtr, regOperPtr->pageIndex);
    cprAddUndoLogWord(signal, lliPtr, regOperPtr->fragPageIdC);
    cprAddUndoLogWord(signal, lliPtr, regOperPtr->pageIndexC);
  }//if
}//Dbtup::cprAddAbortUpdate()

void Dbtup::cprAddUndoLogWord(Signal* signal, LocalLogInfo* const lliPtr, Uint32 undoWord) 
{
  DiskBufferSegmentInfoPtr dbsiPtr;
  UndoPagePtr regUndoPagePtr;

  ljam();
  regUndoPagePtr.i = lliPtr->lliUndoPage;
  ptrCheckGuard(regUndoPagePtr, cnoOfUndoPage, undoPage);
  ndbrequire(lliPtr->lliUndoWord < ZWORDS_ON_PAGE);
  regUndoPagePtr.p->undoPageWord[lliPtr->lliUndoWord] = undoWord;

  lliPtr->lliUndoWord++;
  if (lliPtr->lliUndoWord == ZWORDS_ON_PAGE) {
    ljam();
    lliPtr->lliUndoWord = ZUNDO_PAGE_HEADER_SIZE;
    lliPtr->lliUndoPage++;
    if (clblPageCounter > 0) {
      ljam();
      clblPageCounter--;
    }//if
    dbsiPtr.i = lliPtr->lliUndoBufferSegmentP;
    ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
    dbsiPtr.p->pdxNumDataPages++;
    ndbrequire(dbsiPtr.p->pdxNumDataPages < 16);
    lliPtr->lliLogFilePage++;
    if (dbsiPtr.p->pdxNumDataPages == ZUB_SEGMENT_SIZE) {
      ljam();
      lcpWriteUndoSegment(signal, lliPtr, false);
    }//if
  }//if
}//Dbtup::cprAddUndoLogWord()

void Dbtup::lcpWriteUndoSegment(Signal* signal, LocalLogInfo* const lliPtr, bool flushFlag) 
{
  DiskBufferSegmentInfoPtr dbsiPtr;

  dbsiPtr.i = lliPtr->lliUndoBufferSegmentP;
  ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
  Uint32 flags = 1;
  lliPtr->lliUndoPagesToDiskWithoutSynch += dbsiPtr.p->pdxNumDataPages;
  if ((lliPtr->lliUndoPagesToDiskWithoutSynch > MAX_PAGES_WITHOUT_SYNCH) ||
      (flushFlag)) {
    ljam();
/* ---------------------------------------------------------------- */
// To avoid synching too big chunks at a time we synch after writing
// a certain number of data pages. (e.g. 2 MBytes).
/* ---------------------------------------------------------------- */
    lliPtr->lliUndoPagesToDiskWithoutSynch = 0;
    flags |= 0x10; //Set synch flag unconditionally
  }//if
  dbsiPtr.p->pdxOperation = CHECKPOINT_UNDO_WRITE;
  signal->theData[0] = lliPtr->lliUndoFileHandle;
  signal->theData[1] = cownref;
  signal->theData[2] = dbsiPtr.i;
  signal->theData[3] = flags;
  signal->theData[4] = ZBASE_ADDR_UNDO_WORD;
  signal->theData[5] = dbsiPtr.p->pdxNumDataPages;
  signal->theData[6] = dbsiPtr.p->pdxDataPage[0];
  signal->theData[7] = dbsiPtr.p->pdxFilePage;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);

  DiskBufferSegmentInfoPtr newDbsiPtr;
  UndoPagePtr newUndoPagePtr;

  seizeUndoBufferSegment(signal, newUndoPagePtr);
  seizeDiskBufferSegmentRecord(newDbsiPtr);
  newDbsiPtr.p->pdxBuffertype = UNDO_PAGES;
  for (Uint32 i = 0; i < ZUB_SEGMENT_SIZE; i++) {
    newDbsiPtr.p->pdxDataPage[i] = newUndoPagePtr.i + i;
  }//for
  newDbsiPtr.p->pdxFilePage = lliPtr->lliLogFilePage;
  lliPtr->lliUndoPage = newUndoPagePtr.i;
  lliPtr->lliUndoBufferSegmentP = newDbsiPtr.i;
}//Dbtup::lcpWriteUndoSegment()

void Dbtup::seizeUndoBufferSegment(Signal* signal, UndoPagePtr& regUndoPagePtr)
{
  if (cnoFreeUndoSeg == ZMIN_PAGE_LIMIT_TUP_COMMITREQ) {
    EXECUTE_DIRECT(DBLQH, GSN_TUP_COM_BLOCK, signal, 1);
    ljamEntry();
  }//if
  cnoFreeUndoSeg--;
  ndbrequire(cnoFreeUndoSeg >= 0);
  ndbrequire(cfirstfreeUndoSeg != RNIL);
  regUndoPagePtr.i = cfirstfreeUndoSeg;
  ptrCheckGuard(regUndoPagePtr, cnoOfUndoPage, undoPage);
  cfirstfreeUndoSeg = regUndoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS];
  regUndoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS] = RNIL;
}//Dbtup::seizeUndoBufferSegment()



