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
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/TupCommit.hpp>
#include <signaldata/TupKey.hpp>

#include <signaldata/DropTab.hpp>

#define DEBUG(x) { ndbout << "TUP::" << x << endl; }

#define ljam() { jamLine(24000 + __LINE__); }
#define ljamEntry() { jamEntryLine(24000 + __LINE__); }

void Dbtup::initData() 
{
  cnoOfAttrbufrec = ZNO_OF_ATTRBUFREC;
  cnoOfLcpRec = ZNO_OF_LCP_REC;
  cnoOfConcurrentOpenOp = ZNO_OF_CONCURRENT_OPEN_OP;
  cnoOfConcurrentWriteOp = ZNO_OF_CONCURRENT_WRITE_OP;
  cnoOfFragoprec = 2 * MAX_FRAG_PER_NODE;
  cnoOfPageRangeRec = ZNO_OF_PAGE_RANGE_REC;
  cnoOfParallellUndoFiles = ZNO_OF_PARALLELL_UNDO_FILES;
  cnoOfRestartInfoRec = ZNO_OF_RESTART_INFO_REC;
  c_maxTriggersPerTable = ZDEFAULT_MAX_NO_TRIGGERS_PER_TABLE;
  c_noOfBuildIndexRec = 32;

  attrbufrec = 0;
  checkpointInfo = 0;
  diskBufferSegmentInfo = 0;
  fragoperrec = 0;
  fragrecord = 0;
  hostBuffer = 0;
  localLogInfo = 0;
  operationrec = 0;
  page = 0;
  pageRange = 0;
  pendingFileOpenInfo = 0;
  restartInfoRecord = 0;
  tablerec = 0;
  tableDescriptor = 0;
  undoPage = 0;
  totNoOfPagesAllocated = 0;
  cnoOfAllocatedPages = 0;
  
  // Records with constant sizes
}//Dbtup::initData()

Dbtup::Dbtup(const class Configuration & conf)
  : SimulatedBlock(DBTUP, conf),
  c_storedProcPool(),
  c_buildIndexList(c_buildIndexPool)
{
  BLOCK_CONSTRUCTOR(Dbtup);

  addRecSignal(GSN_DEBUG_SIG, &Dbtup::execDEBUG_SIG);
  addRecSignal(GSN_CONTINUEB, &Dbtup::execCONTINUEB);

  addRecSignal(GSN_DUMP_STATE_ORD, &Dbtup::execDUMP_STATE_ORD);
  addRecSignal(GSN_SEND_PACKED, &Dbtup::execSEND_PACKED);
  addRecSignal(GSN_ATTRINFO, &Dbtup::execATTRINFO);
  addRecSignal(GSN_STTOR, &Dbtup::execSTTOR);
  addRecSignal(GSN_TUP_LCPREQ, &Dbtup::execTUP_LCPREQ);
  addRecSignal(GSN_END_LCPREQ, &Dbtup::execEND_LCPREQ);
  addRecSignal(GSN_START_RECREQ, &Dbtup::execSTART_RECREQ);
  addRecSignal(GSN_MEMCHECKREQ, &Dbtup::execMEMCHECKREQ);
  addRecSignal(GSN_TUPKEYREQ, &Dbtup::execTUPKEYREQ);
  addRecSignal(GSN_TUPSEIZEREQ, &Dbtup::execTUPSEIZEREQ);
  addRecSignal(GSN_TUPRELEASEREQ, &Dbtup::execTUPRELEASEREQ);
  addRecSignal(GSN_STORED_PROCREQ, &Dbtup::execSTORED_PROCREQ);
  addRecSignal(GSN_TUPFRAGREQ, &Dbtup::execTUPFRAGREQ);
  addRecSignal(GSN_TUP_ADD_ATTRREQ, &Dbtup::execTUP_ADD_ATTRREQ);
  addRecSignal(GSN_TUP_COMMITREQ, &Dbtup::execTUP_COMMITREQ);
  addRecSignal(GSN_TUP_ABORTREQ, &Dbtup::execTUP_ABORTREQ);
  addRecSignal(GSN_TUP_SRREQ, &Dbtup::execTUP_SRREQ);
  addRecSignal(GSN_TUP_PREPLCPREQ, &Dbtup::execTUP_PREPLCPREQ);
  addRecSignal(GSN_FSOPENCONF, &Dbtup::execFSOPENCONF);
  addRecSignal(GSN_FSCLOSECONF, &Dbtup::execFSCLOSECONF);
  addRecSignal(GSN_FSWRITECONF, &Dbtup::execFSWRITECONF);
  addRecSignal(GSN_FSREADCONF, &Dbtup::execFSREADCONF);
  addRecSignal(GSN_NDB_STTOR, &Dbtup::execNDB_STTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbtup::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_SET_VAR_REQ,  &Dbtup::execSET_VAR_REQ);

  // Trigger Signals
  addRecSignal(GSN_CREATE_TRIG_REQ, &Dbtup::execCREATE_TRIG_REQ);
  addRecSignal(GSN_DROP_TRIG_REQ,  &Dbtup::execDROP_TRIG_REQ);

  addRecSignal(GSN_DROP_TAB_REQ, &Dbtup::execDROP_TAB_REQ);
  addRecSignal(GSN_FSREMOVECONF, &Dbtup::execFSREMOVECONF);

  addRecSignal(GSN_TUP_ALLOCREQ, &Dbtup::execTUP_ALLOCREQ);
  addRecSignal(GSN_TUP_DEALLOCREQ, &Dbtup::execTUP_DEALLOCREQ);
  addRecSignal(GSN_TUP_WRITELOG_REQ, &Dbtup::execTUP_WRITELOG_REQ);

  // Ordered index related
  addRecSignal(GSN_BUILDINDXREQ, &Dbtup::execBUILDINDXREQ);

  // Tup scan
  addRecSignal(GSN_ACC_SCANREQ, &Dbtup::execACC_SCANREQ);
  addRecSignal(GSN_NEXT_SCANREQ, &Dbtup::execNEXT_SCANREQ);
  addRecSignal(GSN_ACC_CHECK_SCAN, &Dbtup::execACC_CHECK_SCAN);

  initData();

  attrbufrec = 0;  
  checkpointInfo = 0;  
  diskBufferSegmentInfo = 0;  
  fragoperrec = 0;
  fragrecord = 0;  
  hostBuffer = 0;  
  localLogInfo = 0;  
  operationrec = 0;
  page = 0;  
  pageRange = 0;
  pendingFileOpenInfo = 0;  
  restartInfoRecord = 0;  
  tablerec = 0;  
  tableDescriptor = 0;  
  undoPage = 0;
}//Dbtup::Dbtup()

Dbtup::~Dbtup() 
{
  // Records with dynamic sizes
  deallocRecord((void **)&attrbufrec,"Attrbufrec", 
		sizeof(Attrbufrec), 
		cnoOfAttrbufrec);
  
  deallocRecord((void **)&checkpointInfo,"CheckpointInfo",
		sizeof(CheckpointInfo), 
		cnoOfLcpRec);
  
  deallocRecord((void **)&diskBufferSegmentInfo,
		"DiskBufferSegmentInfo",
		sizeof(DiskBufferSegmentInfo), 
		cnoOfConcurrentWriteOp);
  
  deallocRecord((void **)&fragoperrec,"Fragoperrec",
		sizeof(Fragoperrec),
		cnoOfFragoprec);
  
  deallocRecord((void **)&fragrecord,"Fragrecord",
		sizeof(Fragrecord), 
		cnoOfFragrec);
  
  deallocRecord((void **)&hostBuffer,"HostBuffer",
		sizeof(HostBuffer), 
		MAX_NODES);
  
  deallocRecord((void **)&localLogInfo,"LocalLogInfo",
		sizeof(LocalLogInfo), 
		cnoOfParallellUndoFiles);
  
  deallocRecord((void **)&operationrec,"Operationrec",
		sizeof(Operationrec),
		cnoOfOprec);

  deallocRecord((void **)&page,"Page", 
		sizeof(Page), 
		cnoOfPage);
  
  deallocRecord((void **)&pageRange,"PageRange",
		sizeof(PageRange), 
		cnoOfPageRangeRec);

  deallocRecord((void **)&pendingFileOpenInfo,
		"PendingFileOpenInfo",
		sizeof(PendingFileOpenInfo), 
		cnoOfConcurrentOpenOp);
  
  deallocRecord((void **)&restartInfoRecord,
		"RestartInfoRecord",
		sizeof(RestartInfoRecord),
		cnoOfRestartInfoRec);
  
  deallocRecord((void **)&tablerec,"Tablerec",
		sizeof(Tablerec), 
		cnoOfTablerec);
  
  deallocRecord((void **)&tableDescriptor, "TableDescriptor",
		sizeof(TableDescriptor),
		cnoOfTabDescrRec);
  
  deallocRecord((void **)&undoPage,"UndoPage",
		sizeof(UndoPage),
		cnoOfUndoPage);

}//Dbtup::~Dbtup()

BLOCK_FUNCTIONS(Dbtup)

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ----- GENERAL SIGNAL MULTIPLEXER (FS + CONTINUEB) -------------- */
/* ---------------------------------------------------------------- */
/* **************************************************************** */
void Dbtup::execFSCLOSECONF(Signal* signal) 
{
  PendingFileOpenInfoPtr pfoPtr;
  ljamEntry();
  pfoPtr.i = signal->theData[0];
  ptrCheckGuard(pfoPtr, cnoOfConcurrentOpenOp, pendingFileOpenInfo);
  switch (pfoPtr.p->pfoOpenType) {
  case LCP_DATA_FILE_CLOSE:
  {
    CheckpointInfoPtr ciPtr;
    ljam();
    ciPtr.i = pfoPtr.p->pfoCheckpointInfoP;
    ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
    ciPtr.p->lcpDataFileHandle = RNIL;
    lcpClosedDataFileLab(signal, ciPtr);
    break;
  }
  case LCP_UNDO_FILE_CLOSE:
  {
    LocalLogInfoPtr lliPtr;
    ljam();
    lliPtr.i = pfoPtr.p->pfoCheckpointInfoP;
    ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
    lliPtr.p->lliUndoFileHandle = RNIL;
    lcpEndconfLab(signal);
    break;
  }
  case LCP_UNDO_FILE_READ:
    ljam();
    endExecUndoLogLab(signal, pfoPtr.p->pfoCheckpointInfoP);
    break;
  case LCP_DATA_FILE_READ:
    ljam();
    rfrClosedDataFileLab(signal, pfoPtr.p->pfoCheckpointInfoP);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  releasePendingFileOpenInfoRecord(pfoPtr);
}//Dbtup::execFSCLOSECONF()

void Dbtup::execFSOPENCONF(Signal* signal) 
{
  PendingFileOpenInfoPtr pfoPtr;

  ljamEntry();
  pfoPtr.i = signal->theData[0];
  Uint32 fileHandle = signal->theData[1];
  ptrCheckGuard(pfoPtr, cnoOfConcurrentOpenOp, pendingFileOpenInfo);
  switch (pfoPtr.p->pfoOpenType) {
  case LCP_DATA_FILE_READ:
  {
    RestartInfoRecordPtr riPtr;
    ljam();
    riPtr.i = pfoPtr.p->pfoRestartInfoP;
    ptrCheckGuard(riPtr, cnoOfRestartInfoRec, restartInfoRecord);
    riPtr.p->sriDataFileHandle = fileHandle;
    rfrReadRestartInfoLab(signal, riPtr);
    break;
  }
  case LCP_UNDO_FILE_READ:
  {
    RestartInfoRecordPtr riPtr;
    LocalLogInfoPtr lliPtr;
    DiskBufferSegmentInfoPtr dbsiPtr;

    ljam();
    riPtr.i = pfoPtr.p->pfoRestartInfoP;
    ptrCheckGuard(riPtr, cnoOfRestartInfoRec, restartInfoRecord);
    lliPtr.i = riPtr.p->sriLocalLogInfoP;
    ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
    lliPtr.p->lliUndoFileHandle = fileHandle;
    dbsiPtr.i = riPtr.p->sriDataBufferSegmentP;
    ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
    rfrLoadDataPagesLab(signal, riPtr, dbsiPtr);
    break;
  }
  case LCP_DATA_FILE_WRITE_WITH_UNDO:
  {
    CheckpointInfoPtr ciPtr;
    LocalLogInfoPtr lliPtr;

    ljam();
    ciPtr.i = pfoPtr.p->pfoCheckpointInfoP;
    ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
    lliPtr.i = ciPtr.p->lcpLocalLogInfoP;
    ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
    ciPtr.p->lcpDataFileHandle = fileHandle;
    if (lliPtr.p->lliUndoFileHandle != RNIL) {
      ljam();
      signal->theData[0] = ciPtr.p->lcpUserptr;
      signal->theData[1] = ciPtr.i;
      sendSignal(ciPtr.p->lcpBlockref, GSN_TUP_PREPLCPCONF, signal, 2, JBB);
    }//if
    break;
  }
  case LCP_DATA_FILE_WRITE:
  {
    CheckpointInfoPtr ciPtr;

    ljam();
    ciPtr.i = pfoPtr.p->pfoCheckpointInfoP;
    ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
    ciPtr.p->lcpDataFileHandle = fileHandle;
    signal->theData[0] = ciPtr.p->lcpUserptr;
    signal->theData[1] = ciPtr.i;
    sendSignal(ciPtr.p->lcpBlockref, GSN_TUP_PREPLCPCONF, signal, 2, JBB);
    break;
  }
  case LCP_UNDO_FILE_WRITE:
  {
    CheckpointInfoPtr ciPtr;
    LocalLogInfoPtr lliPtr;

    ljam();
    ciPtr.i = pfoPtr.p->pfoCheckpointInfoP;
    ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
    lliPtr.i = ciPtr.p->lcpLocalLogInfoP;
    ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
    lliPtr.p->lliUndoFileHandle = fileHandle;
    if (ciPtr.p->lcpDataFileHandle != RNIL) {
      ljam();
      signal->theData[0] = ciPtr.p->lcpUserptr;
      signal->theData[1] = ciPtr.i;
      sendSignal(ciPtr.p->lcpBlockref, GSN_TUP_PREPLCPCONF, signal, 2, JBB);
    }//if
    break;
  }
  default:
    ndbrequire(false);
    break;
  }//switch
  releasePendingFileOpenInfoRecord(pfoPtr);
}//Dbtup::execFSOPENCONF()

void Dbtup::execFSREADCONF(Signal* signal) 
{
  DiskBufferSegmentInfoPtr dbsiPtr;
  ljamEntry();
  dbsiPtr.i = signal->theData[0];
  ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
  switch (dbsiPtr.p->pdxOperation) {
  case CHECKPOINT_DATA_READ:
  {
    RestartInfoRecordPtr riPtr;
    ljam();
    riPtr.i = dbsiPtr.p->pdxRestartInfoP;
    ptrCheckGuard(riPtr, cnoOfRestartInfoRec, restartInfoRecord);
/************************************************************/
/*       VERIFY THAT THE PAGES ARE CORRECT, HAVE A CORRECT  */
/*       STATE AND A CORRECT PAGE ID.                       */
/************************************************************/
    ndbrequire(dbsiPtr.p->pdxNumDataPages <= 16);
    for (Uint32 i = 0; i < dbsiPtr.p->pdxNumDataPages; i++) {
      PagePtr pagePtr;
      ljam();
      pagePtr.i = dbsiPtr.p->pdxDataPage[i];
      ptrCheckGuard(pagePtr, cnoOfPage, page);
      ndbrequire(pagePtr.p->pageWord[ZPAGE_STATE_POS] != 0);
      ndbrequire(pagePtr.p->pageWord[ZPAGE_STATE_POS] <= ZAC_MM_FREE_COPY);
      ndbrequire(pagePtr.p->pageWord[ZPAGE_FRAG_PAGE_ID_POS] == ((dbsiPtr.p->pdxFilePage - 1) + i));
    }//for
    rfrLoadDataPagesLab(signal, riPtr, dbsiPtr);
    break;
  }
  case CHECKPOINT_DATA_READ_PAGE_ZERO:
  {
    ljam();
    rfrInitRestartInfoLab(signal, dbsiPtr);
    break;
  }
  case CHECKPOINT_UNDO_READ:
  {
    LocalLogInfoPtr lliPtr;
    ljam();
    lliPtr.i = dbsiPtr.p->pdxCheckpointInfoP;
    ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
    xlcGetNextRecordLab(signal, dbsiPtr, lliPtr);
    break;
  }
  case CHECKPOINT_UNDO_READ_FIRST:
    ljam();
    rfrReadSecondUndoLogLab(signal, dbsiPtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dbtup::execFSREADCONF()

void Dbtup::execFSWRITECONF(Signal* signal) 
{
  DiskBufferSegmentInfoPtr dbsiPtr;

  ljamEntry();
  dbsiPtr.i = signal->theData[0];
  ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
  switch (dbsiPtr.p->pdxOperation) {
  case CHECKPOINT_DATA_WRITE:
    ljam();
    lcpSaveDataPageLab(signal, dbsiPtr.p->pdxCheckpointInfoP);
    break;
  case CHECKPOINT_DATA_WRITE_LAST:
  {
    CheckpointInfoPtr ciPtr;
    ljam();
    ciPtr.i = dbsiPtr.p->pdxCheckpointInfoP;
    ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
    lcpFlushLogLab(signal, ciPtr);
    break;
  }
  case CHECKPOINT_DATA_WRITE_FLUSH:
  {
    ljam();
    Uint32 ciIndex = dbsiPtr.p->pdxCheckpointInfoP;
    freeDiskBufferSegmentRecord(signal, dbsiPtr);
    lcpCompletedLab(signal, ciIndex);
    break;
  }
  case CHECKPOINT_UNDO_WRITE_FLUSH:
  {
    ljam();
    Uint32 ciIndex = dbsiPtr.p->pdxCheckpointInfoP;
    freeDiskBufferSegmentRecord(signal, dbsiPtr);
    lcpFlushRestartInfoLab(signal, ciIndex);
    break;
  }
  case CHECKPOINT_UNDO_WRITE:
    ljam();
    freeDiskBufferSegmentRecord(signal, dbsiPtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dbtup::execFSWRITECONF()

void Dbtup::execCONTINUEB(Signal* signal) 
{
  ljamEntry();
  Uint32 actionType = signal->theData[0];
  Uint32 dataPtr = signal->theData[1];
  switch (actionType) {
  case ZSTART_EXEC_UNDO_LOG:
    ljam();
    startExecUndoLogLab(signal, dataPtr);
    break;
  case ZCONT_SAVE_DP:
    ljam();
    lcpSaveDataPageLab(signal, dataPtr);
    break;
  case ZCONT_START_SAVE_CL:
  {
    CheckpointInfoPtr ciPtr;

    ljam();
    ciPtr.i = dataPtr;
    ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
    lcpSaveCopyListLab(signal, ciPtr);
    break;
  }
  case ZCONT_EXECUTE_LC:
  {
    LocalLogInfoPtr lliPtr;
    DiskBufferSegmentInfoPtr dbsiPtr;

    ljam();
    lliPtr.i = dataPtr;
    ptrCheckGuard(lliPtr, cnoOfParallellUndoFiles, localLogInfo);
    dbsiPtr.i = lliPtr.p->lliUndoBufferSegmentP;
    ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
    xlcGetNextRecordLab(signal, dbsiPtr, lliPtr);
    break;
  }
  case ZCONT_LOAD_DP:
  {
    DiskBufferSegmentInfoPtr dbsiPtr;
    RestartInfoRecordPtr riPtr;

    ljam();
    riPtr.i = dataPtr;
    ptrCheckGuard(riPtr, cnoOfRestartInfoRec, restartInfoRecord);
    dbsiPtr.i = riPtr.p->sriDataBufferSegmentP;
    ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
    rfrLoadDataPagesLab(signal, riPtr, dbsiPtr);
    break;
  }
  case ZLOAD_BAL_LCP_TIMER:
    ljam();
    clblPageCounter = clblPagesPerTick;
    signal->theData[0] = ZLOAD_BAL_LCP_TIMER;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 400, 1);
    break;
  case ZINITIALISE_RECORDS:
    ljam();
    initialiseRecordsLab(signal, dataPtr, 
			 signal->theData[2], signal->theData[3]);
    break;
  case ZREL_FRAG:
    ljam();
    releaseFragment(signal, dataPtr);
    break;
  case ZREPORT_MEMORY_USAGE:{
    ljam();
    static int c_currentMemUsed = 0;
    int now = (cnoOfAllocatedPages * 100)/cnoOfPage;
    const int thresholds[] = { 100, 90, 80, 0 };
    
    Uint32 i = 0;
    const Uint32 sz = sizeof(thresholds)/sizeof(thresholds[0]);
    for(i = 0; i<sz; i++){
      if(now >= thresholds[i]){
	now = thresholds[i];
	break;
      }
    }

    if(now != c_currentMemUsed){
      reportMemoryUsage(signal, now > c_currentMemUsed ? 1 : -1);
      c_currentMemUsed = now;
    }
    signal->theData[0] = ZREPORT_MEMORY_USAGE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 2000, 1);    
    return;
  }
  case ZBUILD_INDEX:
    ljam();
    buildIndex(signal, dataPtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dbtup::execTUP_CONTINUEB()

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ------------------- SYSTEM RESTART MODULE ---------------------- */
/* ---------------------------------------------------------------- */
/* **************************************************************** */
void Dbtup::execSTTOR(Signal* signal) 
{
  ljamEntry();
  Uint32 startPhase = signal->theData[1];
  Uint32 sigKey = signal->theData[6];
  switch (startPhase) {
  case ZSTARTPHASE1:
    ljam();
    CLEAR_ERROR_INSERT_VALUE;
    cownref = calcTupBlockRef(0);
    break;
  default:
    ljam();
    break;
  }//switch
  signal->theData[0] = sigKey;
  signal->theData[1] = 3;
  signal->theData[2] = 2;
  signal->theData[3] = ZSTARTPHASE1;
  signal->theData[4] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
  return;
}//Dbtup::execSTTOR()

/************************************************************************************************/
// SIZE_ALTREP INITIALIZE DATA STRUCTURES, FILES AND DS VARIABLES, GET READY FOR EXTERNAL 
// CONNECTIONS.
/************************************************************************************************/
void Dbtup::execREAD_CONFIG_REQ(Signal* signal) 
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);
  
  ljamEntry();

  const ndb_mgm_configuration_iterator * p = 
    theConfiguration.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  Uint32 log_page_size= 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_UNDO_DATA_BUFFER,  
			    &log_page_size);

  /**
   * Always set page size in half MBytes
   */
  cnoOfUndoPage= (log_page_size / sizeof(UndoPage));
  Uint32 mega_byte_part= cnoOfUndoPage & 15;
  if (mega_byte_part != 0) {
    jam();
    cnoOfUndoPage+= (16 - mega_byte_part);
  }

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_FRAG, &cnoOfFragrec));

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_OP_RECS, &cnoOfOprec));
  
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_PAGE, &cnoOfPage));
  Uint32 noOfTriggers= 0;
  
  Uint32 tmp= 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_PAGE_RANGE, &tmp));
  initPageRangeSize(tmp);
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_TABLE, &cnoOfTablerec));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_TABLE_DESC, 
					&cnoOfTabDescrRec));
  Uint32 noOfStoredProc;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_STORED_PROC, 
					&noOfStoredProc));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGERS, 
					&noOfTriggers));

  Uint32 nScanOp;       // use TUX config for now
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUX_SCAN_OP, &nScanOp));


  cnoOfTabDescrRec = (cnoOfTabDescrRec & 0xFFFFFFF0) + 16;

  initRecords();

  c_storedProcPool.setSize(noOfStoredProc);
  c_buildIndexPool.setSize(c_noOfBuildIndexRec);
  c_triggerPool.setSize(noOfTriggers);
  c_scanOpPool.setSize(nScanOp);

  czero = 0;
  cminusOne = czero - 1;
  clastBitMask = 1;
  clastBitMask = clastBitMask << 31;
  cnoOfLocalLogInfo = 0;
  cnoFreeUndoSeg = 0;

  initialiseRecordsLab(signal, 0, ref, senderData);

  clblPagesPerTick = 50;
  ndb_mgm_get_int_parameter(p, CFG_DB_LCP_DISC_PAGES_TUP_SR, 
			    &clblPagesPerTick);

  clblPagesPerTickAfterSr = 50;
  ndb_mgm_get_int_parameter(p, CFG_DB_LCP_DISC_PAGES_TUP, 
			    &clblPagesPerTickAfterSr);

}//Dbtup::execSIZEALT_REP()

void Dbtup::initRecords() 
{
  unsigned i;

  // Records with dynamic sizes
  page = (Page*)allocRecord("Page", 
			    sizeof(Page), 
			    cnoOfPage,
			    false);
  
  undoPage = (UndoPage*)allocRecord("UndoPage",
				    sizeof(UndoPage),
				    cnoOfUndoPage);
  
  operationrec = (Operationrec*)allocRecord("Operationrec",
					    sizeof(Operationrec),
					    cnoOfOprec);

  attrbufrec = (Attrbufrec*)allocRecord("Attrbufrec", 
					sizeof(Attrbufrec), 
					cnoOfAttrbufrec);

  checkpointInfo = (CheckpointInfo*)allocRecord("CheckpointInfo",
						sizeof(CheckpointInfo), 
						cnoOfLcpRec);

  diskBufferSegmentInfo = (DiskBufferSegmentInfo*)
    allocRecord("DiskBufferSegmentInfo",
		sizeof(DiskBufferSegmentInfo), 
		cnoOfConcurrentWriteOp);

  fragoperrec = (Fragoperrec*)allocRecord("Fragoperrec",
					  sizeof(Fragoperrec),
					  cnoOfFragoprec);

  fragrecord = (Fragrecord*)allocRecord("Fragrecord",
					sizeof(Fragrecord), 
					cnoOfFragrec);
  
  for (i = 0; i<cnoOfFragrec; i++) {
    void * p = &fragrecord[i];
    new (p) Fragrecord(c_scanOpPool);
  }

  hostBuffer = (HostBuffer*)allocRecord("HostBuffer",
					sizeof(HostBuffer), 
					MAX_NODES);

  localLogInfo = (LocalLogInfo*)allocRecord("LocalLogInfo",
					    sizeof(LocalLogInfo), 
					    cnoOfParallellUndoFiles);

  pageRange = (PageRange*)allocRecord("PageRange",
				      sizeof(PageRange), 
				      cnoOfPageRangeRec);
  
  pendingFileOpenInfo = (PendingFileOpenInfo*)
    allocRecord("PendingFileOpenInfo",
		sizeof(PendingFileOpenInfo), 
		cnoOfConcurrentOpenOp);
  
  restartInfoRecord = (RestartInfoRecord*)
    allocRecord("RestartInfoRecord",
		sizeof(RestartInfoRecord),
		cnoOfRestartInfoRec);

  
  tablerec = (Tablerec*)allocRecord("Tablerec",
				    sizeof(Tablerec), 
				    cnoOfTablerec);
  
  for (i = 0; i<cnoOfTablerec; i++) {
    void * p = &tablerec[i];
    new (p) Tablerec(c_triggerPool);
  }
  
  tableDescriptor = (TableDescriptor*)
    allocRecord("TableDescriptor",
		sizeof(TableDescriptor),
		cnoOfTabDescrRec);
  
  // Initialize BAT for interface to file system
  NewVARIABLE* bat = allocateBat(3);
  bat[1].WA = &page->pageWord[0];
  bat[1].nrr = cnoOfPage;
  bat[1].ClusterSize = sizeof(Page);
  bat[1].bits.q = 13; /* 8192 words/page */
  bat[1].bits.v = 5;
  bat[2].WA = &undoPage->undoPageWord[0];
  bat[2].nrr = cnoOfUndoPage;
  bat[2].ClusterSize = sizeof(UndoPage);
  bat[2].bits.q = 13; /* 8192 words/page */
  bat[2].bits.v = 5;
}//Dbtup::initRecords()

void Dbtup::initialiseRecordsLab(Signal* signal, Uint32 switchData,
				 Uint32 retRef, Uint32 retData) 
{
  switch (switchData) {
  case 0:
    ljam();
    initializeHostBuffer();
    break;
  case 1:
    ljam();
    initializeOperationrec();
    break;
  case 2:
    ljam();
    initializePage();
    break;
  case 3:
    ljam();
    initializeUndoPage();
    break;
  case 4:
    ljam();
    initializeTablerec();
    break;
  case 5:
    ljam();
    initializeCheckpointInfoRec();
    break;
  case 6:
    ljam();
    initializeFragrecord();
    break;
  case 7:
    ljam();
    initializeFragoperrec();
    break;
  case 8:
    ljam();
    initializePageRange();
    break;
  case 9:
    ljam();
    initializeTabDescr();
    break;
  case 10:
    ljam();
    initializeDiskBufferSegmentRecord();
    break;
  case 11:
    ljam();
    initializeLocalLogInfo();
    break;
  case 12:
    ljam();
    initializeAttrbufrec();
    break;
  case 13:
    ljam();
    initializePendingFileOpenInfoRecord();
    break;
  case 14:
    ljam();
    initializeRestartInfoRec();

    {
      ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = retData;
      sendSignal(retRef, GSN_READ_CONFIG_CONF, signal, 
		 ReadConfigConf::SignalLength, JBB);
    }
    return;
  default:
    ndbrequire(false);
    break;
  }//switch
  signal->theData[0] = ZINITIALISE_RECORDS;
  signal->theData[1] = switchData + 1;
  signal->theData[2] = retRef;
  signal->theData[3] = retData;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
  return;
}//Dbtup::initialiseRecordsLab()

void Dbtup::execNDB_STTOR(Signal* signal) 
{
  ljamEntry();
  cndbcntrRef = signal->theData[0];
  Uint32 ownNodeId = signal->theData[1];
  Uint32 startPhase = signal->theData[2];
  switch (startPhase) {
  case ZSTARTPHASE1:
    ljam();
    cownNodeId = ownNodeId;
    cownref = calcTupBlockRef(ownNodeId);
    break;
  case ZSTARTPHASE2:
    ljam();
    break;
  case ZSTARTPHASE3:
    ljam();
    startphase3Lab(signal, ~0, ~0);
    break;
  case ZSTARTPHASE4:
    ljam();
    break;
  case ZSTARTPHASE6:
    ljam();
/*****************************************/
/*       NOW SET THE DISK WRITE SPEED TO */
/*       PAGES PER TICK AFTER SYSTEM     */
/*       RESTART.                        */
/*****************************************/
    clblPagesPerTick = clblPagesPerTickAfterSr;

    signal->theData[0] = ZREPORT_MEMORY_USAGE;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 2000, 1);    
    break;
  default:
    ljam();
    break;
  }//switch
  signal->theData[0] = cownref;
  sendSignal(cndbcntrRef, GSN_NDB_STTORRY, signal, 1, JBB);
}//Dbtup::execNDB_STTOR()

void Dbtup::startphase3Lab(Signal* signal, Uint32 config1, Uint32 config2) 
{
  clblPageCounter = clblPagesPerTick;
  signal->theData[0] = ZLOAD_BAL_LCP_TIMER;
  sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 100, 1);
}//Dbtup::startphase3Lab()

void Dbtup::initializeAttrbufrec() 
{
  AttrbufrecPtr attrBufPtr;
  for (attrBufPtr.i = 0;
       attrBufPtr.i < cnoOfAttrbufrec; attrBufPtr.i++) {
    refresh_watch_dog();
    ptrAss(attrBufPtr, attrbufrec);
    attrBufPtr.p->attrbuf[ZBUF_NEXT] = attrBufPtr.i + 1;
  }//for
  attrBufPtr.i = cnoOfAttrbufrec - 1;
  ptrAss(attrBufPtr, attrbufrec);
  attrBufPtr.p->attrbuf[ZBUF_NEXT] = RNIL;
  cfirstfreeAttrbufrec = 0;
  cnoFreeAttrbufrec = cnoOfAttrbufrec;
}//Dbtup::initializeAttrbufrec()

void Dbtup::initializeCheckpointInfoRec() 
{
  CheckpointInfoPtr checkpointInfoPtr;
  for (checkpointInfoPtr.i = 0;
       checkpointInfoPtr.i < cnoOfLcpRec; checkpointInfoPtr.i++) {
    ptrAss(checkpointInfoPtr, checkpointInfo);
    checkpointInfoPtr.p->lcpNextRec = checkpointInfoPtr.i + 1;
  }//for
  checkpointInfoPtr.i = cnoOfLcpRec - 1;
  ptrAss(checkpointInfoPtr, checkpointInfo);
  checkpointInfoPtr.p->lcpNextRec = RNIL;
  cfirstfreeLcp = 0;
}//Dbtup::initializeCheckpointInfoRec()

void Dbtup::initializeDiskBufferSegmentRecord() 
{
  DiskBufferSegmentInfoPtr diskBufferSegmentPtr;
  for (diskBufferSegmentPtr.i = 0;
       diskBufferSegmentPtr.i < cnoOfConcurrentWriteOp; diskBufferSegmentPtr.i++) {
    ptrAss(diskBufferSegmentPtr, diskBufferSegmentInfo);
    diskBufferSegmentPtr.p->pdxNextRec = diskBufferSegmentPtr.i + 1;
    diskBufferSegmentPtr.p->pdxBuffertype = NOT_INITIALIZED;
  }//for
  diskBufferSegmentPtr.i = cnoOfConcurrentWriteOp - 1;
  ptrAss(diskBufferSegmentPtr, diskBufferSegmentInfo);
  diskBufferSegmentPtr.p->pdxNextRec = RNIL;
  cfirstfreePdx = 0;
}//Dbtup::initializeDiskBufferSegmentRecord()

void Dbtup::initializeFragoperrec() 
{
  FragoperrecPtr fragoperPtr;
  for (fragoperPtr.i = 0; fragoperPtr.i < cnoOfFragoprec; fragoperPtr.i++) {
    ptrAss(fragoperPtr, fragoperrec);
    fragoperPtr.p->nextFragoprec = fragoperPtr.i + 1;
  }//for
  fragoperPtr.i = cnoOfFragoprec - 1;
  ptrAss(fragoperPtr, fragoperrec);
  fragoperPtr.p->nextFragoprec = RNIL;
  cfirstfreeFragopr = 0;
}//Dbtup::initializeFragoperrec()

void Dbtup::initializeFragrecord() 
{
  FragrecordPtr regFragPtr;
  for (regFragPtr.i = 0; regFragPtr.i < cnoOfFragrec; regFragPtr.i++) {
    refresh_watch_dog();
    ptrAss(regFragPtr, fragrecord);
    regFragPtr.p->nextfreefrag = regFragPtr.i + 1;
    regFragPtr.p->checkpointVersion = RNIL;
    regFragPtr.p->firstusedOprec = RNIL;
    regFragPtr.p->lastusedOprec = RNIL;
    regFragPtr.p->fragStatus = IDLE;
  }//for
  regFragPtr.i = cnoOfFragrec - 1;
  ptrAss(regFragPtr, fragrecord);
  regFragPtr.p->nextfreefrag = RNIL;
  cfirstfreefrag = 0;
}//Dbtup::initializeFragrecord()

void Dbtup::initializeHostBuffer() 
{
  Uint32 hostId;
  cpackedListIndex = 0;
  for (hostId = 0; hostId < MAX_NODES; hostId++) {
    hostBuffer[hostId].inPackedList = false;
    hostBuffer[hostId].noOfPacketsTA = 0;
    hostBuffer[hostId].packetLenTA = 0;
  }//for
}//Dbtup::initializeHostBuffer()

void Dbtup::initializeLocalLogInfo() 
{
  LocalLogInfoPtr localLogInfoPtr;
  for (localLogInfoPtr.i = 0;
       localLogInfoPtr.i < cnoOfParallellUndoFiles; localLogInfoPtr.i++) {
    ptrAss(localLogInfoPtr, localLogInfo);
    localLogInfoPtr.p->lliActiveLcp = 0;
    localLogInfoPtr.p->lliUndoFileHandle = RNIL;
  }//for
}//Dbtup::initializeLocalLogInfo()

void Dbtup::initializeOperationrec() 
{
  OperationrecPtr regOpPtr;
  for (regOpPtr.i = 0; regOpPtr.i < cnoOfOprec; regOpPtr.i++) {
    refresh_watch_dog();
    ptrAss(regOpPtr, operationrec);
    regOpPtr.p->firstAttrinbufrec = RNIL;
    regOpPtr.p->lastAttrinbufrec = RNIL;
    regOpPtr.p->prevOprecInList = RNIL;
    regOpPtr.p->nextOprecInList = regOpPtr.i + 1;
    regOpPtr.p->optype = ZREAD;
    regOpPtr.p->inFragList = ZFALSE;
    regOpPtr.p->inActiveOpList = ZFALSE;
/* FOR ABORT HANDLING BEFORE ANY SUCCESSFUL OPERATION */
    regOpPtr.p->transstate = DISCONNECTED;
    regOpPtr.p->storedProcedureId = ZNIL;
    regOpPtr.p->prevActiveOp = RNIL;
    regOpPtr.p->nextActiveOp = RNIL;
    regOpPtr.p->tupVersion = ZNIL;
    regOpPtr.p->deleteInsertFlag = 0;
  }//for
  regOpPtr.i = cnoOfOprec - 1;
  ptrAss(regOpPtr, operationrec);
  regOpPtr.p->nextOprecInList = RNIL;
  cfirstfreeOprec = 0;
}//Dbtup::initializeOperationrec()

void Dbtup::initializePendingFileOpenInfoRecord() 
{
  PendingFileOpenInfoPtr pendingFileOpenInfoPtr;
  for (pendingFileOpenInfoPtr.i = 0;
       pendingFileOpenInfoPtr.i < cnoOfConcurrentOpenOp; pendingFileOpenInfoPtr.i++) {
    ptrAss(pendingFileOpenInfoPtr, pendingFileOpenInfo);
    pendingFileOpenInfoPtr.p->pfoNextRec = pendingFileOpenInfoPtr.i + 1;
  }//for
  pendingFileOpenInfoPtr.i = cnoOfConcurrentOpenOp - 1;
  ptrAss(pendingFileOpenInfoPtr, pendingFileOpenInfo);
  pendingFileOpenInfoPtr.p->pfoNextRec = RNIL;
  cfirstfreePfo = 0;
}//Dbtup::initializePendingFileOpenInfoRecord()

void Dbtup::initializeRestartInfoRec() 
{
  RestartInfoRecordPtr restartInfoPtr;
  for (restartInfoPtr.i = 0; restartInfoPtr.i < cnoOfRestartInfoRec; restartInfoPtr.i++) {
    ptrAss(restartInfoPtr, restartInfoRecord);
    restartInfoPtr.p->sriNextRec = restartInfoPtr.i + 1;
  }//for
  restartInfoPtr.i = cnoOfRestartInfoRec - 1;
  ptrAss(restartInfoPtr, restartInfoRecord);
  restartInfoPtr.p->sriNextRec = RNIL;
  cfirstfreeSri = 0;
}//Dbtup::initializeRestartInfoRec()

void Dbtup::initializeTablerec() 
{
  TablerecPtr regTabPtr;
  for (regTabPtr.i = 0; regTabPtr.i < cnoOfTablerec; regTabPtr.i++) {
    ljam();
    refresh_watch_dog();
    ptrAss(regTabPtr, tablerec);
    initTab(regTabPtr.p);
  }//for
}//Dbtup::initializeTablerec()

void
Dbtup::initTab(Tablerec* const regTabPtr)
{
  for (Uint32 i = 0; i < (2 * MAX_FRAG_PER_NODE); i++) {
    regTabPtr->fragid[i] = RNIL;
    regTabPtr->fragrec[i] = RNIL;
  }//for
  regTabPtr->readFunctionArray = NULL;
  regTabPtr->updateFunctionArray = NULL;
  regTabPtr->charsetArray = NULL;

  regTabPtr->tabDescriptor = RNIL;
  regTabPtr->attributeGroupDescriptor = RNIL;
  regTabPtr->readKeyArray = RNIL;

  regTabPtr->checksumIndicator = false;
  regTabPtr->GCPIndicator = false;

  regTabPtr->noOfAttr = 0;
  regTabPtr->noOfKeyAttr = 0;
  regTabPtr->noOfNewAttr = 0;
  regTabPtr->noOfAttributeGroups = 0;

  regTabPtr->tupheadsize = 0;
  regTabPtr->tupNullIndex = 0;
  regTabPtr->tupNullWords = 0;
  regTabPtr->tupChecksumIndex = 0;
  regTabPtr->tupGCPIndex = 0;

  regTabPtr->m_dropTable.tabUserPtr = RNIL;
  regTabPtr->m_dropTable.tabUserRef = 0;
  regTabPtr->tableStatus = NOT_DEFINED;

  // Clear trigger data
  if (!regTabPtr->afterInsertTriggers.isEmpty())
    regTabPtr->afterInsertTriggers.release();
  if (!regTabPtr->afterDeleteTriggers.isEmpty())
    regTabPtr->afterDeleteTriggers.release();
  if (!regTabPtr->afterUpdateTriggers.isEmpty())
    regTabPtr->afterUpdateTriggers.release();
  if (!regTabPtr->subscriptionInsertTriggers.isEmpty())
    regTabPtr->subscriptionInsertTriggers.release();
  if (!regTabPtr->subscriptionDeleteTriggers.isEmpty())
    regTabPtr->subscriptionDeleteTriggers.release();
  if (!regTabPtr->subscriptionUpdateTriggers.isEmpty())
    regTabPtr->subscriptionUpdateTriggers.release();
  if (!regTabPtr->constraintUpdateTriggers.isEmpty())
    regTabPtr->constraintUpdateTriggers.release();
  if (!regTabPtr->tuxCustomTriggers.isEmpty())
    regTabPtr->tuxCustomTriggers.release();
}//Dbtup::initTab()

void Dbtup::initializeTabDescr() 
{
  TableDescriptorPtr regTabDesPtr;
  for (Uint32 i = 0; i < 16; i++) {
    cfreeTdList[i] = RNIL;
  }//for
  for (regTabDesPtr.i = 0; regTabDesPtr.i < cnoOfTabDescrRec; regTabDesPtr.i++) {
    refresh_watch_dog();
    ptrAss(regTabDesPtr, tableDescriptor);
    regTabDesPtr.p->tabDescr = RNIL;
  }//for
  freeTabDescr(0, cnoOfTabDescrRec);
}//Dbtup::initializeTabDescr()

void Dbtup::initializeUndoPage() 
{
  UndoPagePtr undoPagep;
  for (undoPagep.i = 0;
       undoPagep.i < cnoOfUndoPage;
       undoPagep.i = undoPagep.i + ZUB_SEGMENT_SIZE) {
    refresh_watch_dog();
    ptrAss(undoPagep, undoPage);
    undoPagep.p->undoPageWord[ZPAGE_NEXT_POS] = undoPagep.i + 
                                                 ZUB_SEGMENT_SIZE;
    cnoFreeUndoSeg++;
  }//for
  undoPagep.i = cnoOfUndoPage - ZUB_SEGMENT_SIZE;
  ptrAss(undoPagep, undoPage);
  undoPagep.p->undoPageWord[ZPAGE_NEXT_POS] = RNIL;
  cfirstfreeUndoSeg = 0;
}//Dbtup::initializeUndoPage()

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* --------------- CONNECT/DISCONNECT MODULE ---------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execTUPSEIZEREQ(Signal* signal)
{
  OperationrecPtr regOperPtr;
  ljamEntry();
  Uint32 userPtr = signal->theData[0];
  BlockReference userRef = signal->theData[1];
  if (cfirstfreeOprec != RNIL) {
    ljam();
    seizeOpRec(regOperPtr);
  } else {
    ljam();
    signal->theData[0] = userPtr;
    signal->theData[1] = ZGET_OPREC_ERROR;
    sendSignal(userRef, GSN_TUPSEIZEREF, signal, 2, JBB);
    return;
  }//if
  regOperPtr.p->optype = ZREAD;
  initOpConnection(regOperPtr.p, 0);
  regOperPtr.p->userpointer = userPtr;
  regOperPtr.p->userblockref = userRef;
  signal->theData[0] = regOperPtr.p->userpointer;
  signal->theData[1] = regOperPtr.i;
  sendSignal(userRef, GSN_TUPSEIZECONF, signal, 2, JBB);
  return;
}//Dbtup::execTUPSEIZEREQ()

#define printFragment(t){ for(Uint32 i = 0; i < (2 * MAX_FRAG_PER_NODE);i++){\
  ndbout_c("table = %d fragid[%d] = %d fragrec[%d] = %d", \
           t.i, t.p->fragid[i], i, t.p->fragrec[i]); }}

void Dbtup::execTUPRELEASEREQ(Signal* signal) 
{
  OperationrecPtr regOperPtr;
  ljamEntry();
  regOperPtr.i = signal->theData[0];
  ptrCheckGuard(regOperPtr, cnoOfOprec, operationrec);
  regOperPtr.p->transstate = DISCONNECTED;
  regOperPtr.p->nextOprecInList = cfirstfreeOprec;
  cfirstfreeOprec = regOperPtr.i;
  signal->theData[0] = regOperPtr.p->userpointer;
  sendSignal(regOperPtr.p->userblockref, GSN_TUPRELEASECONF, signal, 1, JBB);
  return;
}//Dbtup::execTUPRELEASEREQ()

/* ---------------------------------------------------------------- */
/* ---------------- FREE_DISK_BUFFER_SEGMENT_RECORD --------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* THIS ROUTINE DEALLOCATES A DISK SEGMENT AND ITS DATA PAGES       */
/*                                                                  */
/* INPUT:  DISK_BUFFER_SEGMENT_PTR    THE DISK SEGMENT              */
/*                                                                  */
/* -----------------------------------------------------------------*/
void Dbtup::freeDiskBufferSegmentRecord(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr) 
{
  switch (dbsiPtr.p->pdxBuffertype) {
  case UNDO_PAGES:
  case COMMON_AREA_PAGES:
    ljam();
    freeUndoBufferPages(signal, dbsiPtr);
    break;
  case UNDO_RESTART_PAGES:
    ljam();
    dbsiPtr.p->pdxDataPage[0] = dbsiPtr.p->pdxUndoBufferSet[0];
    freeUndoBufferPages(signal, dbsiPtr);
    dbsiPtr.p->pdxDataPage[0] = dbsiPtr.p->pdxUndoBufferSet[1];
    freeUndoBufferPages(signal, dbsiPtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  releaseDiskBufferSegmentRecord(dbsiPtr);
}//Dbtup::freeDiskBufferSegmentRecord()

/* ---------------------------------------------------------------- */
/* -------------------- FREE_UNDO_BUFFER_PAGES -------------------- */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* THIS ROUTINE DEALLOCATES A SEGMENT OF UNDO PAGES                 */
/*                                                                  */
/* INPUT:  UNDO_PAGEP    POINTER TO FIRST PAGE IN SEGMENT          */
/*                                                                  */
/* -----------------------------------------------------------------*/
void Dbtup::freeUndoBufferPages(Signal* signal, DiskBufferSegmentInfoPtr dbsiPtr) 
{
  UndoPagePtr undoPagePtr;

  undoPagePtr.i = dbsiPtr.p->pdxDataPage[0];
  ptrCheckGuard(undoPagePtr, cnoOfUndoPage, undoPage);
  undoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS] = cfirstfreeUndoSeg;
  cfirstfreeUndoSeg = undoPagePtr.i;
  cnoFreeUndoSeg++;
  if (cnoFreeUndoSeg == ZMIN_PAGE_LIMIT_TUP_COMMITREQ) {
    EXECUTE_DIRECT(DBLQH, GSN_TUP_COM_UNBLOCK, signal, 1);
    ljamEntry();
  }//if
}//Dbtup::freeUndoBufferPages()

void Dbtup::releaseCheckpointInfoRecord(CheckpointInfoPtr ciPtr) 
{
  ciPtr.p->lcpNextRec = cfirstfreeLcp;
  cfirstfreeLcp = ciPtr.i;
}//Dbtup::releaseCheckpointInfoRecord()

void Dbtup::releaseDiskBufferSegmentRecord(DiskBufferSegmentInfoPtr dbsiPtr) 
{
  dbsiPtr.p->pdxNextRec = cfirstfreePdx;
  cfirstfreePdx = dbsiPtr.i;
}//Dbtup::releaseDiskBufferSegmentRecord()

void Dbtup::releaseFragrec(FragrecordPtr regFragPtr) 
{
  regFragPtr.p->nextfreefrag = cfirstfreefrag;
  cfirstfreefrag = regFragPtr.i;
}//Dbtup::releaseFragrec()

void Dbtup::releasePendingFileOpenInfoRecord(PendingFileOpenInfoPtr pfoPtr) 
{
  pfoPtr.p->pfoNextRec = cfirstfreePfo;
  cfirstfreePfo = pfoPtr.i;
}//Dbtup::releasePendingFileOpenInfoRecord()

void Dbtup::releaseRestartInfoRecord(RestartInfoRecordPtr riPtr) 
{
  riPtr.p->sriNextRec = cfirstfreeSri;
  cfirstfreeSri = riPtr.i;
}//Dbtup::releaseRestartInfoRecord()

void Dbtup::seizeCheckpointInfoRecord(CheckpointInfoPtr& ciPtr) 
{
  ciPtr.i = cfirstfreeLcp;
  ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
  cfirstfreeLcp = ciPtr.p->lcpNextRec;
  ciPtr.p->lcpNextRec = RNIL;
}//Dbtup::seizeCheckpointInfoRecord()

void Dbtup::seizeDiskBufferSegmentRecord(DiskBufferSegmentInfoPtr& dbsiPtr) 
{
  dbsiPtr.i = cfirstfreePdx;
  ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
  cfirstfreePdx = dbsiPtr.p->pdxNextRec;
  dbsiPtr.p->pdxNextRec = RNIL;
  for (Uint32 i = 0; i < 16; i++) {
    dbsiPtr.p->pdxDataPage[i] = RNIL;
  }//for
  dbsiPtr.p->pdxCheckpointInfoP = RNIL;
  dbsiPtr.p->pdxRestartInfoP = RNIL;
  dbsiPtr.p->pdxLocalLogInfoP = RNIL;
  dbsiPtr.p->pdxFilePage = 0;
  dbsiPtr.p->pdxNumDataPages = 0;
}//Dbtup::seizeDiskBufferSegmentRecord()

void Dbtup::seizeOpRec(OperationrecPtr& regOperPtr) 
{
  regOperPtr.i = cfirstfreeOprec;
  ptrCheckGuard(regOperPtr, cnoOfOprec, operationrec);
  cfirstfreeOprec = regOperPtr.p->nextOprecInList;
}//Dbtup::seizeOpRec()

void Dbtup::seizePendingFileOpenInfoRecord(PendingFileOpenInfoPtr& pfoiPtr) 
{
  pfoiPtr.i = cfirstfreePfo;
  ptrCheckGuard(pfoiPtr, cnoOfConcurrentOpenOp, pendingFileOpenInfo);
  cfirstfreePfo = pfoiPtr.p->pfoNextRec;
  pfoiPtr.p->pfoNextRec = RNIL;
}//Dbtup::seizePendingFileOpenInfoRecord()

void Dbtup::execSET_VAR_REQ(Signal* signal) 
{
#if 0
  SetVarReq* const setVarReq = (SetVarReq*)signal->getDataPtrSend();
  ConfigParamId var = setVarReq->variable();
  int val = setVarReq->value();

  switch (var) {

  case NoOfDiskPagesToDiskAfterRestartTUP:
    clblPagesPerTick = val;
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  case NoOfDiskPagesToDiskDuringRestartTUP:
    // Valid only during start so value not set.
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  default:
    sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
  } // switch
#endif

}//execSET_VAR_REQ()




