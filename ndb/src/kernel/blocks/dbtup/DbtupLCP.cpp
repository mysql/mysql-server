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
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>

#define ljam() { jamLine(10000 + __LINE__); }
#define ljamEntry() { jamEntryLine(10000 + __LINE__); }

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* -------------------- LOCAL CHECKPOINT MODULE ------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execTUP_PREPLCPREQ(Signal* signal) 
{
  CheckpointInfoPtr ciPtr;
  DiskBufferSegmentInfoPtr dbsiPtr;
  FragrecordPtr regFragPtr;
  LocalLogInfoPtr lliPtr;
  TablerecPtr regTabPtr;

  ljamEntry();
  Uint32 userptr = signal->theData[0];
  BlockReference userblockref = signal->theData[1];
  regTabPtr.i = signal->theData[2];
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  Uint32 fragId = signal->theData[3];
  Uint32 checkpointNumber = signal->theData[4];
  cundoFileVersion = signal->theData[5];

  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  ndbrequire(regTabPtr.i != RNIL);
  seizeCheckpointInfoRecord(ciPtr);

  lliPtr.i = (cundoFileVersion << 2) + (regTabPtr.i & 0x3);
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
  cnoOfDataPagesToDiskWithoutSynch = 0;

  ciPtr.p->lcpDataFileHandle = RNIL;
  ciPtr.p->lcpCheckpointVersion = checkpointNumber;
  ciPtr.p->lcpLocalLogInfoP = lliPtr.i;
  ciPtr.p->lcpFragmentP = regFragPtr.i;	/* SET THE FRAGMENT              */
  ciPtr.p->lcpFragmentId = fragId;	/* SAVE THE FRAGMENT IDENTITY    */
  ciPtr.p->lcpTabPtr = regTabPtr.i;	/* SET THE TABLE POINTER         */
  ciPtr.p->lcpBlockref = userblockref;	/* SET THE BLOCK REFERENCE       */
  ciPtr.p->lcpUserptr = userptr;	/* SET THE USERPOINTER           */

   /***************************************************************/
   /* OPEN THE UNDO FILE FOR WRITE                                */
   /* UPON FSOPENCONF                                             */
   /***************************************************************/
  if (lliPtr.p->lliActiveLcp == 0) {                  /* IS THE UNDO LOG FILE OPEN?    */
    PendingFileOpenInfoPtr undoPfoiPtr;
    UndoPagePtr regUndoPagePtr;

    ljam();
    lliPtr.p->lliPrevRecordId = 0;
    lliPtr.p->lliLogFilePage = 0;
    lliPtr.p->lliUndoPagesToDiskWithoutSynch = 0;
    lliPtr.p->lliUndoWord = ZUNDO_PAGE_HEADER_SIZE;

    seizeUndoBufferSegment(signal, regUndoPagePtr);
    seizeDiskBufferSegmentRecord(dbsiPtr);
    dbsiPtr.p->pdxBuffertype = UNDO_PAGES;
    for (Uint32 i = 0; i < ZUB_SEGMENT_SIZE; i++) {
      dbsiPtr.p->pdxDataPage[i] = regUndoPagePtr.i + i;
    }//for
    dbsiPtr.p->pdxFilePage = lliPtr.p->lliLogFilePage;
    lliPtr.p->lliUndoPage = regUndoPagePtr.i;
    lliPtr.p->lliUndoBufferSegmentP = dbsiPtr.i;
                                                /* F LEVEL NOT USED              */
    Uint32 fileType = 1;	                /* VERSION                       */
    fileType = (fileType << 8) | 2;	        /* .LOCLOG                       */
    fileType = (fileType << 8) | 6;	        /* D6                            */
    fileType = (fileType << 8) | 0xff;	        /* DON'T USE P DIRECTORY LEVEL   */
    Uint32 fileFlag = 0x301;	                /* CREATE, WRITE ONLY, TRUNCATE  */

    seizePendingFileOpenInfoRecord(undoPfoiPtr);
    undoPfoiPtr.p->pfoOpenType = LCP_UNDO_FILE_WRITE;
    undoPfoiPtr.p->pfoCheckpointInfoP = ciPtr.i;

    signal->theData[0] = cownref;
    signal->theData[1] = undoPfoiPtr.i;
    signal->theData[2] = lliPtr.i;
    signal->theData[3] = 0xFFFFFFFF;
    signal->theData[4] = cundoFileVersion;
    signal->theData[5] = fileType;
    signal->theData[6] = fileFlag;
    sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
  }//if
   /***************************************************************/
   /* OPEN THE DATA FILE FOR WRITE                                */
   /* THE FILE HANDLE WILL BE SET IN THE CHECKPOINT_INFO_RECORD   */
   /* UPON FSOPENCONF                                             */
   /***************************************************************/
   /* OPEN THE DATA FILE IN THE FOLLOWING FORM              */
   /* D5/DBTUP/T<TABID>/F<FRAGID>/S<CHECKPOINT_NUMBER>.DATA */

  PendingFileOpenInfoPtr dataPfoiPtr;

  Uint32 fileType = 1;	                /* VERSION                       */
  fileType = (fileType << 8) | 0;	/* .DATA                         */
  fileType = (fileType << 8) | 5;	/* D5                            */
  fileType = (fileType << 8) | 0xff;	/* DON'T USE P DIRECTORY LEVEL   */
  Uint32 fileFlag = 0x301;	        /* CREATE, WRITE ONLY, TRUNCATE  */

  seizePendingFileOpenInfoRecord(dataPfoiPtr);	/* SEIZE A NEW FILE OPEN INFO    */
  if (lliPtr.p->lliActiveLcp == 0) {
    ljam();
    dataPfoiPtr.p->pfoOpenType = LCP_DATA_FILE_WRITE_WITH_UNDO;
  } else {
    ljam();
    dataPfoiPtr.p->pfoOpenType = LCP_DATA_FILE_WRITE;
  }//if
  dataPfoiPtr.p->pfoCheckpointInfoP = ciPtr.i;

   /* LET'S OPEN THE DATA FILE FOR WRITE */
   /* INCREASE NUMBER OF ACTIVE CHECKPOINTS */
  lliPtr.p->lliActiveLcp = 1;
  signal->theData[0] = cownref;
  signal->theData[1] = dataPfoiPtr.i;
  signal->theData[2] = ciPtr.p->lcpTabPtr;
  signal->theData[3] = ciPtr.p->lcpFragmentId;
  signal->theData[4] = ciPtr.p->lcpCheckpointVersion;
  signal->theData[5] = fileType;
  signal->theData[6] = fileFlag;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
  return;
}//Dbtup::execTUP_PREPLCPREQ()

/* ---------------------------------------------------------------- */
/* ------------------------ START CHECKPOINT  --------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execTUP_LCPREQ(Signal* signal) 
{
  CheckpointInfoPtr ciPtr;
  DiskBufferSegmentInfoPtr dbsiPtr;
  FragrecordPtr regFragPtr;
  LocalLogInfoPtr lliPtr;

  ljamEntry();
//  Uint32 userptr = signal->theData[0];
//  BlockReference userblockref = signal->theData[1];
  ciPtr.i = signal->theData[2];

  ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
  regFragPtr.i = ciPtr.p->lcpFragmentP;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);

/* ---------------------------------------------------------------- */
/*       ASSIGNING A VALUE DIFFERENT FROM RNIL TO CHECKPOINT VERSION*/
/*       TRIGGERS THAT UNDO LOGGING WILL START FOR THIS FRAGMENT.   */
/*       WE ASSIGN IT THE POINTER TO THE CHECKPOINT RECORD FOR      */
/*       OPTIMISATION OF THE WRITING OF THE UNDO LOG.               */
/* ---------------------------------------------------------------- */
  regFragPtr.p->checkpointVersion = ciPtr.p->lcpLocalLogInfoP;	/* MARK START OF UNDO LOGGING */

  regFragPtr.p->maxPageWrittenInCheckpoint = getNoOfPages(regFragPtr.p);
  regFragPtr.p->minPageNotWrittenInCheckpoint = 0;
  ndbrequire(getNoOfPages(regFragPtr.p) > 0);
  allocDataBufferSegment(signal, dbsiPtr);

  dbsiPtr.p->pdxNumDataPages =  0;
  dbsiPtr.p->pdxFilePage = 1;
  ciPtr.p->lcpDataBufferSegmentP = dbsiPtr.i;
  dbsiPtr.p->pdxCheckpointInfoP = ciPtr.i;
  ciPtr.p->lcpNoOfPages = getNoOfPages(regFragPtr.p);
  ciPtr.p->lcpNoCopyPagesAlloc = regFragPtr.p->noCopyPagesAlloc;
  ciPtr.p->lcpEmptyPrimPage = regFragPtr.p->emptyPrimPage;
  ciPtr.p->lcpThFreeFirst = regFragPtr.p->thFreeFirst;
  ciPtr.p->lcpThFreeCopyFirst = regFragPtr.p->thFreeCopyFirst;
  lliPtr.i = ciPtr.p->lcpLocalLogInfoP;
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
/* ---------------------------------------------------------------- */
/* --- PERFORM A COPY OF THE TABLE DESCRIPTOR FOR THIS FRAGMENT --- */
/* ---------------------------------------------------------------- */
  cprAddLogHeader(signal,
                  lliPtr.p,
                  ZTABLE_DESCRIPTOR,
                  ciPtr.p->lcpTabPtr,
                  ciPtr.p->lcpFragmentId);

/* ---------------------------------------------------------------- */
/*       CONTINUE WITH SAVING ACTIVE OPERATIONS AFTER A REAL-TIME   */
/*       BREAK.                                                     */
/* ---------------------------------------------------------------- */
  ciPtr.p->lcpTmpOperPtr = regFragPtr.p->firstusedOprec;
  lcpSaveCopyListLab(signal, ciPtr);
  return;
}//Dbtup::execTUP_LCPREQ()

void Dbtup::allocDataBufferSegment(Signal* signal, DiskBufferSegmentInfoPtr& dbsiPtr) 
{
  UndoPagePtr regUndoPagePtr;

  seizeDiskBufferSegmentRecord(dbsiPtr);
  dbsiPtr.p->pdxBuffertype = COMMON_AREA_PAGES;
  ndbrequire(cfirstfreeUndoSeg != RNIL);
  if (cnoFreeUndoSeg == ZMIN_PAGE_LIMIT_TUP_COMMITREQ) {
    EXECUTE_DIRECT(DBLQH, GSN_TUP_COM_BLOCK, signal, 1);
    ljamEntry();
  }//if
  cnoFreeUndoSeg--;
  ndbrequire(cnoFreeUndoSeg >= 0);

  regUndoPagePtr.i = cfirstfreeUndoSeg;
  ptrCheckGuard(regUndoPagePtr, cnoOfUndoPage, undoPage);
  cfirstfreeUndoSeg = regUndoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS];
  regUndoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS] = RNIL;
  for (Uint32 i = 0; i < ZUB_SEGMENT_SIZE; i++) {
    dbsiPtr.p->pdxDataPage[i] = regUndoPagePtr.i + i;
  }//for
}//Dbtup::allocDataBufferSegment()

/* ---------------------------------------------------------------- */
/* --- PERFORM A COPY OF THE ACTIVE OPERATIONS FOR THIS FRAGMENT -- */
/* ---------------------------------------------------------------- */
void Dbtup::lcpSaveCopyListLab(Signal* signal, CheckpointInfoPtr ciPtr) 
{
  FragrecordPtr regFragPtr;
  LocalLogInfoPtr lliPtr;
  OperationrecPtr regOpPtr;

  regFragPtr.i = ciPtr.p->lcpFragmentP;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
  lliPtr.i = ciPtr.p->lcpLocalLogInfoP;
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
  regOpPtr.i = ciPtr.p->lcpTmpOperPtr;

/* -------------------------------------------------------------------------------- */
/* TRAVERSE THE ENTIRE BLOCK OF OPERATIONS. CHECK IF THERE ARE EXISTING COPYS OF    */
/* TUPLES IN THE CHECKPOINTED FRAGMENT. SAVE THOSE IN A LIST IN THE FOLLOWING FORM: */
/*                                                                                  */
/*    SOURCE PAGE                                                                   */
/*    SOURCE INDEX                                                                  */
/*    COPY PAGE                                                                     */
/*    COPY INDEX                                                                    */
/* -------------------------------------------------------------------------------- */
  Uint32 loopCount = 0;
  while ((regOpPtr.i != RNIL) && (loopCount < 50)) {
    ljam();
    ptrCheckGuard(regOpPtr, cnoOfOprec, operationrec);
    if (regOpPtr.p->realPageId != RNIL) {
/* ---------------------------------------------------------------- */
// We ensure that we have actually allocated the tuple header and
// also found it. Otherwise we will fill the undo log with garbage.
/* ---------------------------------------------------------------- */
      if (regOpPtr.p->optype == ZUPDATE) {
        ljam();
        if (regOpPtr.p->realPageIdC != RNIL) {
/* ---------------------------------------------------------------- */
// We ensure that we have actually allocated the tuple header copy.
// Otherwise we will fill the undo log with garbage.
/* ---------------------------------------------------------------- */
          cprAddLogHeader(signal,
                          lliPtr.p,
                          ZLCPR_ABORT_UPDATE,
                          ciPtr.p->lcpTabPtr,
                          ciPtr.p->lcpFragmentId);
          cprAddAbortUpdate(signal, lliPtr.p, regOpPtr.p);
        }//if
      } else if (regOpPtr.p->optype == ZINSERT) {
        ljam();
        cprAddUndoLogRecord(signal,
                            ZLCPR_ABORT_INSERT,
                            regOpPtr.p->fragPageId,
                            regOpPtr.p->pageIndex,
                            regOpPtr.p->tableRef,
                            regOpPtr.p->fragId,
                            regFragPtr.p->checkpointVersion);
      } else {
        ndbrequire(regOpPtr.p->optype == ZDELETE);
        ljam();
        cprAddUndoLogRecord(signal,
                            ZINDICATE_NO_OP_ACTIVE,
                            regOpPtr.p->fragPageId,
                            regOpPtr.p->pageIndex,
                            regOpPtr.p->tableRef,
                            regOpPtr.p->fragId,
                            regFragPtr.p->checkpointVersion);
      }//if
    }//if
    loopCount++;;
    regOpPtr.i = regOpPtr.p->nextOprecInList;
  }//while
  if (regOpPtr.i == RNIL) {
    ljam();

    signal->theData[0] = ciPtr.p->lcpUserptr;
    sendSignal(ciPtr.p->lcpBlockref, GSN_TUP_LCPSTARTED, signal, 1, JBA);

    signal->theData[0] = ZCONT_SAVE_DP;
    signal->theData[1] = ciPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  } else {
    ljam();
    ciPtr.p->lcpTmpOperPtr = regOpPtr.i;
    signal->theData[0] = ZCONT_START_SAVE_CL;
    signal->theData[1] = ciPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  }//if
}//Dbtup::lcpSaveCopyListLab()

/* ---------------------------------------------------------------- */
/* ------- PERFORM A COPY OF ONE DATAPAGE DURING CHECKPOINT ------- */
/* ---------------------------------------------------------------- */
/* THE RANGE OF DATA PAGES IS INCLUDED IN THE CHECKPOINT_INFO_PTR   */
/* LAST_PAGE_TO_BUFFER ELEMENT IS INCREASED UNTIL ALL PAGES ARE     */
/* COPIED TO THE DISK BUFFER. WHEN A DISK BUFFER SEGMENT IS FULL    */
/* IT WILL BE WRITTEN TO DISK (TYPICALLY EACH 8:TH PAGE)            */
/* ---------------------------------------------------------------- */
void Dbtup::lcpSaveDataPageLab(Signal* signal, Uint32 ciIndex) 
{
  CheckpointInfoPtr ciPtr;
  DiskBufferSegmentInfoPtr dbsiPtr;
  FragrecordPtr regFragPtr;
  LocalLogInfoPtr lliPtr;
  UndoPagePtr undoCopyPagePtr;
  PagePtr pagePtr;

  ciPtr.i = ciIndex;
  ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
  if (ERROR_INSERTED(4000)){
    if (ciPtr.p->lcpTabPtr == c_errorInsert4000TableId) {
    // Delay writing of data pages during LCP
      ndbout << "Delay writing of data pages during LCP" << endl;
      signal->theData[0] = ZCONT_SAVE_DP;
      signal->theData[1] = ciIndex;
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 1000, 2);
      return;
    }//if
  }//if
  if (clblPageCounter == 0) {
    ljam();
    signal->theData[0] = ZCONT_SAVE_DP;
    signal->theData[1] = ciPtr.i;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 100, 2);
    return;
  } else {
    ljam();
    clblPageCounter--;
  }//if

  regFragPtr.i = ciPtr.p->lcpFragmentP;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
  dbsiPtr.i = ciPtr.p->lcpDataBufferSegmentP;
  ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);

  pagePtr.i = getRealpid(regFragPtr.p, regFragPtr.p->minPageNotWrittenInCheckpoint);
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  ndbrequire(dbsiPtr.p->pdxNumDataPages < 16);
  undoCopyPagePtr.i = dbsiPtr.p->pdxDataPage[dbsiPtr.p->pdxNumDataPages];
  ptrCheckGuard(undoCopyPagePtr, cnoOfUndoPage, undoPage);
  MEMCOPY_NO_WORDS(&undoCopyPagePtr.p->undoPageWord[0],
                   &pagePtr.p->pageWord[0],
                   ZWORDS_ON_PAGE);
  regFragPtr.p->minPageNotWrittenInCheckpoint++;
  dbsiPtr.p->pdxNumDataPages++;
  if (regFragPtr.p->minPageNotWrittenInCheckpoint == regFragPtr.p->maxPageWrittenInCheckpoint) {
    /* ---------------------------------------------------------- */
    /* ALL PAGES ARE COPIED, TIME TO FINISH THE CHECKPOINT        */
    /* SAVE THE END POSITIONS OF THE LOG RECORDS SINCE ALL DATA   */
    /* PAGES ARE NOW SAFE ON DISK AND NO MORE LOGGING WILL APPEAR */
    /* ---------------------------------------------------------- */
    ljam();
    lliPtr.i = ciPtr.p->lcpLocalLogInfoP;
    ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
    regFragPtr.p->checkpointVersion = RNIL;	/* UNDO LOGGING IS SHUT OFF */
    lcpWriteListDataPageSegment(signal, dbsiPtr, ciPtr, false);
    dbsiPtr.p->pdxOperation = CHECKPOINT_DATA_WRITE_LAST;
  } else if (dbsiPtr.p->pdxNumDataPages == ZDB_SEGMENT_SIZE) {
    ljam();
    lcpWriteListDataPageSegment(signal, dbsiPtr, ciPtr, false);
    dbsiPtr.p->pdxOperation = CHECKPOINT_DATA_WRITE;
  } else {
    ljam();
    signal->theData[0] = ZCONT_SAVE_DP;
    signal->theData[1] = ciPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  }//if
}//Dbtup::lcpSaveDataPageLab()

void Dbtup::lcpWriteListDataPageSegment(Signal* signal,
                                        DiskBufferSegmentInfoPtr dbsiPtr,
                                        CheckpointInfoPtr ciPtr,
                                        bool flushFlag) 
{
  Uint32 flags = 1;
  cnoOfDataPagesToDiskWithoutSynch += dbsiPtr.p->pdxNumDataPages;
  if ((cnoOfDataPagesToDiskWithoutSynch > MAX_PAGES_WITHOUT_SYNCH) ||
      (flushFlag)) {
    ljam();
/* ---------------------------------------------------------------- */
// To avoid synching too big chunks at a time we synch after writing
// a certain number of data pages. (e.g. 2 MBytes).
/* ---------------------------------------------------------------- */
    cnoOfDataPagesToDiskWithoutSynch = 0;
    flags |= 0x10; //Set synch flag unconditionally
  }//if
  signal->theData[0] = ciPtr.p->lcpDataFileHandle;
  signal->theData[1] = cownref;
  signal->theData[2] = dbsiPtr.i;
  signal->theData[3] = flags;
  signal->theData[4] = ZBASE_ADDR_UNDO_WORD;
  signal->theData[5] = dbsiPtr.p->pdxNumDataPages;
  signal->theData[6] = dbsiPtr.p->pdxDataPage[0];
  signal->theData[7] = dbsiPtr.p->pdxFilePage;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);

  dbsiPtr.p->pdxFilePage += dbsiPtr.p->pdxNumDataPages;
  dbsiPtr.p->pdxNumDataPages = 0;
}//Dbtup::lcpWriteListDataPageSegment()

void Dbtup::lcpFlushLogLab(Signal* signal, CheckpointInfoPtr ciPtr) 
{
  DiskBufferSegmentInfoPtr oldDbsiPtr;
  LocalLogInfoPtr lliPtr;
  UndoPagePtr oldUndoPagePtr;
  UndoPagePtr newUndoPagePtr;

  lliPtr.i = ciPtr.p->lcpLocalLogInfoP;
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
  oldDbsiPtr.i = lliPtr.p->lliUndoBufferSegmentP;
  ptrCheckGuard(oldDbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
  oldDbsiPtr.p->pdxNumDataPages++;
  if (clblPageCounter > 0) {
    ljam();
    clblPageCounter--;
  }//if
  oldUndoPagePtr.i = lliPtr.p->lliUndoPage;
  ptrCheckGuard(oldUndoPagePtr, cnoOfUndoPage, undoPage);
  lcpWriteUndoSegment(signal, lliPtr.p, true);
  oldDbsiPtr.p->pdxOperation = CHECKPOINT_UNDO_WRITE_FLUSH;
  oldDbsiPtr.p->pdxCheckpointInfoP = ciPtr.i;

/* ---------------------------------------------------------------- */
/*       SINCE LAST PAGE SENT TO DISK WAS NOT FULL YET WE COPY IT   */
/*       TO THE NEW LAST PAGE.                                      */
/* ---------------------------------------------------------------- */
  newUndoPagePtr.i = lliPtr.p->lliUndoPage;
  ptrCheckGuard(newUndoPagePtr, cnoOfUndoPage, undoPage);
  ndbrequire(lliPtr.p->lliUndoWord < ZWORDS_ON_PAGE);
  MEMCOPY_NO_WORDS(&newUndoPagePtr.p->undoPageWord[0],
                   &oldUndoPagePtr.p->undoPageWord[0],
                   lliPtr.p->lliUndoWord);
}//Dbtup::lcpFlushLogLab()

void Dbtup::lcpFlushRestartInfoLab(Signal* signal, Uint32 ciIndex) 
{
  CheckpointInfoPtr ciPtr;
  DiskBufferSegmentInfoPtr dbsiPtr;
  LocalLogInfoPtr lliPtr;
  UndoPagePtr undoCopyPagePtr;

  ciPtr.i = ciIndex;
  ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);

  lliPtr.i = ciPtr.p->lcpLocalLogInfoP;
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
  dbsiPtr.i = ciPtr.p->lcpDataBufferSegmentP;
  ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
  undoCopyPagePtr.i = dbsiPtr.p->pdxDataPage[0];	/* UNDO INFO STORED AT PAGE 0 */
  ptrCheckGuard(undoCopyPagePtr, cnoOfUndoPage, undoPage);
  ndbrequire(ciPtr.p->lcpNoOfPages > 0);
  undoCopyPagePtr.p->undoPageWord[ZSRI_NO_OF_FRAG_PAGES_POS] = ciPtr.p->lcpNoOfPages;
  undoCopyPagePtr.p->undoPageWord[ZSRI_NO_COPY_PAGES_ALLOC] = ciPtr.p->lcpNoCopyPagesAlloc;
  undoCopyPagePtr.p->undoPageWord[ZSRI_EMPTY_PRIM_PAGE] = ciPtr.p->lcpEmptyPrimPage;
  undoCopyPagePtr.p->undoPageWord[ZSRI_TH_FREE_FIRST] = ciPtr.p->lcpThFreeFirst;
  undoCopyPagePtr.p->undoPageWord[ZSRI_TH_FREE_COPY_FIRST] = ciPtr.p->lcpThFreeCopyFirst;
  undoCopyPagePtr.p->undoPageWord[ZSRI_UNDO_LOG_END_REC_ID] = lliPtr.p->lliPrevRecordId;
  undoCopyPagePtr.p->undoPageWord[ZSRI_UNDO_FILE_VER] = cundoFileVersion;
  if (lliPtr.p->lliUndoWord == ZUNDO_PAGE_HEADER_SIZE) {
    ljam();
    undoCopyPagePtr.p->undoPageWord[ZSRI_UNDO_LOG_END_PAGE_ID] = lliPtr.p->lliLogFilePage - 1;
  } else {
    ljam();
    undoCopyPagePtr.p->undoPageWord[ZSRI_UNDO_LOG_END_PAGE_ID] = lliPtr.p->lliLogFilePage;
  }//if
  dbsiPtr.p->pdxNumDataPages = 1;
  dbsiPtr.p->pdxFilePage = 0;
  if (clblPageCounter > 0) {
    ljam();
    clblPageCounter--;
  }//if
  lcpWriteListDataPageSegment(signal, dbsiPtr, ciPtr, true);
  dbsiPtr.p->pdxOperation = CHECKPOINT_DATA_WRITE_FLUSH;
  return;
}//Dbtup::lcpFlushRestartInfoLab()

void Dbtup::lcpCompletedLab(Signal* signal, Uint32 ciIndex) 
{
  CheckpointInfoPtr ciPtr;
  PendingFileOpenInfoPtr pfoiPtr;
/* ---------------------------------------------------------------------- */
/*       INSERT CODE TO CLOSE DATA FILE HERE. DO THIS BEFORE SEND CONF    */
/* ---------------------------------------------------------------------- */
  ciPtr.i = ciIndex;
  ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);

  seizePendingFileOpenInfoRecord(pfoiPtr);
  pfoiPtr.p->pfoOpenType = LCP_DATA_FILE_CLOSE;
  pfoiPtr.p->pfoCheckpointInfoP = ciPtr.i;

  signal->theData[0] = ciPtr.p->lcpDataFileHandle;
  signal->theData[1] = cownref;
  signal->theData[2] = pfoiPtr.i;
  signal->theData[3] = 0;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
  return;
}//Dbtup::lcpCompletedLab()

void Dbtup::lcpClosedDataFileLab(Signal* signal, CheckpointInfoPtr ciPtr) 
{
  signal->theData[0] = ciPtr.p->lcpUserptr;
  sendSignal(ciPtr.p->lcpBlockref, GSN_TUP_LCPCONF, signal, 1, JBB);
  releaseCheckpointInfoRecord(ciPtr);
  return;
}//Dbtup::lcpClosedDataFileLab()

/* ---------------------------------------------------------------------- */
/* LCP END IS THE LAST STEP IN THE LCP PROCESS IT WILL CLOSE THE LOGFILES */
/* AND RELEASE THE ALLOCATED CHECKPOINT_INFO_RECORDS                      */
/* ---------------------------------------------------------------------- */
void Dbtup::execEND_LCPREQ(Signal* signal) 
{
  DiskBufferSegmentInfoPtr dbsiPtr;
  LocalLogInfoPtr lliPtr;
  PendingFileOpenInfoPtr pfoiPtr;

  ljamEntry();
  clqhUserpointer = signal->theData[0];
  clqhBlockref = signal->theData[1];
  for (lliPtr.i = 0; lliPtr.i < 16; lliPtr.i++) {
    ljam();
    ptrAss(lliPtr, localLogInfo);
    if (lliPtr.p->lliActiveLcp > 0) {
      ljam();
      dbsiPtr.i = lliPtr.p->lliUndoBufferSegmentP;
      ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
      freeDiskBufferSegmentRecord(signal, dbsiPtr);

      seizePendingFileOpenInfoRecord(pfoiPtr);	/* SEIZE A NEW FILE OPEN INFO    */
      pfoiPtr.p->pfoOpenType = LCP_UNDO_FILE_CLOSE;
      pfoiPtr.p->pfoCheckpointInfoP = lliPtr.i;

      signal->theData[0] = lliPtr.p->lliUndoFileHandle;
      signal->theData[1] = cownref;
      signal->theData[2] = pfoiPtr.i;
      signal->theData[3] = 0;
      sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
      lliPtr.p->lliActiveLcp = 0;
    }//if
  }//for
  return;
}//Dbtup::execEND_LCPREQ()

void Dbtup::lcpEndconfLab(Signal* signal) 
{
  LocalLogInfoPtr lliPtr;
  for (lliPtr.i = 0; lliPtr.i < 16; lliPtr.i++) {
    ljam();
    ptrAss(lliPtr, localLogInfo);
    if (lliPtr.p->lliUndoFileHandle != RNIL) {
      ljam();
/* ---------------------------------------------------------------------- */
/*       WAIT UNTIL ALL LOG FILES HAVE BEEN CLOSED.                       */
/* ---------------------------------------------------------------------- */
      return;
    }//if
  }//for
  signal->theData[0] = clqhUserpointer;
  sendSignal(clqhBlockref, GSN_END_LCPCONF, signal, 1, JBB);
  return;
}//Dbtup::lcpEndconfLab()

