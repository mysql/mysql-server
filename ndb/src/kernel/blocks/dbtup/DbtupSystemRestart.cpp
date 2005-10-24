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
#include <signaldata/EventReport.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>

#define ljam() { jamLine(26000 + __LINE__); }
#define ljamEntry() { jamEntryLine(26000 + __LINE__); }

/* **************************************************************** */
/* ********************* SYSTEM RESTART MANAGER ******************* */
/* **************************************************************** */
/***************************************************************/
/* CHECK RESTART STATE AND SET NEW STATE CALLED IN OPEN,       */
/* READ AND COPY STATES                                        */
/***************************************************************/
void Dbtup::execTUP_SRREQ(Signal* signal) 
{
  RestartInfoRecordPtr riPtr;
  PendingFileOpenInfoPtr pfoiPtr;

  ljamEntry();
  Uint32 userPtr = signal->theData[0];
  Uint32 userBlockref = signal->theData[1];
  Uint32 tableId = signal->theData[2];
  Uint32 fragId = signal->theData[3];
  Uint32 checkpointNumber = signal->theData[4];

  seizeRestartInfoRecord(riPtr);

  riPtr.p->sriUserptr = userPtr;
  riPtr.p->sriBlockref = userBlockref;
  riPtr.p->sriState = OPENING_DATA_FILE;
  riPtr.p->sriCheckpointVersion = checkpointNumber;
  riPtr.p->sriFragid = fragId;
  riPtr.p->sriTableId = tableId;

   /* OPEN THE DATA FILE IN THE FOLLOWING FORM              */
   /* D5/DBTUP/T<TABID>/F<FRAGID>/S<CHECKPOINT_NUMBER>.DATA */
  Uint32 fileType = 1;	                /* VERSION                       */
  fileType = (fileType << 8) | 0;	/* .DATA                         */
  fileType = (fileType << 8) | 5;	/* D5                            */
  fileType = (fileType << 8) | 0xff;	/* DON'T USE P DIRECTORY LEVEL   */
  Uint32 fileFlag = 0;	                /* READ ONLY                     */

  seizePendingFileOpenInfoRecord(pfoiPtr);
  pfoiPtr.p->pfoOpenType = LCP_DATA_FILE_READ;
  pfoiPtr.p->pfoRestartInfoP = riPtr.i;

  signal->theData[0] = cownref;
  signal->theData[1] = pfoiPtr.i;
  signal->theData[2] = tableId;
  signal->theData[3] = fragId;
  signal->theData[4] = checkpointNumber;
  signal->theData[5] = fileType;
  signal->theData[6] = fileFlag;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
  return;
}//Dbtup::execTUP_SRREQ()

void Dbtup::seizeRestartInfoRecord(RestartInfoRecordPtr& riPtr) 
{
  riPtr.i = cfirstfreeSri;
  ptrCheckGuard(riPtr, cnoOfRestartInfoRec, restartInfoRecord);
  cfirstfreeSri = riPtr.p->sriNextRec;
  riPtr.p->sriNextRec = RNIL;
}//Dbtup::seizeRestartInfoRecord()

void Dbtup::rfrReadRestartInfoLab(Signal* signal, RestartInfoRecordPtr riPtr) 
{
  DiskBufferSegmentInfoPtr dbsiPtr;

  seizeDiskBufferSegmentRecord(dbsiPtr);
  riPtr.p->sriDataBufferSegmentP = dbsiPtr.i;
  Uint32 retPageRef = RNIL;
  Uint32 noAllocPages = 1;
  Uint32 noOfPagesAllocated;
  {
    /**
     * Use low pages for 0-pages during SR
     *   bitmask of free pages is kept in c_sr_free_page_0
     */
    Uint32 tmp = c_sr_free_page_0;
    for(Uint32 i = 1; i<(1+MAX_PARALLELL_TUP_SRREQ); i++){
      if(tmp & (1 << i)){
	retPageRef = i;
	c_sr_free_page_0 = tmp & (~(1 << i));
	break;
      }
    }
    ndbrequire(retPageRef != RNIL);
  }
  
  dbsiPtr.p->pdxDataPage[0] = retPageRef;
  dbsiPtr.p->pdxNumDataPages = 1;
  dbsiPtr.p->pdxFilePage = 0;
  rfrReadNextDataSegment(signal, riPtr, dbsiPtr);
  dbsiPtr.p->pdxOperation = CHECKPOINT_DATA_READ_PAGE_ZERO;
}//Dbtup::rfrReadRestartInfoLab()

/***************************************************************/
/* THE RESTART INFORMATION IS NOW READ INTO THE DATA BUFFER    */
/* USE THE RESTART INFORMATION TO INITIATE THE RESTART RECORD  */
/***************************************************************/
void 
Dbtup::rfrInitRestartInfoLab(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr) 
{
  Uint32 TzeroDataPage[64];
  Uint32 Ti;
  FragrecordPtr regFragPtr;
  LocalLogInfoPtr lliPtr;
  PagePtr pagePtr;
  RestartInfoRecordPtr riPtr;
  TablerecPtr regTabPtr;

  riPtr.i = dbsiPtr.p->pdxRestartInfoP;
  ptrCheckGuard(riPtr, cnoOfRestartInfoRec, restartInfoRecord);
  
  regTabPtr.i = riPtr.p->sriTableId;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  Uint32 fragId = riPtr.p->sriFragid;
  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  riPtr.p->sriFragP = regFragPtr.i;
  
  /* ----- PAGE ALLOCATION --- */
  /* ALLOCATE PAGES TO FRAGMENT, INSERT THEM INTO PAGE RANGE TABLE AND       */
  /* ALSO CONVERT THEM INTO EMPTY PAGES AND INSERT THEM INTO THE EMPTY LIST  */
  /* OF THE FRAGMENT. SET ALL LISTS OF FREE PAGES TO RNIL                    */

  ndbrequire(cfirstfreerange != RNIL);
  pagePtr.i = dbsiPtr.p->pdxDataPage[0];
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  for (Ti = 0; Ti < 63; Ti++) {
    /***************************************************************/
    // Save Important content from Page zero in stack variable so
    // that we can immediately release page zero.
    /***************************************************************/
    TzeroDataPage[Ti] = pagePtr.p->pageWord[Ti];
  }//for
  /************************************************************************/
  /* NOW WE DON'T NEED THE RESTART INFO BUFFER PAGE ANYMORE               */
  /* LETS REMOVE IT AND REUSE THE SEGMENT FOR REAL DATA PAGES             */
  /* REMOVE ONE PAGE ONLY, PAGEP IS ALREADY SET TO THE RESTART INFO PAGE */
  /************************************************************************/
  {
    ndbrequire(pagePtr.i > 0 && pagePtr.i <= MAX_PARALLELL_TUP_SRREQ);
    c_sr_free_page_0 |= (1 << pagePtr.i);
  }

  Uint32 undoFileVersion = TzeroDataPage[ZSRI_UNDO_FILE_VER];
  lliPtr.i = (undoFileVersion << 2) + (regTabPtr.i & 0x3);
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
  riPtr.p->sriLocalLogInfoP = lliPtr.i;

  ndbrequire(regFragPtr.p->fragTableId == regTabPtr.i);
  ndbrequire(regFragPtr.p->fragmentId == fragId);
  
  regFragPtr.p->fragStatus = SYSTEM_RESTART;

  regFragPtr.p->noCopyPagesAlloc = TzeroDataPage[ZSRI_NO_COPY_PAGES_ALLOC];

  riPtr.p->sriCurDataPageFromBuffer = 0;
  riPtr.p->sriNumDataPages = TzeroDataPage[ZSRI_NO_OF_FRAG_PAGES_POS];

  ndbrequire(riPtr.p->sriNumDataPages >= regFragPtr.p->noOfPages);
  const Uint32 pageCount = riPtr.p->sriNumDataPages - regFragPtr.p->noOfPages;
  if(pageCount > 0){
    Uint32 noAllocPages = allocFragPages(regFragPtr.p, pageCount);
    ndbrequireErr(noAllocPages == pageCount, NDBD_EXIT_SR_OUT_OF_DATAMEMORY);
  }//if
  ndbrequire(getNoOfPages(regFragPtr.p) == riPtr.p->sriNumDataPages);

/***************************************************************/
// Set the variables on fragment record which might have been
// affected by allocFragPages.
/***************************************************************/

  regFragPtr.p->emptyPrimPage = TzeroDataPage[ZSRI_EMPTY_PRIM_PAGE];
  regFragPtr.p->thFreeFirst = TzeroDataPage[ZSRI_TH_FREE_FIRST];
  regFragPtr.p->thFreeCopyFirst = TzeroDataPage[ZSRI_TH_FREE_COPY_FIRST];

/***************************************************************/
/* THE RESTART INFORMATION IS NOW READ INTO THE DATA BUFFER    */
/* USE THE RESTART INFORMATION TO INITIATE THE FRAGMENT        */
/***************************************************************/
  /**
   * IF THIS UNDO FILE IS NOT OPEN, IT WILL BE OPENED HERE AND THE EXECUTION 
   * WILL CONTINUE WHEN THE FSOPENCONF IS ENTERED.
   * IF IT'S ALREADY IN USE THE EXECUTION WILL CONTINUE BY A 
   * CONTINUE B SIGNAL
   */
  if (lliPtr.p->lliActiveLcp == 0) {
    PendingFileOpenInfoPtr pfoiPtr;
    ljam();
/***************************************************************/
/* OPEN THE UNDO FILE FOR READ                                 */
/* THE FILE HANDLE WILL BE SET IN THE LOCAL_LOG_INFO_REC       */
/* UPON FSOPENCONF                                             */
/***************************************************************/
    cnoOfLocalLogInfo++;
                                                /* F_LEVEL NOT USED              */
    Uint32 fileType = 1;                        /* VERSION                       */
    fileType = (fileType << 8) | 2;	        /* .LOCLOG                       */
    fileType = (fileType << 8) | 6;	        /* D6                            */
    fileType = (fileType << 8) | 0xff;	        /* DON'T USE P DIRECTORY LEVEL   */
    Uint32 fileFlag = 0;	                /* READ ONLY                     */

    seizePendingFileOpenInfoRecord(pfoiPtr);
    pfoiPtr.p->pfoOpenType = LCP_UNDO_FILE_READ;
    pfoiPtr.p->pfoRestartInfoP = riPtr.i;

    signal->theData[0] = cownref;
    signal->theData[1] = pfoiPtr.i;
    signal->theData[2] = lliPtr.i;
    signal->theData[3] = 0xFFFFFFFF;
    signal->theData[4] = undoFileVersion;
    signal->theData[5] = fileType;
    signal->theData[6] = fileFlag;
    sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);

    lliPtr.p->lliPrevRecordId = 0;
    lliPtr.p->lliActiveLcp = 1;
    lliPtr.p->lliNumFragments = 1;
  } else {
    ljam();
    signal->theData[0] = ZCONT_LOAD_DP;
    signal->theData[1] = riPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    lliPtr.p->lliNumFragments++;
  }//if
   /* RETAIN THE HIGH- AND LOWSCORE ID:S OF THE LOGRECORD POSITIONS. WE HAVE TO EXECUTE THE  */
   /* UNDO LOG BETWEEN THE END AND START RECORDS FOR ALL RECORDS THAT INCLUDE FRAGMENTS OF   */
   /* THE RIGHT CHECKPOINT VERSION TO COMPLETE THE OPERATION WE HAVE TO RUN ALL LOGS THAT    */
   /* HAS THE NUMBER OF LCP ELEMENT GREATER THAN 0, I.E. IS INCLUDED.                        */
  if (TzeroDataPage[ZSRI_UNDO_LOG_END_REC_ID] > lliPtr.p->lliPrevRecordId) {
    ljam();
    lliPtr.p->lliPrevRecordId = TzeroDataPage[ZSRI_UNDO_LOG_END_REC_ID];
    lliPtr.p->lliEndPageId = TzeroDataPage[ZSRI_UNDO_LOG_END_PAGE_ID];
  }//if
  return;
}//Dbtup::rfrInitRestartInfoLab()

/***************************************************************/
/* LOAD THE NEXT DATA PAGE SEGMENT INTO MEMORY                 */
/***************************************************************/
void Dbtup::rfrLoadDataPagesLab(Signal* signal, RestartInfoRecordPtr riPtr, DiskBufferSegmentInfoPtr dbsiPtr) 
{
  FragrecordPtr regFragPtr;

  if (riPtr.p->sriCurDataPageFromBuffer >= riPtr.p->sriNumDataPages) {
    ljam();
    rfrCompletedLab(signal, riPtr);
    return;
  }//if
  Uint32 startPage = riPtr.p->sriCurDataPageFromBuffer;
  Uint32 endPage;
  if ((startPage + ZDB_SEGMENT_SIZE) < riPtr.p->sriNumDataPages) {
    ljam();
    endPage = startPage + ZDB_SEGMENT_SIZE;
  } else {
    ljam();
    endPage = riPtr.p->sriNumDataPages;
  }//if
  regFragPtr.i = riPtr.p->sriFragP;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
  ndbrequire((endPage - startPage) <= 16);
  Uint32 i = 0;
  for (Uint32 pageId = startPage; pageId < endPage; pageId++) {
    ljam();
    dbsiPtr.p->pdxDataPage[i] = getRealpid(regFragPtr.p, pageId);
    i++;
  }//for
  dbsiPtr.p->pdxNumDataPages = endPage - startPage;	/* SET THE NUMBER OF DATA PAGES */
  riPtr.p->sriCurDataPageFromBuffer = endPage;
  dbsiPtr.p->pdxFilePage = startPage + 1;
  rfrReadNextDataSegment(signal, riPtr, dbsiPtr);
  return;
}//Dbtup::rfrLoadDataPagesLab()

void Dbtup::rfrCompletedLab(Signal* signal, RestartInfoRecordPtr riPtr) 
{
  PendingFileOpenInfoPtr pfoPtr;
/* ---------------------------------------------------------------------- */
/*       CLOSE THE DATA FILE BEFORE SENDING TUP_SRCONF                    */
/* ---------------------------------------------------------------------- */
  seizePendingFileOpenInfoRecord(pfoPtr);
  pfoPtr.p->pfoOpenType = LCP_DATA_FILE_READ;
  pfoPtr.p->pfoCheckpointInfoP = riPtr.i;

  signal->theData[0] = riPtr.p->sriDataFileHandle;
  signal->theData[1] = cownref;
  signal->theData[2] = pfoPtr.i;
  signal->theData[3] = 0;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
}//Dbtup::rfrCompletedLab()

void Dbtup::rfrClosedDataFileLab(Signal* signal, Uint32 restartIndex) 
{
  RestartInfoRecordPtr riPtr;
  DiskBufferSegmentInfoPtr dbsiPtr;

  riPtr.i = restartIndex;
  ptrCheckGuard(riPtr, cnoOfRestartInfoRec, restartInfoRecord);
  riPtr.p->sriDataFileHandle = RNIL;
  dbsiPtr.i = riPtr.p->sriDataBufferSegmentP;
  ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
  releaseDiskBufferSegmentRecord(dbsiPtr);
  signal->theData[0] = riPtr.p->sriUserptr;
  signal->theData[1] = riPtr.p->sriFragP;
  sendSignal(riPtr.p->sriBlockref, GSN_TUP_SRCONF, signal, 2, JBB);
  releaseRestartInfoRecord(riPtr);
}//Dbtup::rfrClosedDataFileLab()

/* ---------------------------------------------------------------- */
/* ---------------------- EXECUTE LOCAL LOG  ---------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execSTART_RECREQ(Signal* signal) 
{
  ljamEntry();
  clqhUserpointer = signal->theData[0];
  clqhBlockref = signal->theData[1];

  for (int i = 0; i < ZNO_CHECKPOINT_RECORDS; i++){
    cSrUndoRecords[i] = 0;
  }//for

  if (cnoOfLocalLogInfo == 0) {
    ljam();
/* ---------------------------------------------------------------- */
/*       THERE WERE NO LOCAL LOGS TO EXECUTE IN THIS SYSTEM RESTART */
/* ---------------------------------------------------------------- */
    xlcRestartCompletedLab(signal);
    return;
  }//if
  LocalLogInfoPtr lliPtr;
  for (lliPtr.i = 0; lliPtr.i < 16; lliPtr.i++) {
    ljam();
    ptrAss(lliPtr, localLogInfo);
    if (lliPtr.p->lliActiveLcp == 1) {
      ljam();
      signal->theData[0] = ZSTART_EXEC_UNDO_LOG;
      signal->theData[1] = lliPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    }//if
  }//for
  return;
}//Dbtup::execSTART_RECREQ()

void Dbtup::closeExecUndoLogLab(Signal* signal, LocalLogInfoPtr lliPtr) 
{
  PendingFileOpenInfoPtr pfoPtr;
/* ---------------------------------------------------------------------- */
/*       CLOSE THE UNDO LOG BEFORE COMPLETION OF THE SYSTEM RESTART       */
/* ---------------------------------------------------------------------- */
  seizePendingFileOpenInfoRecord(pfoPtr);
  pfoPtr.p->pfoOpenType = LCP_UNDO_FILE_READ;
  pfoPtr.p->pfoCheckpointInfoP = lliPtr.i;

  signal->theData[0] = lliPtr.p->lliUndoFileHandle;
  signal->theData[1] = cownref;
  signal->theData[2] = pfoPtr.i;
  signal->theData[3] = 0;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
  return;
}//Dbtup::closeExecUndoLogLab()

void Dbtup::endExecUndoLogLab(Signal* signal, Uint32 lliIndex) 
{
  DiskBufferSegmentInfoPtr dbsiPtr;
  LocalLogInfoPtr lliPtr;

  lliPtr.i = lliIndex;
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
  lliPtr.p->lliUndoFileHandle = RNIL;
  lliPtr.p->lliActiveLcp = 0;
/* ---------------------------------------------------------------------- */
/*       WE HAVE NOW CLOSED THE LOG. WE WAIT FOR ALL LOCAL LOGS TO        */
/*       COMPLETE LOG EXECUTION BEFORE SENDING THE RESPONSE TO LQH.       */
/* ---------------------------------------------------------------------- */
  dbsiPtr.i = lliPtr.p->lliUndoBufferSegmentP;
  ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
  freeDiskBufferSegmentRecord(signal, dbsiPtr);
  lliPtr.p->lliUndoBufferSegmentP = RNIL;
  for (lliPtr.i = 0; lliPtr.i < 16; lliPtr.i++) {
    ljam();
    ptrAss(lliPtr, localLogInfo);
    if (lliPtr.p->lliActiveLcp == 1) {
      ljam();
      return;
    }//if
  }//for
  xlcRestartCompletedLab(signal);
  return;
}//Dbtup::endExecUndoLogLab()

void Dbtup::xlcRestartCompletedLab(Signal* signal) 
{
  cnoOfLocalLogInfo = 0;

  signal->theData[0] = NDB_LE_UNDORecordsExecuted;
  signal->theData[1] = DBTUP; // From block
  signal->theData[2] = 0;     // Total records executed
  for (int i = 0; i < 10; i++) {
    if (i < ZNO_CHECKPOINT_RECORDS){
      signal->theData[i+3] = cSrUndoRecords[i];
      signal->theData[2] += cSrUndoRecords[i]; 
    } else {
      signal->theData[i+3] = 0; // Unsused data
    }//if
  }//for
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 12, JBB);

/* ---------------------------------------------------------------------- */
/*       ALL LOCAL LOGS HAVE COMPLETED. WE HAVE COMPLETED OUR PART OF THE */
/*       SYSTEM RESTART.                                                  */
/* ---------------------------------------------------------------------- */
  signal->theData[0] = clqhUserpointer;
  sendSignal(clqhBlockref, GSN_START_RECCONF, signal, 1, JBB);
  return;
}//Dbtup::xlcRestartCompletedLab()

void Dbtup::startExecUndoLogLab(Signal* signal, Uint32 lliIndex) 
{
  DiskBufferSegmentInfoPtr dbsiPtr;
  LocalLogInfoPtr lliPtr;

/* ---------------------------------------------------------------------- */
/*       START EXECUTING THE LOG FOR THIS PART. WE BEGIN BY READING THE   */
/*       LAST 16 PAGES.                                                   */
/* ---------------------------------------------------------------------- */
   /* SET THE PREVIOS RECORD TO THE LAST ONE BECAUSE THAT'S WHERE TO START */
  lliPtr.i = lliIndex;
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);

  allocRestartUndoBufferSegment(signal, dbsiPtr, lliPtr);
  lliPtr.p->lliUndoBufferSegmentP = dbsiPtr.i;
  dbsiPtr.p->pdxCheckpointInfoP = lliPtr.i;
  if (lliPtr.p->lliEndPageId > ((2 * ZUB_SEGMENT_SIZE) - 1)) {
    ljam();
    dbsiPtr.p->pdxNumDataPages = ZUB_SEGMENT_SIZE;
    dbsiPtr.p->pdxFilePage = lliPtr.p->lliEndPageId - (ZUB_SEGMENT_SIZE - 1);
  } else if (lliPtr.p->lliEndPageId > (ZUB_SEGMENT_SIZE - 1)) {
    ljam();
    dbsiPtr.p->pdxNumDataPages = lliPtr.p->lliEndPageId - (ZUB_SEGMENT_SIZE - 1);
    dbsiPtr.p->pdxFilePage = ZUB_SEGMENT_SIZE;
  } else {
    ljam();
    dbsiPtr.p->pdxNumDataPages = lliPtr.p->lliEndPageId + 1;
    dbsiPtr.p->pdxFilePage = 0;
    rfrReadNextUndoSegment(signal, dbsiPtr, lliPtr);
    return;
  }//if
  rfrReadFirstUndoSegment(signal, dbsiPtr, lliPtr);
  return;
}//Dbtup::startExecUndoLogLab()

void Dbtup::rfrReadSecondUndoLogLab(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr) 
{
  LocalLogInfoPtr lliPtr;
  lliPtr.i = dbsiPtr.p->pdxCheckpointInfoP;
  ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);

  dbsiPtr.p->pdxNumDataPages = ZUB_SEGMENT_SIZE;
  dbsiPtr.p->pdxFilePage -= ZUB_SEGMENT_SIZE;
  rfrReadNextUndoSegment(signal, dbsiPtr, lliPtr);
  return;
}//Dbtup::rfrReadSecondUndoLogLab()

void Dbtup::readExecUndoLogLab(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr, LocalLogInfoPtr lliPtr) 
{
/* ---------------------------------------------------------------------- */
/*       THE NEXT UNDO LOG RECORD HAS NOT BEEN READ FROM DISK YET. WE WILL*/
/*       READ UPTO 8 PAGES BACKWARDS OF THE UNDO LOG FILE. WE WILL KEEP   */
/*       THE LAST 8 PAGES TO ENSURE THAT WE WILL BE ABLE TO READ THE NEXT */
/*       LOG RECORD EVEN IF IT SPANS UPTO 8 PAGES.                        */
/* ---------------------------------------------------------------------- */
  if (dbsiPtr.p->pdxFilePage >= ZUB_SEGMENT_SIZE) {
    ljam();
    for (Uint32 i = 0; i < ZUB_SEGMENT_SIZE; i++) {
      ljam();
      Uint32 savePageId = dbsiPtr.p->pdxDataPage[i + ZUB_SEGMENT_SIZE];
      dbsiPtr.p->pdxDataPage[i + ZUB_SEGMENT_SIZE] = dbsiPtr.p->pdxDataPage[i];
      dbsiPtr.p->pdxDataPage[i] = savePageId;
    }//for
    dbsiPtr.p->pdxNumDataPages = ZUB_SEGMENT_SIZE;
    dbsiPtr.p->pdxFilePage = dbsiPtr.p->pdxFilePage - ZUB_SEGMENT_SIZE;
  } else {
    ljam();
    Uint32 dataPages[16];
    ndbrequire(dbsiPtr.p->pdxFilePage > 0);
    ndbrequire(dbsiPtr.p->pdxFilePage <= ZUB_SEGMENT_SIZE);
    Uint32 i;
    for (i = 0; i < dbsiPtr.p->pdxFilePage; i++) {
      ljam();
      dataPages[i] = dbsiPtr.p->pdxDataPage[i + ZUB_SEGMENT_SIZE];
    }//for
    for (i = 0; i < ZUB_SEGMENT_SIZE; i++) {
      ljam();
      dataPages[i + dbsiPtr.p->pdxFilePage] = dbsiPtr.p->pdxDataPage[i];
    }//for
    Uint32 limitLoop = ZUB_SEGMENT_SIZE + dbsiPtr.p->pdxFilePage;
    for (i = 0; i < limitLoop; i++) {
      ljam();
      dbsiPtr.p->pdxDataPage[i] = dataPages[i];
    }//for
    dbsiPtr.p->pdxNumDataPages = dbsiPtr.p->pdxFilePage;
    dbsiPtr.p->pdxFilePage = 0;
  }//if
  rfrReadNextUndoSegment(signal, dbsiPtr, lliPtr);
  return;
}//Dbtup::readExecUndoLogLab()

void Dbtup::rfrReadNextDataSegment(Signal* signal, RestartInfoRecordPtr riPtr, DiskBufferSegmentInfoPtr dbsiPtr) 
{
  dbsiPtr.p->pdxRestartInfoP = riPtr.i;
  dbsiPtr.p->pdxOperation = CHECKPOINT_DATA_READ;
  ndbrequire(dbsiPtr.p->pdxNumDataPages <= 8);

  signal->theData[0] = riPtr.p->sriDataFileHandle;
  signal->theData[1] = cownref;
  signal->theData[2] = dbsiPtr.i;
  signal->theData[3] = 2;
  signal->theData[4] = ZBASE_ADDR_PAGE_WORD;
  signal->theData[5] = dbsiPtr.p->pdxNumDataPages;
  for (Uint32 i = 0; i < dbsiPtr.p->pdxNumDataPages; i++) {
    ljam();
    signal->theData[6 + i] = dbsiPtr.p->pdxDataPage[i];
  }//for
  signal->theData[6 + dbsiPtr.p->pdxNumDataPages] = dbsiPtr.p->pdxFilePage;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 15, JBA);
}//Dbtup::rfrReadNextDataSegment()

/* ---------------------------------------------------------------- */
/* ------------------- RFR_READ_FIRST_UNDO_SEGMENT ---------------- */
/* ---------------------------------------------------------------- */
/* THIS ROUTINE READS IN THE FIRST UNDO SEGMENT INTO THE CURRENTLY  */
/* ACTIVE UNDO BUFFER SEGMENT                                       */
/* -----------------------------------------------------------------*/
void Dbtup::rfrReadFirstUndoSegment(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr, LocalLogInfoPtr lliPtr) 
{
  dbsiPtr.p->pdxOperation = CHECKPOINT_UNDO_READ_FIRST;

  signal->theData[0] = lliPtr.p->lliUndoFileHandle;
  signal->theData[1] = cownref;
  signal->theData[2] = dbsiPtr.i;
  signal->theData[3] = 1;
  signal->theData[4] = ZBASE_ADDR_UNDO_WORD;
  signal->theData[5] = dbsiPtr.p->pdxNumDataPages;
  signal->theData[6] = dbsiPtr.p->pdxDataPage[ZUB_SEGMENT_SIZE];
  signal->theData[7] = dbsiPtr.p->pdxFilePage;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 8, JBA);
}//Dbtup::rfrReadFirstUndoSegment()

/* ---------------------------------------------------------------- */
/* ------------------- RFR_READ_NEXT_UNDO_SEGMENT ----------------- */
/* ---------------------------------------------------------------- */
/* THIS ROUTINE READS IN THE NEXT UNDO SEGMENT INTO THE CURRENTLY   */
/* ACTIVE UNDO BUFFER SEGMENT AND SWITCH TO THE UNACTIVE, READY ONE */
/* -----------------------------------------------------------------*/
void Dbtup::rfrReadNextUndoSegment(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr, LocalLogInfoPtr lliPtr) 
{
  dbsiPtr.p->pdxOperation = CHECKPOINT_UNDO_READ;

  signal->theData[0] = lliPtr.p->lliUndoFileHandle;
  signal->theData[1] = cownref;
  signal->theData[2] = dbsiPtr.i;
  signal->theData[3] = 1;
  signal->theData[4] = ZBASE_ADDR_UNDO_WORD;
  signal->theData[5] = dbsiPtr.p->pdxNumDataPages;
  signal->theData[6] = dbsiPtr.p->pdxDataPage[0];
  signal->theData[7] = dbsiPtr.p->pdxFilePage;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 8, JBA);
}//Dbtup::rfrReadNextUndoSegment()

void Dbtup::xlcGetNextRecordLab(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr, LocalLogInfoPtr lliPtr) 
{
  Uint32 loopCount = 0;
/* ---------------------------------------------------------------------- */
/*       EXECUTE A NEW SET OF UNDO LOG RECORDS.                           */
/* ---------------------------------------------------------------------- */
  XlcStruct xlcStruct;

  xlcStruct.LliPtr = lliPtr;
  xlcStruct.DbsiPtr = dbsiPtr;

  do {
    ljam();
    loopCount++;
    if (loopCount == 20) {
      ljam();
      signal->theData[0] = ZCONT_EXECUTE_LC;
      signal->theData[1] = xlcStruct.LliPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      return;
    }//if
    if (xlcStruct.LliPtr.p->lliPrevRecordId == 0) {
      ljam();
      closeExecUndoLogLab(signal, xlcStruct.LliPtr);
      return;
    }//if
    xlcStruct.PageId = xlcStruct.LliPtr.p->lliPrevRecordId >> ZUNDO_RECORD_ID_PAGE_INDEX;
    xlcStruct.PageIndex = xlcStruct.LliPtr.p->lliPrevRecordId & ZUNDO_RECORD_ID_PAGE_INDEX_MASK;
    if (xlcStruct.PageId < xlcStruct.DbsiPtr.p->pdxFilePage) {
      ljam();
      readExecUndoLogLab(signal, xlcStruct.DbsiPtr, xlcStruct.LliPtr);
      return;
    }//if
    ndbrequire((xlcStruct.PageId - xlcStruct.DbsiPtr.p->pdxFilePage) < 16);
    xlcStruct.UPPtr.i = xlcStruct.DbsiPtr.p->pdxDataPage[xlcStruct.PageId - xlcStruct.DbsiPtr.p->pdxFilePage];
    ptrCheckGuard(xlcStruct.UPPtr, cnoOfUndoPage, undoPage);
    xlcGetLogHeader(xlcStruct);
    getFragmentrec(xlcStruct.FragPtr, xlcStruct.FragId, xlcStruct.TabPtr.p);
    if (xlcStruct.FragPtr.i == RNIL) {
      ljam();
      continue;
    }//if
    if (xlcStruct.FragPtr.p->fragStatus != SYSTEM_RESTART) {
      ljam();
      continue;
    }//if
    ndbrequire(xlcStruct.LogRecordType < ZNO_CHECKPOINT_RECORDS);
    cSrUndoRecords[xlcStruct.LogRecordType]++;
    switch (xlcStruct.LogRecordType) {
    case ZLCPR_TYPE_INSERT_TH:
      ljam();
      xlcInsertTh(xlcStruct);
      break;
    case ZLCPR_TYPE_DELETE_TH:
      ljam();
      xlcDeleteTh(xlcStruct);
      break;
    case ZLCPR_TYPE_UPDATE_TH:
      ljam();
      xlcUpdateTh(xlcStruct);
      break;
    case ZLCPR_TYPE_INSERT_TH_NO_DATA:
      ljam();
      xlcInsertTh(xlcStruct);
      break;
    case ZLCPR_ABORT_UPDATE:
      ljam();
      xlcAbortUpdate(signal, xlcStruct);
      break;
    case ZLCPR_ABORT_INSERT:
      ljam();
      xlcAbortInsert(signal, xlcStruct);
      break;
    case ZTABLE_DESCRIPTOR:
      ljam();
      xlcTableDescriptor(xlcStruct);
      if (xlcStruct.LliPtr.p->lliNumFragments == 0) {
        ljam();
        closeExecUndoLogLab(signal, xlcStruct.LliPtr);
        return;
      }//if
      break;
    case ZLCPR_UNDO_LOG_PAGE_HEADER:
      ljam();
      xlcUndoLogPageHeader(xlcStruct);
      break;
    case ZINDICATE_NO_OP_ACTIVE:
      ljam();
      xlcIndicateNoOpActive(xlcStruct);
      break;
    case ZLCPR_TYPE_UPDATE_GCI:
      ljam();
      xlcUpdateGCI(xlcStruct);
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
  } while (1);
}//Dbtup::xlcGetNextRecordLab()

/* ---------------------------------------------------------------- */
/* -----------------   XLC_GET_LOG_HEADER    ---------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::xlcGetLogHeader(XlcStruct& xlcStruct) 
{
  Uint32 pIndex = xlcStruct.PageIndex;
  Uint32 fragId;
  Uint32 tableId;
  Uint32 prevId;
  if ((pIndex + 4) < ZWORDS_ON_PAGE) {
    UndoPage* const regUndoPagePtr = xlcStruct.UPPtr.p;
    ljam();
    xlcStruct.LogRecordType = regUndoPagePtr->undoPageWord[pIndex];
    prevId = regUndoPagePtr->undoPageWord[pIndex + 1];
    tableId = regUndoPagePtr->undoPageWord[pIndex + 2];
    fragId = regUndoPagePtr->undoPageWord[pIndex + 3];
    xlcStruct.PageIndex = pIndex + 4;
  } else {
    ljam();
    xlcStruct.LogRecordType = xlcGetLogWord(xlcStruct);
    prevId = xlcGetLogWord(xlcStruct);
    tableId = xlcGetLogWord(xlcStruct);
    fragId = xlcGetLogWord(xlcStruct);
  }//if
  xlcStruct.LliPtr.p->lliPrevRecordId = prevId;
  xlcStruct.FragId = fragId;
  xlcStruct.TabPtr.i = tableId;
  ptrCheckGuard(xlcStruct.TabPtr, cnoOfTablerec, tablerec);
}//Dbtup::xlcGetLogHeader()

/* ------------------------------------------------------------------- */
/* ---------------------- XLC_GET_LOG_WORD --------------------------- */
/* ------------------------------------------------------------------- */
Uint32 Dbtup::xlcGetLogWord(XlcStruct& xlcStruct) 
{
  Uint32 pIndex = xlcStruct.PageIndex;
  ndbrequire(xlcStruct.UPPtr.p != NULL);
  ndbrequire(pIndex < ZWORDS_ON_PAGE);
  Uint32 logWord = xlcStruct.UPPtr.p->undoPageWord[pIndex];
  pIndex++;
  xlcStruct.PageIndex = pIndex;
  if (pIndex == ZWORDS_ON_PAGE) {
    ljam();
    xlcStruct.PageIndex = ZUNDO_PAGE_HEADER_SIZE;
    xlcStruct.PageId++;
    if ((xlcStruct.PageId - xlcStruct.DbsiPtr.p->pdxFilePage) >= (2 * ZUB_SEGMENT_SIZE)) {
      ljam();
      xlcStruct.UPPtr.i = RNIL;
      ptrNull(xlcStruct.UPPtr);
    } else {
      ljam();
      Uint32 index = xlcStruct.PageId - xlcStruct.DbsiPtr.p->pdxFilePage;
      ndbrequire(index < 16);
      xlcStruct.UPPtr.i = xlcStruct.DbsiPtr.p->pdxDataPage[index];
      ptrCheckGuard(xlcStruct.UPPtr, cnoOfUndoPage, undoPage);
    }//if
  }//if
  return logWord;
}//Dbtup::xlcGetLogWord()

   /****************************************************/
   /* INSERT A TUPLE HEADER THE DATA IS THE TUPLE DATA */
   /****************************************************/
void Dbtup::xlcInsertTh(XlcStruct& xlcStruct) 
{
  PagePtr pagePtr;
  Fragrecord* const regFragPtr = xlcStruct.FragPtr.p;
  Tablerec* const regTabPtr = xlcStruct.TabPtr.p;

  Uint32 fragPageId = xlcGetLogWord(xlcStruct);
  Uint32 pageIndex = xlcGetLogWord(xlcStruct);
  ndbrequire((pageIndex & 1) == 0);
  pagePtr.i = getRealpid(regFragPtr, fragPageId);
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 pageOffset;
  getThAtPageSr(pagePtr.p, pageOffset);
  ndbrequire(pageOffset == (ZPAGE_HEADER_SIZE + (regTabPtr->tupheadsize * (pageIndex >> 1))));
  if (xlcStruct.LogRecordType == ZLCPR_TYPE_INSERT_TH) {
    ljam();
    xlcCopyData(xlcStruct, pageOffset, regTabPtr->tupheadsize, pagePtr);
  } else {
    ndbrequire(xlcStruct.LogRecordType == ZLCPR_TYPE_INSERT_TH_NO_DATA);
    ljam();
  }//if
/* ----------------------------------------*/
/* INDICATE THAT NO OPERATIONS ACTIVE      */
/* ----------------------------------------*/
  ndbrequire(pageOffset < ZWORDS_ON_PAGE);
  pagePtr.p->pageWord[pageOffset] = RNIL;
}//Dbtup::xlcInsertTh()

   /**********************************************/
   /* DELETE A TUPLE HEADER - NO ADDITIONAL DATA */
   /**********************************************/
void Dbtup::xlcDeleteTh(XlcStruct& xlcStruct) 
{
  PagePtr pagePtr;
  Fragrecord* const regFragPtr = xlcStruct.FragPtr.p;
  Tablerec* const regTabPtr = xlcStruct.TabPtr.p;

  Uint32 fragPageId = xlcGetLogWord(xlcStruct);
  Uint32 pageIndex = xlcGetLogWord(xlcStruct);
  ndbrequire((pageIndex & 1) == 0);
  pagePtr.i = getRealpid(regFragPtr, fragPageId);
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 pageOffset = ZPAGE_HEADER_SIZE + (regTabPtr->tupheadsize * (pageIndex >> 1));
  freeThSr(regTabPtr, pagePtr.p, pageOffset);
}//Dbtup::xlcDeleteTh()

   /*****************************************************/
   /* UPDATE A TUPLE HEADER, THE DATA IS THE TUPLE DATA */
   /*****************************************************/
void Dbtup::xlcUpdateTh(XlcStruct& xlcStruct) 
{
  PagePtr pagePtr;
  Fragrecord* const regFragPtr = xlcStruct.FragPtr.p;
  Tablerec* const regTabPtr = xlcStruct.TabPtr.p;

  Uint32 fragPageId = xlcGetLogWord(xlcStruct);
  Uint32 pageIndex = xlcGetLogWord(xlcStruct);
  ndbrequire((pageIndex & 1) == 0);
  pagePtr.i = getRealpid(regFragPtr, fragPageId);
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 pageOffset = ZPAGE_HEADER_SIZE + (regTabPtr->tupheadsize * (pageIndex >> 1));
  xlcCopyData(xlcStruct, pageOffset, regTabPtr->tupheadsize, pagePtr);
/* ----------------------------------------*/
/* INDICATE THAT NO OPERATIONS ACTIVE      */
/* ----------------------------------------*/
  ndbrequire(pageOffset < ZWORDS_ON_PAGE);
  pagePtr.p->pageWord[pageOffset] = RNIL;
}//Dbtup::xlcUpdateTh()

   /**************************************************/
   /* ABORT AN INSERT OPERATION - NO ADDITIONAL DATA */
   /**************************************************/
void Dbtup::xlcAbortInsert(Signal* signal, XlcStruct& xlcStruct) 
{
  PagePtr pagePtr;
  Fragrecord* const regFragPtr = xlcStruct.FragPtr.p;
  Tablerec* const regTabPtr = xlcStruct.TabPtr.p;

  Uint32 fragPageId = xlcGetLogWord(xlcStruct);
  Uint32 pageIndex = xlcGetLogWord(xlcStruct);
  ndbrequire((pageIndex & 1) == 0);
  pagePtr.i = getRealpid(regFragPtr, fragPageId);
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 pageOffset = ZPAGE_HEADER_SIZE + (regTabPtr->tupheadsize * (pageIndex >> 1));
  freeTh(regFragPtr, regTabPtr, signal, pagePtr.p, pageOffset);
}//Dbtup::xlcAbortInsert()

   /*****************************************************/
   /* COPY DATA FROM COPY TUPLE TO ORIGINAL TUPLE       */
   /*****************************************************/
void Dbtup::xlcAbortUpdate(Signal* signal, XlcStruct& xlcStruct) 
{
  PagePtr pagePtr;
  Fragrecord* const regFragPtr = xlcStruct.FragPtr.p;
  Tablerec* const regTabPtr = xlcStruct.TabPtr.p;
  Uint32 tuple_size = regTabPtr->tupheadsize;

  Uint32 fragPageIdC = xlcGetLogWord(xlcStruct);
  Uint32 pageIndexC = xlcGetLogWord(xlcStruct);
  ndbrequire((pageIndexC & 1) == 0);
  Uint32 TdestPageId = getRealpid(regFragPtr, fragPageIdC);
  Uint32 TcmDestIndex = ZPAGE_HEADER_SIZE +
                       (tuple_size * (pageIndexC >> 1));

  Uint32 fragPageId = xlcGetLogWord(xlcStruct);
  Uint32 pageIndex = xlcGetLogWord(xlcStruct);
  ndbrequire((pageIndex & 1) == 0);
  Uint32 TsourcePageId = getRealpid(regFragPtr, fragPageId);
  Uint32 TcmSourceIndex = ZPAGE_HEADER_SIZE +
                         (tuple_size * (pageIndex >> 1));
  Uint32 end_source = tuple_size + TcmSourceIndex;
  Uint32 end_dest = tuple_size + TcmDestIndex;

  void* Tdestination = (void*)&page[TdestPageId].pageWord[TcmDestIndex + 1]; 
  const void* Tsource = 
    (void*)&page[TsourcePageId].pageWord[TcmSourceIndex + 1];

  ndbrequire(TsourcePageId < cnoOfPage &&
             TdestPageId < cnoOfPage &&
             end_source <= ZWORDS_ON_PAGE &&
             end_dest <= ZWORDS_ON_PAGE);
  MEMCOPY_NO_WORDS(Tdestination, Tsource, (tuple_size - 1));

  pagePtr.i = TsourcePageId;
  ptrAss(pagePtr, page);
  freeTh(regFragPtr, regTabPtr, signal, pagePtr.p, TcmSourceIndex);

  pagePtr.i = TdestPageId;
  ptrAss(pagePtr, page);
  pagePtr.p->pageWord[TcmDestIndex] = RNIL;
}//Dbtup::xlcAbortUpdate()

   /*****************************/
   /* RESTORE UPDATED GCI VALUE */
   /*****************************/
void Dbtup::xlcUpdateGCI(XlcStruct& xlcStruct) 
{
  PagePtr pagePtr;
  Fragrecord* const regFragPtr = xlcStruct.FragPtr.p;
  Tablerec* const regTabPtr = xlcStruct.TabPtr.p;

  Uint32 fragPageId = xlcGetLogWord(xlcStruct);
  Uint32 pageIndex = xlcGetLogWord(xlcStruct);
  Uint32 restoredGCI = xlcGetLogWord(xlcStruct);

  ndbrequire((pageIndex & 1) == 0);
  pagePtr.i = getRealpid(regFragPtr, fragPageId);
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 pageOffset = ZPAGE_HEADER_SIZE + (regTabPtr->tupheadsize * (pageIndex >> 1));
  Uint32 gciOffset = pageOffset + regTabPtr->tupGCPIndex;
  ndbrequire((gciOffset < ZWORDS_ON_PAGE) &&
             (regTabPtr->tupGCPIndex < regTabPtr->tupheadsize));
  pagePtr.p->pageWord[gciOffset] = restoredGCI;
}//Dbtup::xlcUpdateGCI()

   /*****************************************************/
   /* READ TABLE DESCRIPTOR FROM UNDO LOG               */
   /*****************************************************/
void Dbtup::xlcTableDescriptor(XlcStruct& xlcStruct) 
{
  xlcStruct.LliPtr.p->lliNumFragments--;
  xlcStruct.FragPtr.p->fragStatus = ACTIVE;
}//Dbtup::xlcTableDescriptor()

   /********************************************************/
   /* UPDATE PAGE STATE AND NEXT POINTER IN PAGE           */
   /********************************************************/
void Dbtup::xlcUndoLogPageHeader(XlcStruct& xlcStruct) 
{
  Fragrecord* const regFragPtr = xlcStruct.FragPtr.p;
  PagePtr xlcPagep;

  Uint32 fragPageId = xlcGetLogWord(xlcStruct);
  xlcPagep.i = getRealpid(regFragPtr, fragPageId);
  ptrCheckGuard(xlcPagep, cnoOfPage, page);
  Uint32 logWord = xlcGetLogWord(xlcStruct);
  ndbrequire(logWord != 0);
  ndbrequire(logWord <= ZAC_MM_FREE_COPY);

  xlcPagep.p->pageWord[ZPAGE_STATE_POS] = logWord;
  xlcPagep.p->pageWord[ZPAGE_NEXT_POS] = xlcGetLogWord(xlcStruct);
}//Dbtup::xlcUndoLogPageHeader()

   /********************************************************/
   /* INDICATE THAT NO OPERATIONS ACTIVE                   */
   /********************************************************/
void Dbtup::xlcIndicateNoOpActive(XlcStruct& xlcStruct) 
{
  PagePtr pagePtr;
  Fragrecord* const regFragPtr = xlcStruct.FragPtr.p;
  Tablerec* const regTabPtr = xlcStruct.TabPtr.p;

  Uint32 fragPageId = xlcGetLogWord(xlcStruct);
  Uint32 pageIndex = xlcGetLogWord(xlcStruct);
  ndbrequire((pageIndex & 1) == 0);
  pagePtr.i = getRealpid(regFragPtr, fragPageId);
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 pageOffset = ZPAGE_HEADER_SIZE + (regTabPtr->tupheadsize * (pageIndex >> 1));
/* ----------------------------------------*/
/* INDICATE THAT NO OPERATIONS ACTIVE      */
/* ----------------------------------------*/
  ndbrequire(pageOffset < ZWORDS_ON_PAGE);
  pagePtr.p->pageWord[pageOffset] = RNIL;
}//Dbtup::xlcIndicateNoOpActive()

   /********************************************************/
   /* THIS IS THE COMMON ROUTINE TO COPY DATA FROM THE     */
   /* UNDO BUFFER TO THE DATA PAGES. IT USES THE           */
   /* XLC_REQUEST_SEGMENT SUB TO GET MORE DATA WHEN NEEDED */
   /********************************************************/
void Dbtup::xlcCopyData(XlcStruct& xlcStruct, Uint32 pageOffset, Uint32 noOfWords, PagePtr pagePtr) 
{
  ndbrequire((pageOffset + noOfWords - 1) < ZWORDS_ON_PAGE);
  for (Uint32 i = 1; i < noOfWords; i++) {
    ljam();
    pagePtr.p->pageWord[pageOffset + i] = xlcGetLogWord(xlcStruct);
  }//for
}//Dbtup::xlcCopyData()

void Dbtup::allocRestartUndoBufferSegment(Signal* signal, DiskBufferSegmentInfoPtr& dbsiPtr, LocalLogInfoPtr lliPtr) 
{
  UndoPagePtr undoPagePtr;

  ndbrequire(cfirstfreeUndoSeg != RNIL);
  if (cnoFreeUndoSeg == ZMIN_PAGE_LIMIT_TUP_COMMITREQ) {
    EXECUTE_DIRECT(DBLQH, GSN_TUP_COM_BLOCK, signal, 1);
    ljamEntry();
  }//if
  cnoFreeUndoSeg--;
  ndbrequire(cnoFreeUndoSeg >= 0);
  undoPagePtr.i = cfirstfreeUndoSeg;
  ptrCheckGuard(undoPagePtr, cnoOfUndoPage, undoPage);
  cfirstfreeUndoSeg = undoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS];
  undoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS] = RNIL;
  seizeDiskBufferSegmentRecord(dbsiPtr);
  dbsiPtr.p->pdxBuffertype = UNDO_RESTART_PAGES;
  dbsiPtr.p->pdxUndoBufferSet[0] = undoPagePtr.i;
  Uint32 i;
  for (i = 0; i < ZUB_SEGMENT_SIZE; i++) {
    dbsiPtr.p->pdxDataPage[i] = undoPagePtr.i + i;
  }//for

  ndbrequire(cfirstfreeUndoSeg != RNIL);
  if (cnoFreeUndoSeg == ZMIN_PAGE_LIMIT_TUP_COMMITREQ) {
    EXECUTE_DIRECT(DBLQH, GSN_TUP_COM_BLOCK, signal, 1);
    ljamEntry();
  }//if
  cnoFreeUndoSeg--;
  ndbrequire(cnoFreeUndoSeg >= 0);
  undoPagePtr.i = cfirstfreeUndoSeg;
  ptrCheckGuard(undoPagePtr, cnoOfUndoPage, undoPage);
  cfirstfreeUndoSeg = undoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS];
  undoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS] = RNIL;
  dbsiPtr.p->pdxUndoBufferSet[1] = undoPagePtr.i;
//  lliPtr.p->lliUndoPage = undoPagePtr.i;
  for (i = ZUB_SEGMENT_SIZE; i < (2 * ZUB_SEGMENT_SIZE); i++) {
    dbsiPtr.p->pdxDataPage[i] = undoPagePtr.i + (i - ZUB_SEGMENT_SIZE);
  }//for
  return;
}//Dbtup::allocRestartUndoBufferSegment()


