/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include "Backup.hpp"

#include <ndb_version.h>

#include <NdbTCP.h>
#include <Bitmask.hpp>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>

#include <signaldata/DihScanTab.hpp>
#include <signaldata/ScanFrag.hpp>

#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/ListTables.hpp>

#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsAppendReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsRemoveReq.hpp>

#include <signaldata/BackupImpl.hpp>
#include <signaldata/BackupSignalData.hpp>
#include <signaldata/BackupContinueB.hpp>
#include <signaldata/EventReport.hpp>

#include <signaldata/UtilSequence.hpp>

#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <AttributeHeader.hpp>

#include <signaldata/WaitGCP.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/BackupLockTab.hpp>
#include <signaldata/DumpStateOrd.hpp>

#include <signaldata/DumpStateOrd.hpp>

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>

#include <NdbTick.h>
#include <dbtup/Dbtup.hpp>

#define JAM_FILE_ID 475


static NDB_TICKS startTime;

#ifdef VM_TRACE
#define DEBUG_OUT(x) ndbout << x << endl
#else
#define DEBUG_OUT(x) 
#endif

//#define DEBUG_ABORT
//#define dbg globalSignalLoggers.log

static Uint32 g_TypeOfStart = NodeState::ST_ILLEGAL_TYPE;

#define SEND_BACKUP_STARTED_FLAG(A) (((A) & 0x3) > 0)
#define SEND_BACKUP_COMPLETED_FLAG(A) (((A) & 0x3) > 1)

void
Backup::execSTTOR(Signal* signal) 
{
  jamEntry();                            

  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];

  if (startphase == 1)
  {
    m_monitor_words_written = 0;
    m_periods_passed_in_monitor_period = 0;
    m_monitor_snapshot_start = NdbTick_getCurrentTicks();
    m_curr_disk_write_speed = c_defaults.m_disk_write_speed_sr;
    m_overflow_disk_write = 0;
    m_reset_disk_speed_time = NdbTick_getCurrentTicks();
    m_reset_delay_used = Backup::DISK_SPEED_CHECK_DELAY;
    signal->theData[0] = BackupContinueB::RESET_DISK_SPEED_COUNTER;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        Backup::DISK_SPEED_CHECK_DELAY, 1);
  }
  if (startphase == 3) {
    jam();
    g_TypeOfStart = typeOfStart;
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
  }//if

  if (startphase == 7)
  {
    m_monitor_words_written = 0;
    m_periods_passed_in_monitor_period = 0;
    m_monitor_snapshot_start = NdbTick_getCurrentTicks();
    m_curr_disk_write_speed = c_defaults.m_disk_write_speed;
  }

  if(startphase == 7 && g_TypeOfStart == NodeState::ST_INITIAL_START &&
     c_masterNodeId == getOwnNodeId() && !isNdbMtLqh()){
    jam();
    createSequence(signal);
    return;
  }//if
  
  sendSTTORRY(signal);  
  return;
}//Dbdict::execSTTOR()

void
Backup::execREAD_NODESCONF(Signal* signal)
{
  jamEntry();
  ReadNodesConf * conf = (ReadNodesConf *)signal->getDataPtr();
 
  c_aliveNodes.clear();

  Uint32 count = 0;
  for (Uint32 i = 0; i<MAX_NDB_NODES; i++) {
    jam();
    if(NdbNodeBitmask::get(conf->allNodes, i)){
      jam();
      count++;

      NodePtr node;
      ndbrequire(c_nodes.seizeFirst(node));
      
      node.p->nodeId = i;
      if(NdbNodeBitmask::get(conf->inactiveNodes, i)) {
        jam();
	node.p->alive = 0;
      } else {
        jam();
	node.p->alive = 1;
	c_aliveNodes.set(i);
      }//if
    }//if
  }//for
  c_masterNodeId = conf->masterNodeId;
  ndbrequire(count == conf->noOfNodes);
  sendSTTORRY(signal);
}

void
Backup::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 7;
  signal->theData[6] = 255; // No more start phases from missra
  BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : BACKUP_REF;
  sendSignal(cntrRef, GSN_STTORRY, signal, 7, JBB);
}

void
Backup::createSequence(Signal* signal)
{
  UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtrSend();
  
  req->senderData  = RNIL;
  req->sequenceId  = NDB_BACKUP_SEQUENCE;
  req->requestType = UtilSequenceReq::Create;
  
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
Backup::execCONTINUEB(Signal* signal)
{
  jamEntry();
  const Uint32 Tdata0 = signal->theData[0];
  const Uint32 Tdata1 = signal->theData[1];
  const Uint32 Tdata2 = signal->theData[2];
  
  switch(Tdata0) {
  case BackupContinueB::RESET_DISK_SPEED_COUNTER:
  {
    jam();
    /*
      Adjust for upto 10 millisecond delay of this signal. Longer
      delays will not be handled, in this case the system is most
      likely under too high load and it won't matter very much that
      we decrease the speed of checkpoints.

      We use a technique where we allow an overflow write in one
      period. This overflow will be removed from the next period
      such that the load will at average be as specified.
    */
    int delay_time = m_reset_delay_used;
    const NDB_TICKS curr_time = NdbTick_getCurrentTicks();
    int sig_delay = int(NdbTick_Elapsed(m_reset_disk_speed_time,curr_time).milliSec());

    /* If we overflowed in the last period, count it in 
     * this new period, potentially overflowing again into
     * future periods...
     */
    /* How much overflow will we 'consume' this period?
     * +1 to ensure that in the 'period full' case we
     * do not write any more this period
     */
    Uint32 overflowThisPeriod = MIN(m_overflow_disk_write, 
                                    m_curr_disk_write_speed + 1);
    
    /* How much overflow remains after this period? */
    Uint32 remainingOverFlow = m_overflow_disk_write - overflowThisPeriod;
    
    if (overflowThisPeriod)
    {
      jam();
#ifdef DEBUG_CHECKPOINTSPEED
      ndbout_c("Overflow of %u bytes (max/period is %u bytes)",
               overflowThisPeriod * 4, m_curr_disk_write_speed * 4);
#endif
      if (remainingOverFlow)
      {
        jam();
#ifdef DEBUG_CHECKPOINTSPEED
        ndbout_c("  Extra overflow : %u bytes, will take %u further periods to clear",
                 remainingOverFlow * 4,
                 remainingOverFlow / m_curr_disk_write_speed);
#endif
      }
    }

    m_words_written_this_period = overflowThisPeriod;
    m_periods_passed_in_monitor_period++;
    m_overflow_disk_write = remainingOverFlow;
    m_reset_disk_speed_time = curr_time;

    if (sig_delay > delay_time + 10)
      delay_time = Backup::DISK_SPEED_CHECK_DELAY - 10;
    else if (sig_delay < delay_time - 10)
      delay_time = Backup::DISK_SPEED_CHECK_DELAY + 10;
    else
      delay_time = Backup::DISK_SPEED_CHECK_DELAY - (sig_delay - delay_time);
    m_reset_delay_used= delay_time;
    signal->theData[0] = BackupContinueB::RESET_DISK_SPEED_COUNTER;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, delay_time, 1);

    {
      /* Independent check of DiskCheckpointSpeed.
       * We check every second or so that we are roughly sticking
       * to our diet.
       */
      const Uint64 millisPassed = 
        NdbTick_Elapsed(m_monitor_snapshot_start,curr_time).milliSec();

      if (millisPassed >= 1000)
      {
        jam();
        const Uint64 periodsPassed = (millisPassed / DISK_SPEED_CHECK_DELAY) + 1;
        const Uint64 quotaWordsPerPeriod = m_curr_disk_write_speed;
        const Uint64 maxOverFlowWords = c_defaults.m_maxWriteSize / 4;
        const Uint64 maxExpectedWords = (periodsPassed * quotaWordsPerPeriod) + maxOverFlowWords;
        
        if (unlikely(m_monitor_words_written > maxExpectedWords))
        {
          jam();
          /* In the last monitoring interval, we have written more words
           * than allowed by the quota (DiskCheckpointSpeed), including
           * transient spikes due to a single MaxBackupWriteSize write
           */
          ndbout << "Backup : Excessive Backup/LCP write rate in last monitoring period - recorded = "
                 << (m_monitor_words_written * 4 * 1000) / millisPassed
                 << " bytes/s, configured = "
                 << m_curr_disk_write_speed * 4 * 10
                 << " bytes/s" << endl;
          ndbout << "Backup : Monitoring period : " << millisPassed
                 << " millis. Bytes written : " << (m_monitor_words_written * 4)
                 << ".  Max allowed : " << (maxExpectedWords * 4) << endl;
          ndbout << "Actual number of periods in this monitoring interval: ";
          ndbout << m_periods_passed_in_monitor_period;
          ndbout << " calculated number was: " << periodsPassed << endl;
        }
        /* Reset the monitor */
        m_monitor_words_written = 0;
        m_periods_passed_in_monitor_period = 0;
        m_monitor_snapshot_start = curr_time;
      }
    }
#if 0
    ndbout << "Signal delay was = " << sig_delay;
    ndbout << " Current time = " << curr_time << endl;
    ndbout << " Delay time will be = " << delay_time << endl << endl;
#endif
    break;
  }
  case BackupContinueB::BACKUP_FRAGMENT_INFO:
  {
    jam();
    const Uint32 ptr_I = Tdata1;
    Uint32 tabPtr_I = Tdata2;
    Uint32 fragPtr_I = signal->theData[3];

    BackupRecordPtr ptr LINT_SET_PTR;
    c_backupPool.getPtr(ptr, ptr_I);
    TablePtr tabPtr;
    ptr.p->tables.getPtr(tabPtr, tabPtr_I);

    if (fragPtr_I != tabPtr.p->fragments.getSize())
    {
      jam();
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, fragPtr_I);
      
      BackupFilePtr filePtr LINT_SET_PTR;
      ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
      
      const Uint32 sz = sizeof(BackupFormat::CtlFile::FragmentInfo) >> 2;
      Uint32 * dst;
      if (!filePtr.p->operation.dataBuffer.getWritePtr(&dst, sz))
      {
	sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 4);
	return;
      }
      
      BackupFormat::CtlFile::FragmentInfo * fragInfo = 
	(BackupFormat::CtlFile::FragmentInfo*)dst;
      fragInfo->SectionType = htonl(BackupFormat::FRAGMENT_INFO);
      fragInfo->SectionLength = htonl(sz);
      fragInfo->TableId = htonl(fragPtr.p->tableId);
      fragInfo->FragmentNo = htonl(fragPtr_I);
      fragInfo->NoOfRecordsLow = htonl((Uint32)(fragPtr.p->noOfRecords & 0xFFFFFFFF));
      fragInfo->NoOfRecordsHigh = htonl((Uint32)(fragPtr.p->noOfRecords >> 32));
      fragInfo->FilePosLow = htonl(0);
      fragInfo->FilePosHigh = htonl(0);
      
      filePtr.p->operation.dataBuffer.updateWritePtr(sz);
      
      fragPtr_I++;
    }
    
    if (fragPtr_I == tabPtr.p->fragments.getSize())
    {
      BackupLockTab *req = (BackupLockTab *)signal->getDataPtrSend();
      req->m_senderRef = reference();
      req->m_tableId = tabPtr.p->tableId;
      req->m_lock_unlock = BackupLockTab::UNLOCK_TABLE;
      req->m_backup_state = BackupLockTab::BACKUP_FRAGMENT_INFO;
      req->m_backupRecordPtr_I = ptr_I;
      req->m_tablePtr_I = tabPtr_I;
      sendSignal(DBDICT_REF, GSN_BACKUP_LOCK_TAB_REQ, signal,
                 BackupLockTab::SignalLength, JBB);
      return;
    }
    
    signal->theData[0] = BackupContinueB::BACKUP_FRAGMENT_INFO;
    signal->theData[1] = ptr_I;
    signal->theData[2] = tabPtr_I;
    signal->theData[3] = fragPtr_I;
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  case BackupContinueB::START_FILE_THREAD:
  case BackupContinueB::BUFFER_UNDERFLOW:
  {
    jam();
    BackupFilePtr filePtr LINT_SET_PTR;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    checkFile(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_SCAN:
  {
    jam();
    BackupFilePtr filePtr LINT_SET_PTR;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    checkScan(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_FRAG_COMPLETE:
  {
    jam();
    BackupFilePtr filePtr LINT_SET_PTR;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    fragmentCompleted(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_META:
  {
    jam();
    BackupRecordPtr ptr LINT_SET_PTR;
    c_backupPool.getPtr(ptr, Tdata1);
    
    BackupFilePtr filePtr LINT_SET_PTR;
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    FsBuffer & buf = filePtr.p->operation.dataBuffer;
    
    if(buf.getFreeSize() < buf.getMaxWrite()) {
      jam();
      TablePtr tabPtr LINT_SET_PTR;
      c_tablePool.getPtr(tabPtr, Tdata2);
      
      DEBUG_OUT("Backup - Buffer full - " 
                << buf.getFreeSize()
		<< " < " << buf.getMaxWrite()
                << " (sz: " << buf.getUsableSize()
                << " getMinRead: " << buf.getMinRead()
		<< ") - tableId = " << tabPtr.p->tableId);
      
      signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
      signal->theData[1] = Tdata1;
      signal->theData[2] = Tdata2;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 3);
      return;
    }//if
    
    TablePtr tabPtr LINT_SET_PTR;
    c_tablePool.getPtr(tabPtr, Tdata2);
    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = GetTabInfoReq::RequestById |
      GetTabInfoReq::LongSignalConf;
    req->tableId = tabPtr.p->tableId;
    req->schemaTransId = 0;
    sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal, 
	       GetTabInfoReq::SignalLength, JBB);
    return;
  }
  case BackupContinueB::ZDELAY_SCAN_NEXT:
    if (ERROR_INSERTED(10039))
    {
      jam();
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 300, 
			  signal->getLength());
      return;
    }
    else
    {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      ndbout_c("Resuming backup");

      Uint32 filePtr_I = Tdata1;
      BackupFilePtr filePtr;
      c_backupFilePool.getPtr(filePtr, filePtr_I);
      BackupRecordPtr ptr;
      c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
      TablePtr tabPtr;
      findTable(ptr, tabPtr, filePtr.p->tableId);
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, filePtr.p->fragmentNo);

      BlockReference lqhRef = 0;
      if (ptr.p->is_lcp()) {
        lqhRef = calcInstanceBlockRef(DBLQH);
      } else {
        const Uint32 instanceKey = fragPtr.p->lqhInstanceKey;
        ndbrequire(instanceKey != 0);
        lqhRef = numberToRef(DBLQH, instanceKey, getOwnNodeId());
      }

      memmove(signal->theData, signal->theData + 2, 
	      4*ScanFragNextReq::SignalLength);

      sendSignal(lqhRef, GSN_SCAN_NEXTREQ, signal, 
		 ScanFragNextReq::SignalLength, JBB);
      return ;
    }
  default:
    ndbrequire(0);
  }//switch
}

void
Backup::execBACKUP_LOCK_TAB_CONF(Signal *signal)
{
  jamEntry();
  const BackupLockTab *conf = (const BackupLockTab *)signal->getDataPtrSend();
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, conf->m_backupRecordPtr_I);
  TablePtr tabPtr;
  ptr.p->tables.getPtr(tabPtr, conf->m_tablePtr_I);

  switch(conf->m_backup_state) {
  case BackupLockTab::BACKUP_FRAGMENT_INFO:
  {
    jam();
    ptr.p->tables.next(tabPtr);
    if (tabPtr.i == RNIL)
    {
      jam();
      closeFiles(signal, ptr);
      return;
    }

    signal->theData[0] = BackupContinueB::BACKUP_FRAGMENT_INFO;
    signal->theData[1] = ptr.i;
    signal->theData[2] = tabPtr.i;
    signal->theData[3] = 0;       // Start from first fragment of next table
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  case BackupLockTab::GET_TABINFO_CONF:
  {
    jam();
    if (conf->errorCode)
    {
      jam();
      defineBackupRef(signal, ptr, conf->errorCode);
      return;
    }

    ptr.p->tables.next(tabPtr);
    afterGetTabinfoLockTab(signal, ptr, tabPtr);
    return;
  }
  case BackupLockTab::CLEANUP:
  {
    jam();
    ptr.p->tables.next(tabPtr);
    cleanupNextTable(signal, ptr, tabPtr);
    return;
  }
  default:
    ndbrequire(false);
  }
}

void
Backup::execBACKUP_LOCK_TAB_REF(Signal *signal)
{
  jamEntry();
  ndbrequire(false /* Not currently possible. */);
}

void
Backup::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
  
  /* Dump commands used in public interfaces */
  switch (signal->theData[0]) {
  case DumpStateOrd::BackupStatus:
  {
    /* See code in BackupProxy.cpp as well */
    BlockReference result_ref = CMVMI_REF;
    if (signal->length() == 2)
      result_ref = signal->theData[1];

    BackupRecordPtr ptr;
    int reported = 0;
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr))
    {
      if (!ptr.p->is_lcp())
      {
        reportStatus(signal, ptr, result_ref);
        reported++;
      }
    }
    if (!reported)
      reportStatus(signal, ptr, result_ref);
    return;
  }
  default:
    /* continue to debug section */
    break;
  }

  /* Debugging or unclassified section */

  if(signal->theData[0] == 20){
    if(signal->length() > 1){
      c_defaults.m_dataBufferSize = (signal->theData[1] * 1024 * 1024);
    }
    if(signal->length() > 2){
      c_defaults.m_logBufferSize = (signal->theData[2] * 1024 * 1024);
    }
    if(signal->length() > 3){
      c_defaults.m_minWriteSize = signal->theData[3] * 1024;
    }
    if(signal->length() > 4){
      c_defaults.m_maxWriteSize = signal->theData[4] * 1024;
    }
    
    infoEvent("Backup: data: %d log: %d min: %d max: %d",
	      c_defaults.m_dataBufferSize,
	      c_defaults.m_logBufferSize,
	      c_defaults.m_minWriteSize,
	      c_defaults.m_maxWriteSize);
    return;
  }
  if(signal->theData[0] == 21){
    BackupReq * req = (BackupReq*)signal->getDataPtrSend();
    req->senderData = 23;
    req->backupDataLen = 0;
    sendSignal(reference(), GSN_BACKUP_REQ,signal,BackupReq::SignalLength, JBB);
    startTime = NdbTick_getCurrentTicks();
    return;
  }

  if(signal->theData[0] == 22){
    const Uint32 seq = signal->theData[1];
    FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
    req->userReference = reference();
    req->userPointer = 23;
    req->directory = 1;
    req->ownDirectory = 1;
    FsOpenReq::setVersion(req->fileNumber, 2);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
    FsOpenReq::v2_setSequence(req->fileNumber, seq);
    FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
    sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	       FsRemoveReq::SignalLength, JBA);
    return;
  }

  if(signal->theData[0] == 23){
    /**
     * Print records
     */
    BackupRecordPtr ptr LINT_SET_PTR;
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)){
      infoEvent("BackupRecord %d: BackupId: %u MasterRef: %x ClientRef: %x",
		ptr.i, ptr.p->backupId, ptr.p->masterRef, ptr.p->clientRef);
      infoEvent(" State: %d", ptr.p->slaveState.getState());
      BackupFilePtr filePtr;
      for(ptr.p->files.first(filePtr); filePtr.i != RNIL; 
	  ptr.p->files.next(filePtr)){
	jam();
	infoEvent(" file %d: type: %d flags: H'%x",
		  filePtr.i, filePtr.p->fileType, 
		  filePtr.p->m_flags);
      }
    }

    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint64 resetElapsed = NdbTick_Elapsed(m_reset_disk_speed_time,now).milliSec();
    const Uint64 millisPassed = NdbTick_Elapsed(m_monitor_snapshot_start,now).milliSec();
    /* Dump measured disk write speed since last RESET_DISK_SPEED */
    ndbout_c("m_curr_disk_write_speed: %u  m_words_written_this_period: %u  m_overflow_disk_write: %u",
              m_curr_disk_write_speed, m_words_written_this_period, m_overflow_disk_write);
    ndbout_c("m_reset_delay_used: %u  time since last RESET_DISK_SPEED: %llu millis",
             m_reset_delay_used, resetElapsed);
    /* Dump measured rate since last snapshot start */
    Uint64 byteRate = (4000 * m_monitor_words_written) / (millisPassed + 1);
    ndbout_c("m_monitor_words_written : %llu, duration : %llu millis, rate : %llu bytes/s : (%u pct of config)",
             m_monitor_words_written, millisPassed, 
             byteRate,
             (Uint32) (100 * byteRate / (4 * 10)) / m_curr_disk_write_speed);

    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr))
    {
      ndbout_c("BackupRecord %u:  BackupId: %u  MasterRef: %x  ClientRef: %x",
               ptr.i, ptr.p->backupId, ptr.p->masterRef, ptr.p->clientRef);
      ndbout_c(" State: %u", ptr.p->slaveState.getState());
      ndbout_c(" noOfByte: %llu  noOfRecords: %llu",
               ptr.p->noOfBytes, ptr.p->noOfRecords);
      ndbout_c(" noOfLogBytes: %llu  noOfLogRecords: %llu",
               ptr.p->noOfLogBytes, ptr.p->noOfLogRecords);
      ndbout_c(" errorCode: %u", ptr.p->errorCode);
      BackupFilePtr filePtr;
      for(ptr.p->files.first(filePtr); filePtr.i != RNIL; 
	  ptr.p->files.next(filePtr))
      {
	ndbout_c(" file %u:  type: %u  flags: H'%x  tableId: %u  fragmentId: %u",
                 filePtr.i, filePtr.p->fileType, filePtr.p->m_flags,
                 filePtr.p->tableId, filePtr.p->fragmentNo);
      }
      if (ptr.p->slaveState.getState() == SCANNING && ptr.p->dataFilePtr != RNIL)
      {
        c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
        OperationRecord & op = filePtr.p->operation;
        Uint32 *tmp = NULL;
        Uint32 sz = 0;
        bool eof = FALSE;
        bool ready = op.dataBuffer.getReadPtr(&tmp, &sz, &eof);
        ndbout_c("ready: %s  eof: %s", ready ? "TRUE" : "FALSE", eof ? "TRUE" : "FALSE");
      }
    }
    return;
  }
  if(signal->theData[0] == 24){
    /**
     * Print size of records etc.
     */
    infoEvent("Backup - dump pool sizes");
    infoEvent("BackupPool: %d BackupFilePool: %d TablePool: %d",
	      c_backupPool.getSize(), c_backupFilePool.getSize(), 
	      c_tablePool.getSize());
    infoEvent("AttrPool: %d TriggerPool: %d FragmentPool: %d",
	      c_backupPool.getSize(), c_backupFilePool.getSize(), 
	      c_tablePool.getSize());
    infoEvent("PagePool: %d",
	      c_pagePool.getSize());


    if(signal->getLength() == 2 && signal->theData[1] == 2424)
    {
      /**
       * Handle LCP
       */
      BackupRecordPtr lcp;
      ndbrequire(c_backups.first(lcp));
      
      ndbrequire(c_backupPool.getSize() == c_backupPool.getNoOfFree() + 1);
      if(lcp.p->tables.isEmpty())
      {
	ndbrequire(c_tablePool.getSize() == c_tablePool.getNoOfFree());
	ndbrequire(c_fragmentPool.getSize() == c_fragmentPool.getNoOfFree());
	ndbrequire(c_triggerPool.getSize() == c_triggerPool.getNoOfFree());
      }
      ndbrequire(c_backupFilePool.getSize() == c_backupFilePool.getNoOfFree() + 1);
      BackupFilePtr lcp_file;
      c_backupFilePool.getPtr(lcp_file, lcp.p->dataFilePtr);
      ndbrequire(c_pagePool.getSize() == 
		 c_pagePool.getNoOfFree() + 
		 lcp_file.p->pages.getSize());
    }
  }

  if(signal->theData[0] == DumpStateOrd::DumpBackup)
  {
    /* Display a bunch of stuff about Backup defaults */
    infoEvent("Compressed Backup: %d", c_defaults.m_compressed_backup);
    infoEvent("Compressed LCP: %d", c_defaults.m_compressed_lcp);
  }

  if(signal->theData[0] == DumpStateOrd::DumpBackupSetCompressed)
  {
    c_defaults.m_compressed_backup= signal->theData[1];
    infoEvent("Compressed Backup: %d", c_defaults.m_compressed_backup);
  }

  if(signal->theData[0] == DumpStateOrd::DumpBackupSetCompressedLCP)
  {
    c_defaults.m_compressed_lcp= signal->theData[1];
    infoEvent("Compressed LCP: %d", c_defaults.m_compressed_lcp);
  }

  if (signal->theData[0] == DumpStateOrd::BackupErrorInsert)
  {
    if (signal->getLength() == 1)
      ndbout_c("BACKUP: setting error %u", signal->theData[1]);
    else
      ndbout_c("BACKUP: setting error %u, %u",
               signal->theData[1], signal->theData[2]);
    SET_ERROR_INSERT_VALUE2(signal->theData[1], signal->theData[2]);
  }
}

void Backup::execDBINFO_SCANREQ(Signal *signal)
{
  jamEntry();
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));

  Ndbinfo::Ratelimit rl;

  switch(req.tableId){
  case Ndbinfo::POOLS_TABLEID:
  {
    Ndbinfo::pool_entry pools[] =
    {
      { "Backup Record",
        c_backupPool.getUsed(),
        c_backupPool.getSize(),
        c_backupPool.getEntrySize(),
        c_backupPool.getUsedHi(),
        { CFG_DB_PARALLEL_BACKUPS,0,0,0 }},
      { "Backup File",
        c_backupFilePool.getUsed(),
        c_backupFilePool.getSize(),
        c_backupFilePool.getEntrySize(),
        c_backupFilePool.getUsedHi(),
        { CFG_DB_PARALLEL_BACKUPS,0,0,0 }},
      { "Table",
        c_tablePool.getUsed(),
        c_tablePool.getSize(),
        c_tablePool.getEntrySize(),
        c_tablePool.getUsedHi(),
        { CFG_DB_PARALLEL_BACKUPS,
          CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES }},
      { "Trigger",
        c_triggerPool.getUsed(),
        c_triggerPool.getSize(),
        c_triggerPool.getEntrySize(),
        c_triggerPool.getUsedHi(),
        { CFG_DB_PARALLEL_BACKUPS,
          CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES }},
      { "Fragment",
        c_fragmentPool.getUsed(),
        c_fragmentPool.getSize(),
        c_fragmentPool.getEntrySize(),
        c_fragmentPool.getUsedHi(),
        { CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES,0 }},
      { "Page",
        c_pagePool.getUsed(),
        c_pagePool.getSize(),
        c_pagePool.getEntrySize(),
        c_pagePool.getUsedHi(),
        { CFG_DB_BACKUP_MEM,
          CFG_DB_BACKUP_DATA_BUFFER_MEM,0,0 }},
      { NULL, 0,0,0,0, { 0,0,0,0 }}
    };

    const size_t num_config_params =
      sizeof(pools[0].config_params) / sizeof(pools[0].config_params[0]);
    Uint32 pool = cursor->data[0];
    BlockNumber bn = blockToMain(number());
    while(pools[pool].poolname)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(bn);           // block number
      row.write_uint32(instance());   // block instance
      row.write_string(pools[pool].poolname);

      row.write_uint64(pools[pool].used);
      row.write_uint64(pools[pool].total);
      row.write_uint64(pools[pool].used_hi);
      row.write_uint64(pools[pool].entry_size);
      for (size_t i = 0; i < num_config_params; i++)
        row.write_uint32(pools[pool].config_params[i]);
      ndbinfo_send_row(signal, req, row, rl);
      pool++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pool);
        return;
      }
    }
    break;
  }
  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

bool
Backup::findTable(const BackupRecordPtr & ptr, 
		  TablePtr & tabPtr, Uint32 tableId) const
{
  for(ptr.p->tables.first(tabPtr); 
      tabPtr.i != RNIL; 
      ptr.p->tables.next(tabPtr)) {
    jam();
    if(tabPtr.p->tableId == tableId){
      jam();
      return true;
    }//if
  }//for
  tabPtr.i = RNIL;
  tabPtr.p = 0;
  return false;
}

static Uint32 xps(Uint64 x, Uint64 ms)
{
  float fx = float(x);
  float fs = float(ms);
  
  if(ms == 0 || x == 0) {
    jamNoBlock();
    return 0;
  }//if
  jamNoBlock();
  return ((Uint32)(1000.0f * (fx + fs/2.1f))) / ((Uint32)fs);
}

struct Number {
  Number(Uint64 r) { val = r;}
  Number & operator=(Uint64 r) { val = r; return * this; }
  Uint64 val;
};

NdbOut &
operator<< (NdbOut & out, const Number & val){
  char p = 0;
  Uint32 loop = 1;
  while(val.val > loop){
    loop *= 1000;
    p += 3;
  }
  if(loop != 1){
    p -= 3;
    loop /= 1000;
  }

  switch(p){
  case 0:
    break;
  case 3:
    p = 'k';
    break;
  case 6:
    p = 'M';
    break;
  case 9:
    p = 'G';
    break;
  default:
    p = 0;
  }
  char str[2];
  str[0] = p;
  str[1] = 0;
  Uint32 tmp = (Uint32)((val.val + (loop >> 1)) / loop);
#if 1
  if(p > 0)
    out << tmp << str;
  else
    out << tmp;
#else
  out << val.val;
#endif

  return out;
}

void
Backup::execBACKUP_CONF(Signal* signal)
{
  jamEntry();
  BackupConf * conf = (BackupConf*)signal->getDataPtr();
  
  ndbout_c("Backup %u has started", conf->backupId);
}

void
Backup::execBACKUP_REF(Signal* signal)
{
  jamEntry();
  BackupRef * ref = (BackupRef*)signal->getDataPtr();

  ndbout_c("Backup (%u) has NOT started %d", ref->senderData, ref->errorCode);
}

void
Backup::execBACKUP_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  BackupCompleteRep* rep = (BackupCompleteRep*)signal->getDataPtr();
 
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const Uint64 elapsed = NdbTick_Elapsed(startTime,now).milliSec();
  
  ndbout_c("Backup %u has completed", rep->backupId);
  const Uint64 bytes =
    rep->noOfBytesLow + (((Uint64)rep->noOfBytesHigh) << 32);
  const Uint64 records =
    rep->noOfRecordsLow + (((Uint64)rep->noOfRecordsHigh) << 32);

  Number rps = xps(records, elapsed);
  Number bps = xps(bytes, elapsed);

  ndbout << " Data [ "
	 << Number(records) << " rows " 
	 << Number(bytes) << " bytes " << elapsed << " ms ] " 
	 << " => "
	 << rps << " row/s & " << bps << "b/s" << endl;

  bps = xps(rep->noOfLogBytes, elapsed);
  rps = xps(rep->noOfLogRecords, elapsed);

  ndbout << " Log [ "
	 << Number(rep->noOfLogRecords) << " log records " 
	 << Number(rep->noOfLogBytes) << " bytes " << elapsed << " ms ] " 
	 << " => "
	 << rps << " records/s & " << bps << "b/s" << endl;

}

void
Backup::execBACKUP_ABORT_REP(Signal* signal)
{
  jamEntry();
  BackupAbortRep* rep = (BackupAbortRep*)signal->getDataPtr();
  
  ndbout_c("Backup %u has been aborted %d", rep->backupId, rep->reason);
}

const TriggerEvent::Value triggerEventValues[] = {
  TriggerEvent::TE_INSERT,
  TriggerEvent::TE_UPDATE,
  TriggerEvent::TE_DELETE
};

const Backup::State 
Backup::validSlaveTransitions[] = {
  INITIAL,  DEFINING,
  DEFINING, DEFINED,
  DEFINED,  STARTED,
  STARTED,  STARTED, // Several START_BACKUP_REQ is sent
  STARTED,  SCANNING,
  SCANNING, STARTED,
  STARTED,  STOPPING,
  STOPPING, CLEANING,
  CLEANING, INITIAL,
  
  INITIAL,  ABORTING, // Node fail
  DEFINING, ABORTING,
  DEFINED,  ABORTING,
  STARTED,  ABORTING,
  SCANNING, ABORTING,
  STOPPING, ABORTING,
  CLEANING, ABORTING, // Node fail w/ master takeover
  ABORTING, ABORTING, // Slave who initiates ABORT should have this transition
  
  ABORTING, INITIAL,
  INITIAL,  INITIAL
};

const Uint32
Backup::validSlaveTransitionsCount = 
sizeof(Backup::validSlaveTransitions) / sizeof(Backup::State);

void
Backup::CompoundState::setState(State newState){
  bool found = false;
  const State currState = state;
  for(unsigned i = 0; i<noOfValidTransitions; i+= 2) {
    jam();
    if(validTransitions[i]   == currState &&
       validTransitions[i+1] == newState){
      jam();
      found = true;
      break;
    }
  }

  //ndbrequire(found);
  
  if (newState == INITIAL)
    abortState = INITIAL;
  if(newState == ABORTING && currState != ABORTING) {
    jam();
    abortState = currState;
  }
  state = newState;
#ifdef DEBUG_ABORT
  if (newState != currState) {
    ndbout_c("%u: Old state = %u, new state = %u, abort state = %u",
	     id, currState, newState, abortState);
  }
#endif
}

void
Backup::CompoundState::forceState(State newState)
{
  const State currState = state;
  if (newState == INITIAL)
    abortState = INITIAL;
  if(newState == ABORTING && currState != ABORTING) {
    jam();
    abortState = currState;
  }
  state = newState;
#ifdef DEBUG_ABORT
  if (newState != currState) {
    ndbout_c("%u: FORCE: Old state = %u, new state = %u, abort state = %u",
	     id, currState, newState, abortState);
  }
#endif
}

Backup::Table::Table(ArrayPool<Fragment> & fh)
  : fragments(fh)
{
  triggerIds[0] = ILLEGAL_TRIGGER_ID;
  triggerIds[1] = ILLEGAL_TRIGGER_ID;
  triggerIds[2] = ILLEGAL_TRIGGER_ID;
  triggerAllocated[0] = false;
  triggerAllocated[1] = false;
  triggerAllocated[2] = false;
}

/*****************************************************************************
 * 
 * Node state handling
 *
 *****************************************************************************/
void
Backup::execNODE_FAILREP(Signal* signal)
{
  jamEntry();

  NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  
  bool doStuff = false;
  /*
  Start by saving important signal data which will be destroyed before the
  process is completed.
  */
  NodeId new_master_node_id = rep->masterNodeId;
  Uint32 theFailedNodes[NdbNodeBitmask::Size];
  for (Uint32 i = 0; i < NdbNodeBitmask::Size; i++)
    theFailedNodes[i] = rep->theNodes[i];
  
  c_masterNodeId = new_master_node_id;

  NodePtr nodePtr;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NdbNodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)){
      if(nodePtr.p->alive){
	jam();
	ndbrequire(c_aliveNodes.get(nodePtr.p->nodeId));
	doStuff = true;
      } else {
        jam();
	ndbrequire(!c_aliveNodes.get(nodePtr.p->nodeId));
      }//if
      nodePtr.p->alive = 0;
      c_aliveNodes.clear(nodePtr.p->nodeId);
    }//if
  }//for

  if(!doStuff){
    jam();
    return;
  }//if
  
#ifdef DEBUG_ABORT
  ndbout_c("****************** Node fail rep ******************");
#endif

  NodeId newCoordinator = c_masterNodeId;
  BackupRecordPtr ptr;
  for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
    jam();
    checkNodeFail(signal, ptr, newCoordinator, theFailedNodes);
  }

  /* Block level cleanup */
  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(NdbNodeBitmask::get(theFailedNodes, i))
    {
      jam();
      Uint32 elementsCleaned = simBlockNodeFailure(signal, i); // No callback
      ndbassert(elementsCleaned == 0); // Backup should have no distributed frag signals
      (void) elementsCleaned; // Remove compiler warning
    }//if
  }//for
}

bool
Backup::verifyNodesAlive(BackupRecordPtr ptr,
			 const NdbNodeBitmask& aNodeBitMask)
{
  Uint32 version = getNodeInfo(getOwnNodeId()).m_version;
  for (Uint32 i = 0; i < MAX_NDB_NODES; i++) {
    jam();
    if(aNodeBitMask.get(i)) {
      if(!c_aliveNodes.get(i)){
        jam();
	ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
        return false;
      }//if
      if(getNodeInfo(i).m_version != version)
      {
	jam();
	ptr.p->setErrorCode(AbortBackupOrd::IncompatibleVersions);
	return false;
      }
    }//if
  }//for
  return true;
}

void
Backup::checkNodeFail(Signal* signal,
		      BackupRecordPtr ptr,
		      NodeId newCoord,
		      Uint32 theFailedNodes[NdbNodeBitmask::Size])
{
  NdbNodeBitmask mask;
  mask.assign(2, theFailedNodes);

  /* Update ptr.p->nodes to be up to date with current alive nodes
   */
  NodePtr nodePtr;
  bool found = false;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NdbNodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)) {
      jam();
      if (ptr.p->nodes.get(nodePtr.p->nodeId)) {
	jam();
	ptr.p->nodes.clear(nodePtr.p->nodeId); 
	found = true;
      }
    }//if
  }//for

  if(!found) {
    jam();
    return; // failed node is not part of backup process, safe to continue
  }

  if(mask.get(refToNode(ptr.p->masterRef)))
  {
    /**
     * Master died...abort
     */
    ptr.p->masterRef = reference();
    ptr.p->nodes.clear();
    ptr.p->nodes.set(getOwnNodeId());
    ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
    switch(ptr.p->m_gsn){
    case GSN_DEFINE_BACKUP_REQ:
    case GSN_START_BACKUP_REQ:
    case GSN_BACKUP_FRAGMENT_REQ:
    case GSN_STOP_BACKUP_REQ:
      // I'm currently processing...reply to self and abort...
      ptr.p->masterData.gsn = ptr.p->m_gsn;
      ptr.p->masterData.sendCounter = ptr.p->nodes;
      return;
    case GSN_DEFINE_BACKUP_REF:
    case GSN_DEFINE_BACKUP_CONF:
    case GSN_START_BACKUP_REF:
    case GSN_START_BACKUP_CONF:
    case GSN_BACKUP_FRAGMENT_REF:
    case GSN_BACKUP_FRAGMENT_CONF:
    case GSN_STOP_BACKUP_REF:
    case GSN_STOP_BACKUP_CONF:
      ptr.p->masterData.gsn = GSN_DEFINE_BACKUP_REQ;
      masterAbort(signal, ptr);
      return;
    case GSN_ABORT_BACKUP_ORD:
      // Already aborting
      return;
    }
  }
  else if (newCoord == getOwnNodeId())
  {
    /**
     * I'm master for this backup
     */
    jam();
    CRASH_INSERTION((10001));
#ifdef DEBUG_ABORT
    ndbout_c("**** Master: Node failed: Master id = %u", 
	     refToNode(ptr.p->masterRef));
#endif

    Uint32 gsn, len, pos;
    ptr.p->nodes.bitANDC(mask);
    switch(ptr.p->masterData.gsn){
    case GSN_DEFINE_BACKUP_REQ:
    {
      DefineBackupRef * ref = (DefineBackupRef*)signal->getDataPtr();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_DEFINE_BACKUP_REF;
      len= DefineBackupRef::SignalLength;
      pos= Uint32(&ref->nodeId - signal->getDataPtr());
      break;
    }
    case GSN_START_BACKUP_REQ:
    {
      StartBackupRef * ref = (StartBackupRef*)signal->getDataPtr();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_START_BACKUP_REF;
      len= StartBackupRef::SignalLength;
      pos= Uint32(&ref->nodeId - signal->getDataPtr());
      break;
    }
    case GSN_BACKUP_FRAGMENT_REQ:
    {
      BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtr();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_BACKUP_FRAGMENT_REF;
      len= BackupFragmentRef::SignalLength;
      pos= Uint32(&ref->nodeId - signal->getDataPtr());
      break;
    }
    case GSN_STOP_BACKUP_REQ:
    {
      StopBackupRef * ref = (StopBackupRef*)signal->getDataPtr();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      ref->nodeId = getOwnNodeId();
      gsn= GSN_STOP_BACKUP_REF;
      len= StopBackupRef::SignalLength;
      pos= Uint32(&ref->nodeId - signal->getDataPtr());
      break;
    }
    case GSN_WAIT_GCP_REQ:
    case GSN_DROP_TRIG_IMPL_REQ:
    case GSN_CREATE_TRIG_IMPL_REQ:
    case GSN_ALTER_TRIG_IMPL_REQ:
      ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
      return;
    case GSN_UTIL_SEQUENCE_REQ:
    case GSN_UTIL_LOCK_REQ:
      return;
    default:
      ndbrequire(false);
    }
    
    for(Uint32 i = 0; (i = mask.find(i+1)) != NdbNodeBitmask::NotFound; )
    {
      signal->theData[pos] = i;
      sendSignal(reference(), gsn, signal, len, JBB);
#ifdef DEBUG_ABORT
      ndbout_c("sending %d to self from %d", gsn, i);
#endif
    }
    return;
  }//if
  
  /**
   * I abort myself as slave if not master
   */
  CRASH_INSERTION((10021));
} 

void
Backup::execINCL_NODEREQ(Signal* signal)
{
  jamEntry();
  
  const Uint32 senderRef = signal->theData[0];
  const Uint32 inclNode  = signal->theData[1];

  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(inclNode == nodeId){
      jam();
      
      ndbrequire(node.p->alive == 0);
      ndbrequire(!c_aliveNodes.get(nodeId));
      
      node.p->alive = 1;
      c_aliveNodes.set(nodeId);
      
      break;
    }//if
  }//for
  signal->theData[0] = inclNode;
  signal->theData[1] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 2, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Define backup
 *
 *****************************************************************************/

void
Backup::execBACKUP_REQ(Signal* signal)
{
  jamEntry();
  BackupReq * req = (BackupReq*)signal->getDataPtr();
  
  const Uint32 senderData = req->senderData;
  const BlockReference senderRef = signal->senderBlockRef();
  const Uint32 dataLen32 = req->backupDataLen; // In 32 bit words
  const Uint32 flags = signal->getLength() > 2 ? req->flags : 2;
  const Uint32 input_backupId = signal->getLength() > 3 ? req->inputBackupId : 0;

  if (getOwnNodeId() != getMasterNodeId())
  {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData,
                  BackupRef::IAmNotMaster);
    return;
  }//if

  if (c_defaults.m_diskless)
  {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData, 
		  BackupRef::CannotBackupDiskless);
    return;
  }
  
  if (dataLen32 != 0)
  {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData, 
		  BackupRef::BackupDefinitionNotImplemented);
    return;
  }//if
  
#ifdef DEBUG_ABORT
  dumpUsedResources();
#endif
  /**
   * Seize a backup record
   */
  BackupRecordPtr ptr;
  c_backups.seizeFirst(ptr);
  if (ptr.i == RNIL)
  {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData,
                  BackupRef::OutOfBackupRecord);
    return;
  }//if

  ndbrequire(ptr.p->tables.isEmpty());
  
  ptr.p->m_gsn = 0;
  ptr.p->errorCode = 0;
  ptr.p->clientRef = senderRef;
  ptr.p->clientData = senderData;
  ptr.p->flags = flags;
  ptr.p->masterRef = reference();
  ptr.p->nodes = c_aliveNodes;
  if (input_backupId)
  {
    jam();
    ptr.p->backupId = input_backupId;
  }
  else
  {
    jam();
    ptr.p->backupId = 0;
  }
  ptr.p->backupKey[0] = 0;
  ptr.p->backupKey[1] = 0;
  ptr.p->backupDataLen = 0;
  ptr.p->masterData.errorCode = 0;

  ptr.p->masterData.sequence.retriesLeft = 3;
  sendUtilSequenceReq(signal, ptr);
}

void
Backup::sendUtilSequenceReq(Signal* signal, BackupRecordPtr ptr, Uint32 delay)
{
  jam();

  UtilSequenceReq * utilReq = (UtilSequenceReq*)signal->getDataPtrSend();
  ptr.p->masterData.gsn = GSN_UTIL_SEQUENCE_REQ;
  utilReq->senderData  = ptr.i;
  utilReq->sequenceId  = NDB_BACKUP_SEQUENCE;

  if (ptr.p->backupId) 
  {
    jam();
    utilReq->requestType = UtilSequenceReq::SetVal;
    utilReq->value = ptr.p->backupId;
  }
  else
  {
    jam();
    utilReq->requestType = UtilSequenceReq::NextVal;
  }

  if (delay == 0)
  {
    jam();
    sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ,
               signal, UtilSequenceReq::SignalLength, JBB);
  }
  else
  {
    jam();
    sendSignalWithDelay(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ,
                        signal, delay, UtilSequenceReq::SignalLength);
  }
}

void
Backup::execUTIL_SEQUENCE_REF(Signal* signal)
{
  jamEntry();
  BackupRecordPtr ptr LINT_SET_PTR;
  UtilSequenceRef * utilRef = (UtilSequenceRef*)signal->getDataPtr();
  ptr.i = utilRef->senderData;
  c_backupPool.getPtr(ptr);
  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ);

  if (utilRef->errorCode == UtilSequenceRef::TCError)
  {
    jam();
    if (ptr.p->masterData.sequence.retriesLeft > 0)
    {
      jam();
      infoEvent("BACKUP: retrying sequence on error %u",
                utilRef->TCErrorCode);
      ptr.p->masterData.sequence.retriesLeft--;
      sendUtilSequenceReq(signal, ptr, 300);
      return;
    }
  }
  warningEvent("BACKUP: aborting due to sequence error (%u, %u)",
               utilRef->errorCode,
               utilRef->TCErrorCode);

  sendBackupRef(signal, ptr, BackupRef::SequenceFailure);
}//execUTIL_SEQUENCE_REF()

void
Backup::sendBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errorCode)
{
  jam();
  sendBackupRef(ptr.p->clientRef, ptr.p->flags, signal,
                ptr.p->clientData, errorCode);
  cleanup(signal, ptr);
}

void
Backup::sendBackupRef(BlockReference senderRef, Uint32 flags, Signal *signal,
		      Uint32 senderData, Uint32 errorCode)
{
  jam();
  if (SEND_BACKUP_STARTED_FLAG(flags))
  {
    jam();
    BackupRef* ref = (BackupRef*)signal->getDataPtrSend();
    ref->senderData = senderData;
    ref->errorCode = errorCode;
    ref->masterRef = numberToRef(BACKUP, getMasterNodeId());
    sendSignal(senderRef, GSN_BACKUP_REF, signal, BackupRef::SignalLength, JBB);
  }

  if (errorCode != BackupRef::IAmNotMaster)
  {
    jam();
    signal->theData[0] = NDB_LE_BackupFailedToStart;
    signal->theData[1] = senderRef;
    signal->theData[2] = errorCode;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }
}

void
Backup::execUTIL_SEQUENCE_CONF(Signal* signal)
{
  jamEntry();

  UtilSequenceConf * conf = (UtilSequenceConf*)signal->getDataPtr();
  
  if(conf->requestType == UtilSequenceReq::Create) 
  {
    jam();
    sendSTTORRY(signal); // At startup in NDB
    return;
  }

  BackupRecordPtr ptr LINT_SET_PTR;
  ptr.i = conf->senderData;
  c_backupPool.getPtr(ptr);

  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ);

  if (ptr.p->checkError())
  {
    jam();
    sendBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }//if

  if (ERROR_INSERTED(10023)) 
  {
    sendBackupRef(signal, ptr, 323);
    return;
  }//if


  if(!ptr.p->backupId && conf->requestType != UtilSequenceReq::SetVal)
  {
    Uint64 backupId;
    memcpy(&backupId,conf->sequenceValue,8);
    ptr.p->backupId= (Uint32)backupId;
  }

  ptr.p->backupKey[0] = (getOwnNodeId() << 16) | (ptr.p->backupId & 0xFFFF);
  ptr.p->backupKey[1] = Uint32(NdbTick_CurrentMillisecond());

  ptr.p->masterData.gsn = GSN_UTIL_LOCK_REQ;
  Mutex mutex(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  Callback c = { safe_cast(&Backup::defineBackupMutex_locked), ptr.i };
  ndbrequire(mutex.lock(c));

  return;
}

void
Backup::defineBackupMutex_locked(Signal* signal, Uint32 ptrI, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);
  
  BackupRecordPtr ptr LINT_SET_PTR;
  ptr.i = ptrI;
  c_backupPool.getPtr(ptr);
  
  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ);

  ptr.p->masterData.gsn = GSN_UTIL_LOCK_REQ;
  Mutex mutex(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  Callback c = { safe_cast(&Backup::dictCommitTableMutex_locked), ptr.i };
  ndbrequire(mutex.lock(c));
}

void
Backup::dictCommitTableMutex_locked(Signal* signal, Uint32 ptrI,Uint32 retVal)
{
  jamEntry();
  ndbrequire(retVal == 0);
  
  /**
   * We now have both the mutexes
   */
  BackupRecordPtr ptr LINT_SET_PTR;
  ptr.i = ptrI;
  c_backupPool.getPtr(ptr);

  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ);

  if (ERROR_INSERTED(10031)) {
    ptr.p->setErrorCode(331);
  }//if

  if (ptr.p->checkError())
  {
    jam();
    
    /**
     * Unlock mutexes
     */
    jam();
    Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
    jam();
    mutex1.unlock(); // ignore response
    
    jam();
    Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
    jam();
    mutex2.unlock(); // ignore response
    
    sendBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }//if
  
  sendDefineBackupReq(signal, ptr);
}

/*****************************************************************************
 * 
 * Master functionallity - Define backup cont'd (from now on all slaves are in)
 *
 *****************************************************************************/

bool
Backup::haveAllSignals(BackupRecordPtr ptr, Uint32 gsn, Uint32 nodeId)
{ 
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == gsn);
  ndbrequire(!ptr.p->masterData.sendCounter.done());
  ndbrequire(ptr.p->masterData.sendCounter.isWaitingFor(nodeId));
  
  ptr.p->masterData.sendCounter.clearWaitingFor(nodeId);
  return ptr.p->masterData.sendCounter.done();
}

void
Backup::sendDefineBackupReq(Signal *signal, BackupRecordPtr ptr)
{
  /**
   * Sending define backup to all participants
   */
  DefineBackupReq * req = (DefineBackupReq*)signal->getDataPtrSend();
  req->backupId = ptr.p->backupId;
  req->clientRef = ptr.p->clientRef;
  req->clientData = ptr.p->clientData;
  req->senderRef = reference();
  req->backupPtr = ptr.i;
  req->backupKey[0] = ptr.p->backupKey[0];
  req->backupKey[1] = ptr.p->backupKey[1];
  req->nodes = ptr.p->nodes;
  req->backupDataLen = ptr.p->backupDataLen;
  req->flags = ptr.p->flags;
  
  ptr.p->masterData.gsn = GSN_DEFINE_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
  NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
  sendSignal(rg, GSN_DEFINE_BACKUP_REQ, signal, 
	     DefineBackupReq::SignalLength, JBB);
  
  /**
   * Now send backup data
   */
  const Uint32 len = ptr.p->backupDataLen;
  if(len == 0){
    /**
     * No data to send
     */
    jam();
    return;
  }//if
  
  /**
   * Not implemented
   */
  ndbrequire(0);
}

void
Backup::execDEFINE_BACKUP_REF(Signal* signal)
{
  jamEntry();

  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtr();
  
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
  
  ptr.p->setErrorCode(ref->errorCode);
  defineBackupReply(signal, ptr, nodeId);
}

void
Backup::execDEFINE_BACKUP_CONF(Signal* signal)
{
  jamEntry();

  DefineBackupConf* conf = (DefineBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  if (ERROR_INSERTED(10024))
  {
    ptr.p->setErrorCode(324);
  }

  defineBackupReply(signal, ptr, nodeId);
}

void
Backup::defineBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{
  if (!haveAllSignals(ptr, GSN_DEFINE_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  /**
   * Unlock mutexes
   */
  jam();
  Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  jam();
  mutex1.unlock(); // ignore response

  jam();
  Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  jam();
  mutex2.unlock(); // ignore response

  if(ptr.p->checkError())
  {
    jam();
    masterAbort(signal, ptr);
    return;
  }
  
  CRASH_INSERTION((10034));

  /**
   * We've received GSN_DEFINE_BACKUP_CONF from all participants.
   *
   * Our next step is to send START_BACKUP_REQ to all participants,
   * who will then send CREATE_TRIG_REQ for all tables to their local
   * DBTUP.
   */
  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);

  sendStartBackup(signal, ptr, tabPtr);
}

/*****************************************************************************
 * 
 * Master functionallity - Prepare triggers
 *
 *****************************************************************************/
void
Backup::createAttributeMask(TablePtr tabPtr, 
			    Bitmask<MAXNROFATTRIBUTESINWORDS> & mask)
{
  mask.clear();
  for (Uint32 i = 0; i<tabPtr.p->noOfAttributes; i++)
    mask.set(i);
}

void
Backup::sendCreateTrig(Signal* signal, 
			   BackupRecordPtr ptr, TablePtr tabPtr)
{
  CreateTrigImplReq* req = (CreateTrigImplReq*)signal->getDataPtrSend();

  /*
   * First, setup the structures
   */
  for(Uint32 j=0; j<3; j++) {
    jam();

    TriggerPtr trigPtr;
    if (!ptr.p->triggers.seizeFirst(trigPtr)) {
      jam();
      ptr.p->m_gsn = GSN_START_BACKUP_REF;
      StartBackupRef* ref = (StartBackupRef*)signal->getDataPtrSend();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = StartBackupRef::FailedToAllocateTriggerRecord;
      ref->nodeId = getOwnNodeId();
      sendSignal(ptr.p->masterRef, GSN_START_BACKUP_REF, signal,
		 StartBackupRef::SignalLength, JBB);
      return;
    } // if

    const Uint32 triggerId= trigPtr.i;
    tabPtr.p->triggerIds[j] = triggerId;
    tabPtr.p->triggerAllocated[j] = true;
    trigPtr.p->backupPtr = ptr.i;
    trigPtr.p->tableId = tabPtr.p->tableId;
    trigPtr.p->tab_ptr_i = tabPtr.i;
    trigPtr.p->logEntry = 0;
    trigPtr.p->event = j;
    trigPtr.p->maxRecordSize = 4096;
    trigPtr.p->operation =
      &ptr.p->files.getPtr(ptr.p->logFilePtr)->operation;
    trigPtr.p->operation->noOfBytes = 0;
    trigPtr.p->operation->noOfRecords = 0;
    trigPtr.p->errorCode = 0;
  } // for

  /*
   * now ask DBTUP to create
   */
  ptr.p->slaveData.gsn = GSN_CREATE_TRIG_IMPL_REQ;
  ptr.p->slaveData.trigSendCounter = 3;
  ptr.p->slaveData.createTrig.tableId = tabPtr.p->tableId;

  req->senderRef = reference();
  req->receiverRef = reference();
  req->senderData = ptr.i;
  req->requestType = 0;

  Bitmask<MAXNROFATTRIBUTESINWORDS> attrMask;
  createAttributeMask(tabPtr, attrMask);

  req->tableId = tabPtr.p->tableId;
  req->tableVersion = 0;
  req->indexId = RNIL;
  req->indexVersion = 0;

  Uint32 ti = 0;
  /*
   * We always send PK for any operations and any triggertypes.
   * For SUBSCRIPTION_BEFORE
   *   We send after image for INSERT.
   *   We send before image for DELETE.
   *   We send before+after image for UPDATE.
   * For SUBSCRIPTION
   *   We send after image for INSERT.
   *   We send only PK for DELETE.
   *   We send after image for UPDATE.
   */
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION_BEFORE);
  else
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION);
  TriggerInfo::setTriggerActionTime(ti, TriggerActionTime::TA_DETACHED);
  TriggerInfo::setMonitorReplicas(ti, true);
  TriggerInfo::setMonitorAllAttributes(ti, false);

  for (int i=0; i < 3; i++) {
    req->triggerId = tabPtr.p->triggerIds[i];

    Uint32 ti2 = ti;
    TriggerInfo::setTriggerEvent(ti2, triggerEventValues[i]);
    req->triggerInfo = ti2;

    LinearSectionPtr ptr[3];
    ptr[0].p = attrMask.rep.data;
    ptr[0].sz = attrMask.getSizeInWords();

    sendSignal(DBTUP_REF, GSN_CREATE_TRIG_IMPL_REQ,
	       signal, CreateTrigImplReq::SignalLength, JBB, ptr ,1);
  }
}

void
Backup::execCREATE_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const CreateTrigImplConf* conf =
    (const CreateTrigImplConf*)signal->getDataPtr();
  
  const Uint32 ptrI = conf->senderData;
  const Uint32 tableId = conf->tableId;
  const TriggerEvent::Value type =
    TriggerInfo::getTriggerEvent(conf->triggerInfo);

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  /**
   * Verify that I'm waiting for this conf
   *
   * ptr.p->masterRef != reference()
   * as slaves and masters have triggers now.
   */
  ndbrequire(ptr.p->slaveData.gsn == GSN_CREATE_TRIG_IMPL_REQ);
  ndbrequire(ptr.p->slaveData.trigSendCounter.done() == false);
  ndbrequire(ptr.p->slaveData.createTrig.tableId == tableId);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));
  ndbrequire(type < 3); // if some decides to change the enums

  createTrigReply(signal, ptr);
}

void
Backup::execCREATE_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();
  const CreateTrigImplRef* ref =
    (const CreateTrigImplRef*)signal->getDataPtr();

  const Uint32 ptrI = ref->senderData;
  const Uint32 tableId = ref->tableId;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  /**
   * Verify that I'm waiting for this ref
   *
   * ptr.p->masterRef != reference()
   * as slaves and masters have triggers now
   */
  ndbrequire(ptr.p->slaveData.gsn == GSN_CREATE_TRIG_IMPL_REQ);
  ndbrequire(ptr.p->slaveData.trigSendCounter.done() == false);
  ndbrequire(ptr.p->slaveData.createTrig.tableId == tableId);

  ptr.p->setErrorCode(ref->errorCode);

  createTrigReply(signal, ptr);
}

void
Backup::createTrigReply(Signal* signal, BackupRecordPtr ptr)
{
  CRASH_INSERTION(10003);

  /**
   * Check finished with table
   */
  ptr.p->slaveData.trigSendCounter--;
  if(ptr.p->slaveData.trigSendCounter.done() == false){
    jam();
    return;
  }//if

  if (ERROR_INSERTED(10025))
  {
    ptr.p->errorCode = 325;
  }

  if(ptr.p->checkError()) {
    jam();
    ptr.p->m_gsn = GSN_START_BACKUP_REF;
    StartBackupRef* ref = (StartBackupRef*)signal->getDataPtrSend();
    ref->backupPtr = ptr.i;
    ref->backupId = ptr.p->backupId;
    ref->errorCode = ptr.p->errorCode;
    ref->nodeId = getOwnNodeId();
    sendSignal(ptr.p->masterRef, GSN_START_BACKUP_REF, signal,
               StartBackupRef::SignalLength, JBB);
    return;
  }//if

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, ptr.p->slaveData.createTrig.tableId));

  /**
   * Next table
   */
  ptr.p->tables.next(tabPtr);
  if(tabPtr.i != RNIL){
    jam();
    sendCreateTrig(signal, ptr, tabPtr);
    return;
  }//if

  /**
   * We've finished creating triggers.
   *
   * send conf and wait
   */
  ptr.p->m_gsn = GSN_START_BACKUP_CONF;
  StartBackupConf* conf = (StartBackupConf*)signal->getDataPtrSend();
  conf->backupPtr = ptr.i;
  conf->backupId = ptr.p->backupId;
  sendSignal(ptr.p->masterRef, GSN_START_BACKUP_CONF, signal,
	     StartBackupConf::SignalLength, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Start backup
 *
 *****************************************************************************/
void
Backup::sendStartBackup(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr)
{

  ptr.p->masterData.startBackup.tablePtr = tabPtr.i;

  StartBackupReq* req = (StartBackupReq*)signal->getDataPtrSend();
  req->backupId = ptr.p->backupId;
  req->backupPtr = ptr.i;

  /**
   * We use trigger Ids that are unique to BACKUP.
   * These don't interfere with other triggers (e.g. from DBDICT)
   * as there is a special case in DBTUP.
   *
   * Consequently, backups during online upgrade won't work
   */
  ptr.p->masterData.gsn = GSN_START_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
  NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
  sendSignal(rg, GSN_START_BACKUP_REQ, signal,
	     StartBackupReq::SignalLength, JBB);
}

void
Backup::execSTART_BACKUP_REF(Signal* signal)
{
  jamEntry();

  StartBackupRef* ref = (StartBackupRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->setErrorCode(ref->errorCode);
  startBackupReply(signal, ptr, nodeId);
}

void
Backup::execSTART_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  
  StartBackupConf* conf = (StartBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  startBackupReply(signal, ptr, nodeId);
}

void
Backup::startBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{

  CRASH_INSERTION((10004));

  if (!haveAllSignals(ptr, GSN_START_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  if (ERROR_INSERTED(10026))
  {
    ptr.p->errorCode = 326;
  }

  if(ptr.p->checkError()){
    jam();
    masterAbort(signal, ptr);
    return;
  }

  /* 
   * We reply to client after create trigger
   */
  if (SEND_BACKUP_STARTED_FLAG(ptr.p->flags))
  {
    BackupConf * conf = (BackupConf*)signal->getDataPtrSend();
    conf->backupId = ptr.p->backupId;
    conf->senderData = ptr.p->clientData;
    conf->nodes = ptr.p->nodes;
    sendSignal(ptr.p->clientRef, GSN_BACKUP_CONF, signal,
             BackupConf::SignalLength, JBB);
  }

  signal->theData[0] = NDB_LE_BackupStarted;
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;
  ptr.p->nodes.copyto(NdbNodeBitmask::Size, signal->theData+3);
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3+NdbNodeBitmask::Size, JBB);

  /**
   * Wait for GCP
   */
  ptr.p->masterData.gsn = GSN_WAIT_GCP_REQ;
  ptr.p->masterData.waitGCP.startBackup = true;

  WaitGCPReq * waitGCPReq = (WaitGCPReq*)signal->getDataPtrSend();
  waitGCPReq->senderRef = reference();
  waitGCPReq->senderData = ptr.i;
  waitGCPReq->requestType = WaitGCPReq::CompleteForceStart;
  //we delay 10 seconds for testcases to generate events to be recorded in the UNDO log
  if (ERROR_INSERTED(10041))
  {
    sendSignalWithDelay(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 10*1000, WaitGCPReq::SignalLength);
  }
  else
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal,
    	       WaitGCPReq::SignalLength,JBB);
}

void
Backup::execWAIT_GCP_REF(Signal* signal)
{
  jamEntry();
  
  CRASH_INSERTION((10006));

  WaitGCPRef * ref = (WaitGCPRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->senderData;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ);

  WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->requestType = WaitGCPReq::CompleteForceStart;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength,JBB);
}

void
Backup::execWAIT_GCP_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION((10007));

  WaitGCPConf * conf = (WaitGCPConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->senderData;
  const Uint32 gcp = conf->gci_hi;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
  
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ);
  
  if(ptr.p->checkError()) {
    jam();
    masterAbort(signal, ptr);
    return;
  }//if
  
  if(ptr.p->masterData.waitGCP.startBackup) {
    jam();
    CRASH_INSERTION((10008));
    ptr.p->startGCP = gcp;
    ptr.p->masterData.sendCounter= 0;
    ptr.p->masterData.gsn = GSN_BACKUP_FRAGMENT_REQ;
    nextFragment(signal, ptr);
    return;
  } else {
    jam();
    if(gcp >= ptr.p->startGCP + 3)
    {
      CRASH_INSERTION((10009));
      ptr.p->stopGCP = gcp;
      /**
       * Backup is complete - begin cleanup
       * STOP_BACKUP_REQ is sent to participants.
       * They then drop the local triggers
       */
      sendStopBackup(signal, ptr);
      return;
    }//if
    
    /**
     * Make sure that we got entire stopGCP 
     */
    WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = WaitGCPReq::CompleteForceStart;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength,JBB);
    return;
  }
}

/*****************************************************************************
 * 
 * Master functionallity - Backup fragment
 *
 *****************************************************************************/
void
Backup::nextFragment(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  BackupFragmentReq* req = (BackupFragmentReq*)signal->getDataPtrSend();
  req->backupPtr = ptr.i;
  req->backupId = ptr.p->backupId;

  NdbNodeBitmask nodes = ptr.p->nodes;
  Uint32 idleNodes = nodes.count();
  Uint32 saveIdleNodes = idleNodes;
  ndbrequire(idleNodes > 0);

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL && idleNodes > 0; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount && idleNodes > 0; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
      const Uint32 nodeId = fragPtr.p->node;
      if(fragPtr.p->scanning != 0) {
        jam();
	ndbrequire(nodes.get(nodeId));
	nodes.clear(nodeId);
	idleNodes--;
      } else if(fragPtr.p->scanned == 0 && nodes.get(nodeId)){
	jam();
	fragPtr.p->scanning = 1;
	nodes.clear(nodeId);
	idleNodes--;
	
	req->tableId = tabPtr.p->tableId;
	req->fragmentNo = i;
	req->count = 0;

	ptr.p->masterData.sendCounter++;
	BlockReference ref = numberToRef(BACKUP, instanceKey(ptr), nodeId);
	sendSignal(ref, GSN_BACKUP_FRAGMENT_REQ, signal,
		   BackupFragmentReq::SignalLength, JBB);
      }//if
    }//for
  }//for
  
  if(idleNodes != saveIdleNodes){
    jam();
    return;
  }//if

  /**
   * Finished with all tables
   */
  {
    ptr.p->masterData.gsn = GSN_WAIT_GCP_REQ;
    ptr.p->masterData.waitGCP.startBackup = false;
    
    WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = WaitGCPReq::CompleteForceStart;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength, JBB);
  }
}

void
Backup::execBACKUP_FRAGMENT_CONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10010));
  
  BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 tableId = conf->tableId;
  const Uint32 fragmentNo = conf->fragmentNo;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  const Uint64 noOfBytes =
    conf->noOfBytesLow + (((Uint64)conf->noOfBytesHigh) << 32);
  const Uint64 noOfRecords =
    conf->noOfRecordsLow + (((Uint64)conf->noOfRecordsHigh) << 32);

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->noOfBytes += noOfBytes;
  ptr.p->noOfRecords += noOfRecords;
  ptr.p->masterData.sendCounter--;

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  tabPtr.p->noOfRecords += noOfRecords;

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragmentNo);

  fragPtr.p->noOfRecords = noOfRecords;

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 1);
  ndbrequire(fragPtr.p->node == nodeId);

  fragPtr.p->scanned = 1;
  fragPtr.p->scanning = 0;

  if (ERROR_INSERTED(10028)) 
  {
    ptr.p->errorCode = 328;
  }

  if(ptr.p->checkError()) 
  {
    if(ptr.p->masterData.sendCounter.done())
    {
      jam();
      masterAbort(signal, ptr);
      return;
    }//if
  }
  else
  {
    NdbNodeBitmask nodes = ptr.p->nodes;
    nodes.clear(getOwnNodeId());
    if (!nodes.isclear())
    {
      BackupFragmentCompleteRep *rep =
        (BackupFragmentCompleteRep*)signal->getDataPtrSend();
      rep->backupId = ptr.p->backupId;
      rep->backupPtr = ptr.i;
      rep->tableId = tableId;
      rep->fragmentNo = fragmentNo;
      rep->noOfTableRowsLow = (Uint32)(tabPtr.p->noOfRecords & 0xFFFFFFFF);
      rep->noOfTableRowsHigh = (Uint32)(tabPtr.p->noOfRecords >> 32);
      rep->noOfFragmentRowsLow = (Uint32)(noOfRecords & 0xFFFFFFFF);
      rep->noOfFragmentRowsHigh = (Uint32)(noOfRecords >> 32);
      BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
      NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
      sendSignal(rg, GSN_BACKUP_FRAGMENT_COMPLETE_REP, signal,
                 BackupFragmentCompleteRep::SignalLength, JBB);
    }
    nextFragment(signal, ptr);
  }
}

void
Backup::execBACKUP_FRAGMENT_REF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10011));

  BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
        if(fragPtr.p->scanning != 0 && nodeId == fragPtr.p->node) 
      {
        jam();
	ndbrequire(fragPtr.p->scanned == 0);
	fragPtr.p->scanned = 1;
	fragPtr.p->scanning = 0;
	goto done;
      }
    }
  }
  goto err;

done:
  ptr.p->masterData.sendCounter--;
  ptr.p->setErrorCode(ref->errorCode);
  
  if(ptr.p->masterData.sendCounter.done())
  {
    jam();
    masterAbort(signal, ptr);
    return;
  }//if

err:
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->requestType = AbortBackupOrd::LogBufferFull;
  ord->senderData= ptr.i;
  execABORT_BACKUP_ORD(signal);
}

void
Backup::execBACKUP_FRAGMENT_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  BackupFragmentCompleteRep * rep =
    (BackupFragmentCompleteRep*)signal->getDataPtr();

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, rep->backupPtr);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, rep->tableId));

  tabPtr.p->noOfRecords =
    rep->noOfTableRowsLow + (((Uint64)rep->noOfTableRowsHigh) << 32);

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, rep->fragmentNo);

  fragPtr.p->noOfRecords =
    rep->noOfFragmentRowsLow + (((Uint64)rep->noOfFragmentRowsHigh) << 32);
}

/*****************************************************************************
 *
 * Slave functionallity - Drop triggers
 *
 *****************************************************************************/

void
Backup::sendDropTrig(Signal* signal, BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  ptr.p->slaveData.gsn = GSN_DROP_TRIG_IMPL_REQ;

  if (ptr.p->slaveData.dropTrig.tableId == RNIL) {
    jam();
    if(ptr.p->tables.count())
      ptr.p->tables.first(tabPtr);
    else
    {
      // Early abort, go to close files
      jam();
      closeFiles(signal, ptr);
      return;
    }
  } else {
    jam();
    ndbrequire(findTable(ptr, tabPtr, ptr.p->slaveData.dropTrig.tableId));
    ptr.p->tables.next(tabPtr);
  }//if
  if (tabPtr.i != RNIL) {
    jam();
    sendDropTrig(signal, ptr, tabPtr);
  } else {
    /**
     * Insert footers
     */
    //if backup error, we needn't insert footers
    if(ptr.p->checkError())
    {
      jam();
      closeFiles(signal, ptr);
      ptr.p->errorCode = 0;
      return;
    }

    {
      BackupFilePtr filePtr LINT_SET_PTR;
      ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
      Uint32 * dst;
      ndbrequire(filePtr.p->operation.dataBuffer.getWritePtr(&dst, 1));
      * dst = 0;
      filePtr.p->operation.dataBuffer.updateWritePtr(1);
    }

    {
      BackupFilePtr filePtr LINT_SET_PTR;
      ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);

      const Uint32 gcpSz = sizeof(BackupFormat::CtlFile::GCPEntry) >> 2;

      Uint32 * dst;
      ndbrequire(filePtr.p->operation.dataBuffer.getWritePtr(&dst, gcpSz));

      BackupFormat::CtlFile::GCPEntry * gcp = 
	(BackupFormat::CtlFile::GCPEntry*)dst;

      gcp->SectionType   = htonl(BackupFormat::GCP_ENTRY);
      gcp->SectionLength = htonl(gcpSz);
      gcp->StartGCP      = htonl(ptr.p->startGCP);
      gcp->StopGCP       = htonl(ptr.p->stopGCP - 1);
      filePtr.p->operation.dataBuffer.updateWritePtr(gcpSz);

      {
        TablePtr tabPtr;
        if (ptr.p->tables.first(tabPtr))
	{
	  jam();
	  signal->theData[0] = BackupContinueB::BACKUP_FRAGMENT_INFO;
	  signal->theData[1] = ptr.i;
	  signal->theData[2] = tabPtr.i;
	  signal->theData[3] = 0;
	  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
	}
	else
	{
	  jam();
	  closeFiles(signal, ptr);
	}
      }
    }
  }
}

void
Backup::sendDropTrig(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr)
{
  jam();
  DropTrigImplReq* req = (DropTrigImplReq*)signal->getDataPtrSend();

  ptr.p->slaveData.gsn = GSN_DROP_TRIG_IMPL_REQ;
  ptr.p->slaveData.trigSendCounter = 0;
  req->senderRef = reference(); // Sending to myself
  req->senderData = ptr.i;
  req->requestType = 0;
  req->tableId = tabPtr.p->tableId;
  req->tableVersion = 0;
  req->indexId = RNIL;
  req->indexVersion = 0;
  req->receiverRef = reference();

  // TUP needs some triggerInfo to find right list
  Uint32 ti = 0;
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION_BEFORE);
  else
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION);
  TriggerInfo::setTriggerActionTime(ti, TriggerActionTime::TA_DETACHED);
  TriggerInfo::setMonitorReplicas(ti, true);
  TriggerInfo::setMonitorAllAttributes(ti, false);

  ptr.p->slaveData.dropTrig.tableId = tabPtr.p->tableId;
  req->tableId = tabPtr.p->tableId;

  for (int i = 0; i < 3; i++) {
    Uint32 id = tabPtr.p->triggerIds[i];
    req->triggerId = id;

    Uint32 ti2 = ti;
    TriggerInfo::setTriggerEvent(ti2, triggerEventValues[i]);
    req->triggerInfo = ti2;

    sendSignal(DBTUP_REF, GSN_DROP_TRIG_IMPL_REQ,
	       signal, DropTrigImplReq::SignalLength, JBB);
    ptr.p->slaveData.trigSendCounter ++;
  }
}

void
Backup::execDROP_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();

  const DropTrigImplRef* ref = (const DropTrigImplRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->senderData;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  if(ref->triggerId != ~(Uint32) 0)
  {
    ndbout << "ERROR DROPPING TRIGGER: " << ref->triggerId;
    ndbout << " Err: " << ref->errorCode << endl << endl;
  }

  dropTrigReply(signal, ptr);
}

void
Backup::execDROP_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();
  
  const DropTrigImplConf* conf = (const DropTrigImplConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->senderData;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  dropTrigReply(signal, ptr);
}

void
Backup::dropTrigReply(Signal* signal, BackupRecordPtr ptr)
{
  CRASH_INSERTION((10012));

  ndbrequire(ptr.p->slaveData.gsn == GSN_DROP_TRIG_IMPL_REQ);
  ndbrequire(ptr.p->slaveData.trigSendCounter.done() == false);

  // move from .masterData to .slaveData
  ptr.p->slaveData.trigSendCounter--;
  if(ptr.p->slaveData.trigSendCounter.done() == false){
    jam();
    return;
  }//if

  sendDropTrig(signal, ptr); // recursive next
}

/*****************************************************************************
 * 
 * Master functionallity - Stop backup
 *
 *****************************************************************************/
void
Backup::execSTOP_BACKUP_REF(Signal* signal)
{
  jamEntry();

  StopBackupRef* ref = (StopBackupRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->setErrorCode(ref->errorCode);
  stopBackupReply(signal, ptr, nodeId);
}

void
Backup::sendStopBackup(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  StopBackupReq* stop = (StopBackupReq*)signal->getDataPtrSend();
  stop->backupPtr = ptr.i;
  stop->backupId = ptr.p->backupId;
  stop->startGCP = ptr.p->startGCP;
  stop->stopGCP = ptr.p->stopGCP;

  ptr.p->masterData.gsn = GSN_STOP_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
  NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
  sendSignal(rg, GSN_STOP_BACKUP_REQ, signal, 
	     StopBackupReq::SignalLength, JBB);
}

void
Backup::execSTOP_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  
  StopBackupConf* conf = (StopBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->noOfLogBytes += conf->noOfLogBytes;
  ptr.p->noOfLogRecords += conf->noOfLogRecords;
  
  stopBackupReply(signal, ptr, nodeId);
}

void
Backup::stopBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{
  CRASH_INSERTION((10013));

  if (!haveAllSignals(ptr, GSN_STOP_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  sendAbortBackupOrd(signal, ptr, AbortBackupOrd::BackupComplete);
  
  if(!ptr.p->checkError() &&  ptr.p->masterData.errorCode == 0)
  {
    if (SEND_BACKUP_COMPLETED_FLAG(ptr.p->flags))
    {
      BackupCompleteRep * rep = (BackupCompleteRep*)signal->getDataPtrSend();
      rep->backupId = ptr.p->backupId;
      rep->senderData = ptr.p->clientData;
      rep->startGCP = ptr.p->startGCP;
      rep->stopGCP = ptr.p->stopGCP;
      rep->noOfBytesLow = (Uint32)(ptr.p->noOfBytes & 0xFFFFFFFF);
      rep->noOfRecordsLow = (Uint32)(ptr.p->noOfRecords & 0xFFFFFFFF);
      rep->noOfBytesHigh = (Uint32)(ptr.p->noOfBytes >> 32);
      rep->noOfRecordsHigh = (Uint32)(ptr.p->noOfRecords >> 32);
      rep->noOfLogBytes = Uint32(ptr.p->noOfLogBytes); // TODO 64-bit log-bytes
      rep->noOfLogRecords = Uint32(ptr.p->noOfLogRecords); // TODO ^^
      rep->nodes = ptr.p->nodes;
      sendSignal(ptr.p->clientRef, GSN_BACKUP_COMPLETE_REP, signal,
		 BackupCompleteRep::SignalLength, JBB);
    }

    signal->theData[0] = NDB_LE_BackupCompleted;
    signal->theData[1] = ptr.p->clientRef;
    signal->theData[2] = ptr.p->backupId;
    signal->theData[3] = ptr.p->startGCP;
    signal->theData[4] = ptr.p->stopGCP;
    signal->theData[5] = (Uint32)(ptr.p->noOfBytes & 0xFFFFFFFF);
    signal->theData[6] = (Uint32)(ptr.p->noOfRecords & 0xFFFFFFFF);
    signal->theData[7] = (Uint32)(ptr.p->noOfLogBytes & 0xFFFFFFFF);
    signal->theData[8] = (Uint32)(ptr.p->noOfLogRecords & 0xFFFFFFFF);
    ptr.p->nodes.copyto(NdbNodeBitmask::Size, signal->theData+9);
    signal->theData[9+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfBytes >> 32);
    signal->theData[10+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfRecords >> 32);
    signal->theData[11+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfLogBytes >> 32);
    signal->theData[12+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfLogRecords >> 32);
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 13+NdbNodeBitmask::Size, JBB);
  }
  else
  {
    masterAbort(signal, ptr);
  }
}

void
Backup::initReportStatus(Signal *signal, BackupRecordPtr ptr)
{
  ptr.p->m_prev_report = NdbTick_getCurrentTicks();
}

void
Backup::checkReportStatus(Signal *signal, BackupRecordPtr ptr)
{
  if (m_backup_report_frequency == 0)
    return;

  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const Uint64 elapsed = NdbTick_Elapsed(ptr.p->m_prev_report, now).seconds();
  if (elapsed > m_backup_report_frequency)
  {
    reportStatus(signal, ptr);
    ptr.p->m_prev_report = now;
  }
}

void
Backup::reportStatus(Signal* signal, BackupRecordPtr ptr,
                     BlockReference ref)
{
  const int signal_length = 11;

  signal->theData[0] = NDB_LE_BackupStatus;
  for (int i= 1; i < signal_length; i++)
    signal->theData[i] = 0;

  if (ptr.i == RNIL ||
      (ptr.p->m_gsn == 0 &&
       ptr.p->masterData.gsn == 0))
  {
    sendSignal(ref, GSN_EVENT_REP, signal, signal_length, JBB);
    return;
  }
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;

  if (ptr.p->dataFilePtr == RNIL)
  {
    sendSignal(ref, GSN_EVENT_REP, signal, signal_length, JBB);
    return;
  }

  BackupFilePtr dataFilePtr LINT_SET_PTR;
  ptr.p->files.getPtr(dataFilePtr, ptr.p->dataFilePtr);
  signal->theData[3] = (Uint32)(dataFilePtr.p->operation.m_bytes_total & 0xFFFFFFFF);
  signal->theData[4] = (Uint32)(dataFilePtr.p->operation.m_bytes_total >> 32);
  signal->theData[5] = (Uint32)(dataFilePtr.p->operation.m_records_total & 0xFFFFFFFF);
  signal->theData[6] = (Uint32)(dataFilePtr.p->operation.m_records_total >> 32);
 
  if (ptr.p->logFilePtr == RNIL)
  {
    sendSignal(ref, GSN_EVENT_REP, signal, signal_length, JBB);
    return;
  }

  BackupFilePtr logFilePtr LINT_SET_PTR;
  ptr.p->files.getPtr(logFilePtr, ptr.p->logFilePtr);
  signal->theData[7] = (Uint32)(logFilePtr.p->operation.m_bytes_total & 0xFFFFFFFF);
  signal->theData[8] = (Uint32)(logFilePtr.p->operation.m_bytes_total >> 32);
  signal->theData[9] = (Uint32)(logFilePtr.p->operation.m_records_total & 0xFFFFFFFF);
  signal->theData[10]= (Uint32)(logFilePtr.p->operation.m_records_total >> 32);

  sendSignal(ref, GSN_EVENT_REP, signal, signal_length, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Abort backup
 *
 *****************************************************************************/
void
Backup::masterAbort(Signal* signal, BackupRecordPtr ptr)
{
  jam();
#ifdef DEBUG_ABORT
  ndbout_c("************ masterAbort");
#endif

  ndbassert(ptr.p->masterRef == reference());

  if(ptr.p->masterData.errorCode != 0)
  {
    jam();
    return;
  }

  if (SEND_BACKUP_STARTED_FLAG(ptr.p->flags))
  {
    BackupAbortRep* rep = (BackupAbortRep*)signal->getDataPtrSend();
    rep->backupId = ptr.p->backupId;
    rep->senderData = ptr.p->clientData;
    rep->reason = ptr.p->errorCode;
    sendSignal(ptr.p->clientRef, GSN_BACKUP_ABORT_REP, signal, 
	       BackupAbortRep::SignalLength, JBB);
  }
  signal->theData[0] = NDB_LE_BackupAborted;
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;
  signal->theData[3] = ptr.p->errorCode;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  ndbrequire(ptr.p->errorCode);
  ptr.p->masterData.errorCode = ptr.p->errorCode;

  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->senderData= ptr.i;
  BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
  NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
  
  switch(ptr.p->masterData.gsn){
  case GSN_DEFINE_BACKUP_REQ:
    ord->requestType = AbortBackupOrd::BackupFailure;
    sendSignal(rg, GSN_ABORT_BACKUP_ORD, signal, 
	       AbortBackupOrd::SignalLength, JBB);
    return;
  case GSN_CREATE_TRIG_IMPL_REQ:
  case GSN_START_BACKUP_REQ:
  case GSN_ALTER_TRIG_REQ:
  case GSN_WAIT_GCP_REQ:
  case GSN_BACKUP_FRAGMENT_REQ:
    jam();
    ptr.p->stopGCP= ptr.p->startGCP + 1;
    sendStopBackup(signal, ptr); // dropping due to error
    return;
  case GSN_UTIL_SEQUENCE_REQ:
  case GSN_UTIL_LOCK_REQ:
    ndbrequire(false);
    return;
  case GSN_DROP_TRIG_IMPL_REQ:
  case GSN_STOP_BACKUP_REQ:
    return;
  }
}

void
Backup::abort_scan(Signal * signal, BackupRecordPtr ptr)
{
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->senderData= ptr.i;
  ord->requestType = AbortBackupOrd::AbortScan;

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
      const Uint32 nodeId = fragPtr.p->node;
      if(fragPtr.p->scanning != 0 && ptr.p->nodes.get(nodeId)) {
        jam();
	
	BlockReference ref = numberToRef(BACKUP, instanceKey(ptr), nodeId);
	sendSignal(ref, GSN_ABORT_BACKUP_ORD, signal,
		   AbortBackupOrd::SignalLength, JBB);
	
      }
    }
  }
}

/*****************************************************************************
 * 
 * Slave functionallity: Define Backup 
 *
 *****************************************************************************/
void
Backup::defineBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errCode)
{
  jam();
  ptr.p->setErrorCode(errCode);
  if(ptr.p->is_lcp()) 
  {
    jam();
     if (ptr.p->ctlFilePtr == RNIL) {
       ptr.p->m_gsn = GSN_DEFINE_BACKUP_REF;
       ndbrequire(ptr.p->errorCode != 0);
       DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtrSend();
       ref->backupId = ptr.p->backupId;
       ref->backupPtr = ptr.i;
       ref->errorCode = ptr.p->errorCode;
       ref->nodeId = getOwnNodeId();
       sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_REF, signal,
                  DefineBackupRef::SignalLength, JBB);
       return;
     }

    BackupFilePtr filePtr LINT_SET_PTR;
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    if (filePtr.p->m_flags & BackupFile::BF_LCP_META)
    {
      jam();
      ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_FILE_THREAD));
      filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_LCP_META;
      if (filePtr.p->m_flags & BackupFile::BF_OPEN)
      {
	closeFile(signal, ptr, filePtr);
	return;
      }
    }
    
    ndbrequire(filePtr.p->m_flags == 0);
    
    TablePtr tabPtr;
    FragmentPtr fragPtr;
    
    ndbrequire(ptr.p->tables.first(tabPtr));
    tabPtr.p->fragments.getPtr(fragPtr, 0);
    
    LcpPrepareRef* ref= (LcpPrepareRef*)signal->getDataPtrSend();
    ref->senderData = ptr.p->clientData;
    ref->senderRef = reference();
    ref->tableId = tabPtr.p->tableId;
    ref->fragmentId = fragPtr.p->fragmentId;
    ref->errorCode = errCode;
    sendSignal(ptr.p->masterRef, GSN_LCP_PREPARE_REF, 
	       signal, LcpPrepareRef::SignalLength, JBB);
    return;
  }

  ptr.p->m_gsn = GSN_DEFINE_BACKUP_REF;
  ndbrequire(ptr.p->errorCode != 0);
  
  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtrSend();
  ref->backupId = ptr.p->backupId;
  ref->backupPtr = ptr.i;
  ref->errorCode = ptr.p->errorCode;
  ref->nodeId = getOwnNodeId();
  sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_REF, signal, 
	     DefineBackupRef::SignalLength, JBB);
}

void
Backup::execDEFINE_BACKUP_REQ(Signal* signal)
{
  jamEntry();

  DefineBackupReq* req = (DefineBackupReq*)signal->getDataPtr();
  
  BackupRecordPtr ptr LINT_SET_PTR;
  const Uint32 ptrI = req->backupPtr;
  const Uint32 backupId = req->backupId;
  const BlockReference senderRef = req->senderRef;

  if(senderRef == reference()){
    /**
     * Signal sent from myself -> record already seized
     */
    jam();
    c_backupPool.getPtr(ptr, ptrI);
  } else { // from other node
    jam();
#ifdef DEBUG_ABORT
    dumpUsedResources();
#endif
    if (!c_backups.getPool().seizeId(ptr, ptrI)) {
      jam();
      ndbrequire(false); // If master has succeeded slave should succed
    }//if
    c_backups.addFirst(ptr);
  }//if

  CRASH_INSERTION((10014));
  
  ptr.p->m_gsn = GSN_DEFINE_BACKUP_REQ;
  ptr.p->slaveState.forceState(INITIAL);
  ptr.p->slaveState.setState(DEFINING);
  ptr.p->slaveData.dropTrig.tableId = RNIL;
  ptr.p->errorCode = 0;
  ptr.p->clientRef = req->clientRef;
  ptr.p->clientData = req->clientData;
  if(senderRef == reference())
    ptr.p->flags = req->flags;
  else
    ptr.p->flags = req->flags & ~((Uint32)BackupReq::WAITCOMPLETED); /* remove waitCompleted flags
						 * as non master should never
						 * reply
						 */
  ptr.p->masterRef = senderRef;
  ptr.p->nodes = req->nodes;
  ptr.p->backupId = backupId;
  ptr.p->backupKey[0] = req->backupKey[0];
  ptr.p->backupKey[1] = req->backupKey[1];
  ptr.p->backupDataLen = req->backupDataLen;
  ptr.p->masterData.errorCode = 0;
  ptr.p->noOfBytes = 0;
  ptr.p->noOfRecords = 0;
  ptr.p->noOfLogBytes = 0;
  ptr.p->noOfLogRecords = 0;
  ptr.p->currGCP = 0;
  ptr.p->startGCP = 0;
  ptr.p->stopGCP = 0;

  /**
   * Allocate files
   */
  BackupFilePtr files[3];
  Uint32 noOfPages[] = {
    NO_OF_PAGES_META_FILE,
    2,   // 32k
    0    // 3M
  };
  const Uint32 maxInsert[] = {
    MAX_WORDS_META_FILE,
    4096,    // 16k
    // Max 16 tuples
    16 * (MAX_TUPLE_SIZE_IN_WORDS + MAX_ATTRIBUTES_IN_TABLE + 128/* safety */),
  };
  Uint32 minWrite[] = {
    8192,
    8192,
    32768
  };
  Uint32 maxWrite[] = {
    8192,
    8192,
    32768
  };
  
  minWrite[1] = c_defaults.m_minWriteSize;
  maxWrite[1] = c_defaults.m_maxWriteSize;
  noOfPages[1] = (c_defaults.m_logBufferSize + sizeof(Page32) - 1) / 
    sizeof(Page32);
  minWrite[2] = c_defaults.m_minWriteSize;
  maxWrite[2] = c_defaults.m_maxWriteSize;
  noOfPages[2] = (c_defaults.m_dataBufferSize + sizeof(Page32) - 1) / 
    sizeof(Page32);

  if (ptr.p->is_lcp())
  {
    noOfPages[2] = (c_defaults.m_lcp_buffer_size + sizeof(Page32) - 1) / 
      sizeof(Page32);
  }
  
  ptr.p->ctlFilePtr = ptr.p->logFilePtr = ptr.p->dataFilePtr = RNIL;

  for(Uint32 i = 0; i<3; i++) {
    jam();
    if(ptr.p->is_lcp() && i != 2)
    {
      files[i].i = RNIL;
      continue;
    }
    if (!ptr.p->files.seizeFirst(files[i])) {
      jam();
      defineBackupRef(signal, ptr, 
		      DefineBackupRef::FailedToAllocateFileRecord);
      return;
    }//if

    files[i].p->tableId = RNIL;
    files[i].p->backupPtr = ptr.i;
    files[i].p->filePointer = RNIL;
    files[i].p->m_flags = 0;
    files[i].p->errorCode = 0;

    if(ERROR_INSERTED(10035) || files[i].p->pages.seize(noOfPages[i]) == false)
    {
      jam();
      DEBUG_OUT("Failed to seize " << noOfPages[i] << " pages");
      defineBackupRef(signal, ptr, DefineBackupRef::FailedToAllocateBuffers);
      return;
    }//if
    Page32Ptr pagePtr;
    files[i].p->pages.getPtr(pagePtr, 0);
    
    const char * msg = files[i].p->
      operation.dataBuffer.setup((Uint32*)pagePtr.p, 
				 noOfPages[i] * (sizeof(Page32) >> 2),
				 128,
				 minWrite[i] >> 2,
				 maxWrite[i] >> 2,
				 maxInsert[i]);
    if(msg != 0) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedToSetupFsBuffers);
      return;
    }//if

    switch(i){
    case 0:
      files[i].p->fileType = BackupFormat::CTL_FILE;
      ptr.p->ctlFilePtr = files[i].i;
      break;
    case 1:
      if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
        files[i].p->fileType = BackupFormat::UNDO_FILE;
      else
        files[i].p->fileType = BackupFormat::LOG_FILE;
      ptr.p->logFilePtr = files[i].i;
      break;
    case 2:
      files[i].p->fileType = BackupFormat::DATA_FILE;
      ptr.p->dataFilePtr = files[i].i;
    }
    files[i].p->operation.m_bytes_total = 0;
    files[i].p->operation.m_records_total = 0;
  }//for
    
  initReportStatus(signal, ptr);

  if (!verifyNodesAlive(ptr, ptr.p->nodes)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::Undefined);
    return;
  }//if
  if (ERROR_INSERTED(10027)) {
    jam();
    defineBackupRef(signal, ptr, 327);
    return;
  }//if

  if(ptr.p->backupDataLen == 0) {
    jam();
    backupAllData(signal, ptr);
    return;
  }//if
  
  if(ptr.p->is_lcp())
  {
    jam();
    getFragmentInfoDone(signal, ptr);
    return;
  }
  
  /**
   * Not implemented
   */
  ndbrequire(0);
}

void
Backup::backupAllData(Signal* signal, BackupRecordPtr ptr)
{
  /**
   * Get all tables from dict
   */
  ListTablesReq * req = (ListTablesReq*)signal->getDataPtrSend();
  req->init();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->setTableId(0);
  req->setTableType(0);
  sendSignal(DBDICT_REF, GSN_LIST_TABLES_REQ, signal, 
	     ListTablesReq::SignalLength, JBB);
}

void
Backup::execLIST_TABLES_CONF(Signal* signal)
{
  jamEntry();
  Uint32 fragInfo = signal->header.m_fragmentInfo;
  ListTablesConf* conf = (ListTablesConf*)signal->getDataPtr();
  Uint32 noOfTables = conf->noOfTables;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, conf->senderData);

  SectionHandle handle (this, signal);
  signal->header.m_fragmentInfo = 0;
  if (noOfTables > 0)
  {
    ListTablesData ltd;
    const Uint32 listTablesDataSizeInWords = (sizeof(ListTablesData) + 3) / 4;
    SegmentedSectionPtr tableDataPtr;
    handle.getSection(tableDataPtr, ListTablesConf::TABLE_DATA);
    SimplePropertiesSectionReader
      tableDataReader(tableDataPtr, getSectionSegmentPool());

    tableDataReader.reset();
    for(unsigned int i = 0; i<noOfTables; i++) {
      jam();
      tableDataReader.getWords((Uint32 *)&ltd, listTablesDataSizeInWords);
      Uint32 tableId = ltd.getTableId();
      Uint32 tableType = ltd.getTableType();
      Uint32 state= ltd.getTableState();

      if (! (DictTabInfo::isTable(tableType) ||
             DictTabInfo::isIndex(tableType) ||
             DictTabInfo::isFilegroup(tableType) ||
             DictTabInfo::isFile(tableType)
             || DictTabInfo::isHashMap(tableType)
             || DictTabInfo::isForeignKey(tableType)
             ))
      {
        jam();
        continue;
      }

      if (state != DictTabInfo::StateOnline)
      {
        jam();
        continue;
      }

      TablePtr tabPtr;
      ptr.p->tables.seizeLast(tabPtr);
      if(tabPtr.i == RNIL) {
        jam();
        defineBackupRef(signal, ptr, DefineBackupRef::FailedToAllocateTables);
        releaseSections(handle);
        return;
      }//if
      tabPtr.p->tableId = tableId;
      tabPtr.p->tableType = tableType;
    }//for
  }

  releaseSections(handle);

  /*
    If first or not last signal
    then keep accumulating table data
   */
  if ((fragInfo == 1) || (fragInfo == 2))
  {
    return;
  }
  openFiles(signal, ptr);
}

void
Backup::openFiles(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  BackupFilePtr filePtr LINT_SET_PTR;

  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = 
    FsOpenReq::OM_WRITEONLY | 
    FsOpenReq::OM_CREATE_IF_NONE |
    FsOpenReq::OM_APPEND |
    FsOpenReq::OM_AUTOSYNC;

  if (c_defaults.m_compressed_backup)
    req->fileFlags |= FsOpenReq::OM_GZ;

  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  req->auto_sync_size = c_defaults.m_disk_synch_size;
  /**
   * Ctl file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->ctlFilePtr);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;

  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);

  /**
   * Log file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->logFilePtr);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;
  
  //write uncompressed log file when enable undo log,since log file is read from back to front.
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    req->fileFlags &= ~FsOpenReq::OM_GZ;
 
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_LOG);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);

  /**
   * Data file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;

  if (c_defaults.m_o_direct)
    req->fileFlags |= FsOpenReq::OM_DIRECT;
  if (c_defaults.m_compressed_backup)
    req->fileFlags |= FsOpenReq::OM_GZ;
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  FsOpenReq::v2_setCount(req->fileNumber, 0);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::execFSOPENREF(Signal* signal)
{
  jamEntry();

  FsRef * ref = (FsRef *)signal->getDataPtr();
  
  const Uint32 userPtr = ref->userPointer;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, userPtr);
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  ptr.p->setErrorCode(ref->errorCode);
  openFilesReply(signal, ptr, filePtr);
}

void
Backup::execFSOPENCONF(Signal* signal)
{
  jamEntry();
  
  FsConf * conf = (FsConf *)signal->getDataPtr();
  
  const Uint32 userPtr = conf->userPointer;
  const Uint32 filePointer = conf->filePointer;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, userPtr);
  filePtr.p->filePointer = filePointer; 
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_OPEN));
  filePtr.p->m_flags |= BackupFile::BF_OPEN;
  openFilesReply(signal, ptr, filePtr);
}

void
Backup::openFilesReply(Signal* signal, 
		       BackupRecordPtr ptr, BackupFilePtr filePtr)
{
  jam();

  /**
   * Mark files as "opened"
   */
  ndbrequire(filePtr.p->m_flags & BackupFile::BF_OPENING);
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_OPENING;
  filePtr.p->m_flags |= BackupFile::BF_OPEN;
  /**
   * Check if all files have recived open_reply
   */
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL;ptr.p->files.next(filePtr)) 
  {
    jam();
    if(filePtr.p->m_flags & BackupFile::BF_OPENING) {
      jam();
      return;
    }//if
  }//for

  if (ERROR_INSERTED(10037)) {
    jam();
    /**
     * Dont return FailedForBackupFilesAleadyExist
     * cause this will make NdbBackup auto-retry with higher number :-)
     */
    ptr.p->errorCode = DefineBackupRef::FailedInsertFileHeader;
    defineBackupRef(signal, ptr);
    return;
  }
  /**
   * Did open succeed for all files
   */
  if(ptr.p->checkError()) 
  {
    jam();
    if(ptr.p->errorCode == FsRef::fsErrFileExists)
    {
      jam();
      ptr.p->errorCode = DefineBackupRef::FailedForBackupFilesAleadyExist;
    }
    defineBackupRef(signal, ptr);
    return;
  }//if

  if(!ptr.p->is_lcp())
  {
    /**
     * Insert file headers
     */
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    if(!insertFileHeader(BackupFormat::CTL_FILE, ptr.p, filePtr.p)) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
      return;
    }//if
    
    BackupFormat::FileType logfiletype;
    if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
      logfiletype = BackupFormat::UNDO_FILE;
    else
      logfiletype = BackupFormat::LOG_FILE;

    ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
    if(!insertFileHeader(logfiletype, ptr.p, filePtr.p)) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
      return;
    }//if
    
    ptr.p->files.getPtr(filePtr, ptr.p->dataFilePtr);
    if(!insertFileHeader(BackupFormat::DATA_FILE, ptr.p, filePtr.p)) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
      return;
    }//if
  }
  else
  {
    ptr.p->files.getPtr(filePtr, ptr.p->dataFilePtr);
    if(!insertFileHeader(BackupFormat::LCP_FILE, ptr.p, filePtr.p)) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
      return;
    }//if
    
    ptr.p->ctlFilePtr = ptr.p->dataFilePtr;
  }
  
  /**
   * Start CTL file thread
   */
  if (!ptr.p->is_lcp())
  {
    jam();
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    filePtr.p->m_flags |= BackupFile::BF_FILE_THREAD;
    
    signal->theData[0] = BackupContinueB::START_FILE_THREAD;
    signal->theData[1] = filePtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
  }
  else
  {
    jam();
    filePtr.p->m_flags |= BackupFile::BF_LCP_META;
  }
  
  /**
   * Insert table list in ctl file
   */
  FsBuffer & buf = filePtr.p->operation.dataBuffer;
  
  const Uint32 sz = 
    (sizeof(BackupFormat::CtlFile::TableList) >> 2) +
    ptr.p->tables.count() - 1;
  
  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  if(!buf.getWritePtr(&dst, sz)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertTableList);
    return;
  }//if
  
  BackupFormat::CtlFile::TableList* tl = 
    (BackupFormat::CtlFile::TableList*)dst;
  tl->SectionType   = htonl(BackupFormat::TABLE_LIST);
  tl->SectionLength = htonl(sz);

  TablePtr tabPtr;
  Uint32 count = 0;
  for(ptr.p->tables.first(tabPtr); 
      tabPtr.i != RNIL;
      ptr.p->tables.next(tabPtr)){
    jam();
    tl->TableIds[count] = htonl(tabPtr.p->tableId);
    count++;
  }//for
  
  buf.updateWritePtr(sz);
  
  /**
   * Start getting table definition data
   */
  ndbrequire(ptr.p->tables.first(tabPtr));
  
  signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
  signal->theData[1] = ptr.i;
  signal->theData[2] = tabPtr.i;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 3);
  return;
}

bool
Backup::insertFileHeader(BackupFormat::FileType ft, 
			 BackupRecord * ptrP,
			 BackupFile * filePtrP){
  FsBuffer & buf = filePtrP->operation.dataBuffer;

  const Uint32 sz = sizeof(BackupFormat::FileHeader) >> 2;

  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  if(!buf.getWritePtr(&dst, sz)) {
    jam();
    return false;
  }//if
  
  BackupFormat::FileHeader* header = (BackupFormat::FileHeader*)dst;
  ndbrequire(sizeof(header->Magic) == sizeof(BACKUP_MAGIC));
  memcpy(header->Magic, BACKUP_MAGIC, sizeof(BACKUP_MAGIC));
  header->BackupVersion = htonl(NDB_BACKUP_VERSION);
  header->SectionType   = htonl(BackupFormat::FILE_HEADER);
  header->SectionLength = htonl(sz - 3);
  header->FileType      = htonl(ft);
  header->BackupId      = htonl(ptrP->backupId);
  header->BackupKey_0   = htonl(ptrP->backupKey[0]);
  header->BackupKey_1   = htonl(ptrP->backupKey[1]);
  header->ByteOrder     = 0x12345678;
  header->NdbVersion    = htonl(NDB_VERSION_D);
  header->MySQLVersion  = htonl(NDB_MYSQL_VERSION_D);
  
  buf.updateWritePtr(sz);
  return true;
}

void
Backup::execGET_TABINFOREF(Signal* signal)
{
  GetTabInfoRef * ref = (GetTabInfoRef*)signal->getDataPtr();
  
  const Uint32 senderData = ref->senderData;
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, senderData);

  defineBackupRef(signal, ptr, ref->errorCode);
}

void
Backup::execGET_TABINFO_CONF(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal)) {
    jam();
    return;
  }//if

  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();
  //const Uint32 senderRef = info->senderRef;
  const Uint32 len = conf->totalLen;
  const Uint32 senderData = conf->senderData;
  const Uint32 tableType = conf->tableType;
  const Uint32 tableId = conf->tableId;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, senderData);

  SectionHandle handle(this, signal);
  SegmentedSectionPtr dictTabInfoPtr;
  handle.getSection(dictTabInfoPtr, GetTabInfoConf::DICT_TAB_INFO);
  ndbrequire(dictTabInfoPtr.sz == len);

  TablePtr tabPtr ;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  BackupFilePtr filePtr LINT_SET_PTR;
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  FsBuffer & buf = filePtr.p->operation.dataBuffer;
  Uint32* dst = 0;
  { // Write into ctl file
    Uint32 dstLen = len + 3;
    if(!buf.getWritePtr(&dst, dstLen)) {
      jam();
      ndbrequire(false);
      ptr.p->setErrorCode(DefineBackupRef::FailedAllocateTableMem);
      releaseSections(handle);
      defineBackupRef(signal, ptr);
      return;
    }//if
    if(dst != 0) {
      jam();

      BackupFormat::CtlFile::TableDescription * desc = 
        (BackupFormat::CtlFile::TableDescription*)dst;
      desc->SectionType = htonl(BackupFormat::TABLE_DESCRIPTION);
      desc->SectionLength = htonl(len + 3);
      desc->TableType = htonl(tableType);
      dst += 3;
      
      copy(dst, dictTabInfoPtr);
      buf.updateWritePtr(dstLen);
    }//if
  }

  releaseSections(handle);

  if(ptr.p->checkError()) {
    jam();
    defineBackupRef(signal, ptr);
    return;
  }//if

  if (!DictTabInfo::isTable(tabPtr.p->tableType))
  {
    jam();

    TablePtr tmp = tabPtr;
    ptr.p->tables.next(tabPtr);
    ptr.p->tables.release(tmp);
    afterGetTabinfoLockTab(signal, ptr, tabPtr);
    return;
  }
  
  if (!parseTableDescription(signal, ptr, tabPtr, dst, len))
  {
    jam();
    defineBackupRef(signal, ptr);
    return;
  }
  
  if(!ptr.p->is_lcp())
  {
    jam();
    BackupLockTab *req = (BackupLockTab *)signal->getDataPtrSend();
    req->m_senderRef = reference();
    req->m_tableId = tabPtr.p->tableId;
    req->m_lock_unlock = BackupLockTab::LOCK_TABLE;
    req->m_backup_state = BackupLockTab::GET_TABINFO_CONF;
    req->m_backupRecordPtr_I = ptr.i;
    req->m_tablePtr_I = tabPtr.i;
    sendSignal(DBDICT_REF, GSN_BACKUP_LOCK_TAB_REQ, signal,
               BackupLockTab::SignalLength, JBB);
    if (ERROR_INSERTED(10038))
    {
      /* Test */
      AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
      ord->backupId = ptr.p->backupId;
      ord->backupPtr = ptr.i;
      ord->requestType = AbortBackupOrd::ClientAbort;
      ord->senderData= ptr.p->clientData;
      sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal, 
                 AbortBackupOrd::SignalLength, JBB);
    }
    return;
  }

  ptr.p->tables.next(tabPtr);
  afterGetTabinfoLockTab(signal, ptr, tabPtr);
}

void
Backup::afterGetTabinfoLockTab(Signal *signal,
                               BackupRecordPtr ptr, TablePtr tabPtr)
{
  if(tabPtr.i == RNIL) 
  {
    /**
     * Done with all tables...
     */
    jam();
    
    if(ptr.p->is_lcp())
    {
      jam();
      lcp_open_file_done(signal, ptr);
      return;
    }
    
    ndbrequire(ptr.p->tables.first(tabPtr));
    DihScanTabReq * req = (DihScanTabReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->tableId = tabPtr.p->tableId;
    req->schemaTransId = 0;
    sendSignal(DBDIH_REF, GSN_DIH_SCAN_TAB_REQ, signal,
               DihScanTabReq::SignalLength, JBB);
    return;
  }//if

  /**
   * Fetch next table...
   */
  signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
  signal->theData[1] = ptr.i;
  signal->theData[2] = tabPtr.i;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 3);
  return;
}

bool
Backup::parseTableDescription(Signal* signal, 
			      BackupRecordPtr ptr, 
			      TablePtr tabPtr, 
			      const Uint32 * tabdescptr,
			      Uint32 len)
{
  SimplePropertiesLinearReader it(tabdescptr, len);
  
  it.first();
  
  DictTabInfo::Table tmpTab; tmpTab.init();
  SimpleProperties::UnpackStatus stat;
  stat = SimpleProperties::unpack(it, &tmpTab, 
				  DictTabInfo::TableMapping, 
				  DictTabInfo::TableMappingSize, 
				  true, true);
  ndbrequire(stat == SimpleProperties::Break);

  bool lcp = ptr.p->is_lcp();

  ndbrequire(tabPtr.p->tableId == tmpTab.TableId);
  ndbrequire(lcp || (tabPtr.p->tableType == tmpTab.TableType));
  
  /**
   * LCP should not save disk attributes but only mem attributes
   */
  
  /**
   * Initialize table object
   */
  tabPtr.p->noOfRecords = 0;
  tabPtr.p->schemaVersion = tmpTab.TableVersion;
  tabPtr.p->triggerIds[0] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerIds[1] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerIds[2] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerAllocated[0] = false;
  tabPtr.p->triggerAllocated[1] = false;
  tabPtr.p->triggerAllocated[2] = false;

  tabPtr.p->noOfAttributes = tmpTab.NoOfAttributes;
  tabPtr.p->maxRecordSize = 1; // LEN word
  bzero(tabPtr.p->attrInfo, sizeof(tabPtr.p->attrInfo));

  Uint32 *list = tabPtr.p->attrInfo + 1;

  if (lcp)
  {
    jam();
    AttributeHeader::init(tabPtr.p->attrInfo, AttributeHeader::READ_LCP, 0);
  }
  else
  {
    jam();
    AttributeHeader::init(tabPtr.p->attrInfo, AttributeHeader::READ_ALL,
                          tmpTab.NoOfAttributes);
  }

  Uint32 varsize = 0;
  Uint32 disk = 0;
  Uint32 null = 0;
  for(Uint32 i = 0; i<tmpTab.NoOfAttributes; i++) {
    jam();
    DictTabInfo::Attribute tmp; tmp.init();
    stat = SimpleProperties::unpack(it, &tmp, 
				    DictTabInfo::AttributeMapping, 
				    DictTabInfo::AttributeMappingSize,
				    true, true);
    
    ndbrequire(stat == SimpleProperties::Break);
    it.next(); // Move Past EndOfAttribute

    if(lcp && tmp.AttributeStorageType == NDB_STORAGETYPE_DISK)
    {
      disk++;
      continue;
    }

    if (tmp.AttributeArrayType != NDB_ARRAYTYPE_FIXED)
      varsize++;

    if (tmp.AttributeNullableFlag)
      null++;

    if (tmp.AttributeSize == 0)
    {
      tabPtr.p->maxRecordSize += (tmp.AttributeArraySize + 31) >> 5;
    }
    else
    {
      const Uint32 arr = tmp.AttributeArraySize;
      const Uint32 sz = 1 << tmp.AttributeSize;
      const Uint32 sz32 = (sz * arr + 31) >> 5;

      tabPtr.p->maxRecordSize += sz32;
    }
  }

  tabPtr.p->attrInfoLen = Uint32(list - tabPtr.p->attrInfo);

  if (lcp)
  {
    Dbtup* tup = (Dbtup*)globalData.getBlock(DBTUP, instance());
    tabPtr.p->maxRecordSize = 1 + tup->get_max_lcp_record_size(tmpTab.TableId);
  }
  else
  {
    // mask
    tabPtr.p->maxRecordSize += 1 + ((tmpTab.NoOfAttributes + null + 31) >> 5);
    tabPtr.p->maxRecordSize += (2 * varsize + 3) / 4;
  }

  return true;
}

void
Backup::execDIH_SCAN_TAB_CONF(Signal* signal)
{
  jamEntry();
  DihScanTabConf * conf = (DihScanTabConf*)signal->getDataPtr();
  const Uint32 fragCount = conf->fragmentCount;
  const Uint32 tableId = conf->tableId;
  const Uint32 senderData = conf->senderData;
  const Uint32 scanCookie = conf->scanCookie;
  ndbrequire(conf->reorgFlag == 0); // no backup during table reorg

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, senderData);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));
  
  tabPtr.p->m_scan_cookie = scanCookie;
  ndbrequire(tabPtr.p->fragments.seize(fragCount) != false);
  for(Uint32 i = 0; i<fragCount; i++) {
    jam();
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, i);
    fragPtr.p->scanned = 0;
    fragPtr.p->scanning = 0;
    fragPtr.p->tableId = tableId;
    fragPtr.p->fragmentId = i;
    fragPtr.p->lqhInstanceKey = 0;
    fragPtr.p->node = 0;
  }//for
  
  /**
   * Next table
   */
  if(ptr.p->tables.next(tabPtr)) {
    jam();
    DihScanTabReq * req = (DihScanTabReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->tableId = tabPtr.p->tableId;
    req->schemaTransId = 0;
    sendSignal(DBDIH_REF, GSN_DIH_SCAN_TAB_REQ, signal,
               DihScanTabReq::SignalLength, JBB);
    return;
  }//if
  
  ptr.p->tables.first(tabPtr);
  getFragmentInfo(signal, ptr, tabPtr, 0);
}

void
Backup::getFragmentInfo(Signal* signal, 
			BackupRecordPtr ptr, TablePtr tabPtr, Uint32 fragNo)
{
  jam();
  
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    const Uint32 fragCount = tabPtr.p->fragments.getSize();
    for(; fragNo < fragCount; fragNo ++) {
      jam();
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, fragNo);
      
      if(fragPtr.p->scanned == 0 && fragPtr.p->scanning == 0) {
        jam();
        DihScanGetNodesReq* req = (DihScanGetNodesReq*)signal->getDataPtrSend();
        req->senderRef = reference();
        req->tableId = tabPtr.p->tableId;
        req->scanCookie = tabPtr.p->m_scan_cookie;
        req->fragCnt = 1;
        req->fragItem[0].senderData = ptr.i;
        req->fragItem[0].fragId = fragNo;
        sendSignal(DBDIH_REF, GSN_DIH_SCAN_GET_NODES_REQ, signal,
                   DihScanGetNodesReq::FixedSignalLength
                   + DihScanGetNodesReq::FragItem::Length,
                   JBB);
	return;
      }//if
    }//for

    DihScanTabCompleteRep*rep= (DihScanTabCompleteRep*)signal->getDataPtrSend();
    rep->tableId = tabPtr.p->tableId;
    rep->scanCookie = tabPtr.p->m_scan_cookie;
    sendSignal(DBDIH_REF, GSN_DIH_SCAN_TAB_COMPLETE_REP, signal,
               DihScanTabCompleteRep::SignalLength, JBB);

    fragNo = 0;
  }//for
  

  getFragmentInfoDone(signal, ptr);
}

void
Backup::execDIH_SCAN_GET_NODES_CONF(Signal* signal)
{
  jamEntry();
  
  /**
   * Assume only short CONFs with a single FragItem as we only do single
   * fragment requests in DIH_SCAN_GET_NODES_REQ from Backup::getFragmentInfo.
   */
  ndbrequire(signal->getNoOfSections() == 0);
  ndbassert(signal->getLength() ==
            DihScanGetNodesConf::FixedSignalLength
            + DihScanGetNodesConf::FragItem::Length);

  DihScanGetNodesConf* conf = (DihScanGetNodesConf*)signal->getDataPtrSend();
  const Uint32 tableId = conf->tableId;
  const Uint32 senderData = conf->fragItem[0].senderData;
  const Uint32 nodeCount = conf->fragItem[0].count;
  const Uint32 fragNo = conf->fragItem[0].fragId;
  const Uint32 instanceKey = conf->fragItem[0].instanceKey; 

  ndbrequire(nodeCount > 0 && nodeCount <= MAX_REPLICAS);
  
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, senderData);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragNo);
  fragPtr.p->lqhInstanceKey = instanceKey;
  
  fragPtr.p->node = conf->fragItem[0].nodes[0];

  getFragmentInfo(signal, ptr, tabPtr, fragNo + 1);
}

void
Backup::getFragmentInfoDone(Signal* signal, BackupRecordPtr ptr)
{
  ptr.p->m_gsn = GSN_DEFINE_BACKUP_CONF;
  ptr.p->slaveState.setState(DEFINED);
  DefineBackupConf * conf = (DefineBackupConf*)signal->getDataPtr();
  conf->backupPtr = ptr.i;
  conf->backupId = ptr.p->backupId;
  sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_CONF, signal,
	     DefineBackupConf::SignalLength, JBB);
}


/*****************************************************************************
 * 
 * Slave functionallity: Start backup
 *
 *****************************************************************************/
void
Backup::execSTART_BACKUP_REQ(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10015));

  StartBackupReq* req = (StartBackupReq*)signal->getDataPtr();
  const Uint32 ptrI = req->backupPtr;

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->slaveState.setState(STARTED);
  ptr.p->m_gsn = GSN_START_BACKUP_REQ;

  /**
   * Start file threads...
   */
  BackupFilePtr filePtr;
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL;ptr.p->files.next(filePtr))
  {
    jam();
    if(! (filePtr.p->m_flags & BackupFile::BF_FILE_THREAD))
    {
      jam();
      filePtr.p->m_flags |= BackupFile::BF_FILE_THREAD;
      signal->theData[0] = BackupContinueB::START_FILE_THREAD;
      signal->theData[1] = filePtr.i;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
    }//if
  }//for

  /**
   * Tell DBTUP to create triggers
   */
  TablePtr tabPtr;
  ndbrequire(ptr.p->tables.first(tabPtr));
  sendCreateTrig(signal, ptr, tabPtr);
}

/*****************************************************************************
 * 
 * Slave functionallity: Backup fragment
 *
 *****************************************************************************/
void
Backup::execBACKUP_FRAGMENT_REQ(Signal* signal)
{
  jamEntry();
  BackupFragmentReq* req = (BackupFragmentReq*)signal->getDataPtr();

  CRASH_INSERTION((10016));

  const Uint32 ptrI = req->backupPtr;
  //const Uint32 backupId = req->backupId;
  const Uint32 tableId = req->tableId;
  const Uint32 fragNo = req->fragmentNo;
  const Uint32 count = req->count;

  /**
   * Get backup record
   */
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->slaveState.setState(SCANNING);
  ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_REQ;

  /**
   * Get file
   */
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
  
  ndbrequire(filePtr.p->backupPtr == ptrI);
  ndbrequire(filePtr.p->m_flags == 
	     (BackupFile::BF_OPEN | BackupFile::BF_FILE_THREAD));
  
  /**
   * Get table
   */
  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  /**
   * Get fragment
   */
  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragNo);

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 0 || 
	     refToNode(ptr.p->masterRef) == getOwnNodeId());

  /**
   * Init operation
   */
  if(filePtr.p->tableId != tableId) {
    jam();
    filePtr.p->operation.init(tabPtr);
    filePtr.p->tableId = tableId;
  }//if
  
  /**
   * Check for space in buffer
   */
  if(!filePtr.p->operation.newFragment(tableId, fragPtr.p->fragmentId)) {
    jam();
    req->count = count + 1;
    sendSignalWithDelay(reference(), GSN_BACKUP_FRAGMENT_REQ, signal, 50,
			signal->length());
    ptr.p->slaveState.setState(STARTED);
    return;
  }//if
  
  /**
   * Mark things as "in use"
   */
  fragPtr.p->scanning = 1;
  filePtr.p->fragmentNo = fragPtr.p->fragmentId;
  filePtr.p->m_retry_count = 0;

  if (ptr.p->is_lcp())
  {
    jam();
    filePtr.p->fragmentNo = 0;
  }

  sendScanFragReq(signal, ptr, filePtr, tabPtr, fragPtr, 0);
}

void
Backup::sendScanFragReq(Signal* signal,
                        Ptr<BackupRecord> ptr,
                        Ptr<BackupFile> filePtr,
                        Ptr<Table> tabPtr,
                        Ptr<Fragment> fragPtr,
                        Uint32 delay)
{
  /**
   * Start scan
   */
  {
    filePtr.p->m_flags |= BackupFile::BF_SCAN_THREAD;
    
    Table & table = * tabPtr.p;
    ScanFragReq * req = (ScanFragReq *)signal->getDataPtrSend();
    const Uint32 parallelism = 16;

    req->senderData = filePtr.i;
    req->resultRef = reference();
    req->schemaVersion = table.schemaVersion;
    req->fragmentNoKeyLen = fragPtr.p->fragmentId;
    req->requestInfo = 0;
    req->savePointId = 0;
    req->tableId = table.tableId;
    ScanFragReq::setReadCommittedFlag(req->requestInfo, 1);
    ScanFragReq::setLockMode(req->requestInfo, 0);
    ScanFragReq::setHoldLockFlag(req->requestInfo, 0);
    ScanFragReq::setKeyinfoFlag(req->requestInfo, 0);
    ScanFragReq::setTupScanFlag(req->requestInfo, 1);
    if (ptr.p->is_lcp())
    {
      ScanFragReq::setScanPrio(req->requestInfo, 1);
      ScanFragReq::setNoDiskFlag(req->requestInfo, 1);
      ScanFragReq::setLcpScanFlag(req->requestInfo, 1);
    }
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    req->clientOpPtr= filePtr.i;
    req->batch_size_rows= parallelism;
    req->batch_size_bytes= 0;
    BlockReference lqhRef = 0;
    if (ptr.p->is_lcp()) {
      lqhRef = calcInstanceBlockRef(DBLQH);
    } else {
      const Uint32 instanceKey = fragPtr.p->lqhInstanceKey;
      ndbrequire(instanceKey != 0);
      lqhRef = numberToRef(DBLQH, instanceKey, getOwnNodeId());
    }

    Uint32 attrInfo[25];
    attrInfo[0] = table.attrInfoLen;
    attrInfo[1] = 0;
    attrInfo[2] = 0;
    attrInfo[3] = 0;
    attrInfo[4] = 0;
    memcpy(attrInfo + 5, table.attrInfo, 4*table.attrInfoLen);
    LinearSectionPtr ptr[3];
    ptr[0].p = attrInfo;
    ptr[0].sz = 5 + table.attrInfoLen;
    if (delay == 0)
    {
      jam();
      sendSignal(lqhRef, GSN_SCAN_FRAGREQ, signal,
                 ScanFragReq::SignalLength, JBB, ptr, 1);
    }
    else
    {
      jam();
      SectionHandle handle(this);
      ndbrequire(import(handle.m_ptr[0], ptr[0].p, ptr[0].sz));
      handle.m_cnt = 1;
      sendSignalWithDelay(lqhRef, GSN_SCAN_FRAGREQ, signal,
                          delay, ScanFragReq::SignalLength, &handle);
    }
  }
}

void
Backup::execSCAN_HBREP(Signal* signal)
{
  jamEntry();
}

void
Backup::execTRANSID_AI(Signal* signal)
{
  jamEntry();

  const Uint32 filePtrI = signal->theData[0];
  //const Uint32 transId1 = signal->theData[1];
  //const Uint32 transId2 = signal->theData[2];
  Uint32 dataLen  = signal->length() - 3;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
  
  /**
   * Unpack data
   */
  Uint32 * dst = op.dst;
  if (signal->getNoOfSections() == 0)
  {
    jam();
    const Uint32 * src = &signal->theData[3];
    * dst = htonl(dataLen);
    memcpy(dst + 1, src, 4*dataLen);
  }
  else
  {
    jam();
    SectionHandle handle(this, signal);
    SegmentedSectionPtr dataPtr;
    handle.getSection(dataPtr, 0);
    dataLen = dataPtr.sz;

    * dst = htonl(dataLen);
    copy(dst + 1, dataPtr);
    releaseSections(handle);
  }

  op.attrSzTotal += dataLen;
  ndbrequire(dataLen < op.maxRecordSize);

  op.finished(dataLen);

  op.newRecord(dst + dataLen + 1);
}

void
Backup::update_lcp_pages_scanned(Signal *signal,
                                 Uint32 filePtrI,
                                 Uint32 scanned_pages)
{
  BackupFilePtr filePtr;
  jamEntry();

  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
 
  op.set_scanned_pages(scanned_pages);
}

void 
Backup::OperationRecord::init(const TablePtr & ptr)
{
  tablePtr = ptr.i;
  maxRecordSize = ptr.p->maxRecordSize;
  lcpScannedPages = 0;
}

bool
Backup::OperationRecord::newFragment(Uint32 tableId, Uint32 fragNo)
{
  Uint32 * tmp;
  const Uint32 headSz = (sizeof(BackupFormat::DataFile::FragmentHeader) >> 2);
  const Uint32 sz = headSz + 16 * maxRecordSize;
  
  ndbrequire(sz < dataBuffer.getMaxWrite());
  if(dataBuffer.getWritePtr(&tmp, sz)) {
    jam();
    BackupFormat::DataFile::FragmentHeader * head = 
      (BackupFormat::DataFile::FragmentHeader*)tmp;

    head->SectionType   = htonl(BackupFormat::FRAGMENT_HEADER);
    head->SectionLength = htonl(headSz);
    head->TableId       = htonl(tableId);
    head->FragmentNo    = htonl(fragNo);
    head->ChecksumType  = htonl(0);

    opNoDone = opNoConf = opLen = 0;
    newRecord(tmp + headSz);
    scanStart = tmp;
    scanStop  = (tmp + headSz);
    
    noOfRecords = 0;
    noOfBytes = 0;
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::fragComplete(Uint32 tableId, Uint32 fragNo, bool fill_record)
{
  Uint32 * tmp;
  const Uint32 footSz = sizeof(BackupFormat::DataFile::FragmentFooter) >> 2;
  Uint32 sz = footSz + 1;

  if (fill_record)
  {
    Uint32 * new_tmp;
    if (!dataBuffer.getWritePtr(&tmp, sz))
      return false;
    new_tmp = tmp + sz;

    if ((UintPtr)new_tmp & (sizeof(Page32)-1))
    {
      /* padding is needed to get full write */
      new_tmp += 2 /* to fit empty header minimum 2 words*/;
      new_tmp = (Uint32 *)(((UintPtr)new_tmp + sizeof(Page32)-1) &
                            ~(UintPtr)(sizeof(Page32)-1));
      /* new write sz */
      sz = Uint32(new_tmp - tmp);
    }
  }

  if(dataBuffer.getWritePtr(&tmp, sz)) {
    jam();
    * tmp = 0; // Finish record stream
    tmp++;
    BackupFormat::DataFile::FragmentFooter * foot = 
      (BackupFormat::DataFile::FragmentFooter*)tmp;
    foot->SectionType   = htonl(BackupFormat::FRAGMENT_FOOTER);
    foot->SectionLength = htonl(footSz);
    foot->TableId       = htonl(tableId);
    foot->FragmentNo    = htonl(fragNo);
    foot->NoOfRecords   = htonl(Uint32(noOfRecords)); // TODO
    foot->Checksum      = htonl(0);

    if (sz != footSz + 1)
    {
      tmp += footSz;
      memset(tmp, 0, (sz - footSz - 1) * 4);
      *tmp = htonl(BackupFormat::EMPTY_ENTRY);
      tmp++;
      *tmp = htonl(sz - footSz - 1);
    }

    dataBuffer.updateWritePtr(sz);
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::newScan()
{
  Uint32 * tmp;
  ndbrequire(16 * maxRecordSize < dataBuffer.getMaxWrite());
  if(dataBuffer.getWritePtr(&tmp, 16 * maxRecordSize))
  {
    jam();
    opNoDone = opNoConf = opLen = 0;
    newRecord(tmp);
    scanStart = tmp;
    scanStop = tmp;
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::closeScan()
{
  opNoDone = opNoConf = opLen = 0;
  return true;
}

bool 
Backup::OperationRecord::scanConf(Uint32 noOfOps, Uint32 total_len)
{
  const Uint32 done = Uint32(opNoDone-opNoConf);
  
  ndbrequire(noOfOps == done);
  ndbrequire(opLen == total_len);
  opNoConf = opNoDone;
  
  const Uint32 len = Uint32(scanStop - scanStart);
  ndbrequire(len < dataBuffer.getMaxWrite());
  dataBuffer.updateWritePtr(len);
  noOfBytes += (len << 2);
  m_bytes_total += (len << 2);
  m_records_total += noOfOps;
  return true;
}

void
Backup::execSCAN_FRAGREF(Signal* signal)
{
  jamEntry();

  ScanFragRef * ref = (ScanFragRef*)signal->getDataPtr();
  
  const Uint32 filePtrI = ref->senderData;
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  Uint32 errCode = ref->errorCode;
  if (filePtr.p->errorCode == 0)
  {
    // check for transient errors
    switch(errCode){
    case ScanFragRef::ZSCAN_BOOK_ACC_OP_ERROR:
    case ScanFragRef::NO_TC_CONNECT_ERROR:
    case ScanFragRef::ZTOO_MANY_ACTIVE_SCAN_ERROR:
      jam();
      break;
    default:
      jam();
      filePtr.p->errorCode = errCode;
    }
  }

  if (filePtr.p->errorCode == 0)
  {
    jam();
    filePtr.p->m_retry_count++;
    if (filePtr.p->m_retry_count == 10)
    {
      jam();
      filePtr.p->errorCode = errCode;
    }
  }

  if (filePtr.p->errorCode != 0)
  {
    jam();
    filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_SCAN_THREAD;
    backupFragmentRef(signal, filePtr);
  }
  else
  {
    jam();

    // retry

    BackupRecordPtr ptr;
    c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
    TablePtr tabPtr;
    ndbrequire(findTable(ptr, tabPtr, filePtr.p->tableId));
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, filePtr.p->fragmentNo);
    sendScanFragReq(signal, ptr, filePtr, tabPtr, fragPtr, 100);
  }
}

void
Backup::execSCAN_FRAGCONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10017));

  ScanFragConf * conf = (ScanFragConf*)signal->getDataPtr();
  
  const Uint32 filePtrI = conf->senderData;
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
  
  op.scanConf(conf->completedOps, conf->total_len);
  const Uint32 completed = conf->fragmentCompleted;
  if(completed != 2) {
    jam();
    
    checkScan(signal, filePtr);
    return;
  }//if

  fragmentCompleted(signal, filePtr);
}

void
Backup::fragmentCompleted(Signal* signal, BackupFilePtr filePtr)
{
  jam();

  if(filePtr.p->errorCode != 0)
  {
    jam();    
    filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_SCAN_THREAD;
    backupFragmentRef(signal, filePtr); // Scan completed
    return;
  }//if
    
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  OperationRecord & op = filePtr.p->operation;
  if(!op.fragComplete(filePtr.p->tableId, filePtr.p->fragmentNo,
                      c_defaults.m_o_direct))
  {
    jam();
    signal->theData[0] = BackupContinueB::BUFFER_FULL_FRAG_COMPLETE;
    signal->theData[1] = filePtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 50, 2);
    return;
  }//if
  
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_SCAN_THREAD;
  
  if (ptr.p->is_lcp())
  {
    /* Maintain LCP totals */
    ptr.p->noOfRecords+= op.noOfRecords;
    ptr.p->noOfBytes+= op.noOfBytes;
    
    ptr.p->slaveState.setState(STOPPING);
    filePtr.p->operation.dataBuffer.eof();
  }
  else
  {
    BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtrSend();
    conf->backupId = ptr.p->backupId;
    conf->backupPtr = ptr.i;
    conf->tableId = filePtr.p->tableId;
    conf->fragmentNo = filePtr.p->fragmentNo;
    conf->noOfRecordsLow = (Uint32)(op.noOfRecords & 0xFFFFFFFF);
    conf->noOfRecordsHigh = (Uint32)(op.noOfRecords >> 32);
    conf->noOfBytesLow = (Uint32)(op.noOfBytes & 0xFFFFFFFF);
    conf->noOfBytesHigh = (Uint32)(op.noOfBytes >> 32);
    sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_CONF, signal,
	       BackupFragmentConf::SignalLength, JBB);

    ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_CONF;
    ptr.p->slaveState.setState(STARTED);
  }
  return;
}

void
Backup::backupFragmentRef(Signal * signal, BackupFilePtr filePtr)
{
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_REF;

  CRASH_INSERTION((10044));
  CRASH_INSERTION((10045));
  
  BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtrSend();
  ref->backupId = ptr.p->backupId;
  ref->backupPtr = ptr.i;
  ref->nodeId = getOwnNodeId();
  ref->errorCode = filePtr.p->errorCode;
  sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_REF, signal,
	     BackupFragmentRef::SignalLength, JBB);
}
 
void
Backup::checkScan(Signal* signal, BackupFilePtr filePtr)
{  
  OperationRecord & op = filePtr.p->operation;
  BlockReference lqhRef = 0;
  BackupRecordPtr ptr LINT_SET_PTR;
  {
    c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
    if (ptr.p->is_lcp()) {
      lqhRef = calcInstanceBlockRef(DBLQH);
    } else {
      TablePtr tabPtr;
      ndbrequire(findTable(ptr, tabPtr, filePtr.p->tableId));
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, filePtr.p->fragmentNo);
      const Uint32 instanceKey = fragPtr.p->lqhInstanceKey;
      lqhRef = numberToRef(DBLQH, instanceKey, getOwnNodeId());
    }
  }

  if(filePtr.p->errorCode != 0 || ptr.p->checkError())
  {
    jam();

    /**
     * Close scan
     */
    op.closeScan();
    ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
    req->senderData = filePtr.i;
    req->requestInfo = ScanFragNextReq::ZCLOSE;
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    sendSignal(lqhRef, GSN_SCAN_NEXTREQ, signal, 
	       ScanFragNextReq::SignalLength, JBB);
    return;
  }//if
  
  if(op.newScan()) {
    jam();
    
    ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
    req->senderData = filePtr.i;
    req->requestInfo = 0;
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    req->batch_size_rows= 16;
    req->batch_size_bytes= 0;

    if (ERROR_INSERTED(10039) && 
	filePtr.p->tableId >= 2 &&
	filePtr.p->operation.noOfRecords > 0)
    {
      ndbout_c("halting backup for table %d fragment: %d after %llu records",
	       filePtr.p->tableId,
	       filePtr.p->fragmentNo,
	       filePtr.p->operation.noOfRecords);
      memmove(signal->theData+2, signal->theData, 
	      4*ScanFragNextReq::SignalLength);
      signal->theData[0] = BackupContinueB::ZDELAY_SCAN_NEXT;
      signal->theData[1] = filePtr.i;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 
			  300, 2+ScanFragNextReq::SignalLength);
      return;
    }
    if(ERROR_INSERTED(10032))
      sendSignalWithDelay(lqhRef, GSN_SCAN_NEXTREQ, signal, 
			  100, ScanFragNextReq::SignalLength);
    else if(ERROR_INSERTED(10033))
    {
      SET_ERROR_INSERT_VALUE(10032);
      sendSignalWithDelay(lqhRef, GSN_SCAN_NEXTREQ, signal, 
			  10000, ScanFragNextReq::SignalLength);
      
      BackupRecordPtr ptr LINT_SET_PTR;
      c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
      AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
      ord->backupId = ptr.p->backupId;
      ord->backupPtr = ptr.i;
      ord->requestType = AbortBackupOrd::FileOrScanError;
      ord->senderData= ptr.i;
      sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal, 
		 AbortBackupOrd::SignalLength, JBB);
    }
#ifdef ERROR_INSERT
    else if (ERROR_INSERTED(10042) && filePtr.p->tableId ==c_error_insert_extra)
    {
      sendSignalWithDelay(lqhRef, GSN_SCAN_NEXTREQ, signal,
			  10, ScanFragNextReq::SignalLength);
    }
#endif
    else
    {
      sendSignal(lqhRef, GSN_SCAN_NEXTREQ, signal, 
		 ScanFragNextReq::SignalLength, JBB);

      /*
        check if it is time to report backup status
      */
      BackupRecordPtr ptr LINT_SET_PTR;
      c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
      if (!ptr.p->is_lcp())
        checkReportStatus(signal, ptr);
    }
    return;
  }//if
  
  signal->theData[0] = BackupContinueB::BUFFER_FULL_SCAN;
  signal->theData[1] = filePtr.i;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 50, 2);
}

void
Backup::execFSAPPENDREF(Signal* signal)
{
  jamEntry();
  
  FsRef * ref = (FsRef *)signal->getDataPtr();

  const Uint32 filePtrI = ref->userPointer;
  const Uint32 errCode = ref->errorCode;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_FILE_THREAD;
  filePtr.p->errorCode = errCode;

  checkFile(signal, filePtr);
}

void
Backup::execFSAPPENDCONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10018));

  //FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 filePtrI = signal->theData[0]; //conf->userPointer;
  const Uint32 bytes = signal->theData[1]; //conf->bytes;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);
  
  OperationRecord & op = filePtr.p->operation;
  
  op.dataBuffer.updateReadPtr(bytes >> 2);

  checkFile(signal, filePtr);
}

/*
  This routine handles two problems with writing to disk during local
  checkpoints and backups. The first problem is that we need to limit
  the writing to ensure that we don't use too much CPU and disk resources
  for backups and checkpoints. The perfect solution to this is to use
  a dynamic algorithm that adapts to the environment. Until we have
  implemented this we can satisfy ourselves with an algorithm that
  uses a configurable limit.

  The second problem is that in Linux we can get severe problems if we
  write very much to the disk without synching. In the worst case we
  can have Gigabytes of data in the Linux page cache before we reach
  the limit of how much we can write. If this happens the performance
  will drop significantly when we reach this limit since the Linux flush
  daemon will spend a few minutes on writing out the page cache to disk.
  To avoid this we ensure that a file never have more than a certain
  amount of data outstanding before synch. This variable is also
  configurable.
*/
bool
Backup::ready_to_write(bool ready, Uint32 sz, bool eof, BackupFile *fileP)
{
#if 0
  ndbout << "ready_to_write: ready = " << ready << " eof = " << eof;
  ndbout << " sz = " << sz << endl;
  ndbout << "words this period = " << m_words_written_this_period;
  ndbout << endl << "overflow disk write = " << m_overflow_disk_write;
  ndbout << endl << "Current Millisecond is = ";
  ndbout << NdbTick_CurrentMillisecond() << endl;
#endif

  if (ERROR_INSERTED(10043) && eof)
  {
    /* Block indefinitely without closing the file */
    return false;
  }

  if ((ready || eof) &&
      m_words_written_this_period <= m_curr_disk_write_speed)
  {
    /*
      We have a buffer ready to write or we have reached end of
      file and thus we must write the last before closing the
      file.
      We have already check that we are allowed to write at this
      moment. We only worry about history of last 100 milliseconds.
      What happened before that is of no interest since a disk
      write that was issued more than 100 milliseconds should be
      completed by now.
    */
    int overflow;
    m_monitor_words_written+= sz;
    m_words_written_this_period += sz;
    overflow = m_words_written_this_period - m_curr_disk_write_speed;
    if (overflow > 0)
      m_overflow_disk_write = overflow;
#if 0
    ndbout << "Will write with " << endl;
    ndbout << endl;
#endif
    return true;
  }
  else
  {
#if 0
    ndbout << "Will not write now" << endl << endl;
#endif
    return false;
  }
}

void
Backup::checkFile(Signal* signal, BackupFilePtr filePtr)
{

#ifdef DEBUG_ABORT
  //  ndbout_c("---- check file filePtr.i = %u", filePtr.i);
#endif

  OperationRecord & op = filePtr.p->operation;
  Uint32 *tmp = NULL;
  Uint32 sz = 0;
  bool eof = FALSE;
  bool ready = op.dataBuffer.getReadPtr(&tmp, &sz, &eof); 
#if 0
  ndbout << "Ptr to data = " << hex << tmp << endl;
#endif
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  if (ERROR_INSERTED(10036))
  {
    jam();
    filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_FILE_THREAD;
    filePtr.p->errorCode = 2810;
    ptr.p->setErrorCode(2810);

    if(ptr.p->m_gsn == GSN_STOP_BACKUP_REQ)
    {
      jam();
      closeFile(signal, ptr, filePtr);
    }
    return;
  }

  if(filePtr.p->errorCode != 0)
  {
    jam();
    ptr.p->setErrorCode(filePtr.p->errorCode);

    if(ptr.p->m_gsn == GSN_STOP_BACKUP_REQ)
    {
      jam();
      closeFile(signal, ptr, filePtr);
    }

    if (ptr.p->is_lcp())
    {
      jam();
      /* Close file with error - will delete it */
      closeFile(signal, ptr, filePtr);
    }
   
    return;
  }

  if (!ready_to_write(ready, sz, eof, filePtr.p))
  {
    jam();
    signal->theData[0] = BackupContinueB::BUFFER_UNDERFLOW;
    signal->theData[1] = filePtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 20, 2);
    return;
  }
  else if (sz > 0)
  {
    jam();
#ifdef ERROR_INSERT
    /* Test APPENDREF handling */
    if (filePtr.p->fileType == BackupFormat::DATA_FILE)
    {
      if (ERROR_INSERTED(10045))
      {
        ndbout_c("BF_SCAN_THREAD = %u",
                 (filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD));
      }

      if ((ERROR_INSERTED(10044) &&
           !(filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD)) ||
          (ERROR_INSERTED(10045) && 
           (filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD)))
      { 
        jam();
        ndbout_c("REFing on append to data file for table %u, fragment %u, "
                 "BF_SCAN_THREAD running : %u",
                 filePtr.p->tableId,
                 filePtr.p->fragmentNo,
                 filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD);
        FsRef* ref = (FsRef *)signal->getDataPtrSend();
        ref->userPointer = filePtr.i;
        ref->errorCode = FsRef::fsErrInvalidParameters;
        ref->osErrorCode = ~0;
        /* EXEC DIRECT to avoid change in BF_SCAN_THREAD state */
        EXECUTE_DIRECT(BACKUP, GSN_FSAPPENDREF, signal,
                       3);
        return;
      }
    }
#endif

    ndbassert((Uint64(tmp - c_startOfPages) >> 32) == 0); // 4Gb buffers!
    FsAppendReq * req = (FsAppendReq *)signal->getDataPtrSend();
    req->filePointer   = filePtr.p->filePointer;
    req->userPointer   = filePtr.i;
    req->userReference = reference();
    req->varIndex      = 0;
    req->offset        = Uint32(tmp - c_startOfPages); // 4Gb buffers!
    req->size          = sz;
    req->synch_flag    = 0;
    
    sendSignal(NDBFS_REF, GSN_FSAPPENDREQ, signal, 
	       FsAppendReq::SignalLength, JBA);
    return;
  }

  Uint32 flags = filePtr.p->m_flags;
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_FILE_THREAD;
  
  ndbrequire(flags & BackupFile::BF_OPEN);
  ndbrequire(flags & BackupFile::BF_FILE_THREAD);
  
  closeFile(signal, ptr, filePtr);
}


/****************************************************************************
 * 
 * Slave functionallity: Perform logging
 *
 ****************************************************************************/
void
Backup::execBACKUP_TRIG_REQ(Signal* signal)
{
  /*
  TUP asks if this trigger is to be fired on this node.
  */
  TriggerPtr trigPtr LINT_SET_PTR;
  TablePtr tabPtr LINT_SET_PTR;
  FragmentPtr fragPtr;
  Uint32 trigger_id = signal->theData[0];
  Uint32 frag_id = signal->theData[1];
  Uint32 result;

  jamEntry();

  c_triggerPool.getPtr(trigPtr, trigger_id);

  c_tablePool.getPtr(tabPtr, trigPtr.p->tab_ptr_i);
  tabPtr.p->fragments.getPtr(fragPtr, frag_id);
  if (fragPtr.p->node != getOwnNodeId()) {

    jam();
    result = ZFALSE;
  } else {
    jam();
    result = ZTRUE;
  }//if
  signal->theData[0] = result;
}

BackupFormat::LogFile::LogEntry *
Backup::get_log_buffer(Signal* signal,
                       TriggerPtr trigPtr, Uint32 sz)
{
  Uint32 * dst;
  if(ERROR_INSERTED(10030))
  {
    jam();
    dst = 0;
  }
  else
  {
    jam();
    FsBuffer & buf = trigPtr.p->operation->dataBuffer;
    ndbrequire(sz <= buf.getMaxWrite());
    if (unlikely(!buf.getWritePtr(&dst, sz)))
    {
      jam();
      dst = 0;
    }
  }

  if (unlikely(dst == 0))
  {
    Uint32 save[TrigAttrInfo::StaticLength];
    memcpy(save, signal->getDataPtr(), 4*TrigAttrInfo::StaticLength);
    BackupRecordPtr ptr LINT_SET_PTR;
    c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);
    trigPtr.p->errorCode = AbortBackupOrd::LogBufferFull;
    AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
    ord->backupId = ptr.p->backupId;
    ord->backupPtr = ptr.i;
    ord->requestType = AbortBackupOrd::LogBufferFull;
    ord->senderData= ptr.i;
    sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal,
               AbortBackupOrd::SignalLength, JBB);

    memcpy(signal->getDataPtrSend(), save, 4*TrigAttrInfo::StaticLength);
    return 0;
  }//if

  BackupFormat::LogFile::LogEntry * logEntry =
    (BackupFormat::LogFile::LogEntry *)dst;
  logEntry->Length       = 0;
  logEntry->TableId      = htonl(trigPtr.p->tableId);

  if(trigPtr.p->event==0)
    logEntry->TriggerEvent= htonl(TriggerEvent::TE_INSERT);
  else if(trigPtr.p->event==1)
    logEntry->TriggerEvent= htonl(TriggerEvent::TE_UPDATE);
  else if(trigPtr.p->event==2)
    logEntry->TriggerEvent= htonl(TriggerEvent::TE_DELETE);
  else {
    ndbout << "Bad Event: " << trigPtr.p->event << endl;
    ndbrequire(false);
  }

  return logEntry;
}

void
Backup::execTRIG_ATTRINFO(Signal* signal) {
  jamEntry();

  CRASH_INSERTION((10019));

  TrigAttrInfo * trg = (TrigAttrInfo*)signal->getDataPtr();

  TriggerPtr trigPtr LINT_SET_PTR;
  c_triggerPool.getPtr(trigPtr, trg->getTriggerId());
  ndbrequire(trigPtr.p->event != ILLEGAL_TRIGGER_ID); // Online...

  if(trigPtr.p->errorCode != 0) {
    jam();
    return;
  }//if

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);

  if(ptr.p->flags & BackupReq::USE_UNDO_LOG) {
    if(trg->getAttrInfoType() == TrigAttrInfo::AFTER_VALUES) {
      jam();
      /**
       * Backup is doing UNDO logging and don't need after values
       */
      return;
    }//if
  }
  else {
    if(trg->getAttrInfoType() == TrigAttrInfo::BEFORE_VALUES) {
      jam();
      /**
       * Backup is doing REDO logging and don't need before values
       */
      return;
    }//if
  }

  BackupFormat::LogFile::LogEntry * logEntry = trigPtr.p->logEntry;
  if(logEntry == 0) 
  {
    jam();
    Uint32 sz = trigPtr.p->maxRecordSize;
    logEntry = trigPtr.p->logEntry = get_log_buffer(signal, trigPtr, sz);
    if (unlikely(logEntry == 0))
    {
      jam();
      return;
    }
  } else {
    ndbrequire(logEntry->TableId == htonl(trigPtr.p->tableId));
//    ndbrequire(logEntry->TriggerEvent == htonl(trigPtr.p->event));
  }//if

  const Uint32 pos = logEntry->Length;
  const Uint32 dataLen = signal->length() - TrigAttrInfo::StaticLength;
  memcpy(&logEntry->Data[pos], trg->getData(), dataLen << 2);

  logEntry->Length = pos + dataLen;
}

void
Backup::execFIRE_TRIG_ORD(Signal* signal)
{
  jamEntry();
  FireTrigOrd* trg = (FireTrigOrd*)signal->getDataPtr();

  const Uint32 gci = trg->getGCI();
  const Uint32 trI = trg->getTriggerId();
  const Uint32 fragId = trg->fragId;

  TriggerPtr trigPtr LINT_SET_PTR;
  c_triggerPool.getPtr(trigPtr, trI);
  
  ndbrequire(trigPtr.p->event != ILLEGAL_TRIGGER_ID);

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);

  if(trigPtr.p->errorCode != 0) {
    jam();
    SectionHandle handle(this, signal);
    releaseSections(handle);
    return;
  }//if

  if (isNdbMtLqh())
  {
    jam();
    /* This is the decision point for including
     * this row change in the log file on ndbmtd
     */
    TablePtr tabPtr;
    c_tablePool.getPtr(tabPtr, trigPtr.p->tab_ptr_i);
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, fragId);
    if (fragPtr.p->node != getOwnNodeId()) 
    {
      jam();
      trigPtr.p->logEntry = 0;
      SectionHandle handle(this,signal);
      releaseSections(handle);
      return;
    }
  }

  if (signal->getNoOfSections())
  {
    jam();
    SectionHandle handle(this,signal);
    SegmentedSectionPtr dataPtr[3];
    handle.getSection(dataPtr[0], 0);
    handle.getSection(dataPtr[1], 1);
    handle.getSection(dataPtr[2], 2);
    /**
     * dataPtr[0] : Primary key info
     * dataPtr[1] : Before values
     * dataPtr[2] : After values
     */

    /* Backup is doing UNDO logging and need before values
     * Add 2 extra words to get_log_buffer for potential gci and logEntry length info stored at end.
     */
    if(ptr.p->flags & BackupReq::USE_UNDO_LOG) {
      trigPtr.p->logEntry = get_log_buffer(signal,
                                           trigPtr, dataPtr[0].sz + dataPtr[1].sz + 2);
      if (unlikely(trigPtr.p->logEntry == 0))
      {
        jam();
        releaseSections(handle);
        return;
      }
      copy(trigPtr.p->logEntry->Data, dataPtr[0]);
      copy(trigPtr.p->logEntry->Data+dataPtr[0].sz, dataPtr[1]);
      trigPtr.p->logEntry->Length = dataPtr[0].sz + dataPtr[1].sz;
    }
    //  Backup is doing REDO logging and need after values
    else {
      trigPtr.p->logEntry = get_log_buffer(signal,
                                           trigPtr, dataPtr[0].sz + dataPtr[2].sz + 1);
      if (unlikely(trigPtr.p->logEntry == 0))
      {
        jam();
        releaseSections(handle);
        return;
      }
      copy(trigPtr.p->logEntry->Data, dataPtr[0]);
      copy(trigPtr.p->logEntry->Data+dataPtr[0].sz, dataPtr[2]);
      trigPtr.p->logEntry->Length = dataPtr[0].sz + dataPtr[2].sz;
    }

    releaseSections(handle);
  }

  ndbrequire(trigPtr.p->logEntry != 0);
  Uint32 len = trigPtr.p->logEntry->Length;
  trigPtr.p->logEntry->FragId = htonl(fragId);

  if(gci != ptr.p->currGCP)
  {
    jam();
    trigPtr.p->logEntry->TriggerEvent|= htonl(0x10000);
    trigPtr.p->logEntry->Data[len] = htonl(gci);
    len++;
    ptr.p->currGCP = gci;
  }

  Uint32 datalen = len;
  len += (sizeof(BackupFormat::LogFile::LogEntry) >> 2) - 2;
  trigPtr.p->logEntry->Length = htonl(len);

  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
  {
    /* keep the length at both the end of logEntry and ->logEntry variable
       The total length of logEntry is len + 2
    */
    trigPtr.p->logEntry->Data[datalen] = htonl(len);
  }

  Uint32 entryLength = len +1;
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    entryLength ++;

  ndbrequire(entryLength <= trigPtr.p->operation->dataBuffer.getMaxWrite());
  trigPtr.p->operation->dataBuffer.updateWritePtr(entryLength);
  trigPtr.p->logEntry = 0;
  
  {
    const Uint32 entryByteLength = entryLength << 2;
    trigPtr.p->operation->noOfBytes     += entryByteLength;
    trigPtr.p->operation->m_bytes_total += entryByteLength;
    trigPtr.p->operation->noOfRecords     += 1;
    trigPtr.p->operation->m_records_total += 1;
  }
}

void
Backup::sendAbortBackupOrd(Signal* signal, BackupRecordPtr ptr, 
			   Uint32 requestType)
{
  jam();
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->requestType = requestType;
  ord->senderData= ptr.i;
  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(node.p->alive && ptr.p->nodes.get(nodeId)) {
      jam();
      BlockReference ref = numberToRef(BACKUP, instanceKey(ptr), nodeId);
      sendSignal(ref, GSN_ABORT_BACKUP_ORD, signal, 
		 AbortBackupOrd::SignalLength, JBB);
    }//if
  }//for
}

/*****************************************************************************
 * 
 * Slave functionallity: Stop backup
 *
 *****************************************************************************/
void
Backup::execSTOP_BACKUP_REQ(Signal* signal)
{
  jamEntry();
  StopBackupReq * req = (StopBackupReq*)signal->getDataPtr();
  
  CRASH_INSERTION((10020));

  const Uint32 ptrI = req->backupPtr;
  //const Uint32 backupId = req->backupId;
  const Uint32 startGCP = req->startGCP;
  const Uint32 stopGCP = req->stopGCP;

  /**
   * At least one GCP must have passed
   */
  ndbrequire(stopGCP > startGCP);

  /**
   * Get backup record
   */
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->slaveState.setState(STOPPING);
  ptr.p->m_gsn = GSN_STOP_BACKUP_REQ;
  ptr.p->startGCP= startGCP;
  ptr.p->stopGCP= stopGCP;

  /**
   * Ensure that any in-flight changes are
   * included in the backup log before
   * dropping the triggers
   *
   * This is necessary as the trigger-drop
   * signals are routed :
   *
   *   Backup Worker 1 <-> Proxy <-> TUP Worker 1..n
   * 
   * While the trigger firing signals are
   * routed :
   *
   *   TUP Worker 1..n   -> Backup Worker 1
   *
   * So the arrival of signal-drop acks
   * does not imply that all fired 
   * triggers have been seen.
   *
   *  Backup Worker 1
   *
   *        |             SYNC_PATH_REQ
   *        V
   *     TUP Proxy
   *    |  | ... |
   *    V  V     V
   *    1  2 ... n        (Workers)
   *    |  |     |
   *    |  |     |
   *   
   *   Backup Worker 1
   */

  Uint32 path[] = { DBTUP, 0 };
  Callback cb = { safe_cast(&Backup::startDropTrig_synced), ptrI };
  synchronize_path(signal,
                   path,
                   cb);
}

void
Backup::startDropTrig_synced(Signal* signal, Uint32 ptrI, Uint32 retVal)
{
  jamEntry();
  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  ndbrequire(ptr.p->m_gsn == GSN_STOP_BACKUP_REQ);
  
  /**
   * Now drop the triggers
   */
  sendDropTrig(signal, ptr);
}

void
Backup::closeFiles(Signal* sig, BackupRecordPtr ptr)
{
  /**
   * Close all files
   */
  BackupFilePtr filePtr;
  int openCount = 0;
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL; ptr.p->files.next(filePtr))
  {
    if(! (filePtr.p->m_flags & BackupFile::BF_OPEN))
    {
      jam();
      continue;
    }
    
    jam();
    openCount++;
    
    if(filePtr.p->m_flags & BackupFile::BF_CLOSING)
    {
      jam();
      continue;
    }//if
    
    filePtr.p->operation.dataBuffer.eof();
    if(filePtr.p->m_flags & BackupFile::BF_FILE_THREAD)
    {
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("Close files fileRunning == 1, filePtr.i=%u", filePtr.i);
#endif
    } 
    else 
    {
      jam();
      closeFile(sig, ptr, filePtr);
    }
  }
  
  if(openCount == 0){
    jam();
    closeFilesDone(sig, ptr);
  }//if
}

void
Backup::closeFile(Signal* signal, BackupRecordPtr ptr, BackupFilePtr filePtr)
{
  ndbrequire(filePtr.p->m_flags & BackupFile::BF_OPEN);
  ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_OPENING));
  ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_CLOSING));
  filePtr.p->m_flags |= BackupFile::BF_CLOSING;
  
  FsCloseReq * req = (FsCloseReq *)signal->getDataPtrSend();
  req->filePointer = filePtr.p->filePointer;
  req->userPointer = filePtr.i;
  req->userReference = reference();
  req->fileFlag = 0;
  
  if (ptr.p->errorCode)
  {
    FsCloseReq::setRemoveFileFlag(req->fileFlag, 1);
  }

#ifdef DEBUG_ABORT
  ndbout_c("***** a FSCLOSEREQ filePtr.i = %u flags: %x", 
	   filePtr.i, filePtr.p->m_flags);
#endif
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBA);

}

void
Backup::execFSCLOSEREF(Signal* signal)
{
  jamEntry();
  
  FsRef * ref = (FsRef*)signal->getDataPtr();
  const Uint32 filePtrI = ref->userPointer;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  
  FsConf * conf = (FsConf*)signal->getDataPtr();
  conf->userPointer = filePtrI;
  
  execFSCLOSECONF(signal);
}

void
Backup::execFSCLOSECONF(Signal* signal)
{
  jamEntry();

  FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 filePtrI = conf->userPointer;
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, filePtrI);

#ifdef DEBUG_ABORT
  ndbout_c("***** FSCLOSECONF filePtrI = %u", filePtrI);
#endif
  
  ndbrequire(filePtr.p->m_flags == (BackupFile::BF_OPEN |
				    BackupFile::BF_CLOSING));

  
  filePtr.p->m_flags &= ~(Uint32)(BackupFile::BF_OPEN |BackupFile::BF_CLOSING);
  filePtr.p->operation.dataBuffer.reset();

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  closeFiles(signal, ptr);
}

void
Backup::closeFilesDone(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  if(ptr.p->is_lcp())
  {
    lcp_close_file_conf(signal, ptr);
    return;
  }
  
  jam();

  //error when do insert footer or close file
  if(ptr.p->checkError())
  {
    StopBackupRef * ref = (StopBackupRef*)signal->getDataPtr();
    ref->backupPtr = ptr.i;
    ref->backupId = ptr.p->backupId;
    ref->errorCode = ptr.p->errorCode;
    ref->nodeId = getOwnNodeId();
    sendSignal(ptr.p->masterRef, GSN_STOP_BACKUP_REF, signal,
             StopBackupConf::SignalLength, JBB);

    ptr.p->m_gsn = GSN_STOP_BACKUP_REF;
    ptr.p->slaveState.setState(CLEANING);
    return;
  }

  StopBackupConf* conf = (StopBackupConf*)signal->getDataPtrSend();
  conf->backupId = ptr.p->backupId;
  conf->backupPtr = ptr.i;

  BackupFilePtr filePtr LINT_SET_PTR;
  if(ptr.p->logFilePtr != RNIL)
  {
    ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
    conf->noOfLogBytes= Uint32(filePtr.p->operation.noOfBytes);     // TODO
    conf->noOfLogRecords= Uint32(filePtr.p->operation.noOfRecords); // TODO
  }
  else
  {
    conf->noOfLogBytes= 0;
    conf->noOfLogRecords= 0;
  }

  sendSignal(ptr.p->masterRef, GSN_STOP_BACKUP_CONF, signal,
	     StopBackupConf::SignalLength, JBB);
  
  ptr.p->m_gsn = GSN_STOP_BACKUP_CONF;
  ptr.p->slaveState.setState(CLEANING);
}

/*****************************************************************************
 * 
 * Slave functionallity: Abort backup
 *
 *****************************************************************************/
/*****************************************************************************
 * 
 * Slave functionallity: Abort backup
 *
 *****************************************************************************/
void
Backup::execABORT_BACKUP_ORD(Signal* signal)
{
  jamEntry();
  AbortBackupOrd* ord = (AbortBackupOrd*)signal->getDataPtr();

  const Uint32 backupId = ord->backupId;
  const AbortBackupOrd::RequestType requestType = 
    (AbortBackupOrd::RequestType)ord->requestType;
  const Uint32 senderData = ord->senderData;
  
#ifdef DEBUG_ABORT
  ndbout_c("******** ABORT_BACKUP_ORD ********* nodeId = %u", 
	   refToNode(signal->getSendersBlockRef()));
  ndbout_c("backupId = %u, requestType = %u, senderData = %u, ",
	   backupId, requestType, senderData);
  dumpUsedResources();
#endif

  BackupRecordPtr ptr LINT_SET_PTR;
  if(requestType == AbortBackupOrd::ClientAbort) {
    if (getOwnNodeId() != getMasterNodeId()) {
      jam();
      // forward to master
#ifdef DEBUG_ABORT
      ndbout_c("---- Forward to master nodeId = %u", getMasterNodeId());
#endif
      BlockReference ref = numberToRef(BACKUP, UserBackupInstanceKey, 
                                       getMasterNodeId());
      sendSignal(ref, GSN_ABORT_BACKUP_ORD, 
		 signal, AbortBackupOrd::SignalLength, JBB);
      return;
    }
    jam();
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
      jam();
      if(ptr.p->backupId == backupId && ptr.p->clientData == senderData) {
        jam();
	break;
      }//if
    }//for
    if(ptr.i == RNIL) {
      jam();
      return;
    }//if
  } else {
    if (c_backupPool.findId(senderData)) {
      jam();
      c_backupPool.getPtr(ptr, senderData);
    } else { 
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("Backup: abort request type=%u on id=%u,%u not found",
	       requestType, backupId, senderData);
#endif
      return;
    }
  }//if
  
  ptr.p->m_gsn = GSN_ABORT_BACKUP_ORD;
  const bool isCoordinator = (ptr.p->masterRef == reference());
  
  bool ok = false;
  switch(requestType){

    /**
     * Requests sent to master
     */
  case AbortBackupOrd::ClientAbort:
    jam();
    // fall through
  case AbortBackupOrd::LogBufferFull:
    jam();
    // fall through
  case AbortBackupOrd::FileOrScanError:
    jam();
    ndbrequire(isCoordinator);
    ptr.p->setErrorCode(requestType);
    if(ptr.p->masterData.gsn == GSN_BACKUP_FRAGMENT_REQ)
    {
      /**
       * Only scans are actively aborted
       */
      abort_scan(signal, ptr);
    }
    return;
    
    /**
     * Requests sent to slave
     */
  case AbortBackupOrd::AbortScan:
    jam();
    ptr.p->setErrorCode(requestType);
    return;
    
  case AbortBackupOrd::BackupComplete:
    jam();
    cleanup(signal, ptr);
    return;
  case AbortBackupOrd::BackupFailure:
  case AbortBackupOrd::BackupFailureDueToNodeFail:
  case AbortBackupOrd::OkToClean:
  case AbortBackupOrd::IncompatibleVersions:
#ifndef VM_TRACE
  default:
#endif
    ptr.p->setErrorCode(requestType);
    ptr.p->masterData.errorCode = requestType;
    ok= true;
  }
  ndbrequire(ok);
  
  ptr.p->masterRef = reference();
  ptr.p->nodes.clear();
  ptr.p->nodes.set(getOwnNodeId());


  ptr.p->stopGCP= ptr.p->startGCP + 1;
  sendStopBackup(signal, ptr);
}


void
Backup::dumpUsedResources()
{
  jam();
  BackupRecordPtr ptr;

  for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
    ndbout_c("Backup id=%u, slaveState.getState = %u, errorCode=%u",
	     ptr.p->backupId,
	     ptr.p->slaveState.getState(),
	     ptr.p->errorCode);

    TablePtr tabPtr;
    for(ptr.p->tables.first(tabPtr);
	tabPtr.i != RNIL;
	ptr.p->tables.next(tabPtr)) {
      jam();
      for(Uint32 j = 0; j<3; j++) {
	jam();
	TriggerPtr trigPtr LINT_SET_PTR;
	if(tabPtr.p->triggerAllocated[j]) {
	  jam();
	  c_triggerPool.getPtr(trigPtr, tabPtr.p->triggerIds[j]);
	  ndbout_c("Allocated[%u] Triggerid = %u, event = %u",
		 j,
		 tabPtr.p->triggerIds[j],
		 trigPtr.p->event);
	}//if
      }//for
    }//for
    
    BackupFilePtr filePtr;
    for(ptr.p->files.first(filePtr);
	filePtr.i != RNIL;
	ptr.p->files.next(filePtr)) {
      jam();
      ndbout_c("filePtr.i = %u, flags: H'%x ",
	       filePtr.i, filePtr.p->m_flags);
    }//for
  }
}

void
Backup::cleanup(Signal* signal, BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  cleanupNextTable(signal, ptr, tabPtr);
}

void
Backup::cleanupNextTable(Signal *signal, BackupRecordPtr ptr, TablePtr tabPtr)
{
  if (tabPtr.i != RNIL)
  {
    jam();
    tabPtr.p->fragments.release();
    for(Uint32 j = 0; j<3; j++) {
      jam();
      TriggerPtr trigPtr LINT_SET_PTR;
      if(tabPtr.p->triggerAllocated[j]) {
        jam();
	c_triggerPool.getPtr(trigPtr, tabPtr.p->triggerIds[j]);
	trigPtr.p->event = ILLEGAL_TRIGGER_ID;
        tabPtr.p->triggerAllocated[j] = false;
      }//if
      tabPtr.p->triggerIds[j] = ILLEGAL_TRIGGER_ID;
    }//for
    {
      BackupLockTab *req = (BackupLockTab *)signal->getDataPtrSend();
      req->m_senderRef = reference();
      req->m_tableId = tabPtr.p->tableId;
      req->m_lock_unlock = BackupLockTab::UNLOCK_TABLE;
      req->m_backup_state = BackupLockTab::CLEANUP;
      req->m_backupRecordPtr_I = ptr.i;
      req->m_tablePtr_I = tabPtr.i;
      sendSignal(DBDICT_REF, GSN_BACKUP_LOCK_TAB_REQ, signal,
                 BackupLockTab::SignalLength, JBB);
      return;
    }
  }

  BackupFilePtr filePtr;
  for(ptr.p->files.first(filePtr);filePtr.i != RNIL;ptr.p->files.next(filePtr))
  {
    jam();
    ndbrequire(filePtr.p->m_flags == 0);
    filePtr.p->pages.release();
  }//for

  while (ptr.p->files.releaseFirst());
  while (ptr.p->tables.releaseFirst());
  while (ptr.p->triggers.releaseFirst());
  ptr.p->backupId = ~0;
  
  /*
    report of backup status uses these variables to keep track
    if files are used
  */
  ptr.p->ctlFilePtr = ptr.p->logFilePtr = ptr.p->dataFilePtr = RNIL;

  if(ptr.p->checkError())
    removeBackup(signal, ptr);
  else
  {
    /*
      report of backup status uses these variables to keep track
      if backup ia running and current state
    */
    ptr.p->m_gsn = 0;
    ptr.p->masterData.gsn = 0;
    c_backups.release(ptr);
  }
}


void
Backup::removeBackup(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  
  FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->directory = 1;
  req->ownDirectory = 1;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	     FsRemoveReq::SignalLength, JBA);
}

void
Backup::execFSREMOVEREF(Signal* signal)
{
  jamEntry();
  FsRef * ref = (FsRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->userPointer;

  FsConf * conf = (FsConf*)signal->getDataPtr();
  conf->userPointer = ptrI;
  execFSREMOVECONF(signal);
}

void
Backup::execFSREMOVECONF(Signal* signal){
  jamEntry();

  FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->userPointer;
  
  /**
   * Get backup record
   */
  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, ptrI);
  /*
    report of backup status uses these variables to keep track
    if backup ia running and current state
  */
  ptr.p->m_gsn = 0;
  ptr.p->masterData.gsn = 0;
  c_backups.release(ptr);
}

/**
 * LCP
 */
void
Backup::execLCP_PREPARE_REQ(Signal* signal)
{
  jamEntry();
  LcpPrepareReq req = *(LcpPrepareReq*)signal->getDataPtr();

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, req.backupPtr);

  ptr.p->m_gsn = GSN_LCP_PREPARE_REQ;

  TablePtr tabPtr;
  FragmentPtr fragPtr;
  if (!ptr.p->tables.isEmpty())
  {
    jam();
    ndbrequire(ptr.p->errorCode);
    ptr.p->tables.first(tabPtr);
    if (tabPtr.p->tableId == req.tableId)
    {
      jam();
      ndbrequire(!tabPtr.p->fragments.empty());
      tabPtr.p->fragments.getPtr(fragPtr, 0);
      fragPtr.p->fragmentId = req.fragmentId;
      defineBackupRef(signal, ptr, ptr.p->errorCode);
      return;
    }
    else
    {
      jam();
      tabPtr.p->fragments.release();
      while (ptr.p->tables.releaseFirst());
      ptr.p->errorCode = 0;
      // fall-through
    }
  }
  
  if (!ptr.p->tables.seizeLast(tabPtr) || !tabPtr.p->fragments.seize(1))
  {
    if(!tabPtr.isNull())
      while (ptr.p->tables.releaseFirst());
    ndbrequire(false); // TODO
  }
  tabPtr.p->tableId = req.tableId;
  tabPtr.p->fragments.getPtr(fragPtr, 0);
  tabPtr.p->tableType = DictTabInfo::UserTable;
  fragPtr.p->fragmentId = req.fragmentId;
  fragPtr.p->lcp_no = req.lcpNo;
  fragPtr.p->scanned = 0;
  fragPtr.p->scanning = 0;
  fragPtr.p->tableId = req.tableId;

  if (req.backupId != ptr.p->backupId)
  {
    jam();
    /* New LCP, reset per-LCP counters */
    ptr.p->noOfBytes = 0;
    ptr.p->noOfRecords = 0;
  }
  ptr.p->backupId= req.backupId;
  lcp_open_file(signal, ptr);
}

void
Backup::lcp_close_file_conf(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  
  TablePtr tabPtr;
  ndbrequire(ptr.p->tables.first(tabPtr));
  Uint32 tableId = tabPtr.p->tableId;

  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
  ndbrequire(filePtr.p->m_flags == 0);

  if (ptr.p->m_gsn == GSN_LCP_PREPARE_REQ)
  {
    jam();
    defineBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }
    
  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, 0);
  Uint32 fragmentId = fragPtr.p->fragmentId;
  
  tabPtr.p->fragments.release();
  while (ptr.p->tables.releaseFirst());

  if (ptr.p->errorCode != 0)
  {
    jam();
    ndbout_c("Fatal : LCP Frag scan failed with error %u",
             ptr.p->errorCode);
    ndbrequire(filePtr.p->errorCode == ptr.p->errorCode);
    
    if ((filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD) == 0)
    {
      jam();
      /* No active scan thread to 'find' the file error.
       * Scan is closed, so let's send backupFragmentRef 
       * back to LQH now...
       */
      backupFragmentRef(signal, filePtr);
    }
    return;
  }

  ptr.p->errorCode = 0;
  
  BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtrSend();
  conf->backupId = ptr.p->backupId;
  conf->backupPtr = ptr.i;
  conf->tableId = tableId;
  conf->fragmentNo = fragmentId;
  conf->noOfRecordsLow = 0;
  conf->noOfRecordsHigh = 0;
  conf->noOfBytesLow = 0;
  conf->noOfBytesHigh = 0;
  sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_CONF, signal,
	     BackupFragmentConf::SignalLength, JBB);
}

void
Backup::lcp_open_file(Signal* signal, BackupRecordPtr ptr)
{
  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = 
    FsOpenReq::OM_WRITEONLY | 
    FsOpenReq::OM_TRUNCATE |
    FsOpenReq::OM_CREATE | 
    FsOpenReq::OM_APPEND |
    FsOpenReq::OM_AUTOSYNC;

  if (c_defaults.m_compressed_lcp)
    req->fileFlags |= FsOpenReq::OM_GZ;

  if (c_defaults.m_o_direct)
    req->fileFlags |= FsOpenReq::OM_DIRECT;
  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  req->auto_sync_size = c_defaults.m_disk_synch_size;
  
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  
  ndbrequire(ptr.p->tables.first(tabPtr));
  tabPtr.p->fragments.getPtr(fragPtr, 0);

  /**
   * Lcp file
   */
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
  ndbrequire(filePtr.p->m_flags == 0);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;
  filePtr.p->tableId = RNIL; // Will force init
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 5);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v5_setLcpNo(req->fileNumber, fragPtr.p->lcp_no);
  FsOpenReq::v5_setTableId(req->fileNumber, tabPtr.p->tableId);
  FsOpenReq::v5_setFragmentId(req->fileNumber, fragPtr.p->fragmentId);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::lcp_open_file_done(Signal* signal, BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  FragmentPtr fragPtr;

  ndbrequire(ptr.p->tables.first(tabPtr));
  tabPtr.p->fragments.getPtr(fragPtr, 0);
  
  BackupFilePtr filePtr LINT_SET_PTR;
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);  
  ndbrequire(filePtr.p->m_flags == 
	     (BackupFile::BF_OPEN | BackupFile::BF_LCP_META));
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_LCP_META;

  ptr.p->slaveState.setState(STARTED);
  
  LcpPrepareConf* conf= (LcpPrepareConf*)signal->getDataPtrSend();
  conf->senderData = ptr.p->clientData;
  conf->senderRef = reference();
  conf->tableId = tabPtr.p->tableId;
  conf->fragmentId = fragPtr.p->fragmentId;
  sendSignal(ptr.p->masterRef, GSN_LCP_PREPARE_CONF, 
	     signal, LcpPrepareConf::SignalLength, JBB);

  /**
   * Start file thread
   */
  filePtr.p->m_flags |= BackupFile::BF_FILE_THREAD;
  
  signal->theData[0] = BackupContinueB::START_FILE_THREAD;
  signal->theData[1] = filePtr.i;
  signal->theData[2] = __LINE__;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 3);
}

void
Backup::execEND_LCPREQ(Signal* signal)
{
  EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();

  BackupRecordPtr ptr LINT_SET_PTR;
  c_backupPool.getPtr(ptr, req->backupPtr);
  ndbrequire(ptr.p->backupId == req->backupId);

  BackupFilePtr filePtr LINT_SET_PTR;
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  ndbrequire(filePtr.p->m_flags == 0);

  if (!ptr.p->tables.isEmpty())
  {
    jam();
    ndbrequire(ptr.p->errorCode);
    TablePtr tabPtr;
    ptr.p->tables.first(tabPtr);
    tabPtr.p->fragments.release();
    while (ptr.p->tables.releaseFirst());
    ptr.p->errorCode = 0;
  }

  ptr.p->errorCode = 0;
  ptr.p->slaveState.setState(CLEANING);
  ptr.p->slaveState.setState(INITIAL);
  ptr.p->slaveState.setState(DEFINING);
  ptr.p->slaveState.setState(DEFINED);

  EndLcpConf* conf= (EndLcpConf*)signal->getDataPtr();
  conf->senderData = ptr.p->clientData;
  conf->senderRef = reference();
  sendSignal(ptr.p->masterRef, GSN_END_LCPCONF,
	     signal, EndLcpConf::SignalLength, JBB);
}

inline
static 
void setWords(const Uint64 src, Uint32& hi, Uint32& lo)
{
  hi = (Uint32) (src >> 32);
  lo = (Uint32) (src & 0xffffffff);
}

void
Backup::execLCP_STATUS_REQ(Signal* signal)
{
  jamEntry();
  const LcpStatusReq* req = (const LcpStatusReq*) signal->getDataPtr();
  
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  Uint32 failCode = LcpStatusRef::NoLCPRecord;

  /* Find LCP backup, if there is one */
  BackupRecordPtr ptr;
  bool found_lcp = false;
  for (c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr))
  {
    jam();
    if (ptr.p->is_lcp())
    {
      jam();
      ndbrequire(found_lcp == false); /* Just one LCP */
      found_lcp = true;
      
      LcpStatusConf::LcpState state = LcpStatusConf::LCP_IDLE;
      switch (ptr.p->slaveState.getState())
      {
      case STARTED:
        jam();
        state = LcpStatusConf::LCP_PREPARED;
        break;
      case SCANNING:
        jam();
        state = LcpStatusConf::LCP_SCANNING;
        break;
      case STOPPING:
        jam();
        state = LcpStatusConf::LCP_SCANNED;
        break;
      case DEFINED:
        jam();
        state = LcpStatusConf::LCP_IDLE;
        break;
      default:
        jam();
        ndbout_c("Unusual LCP state in LCP_STATUS_REQ() : %u",
                 ptr.p->slaveState.getState());
        state = LcpStatusConf::LCP_IDLE;
      };
        
      /* Not all values are set here */
      const Uint32 UnsetConst = ~0;
      
      LcpStatusConf* conf = (LcpStatusConf*) signal->getDataPtr();
      conf->senderRef = reference();
      conf->senderData = senderData;
      conf->lcpState = state;
      conf->tableId = UnsetConst;
      conf->fragId = UnsetConst;
      conf->completionStateHi = UnsetConst;
      conf->completionStateLo = UnsetConst;
      setWords(ptr.p->noOfRecords,
               conf->lcpDoneRowsHi,
               conf->lcpDoneRowsLo);
      setWords(ptr.p->noOfBytes,
               conf->lcpDoneBytesHi,
               conf->lcpDoneBytesLo);
      conf->lcpScannedPages = 0;
      
      if (state == LcpStatusConf::LCP_SCANNING ||
          state == LcpStatusConf::LCP_SCANNED)
      {
        jam();
        /* Actually scanning/closing a fragment, let's grab the details */
        TablePtr tabPtr;
        FragmentPtr fragPtr;
        BackupFilePtr filePtr;
        
        if (ptr.p->dataFilePtr == RNIL)
        {
          jam();
          failCode = LcpStatusRef::NoFileRecord;
          break;
        }
        c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
        ndbrequire(filePtr.p->backupPtr == ptr.i);

        ptr.p->tables.first(tabPtr);
        if (tabPtr.i != RNIL)
        {
          jam();
          tabPtr.p->fragments.getPtr(fragPtr, 0);
          ndbrequire(fragPtr.p->tableId == tabPtr.p->tableId);
          conf->tableId = tabPtr.p->tableId;
          conf->fragId = fragPtr.p->fragmentId;
        }
        
        if (state == LcpStatusConf::LCP_SCANNING)
        {
          jam();
          setWords(filePtr.p->operation.noOfRecords,
                   conf->completionStateHi,
                   conf->completionStateLo);
          conf->lcpScannedPages = filePtr.p->operation.lcpScannedPages;
        }
        else if (state == LcpStatusConf::LCP_SCANNED)
        {
          jam();
          /* May take some time to drain the FS buffer, depending on
           * size of buff, achieved rate.
           * We provide the buffer fill level so that requestors
           * can observe whether there's progress in this phase.
           */
          Uint64 flushBacklog = 
            filePtr.p->operation.dataBuffer.getUsableSize() -
            filePtr.p->operation.dataBuffer.getFreeSize();
          
          setWords(flushBacklog,
                   conf->completionStateHi,
                   conf->completionStateLo);
        }
      }
      
      failCode = 0;
    }
  }

  if (failCode == 0)
  {
    jam();
    sendSignal(senderRef, GSN_LCP_STATUS_CONF, 
               signal, LcpStatusConf::SignalLength, JBB);
    return;
  }

  jam();
  LcpStatusRef* ref = (LcpStatusRef*) signal->getDataPtr();
  
  ref->senderRef = reference();
  ref->senderData = senderData;
  ref->error = failCode;
  
  sendSignal(senderRef, GSN_LCP_STATUS_REF, 
             signal, LcpStatusRef::SignalLength, JBB);
  return;
}
