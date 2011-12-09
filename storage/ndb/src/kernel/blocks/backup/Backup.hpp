/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BACKUP_H
#define BACKUP_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>

#include "FsBuffer.hpp"
#include "BackupFormat.hpp"

#include <NodeBitmask.hpp>
#include <SimpleProperties.hpp>

#include <SLList.hpp>
#include <DLFifoList.hpp>
#include <DLCFifoList.hpp>
#include <SignalCounter.hpp>
#include <blocks/mutexes.hpp>

#include <NdbTCP.h>
#include <NdbTick.h>
#include <Array.hpp>

/**
 * Backup - This block manages database backup and restore
 */
class Backup : public SimulatedBlock
{
  friend class BackupProxy;

public:
  Backup(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Backup();
  BLOCK_DEFINES(Backup);
  
protected:

  void execSTTOR(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  void execCONTINUEB(Signal* signal);
  
  /**
   * Testing
   */
  void execBACKUP_REF(Signal* signal);
  void execBACKUP_CONF(Signal* signal);
  void execBACKUP_ABORT_REP(Signal* signal);
  void execBACKUP_COMPLETE_REP(Signal* signal);
  
  /**
   * Signals sent from master
   */
  void execDEFINE_BACKUP_REQ(Signal* signal);
  void execBACKUP_DATA(Signal* signal);
  void execSTART_BACKUP_REQ(Signal* signal);
  void execBACKUP_FRAGMENT_REQ(Signal* signal);
  void execBACKUP_FRAGMENT_COMPLETE_REP(Signal* signal);
  void execSTOP_BACKUP_REQ(Signal* signal);
  void execBACKUP_STATUS_REQ(Signal* signal);
  void execABORT_BACKUP_ORD(Signal* signal);
 
  /**
   * The actual scan
   */
  void execSCAN_HBREP(Signal* signal);
  void execTRANSID_AI(Signal* signal);
  void execSCAN_FRAGREF(Signal* signal);
  void execSCAN_FRAGCONF(Signal* signal);

  /**
   * Trigger logging
   */
  void execBACKUP_TRIG_REQ(Signal* signal);
  void execTRIG_ATTRINFO(Signal* signal);
  void execFIRE_TRIG_ORD(Signal* signal);
  
  /**
   * DICT signals
   */
  void execLIST_TABLES_CONF(Signal* signal);
  void execGET_TABINFOREF(Signal* signal);
  void execGET_TABINFO_CONF(Signal* signal);
  void execCREATE_TRIG_IMPL_REF(Signal* signal);
  void execCREATE_TRIG_IMPL_CONF(Signal* signal);
  void execDROP_TRIG_IMPL_REF(Signal* signal);
  void execDROP_TRIG_IMPL_CONF(Signal* signal);

  /**
   * DIH signals
   */
  void execDIH_SCAN_TAB_CONF(Signal* signal);
  void execDIH_SCAN_GET_NODES_CONF(Signal* signal);

  /**
   * FS signals
   */
  void execFSOPENREF(Signal* signal);
  void execFSOPENCONF(Signal* signal);

  void execFSCLOSEREF(Signal* signal);
  void execFSCLOSECONF(Signal* signal);
  
  void execFSAPPENDREF(Signal* signal);
  void execFSAPPENDCONF(Signal* signal);
  
  void execFSREMOVEREF(Signal* signal);
  void execFSREMOVECONF(Signal* signal);

  /**
   * Master functinallity
   */
  void execBACKUP_REQ(Signal* signal);
  void execABORT_BACKUP_REQ(Signal* signal);
  
  void execDEFINE_BACKUP_REF(Signal* signal);
  void execDEFINE_BACKUP_CONF(Signal* signal);

  void execSTART_BACKUP_REF(Signal* signal);
  void execSTART_BACKUP_CONF(Signal* signal);

  void execBACKUP_FRAGMENT_REF(Signal* signal);
  void execBACKUP_FRAGMENT_CONF(Signal* signal);

  void execSTOP_BACKUP_REF(Signal* signal);
  void execSTOP_BACKUP_CONF(Signal* signal);
  
  void execBACKUP_STATUS_CONF(Signal* signal);

  void execUTIL_SEQUENCE_REF(Signal* signal);
  void execUTIL_SEQUENCE_CONF(Signal* signal);

  void execWAIT_GCP_REF(Signal* signal);
  void execWAIT_GCP_CONF(Signal* signal);
  void execBACKUP_LOCK_TAB_CONF(Signal *signal);
  void execBACKUP_LOCK_TAB_REF(Signal *signal);

  void execLCP_PREPARE_REQ(Signal* signal);
  void execLCP_FRAGMENT_REQ(Signal*);
  void execEND_LCPREQ(Signal* signal);

  void execDBINFO_SCANREQ(Signal *signal);

private:
  void defineBackupMutex_locked(Signal* signal, Uint32 ptrI,Uint32 retVal);
  void dictCommitTableMutex_locked(Signal* signal, Uint32 ptrI,Uint32 retVal);

public:
  struct Node {
    Uint32 nodeId;
    Uint32 alive;
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };
  };
  typedef Ptr<Node> NodePtr;

#define BACKUP_WORDS_PER_PAGE 8191
  struct Page32 {
    union {
      Uint32 data[BACKUP_WORDS_PER_PAGE];
      Uint32 chunkSize;
      Uint32 nextChunk;
      Uint32 lastChunk;
    };
    Uint32 nextPool;
  };
  typedef Ptr<Page32> Page32Ptr;

  struct Fragment {
    Uint64 noOfRecords;
    Uint32 tableId;
    Uint16 node;
    Uint16 fragmentId;
    Uint8 lqhInstanceKey;
    Uint8 scanned;  // 0 = not scanned x = scanned by node x
    Uint8 scanning; // 0 = not scanning x = scanning on node x
    Uint8 lcp_no;
    union {
      Uint32 nextPool;
      Uint32 chunkSize;
      Uint32 nextChunk;
      Uint32 lastChunk;
    };
  };
  typedef Ptr<Fragment> FragmentPtr;

  struct Table {
    Table(ArrayPool<Fragment> &);
    
    Uint64 noOfRecords;

    Uint32 tableId;
    Uint32 schemaVersion;
    Uint32 tableType;
    Uint32 m_scan_cookie;
    Uint32 triggerIds[3];
    bool   triggerAllocated[3];
    Uint32 maxRecordSize;
    Uint32 attrInfoLen;
    Uint32 noOfAttributes;
    /**
     * AttributeHeader::READ_PACKED + full mask + ( DISKREF ROWID ROWGCI )
     */
    Uint32 attrInfo[1+MAXNROFATTRIBUTESINWORDS+3];
    
    Array<Fragment> fragments;

    Uint32 nextList;
    union { Uint32 nextPool; Uint32 prevList; };
  };
  typedef Ptr<Table> TablePtr;

  struct OperationRecord {
  public:
    OperationRecord(Backup & b) : backup(b) {}

    /**
     * Once per table
     */
    void init(const TablePtr & ptr);
    
    /**
     * Once per fragment
     */
    bool newFragment(Uint32 tableId, Uint32 fragNo);
    bool fragComplete(Uint32 tableId, Uint32 fragNo, bool fill_record);
    
    /**
     * Once per scan frag (next) req/conf
     */
    bool newScan();
    bool scanConf(Uint32 noOfOps, Uint32 opLen);
    bool closeScan();
    
    /**
     * Per record
     */
    void newRecord(Uint32 * base);
    void finished(Uint32 len);
    
  private:
    Uint32* base; 
    Uint32 opNoDone;
    Uint32 opNoConf;
    Uint32 opLen;

  public:
    Uint32* dst;
    Uint32 attrSzTotal; // No of AI words received
    Uint32 tablePtr;    // Ptr.i to current table

    FsBuffer dataBuffer;
    Uint64 noOfRecords;
    Uint64 noOfBytes;
    Uint32 maxRecordSize;
    
    /*
      keeps track of total written into backup file to be able to show
      backup status
    */
    Uint64 m_records_total;
    Uint64 m_bytes_total;

  private:
    Uint32* scanStart;
    Uint32* scanStop;

  public:
    union { Uint32 nextPool; Uint32 nextList; };
    Uint32 prevList;
  private:

    Backup & backup;
    BlockNumber number() const { return backup.number(); }
    EmulatedJamBuffer *jamBuffer() const { return backup.jamBuffer(); }
    void progError(int line, int cause, const char * extra) { 
      backup.progError(line, cause, extra); 
    }
  };
  friend struct OperationRecord;

  struct TriggerRecord {
    TriggerRecord() { event = ~0;}
    OperationRecord * operation;
    BackupFormat::LogFile::LogEntry * logEntry;
    Uint32 maxRecordSize;
    Uint32 tableId;
    Uint32 tab_ptr_i;
    Uint32 event;
    Uint32 backupPtr;
    Uint32 errorCode;
    union { Uint32 nextPool; Uint32 nextList; };
  };
  typedef Ptr<TriggerRecord> TriggerPtr;
  
  /**
   * BackupFile - At least 3 per backup
   */
  struct BackupFile {
    BackupFile(Backup & backup, ArrayPool<Page32> & pp) 
      : operation(backup),  pages(pp) { m_retry_count = 0; }
    
    Uint32 backupPtr; // Pointer to backup record
    Uint32 tableId;
    Uint32 fragmentNo;
    Uint32 filePointer;
    Uint32 m_retry_count;
    Uint32 errorCode;
    BackupFormat::FileType fileType;
    OperationRecord operation;
    
    Array<Page32> pages;
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };
    
    enum {
      BF_OPEN         = 0x1
      ,BF_OPENING     = 0x2
      ,BF_CLOSING     = 0x4
      ,BF_FILE_THREAD = 0x8
      ,BF_SCAN_THREAD = 0x10
      ,BF_LCP_META    = 0x20
    };
    Uint32 m_flags;
    Uint32 m_pos;
  }; 
  typedef Ptr<BackupFile> BackupFilePtr;
 

  /**
   * State for BackupRecord
   */
  enum State {
    INITIAL  = 0,
    DEFINING = 1, // Defining backup content and parameters
    DEFINED  = 2,  // DEFINE_BACKUP_CONF sent in slave, received all in master
    STARTED  = 3,  // Creating triggers
    SCANNING = 4, // Scanning fragments
    STOPPING = 5, // Closing files
    CLEANING = 6, // Cleaning resources
    ABORTING = 7  // Aborting backup
  };

  static const Uint32 validSlaveTransitionsCount;
  static const Uint32 validMasterTransitionsCount;
  static const State validSlaveTransitions[];
  static const State validMasterTransitions[];
  
  class CompoundState {
  public:
    CompoundState(Backup & b, 
		  const State valid[],
		  Uint32 count, Uint32 _id) 
      : backup(b)
      , validTransitions(valid),
	noOfValidTransitions(count), id(_id)
    { 
      state = INITIAL;
      abortState = state;
    }
    
    void setState(State s);
    State getState() const { return state;}
    State getAbortState() const { return abortState;}
    
    void forceState(State s);
    
    BlockNumber number() const { return backup.number(); }
    EmulatedJamBuffer *jamBuffer() const { return backup.jamBuffer(); }
    void progError(int line, int cause, const char * extra) { 
      backup.progError(line, cause, extra); 
    }
  private:
    Backup & backup;
    State state;     
    State abortState;     /**
			     When state == ABORTING, this contains the state 
			     when the abort started
			  */
    const State * validTransitions;
    const Uint32 noOfValidTransitions;
    const Uint32 id;
  };
  friend class CompoundState;
  
  /**
   * Backup record
   *
   * One record per backup
   */
  struct BackupRecord {
    BackupRecord(Backup& b, 
		 ArrayPool<Table> & tp, 
		 ArrayPool<BackupFile> & bp,
		 ArrayPool<TriggerRecord> & trp) 
      : slaveState(b, validSlaveTransitions, validSlaveTransitionsCount,1)
      , tables(tp), triggers(trp), files(bp)
      , ctlFilePtr(RNIL), logFilePtr(RNIL), dataFilePtr(RNIL)
      , masterData(b), backup(b)

      {
        /*
          report of backup status uses these variables to keep track
          if backup ia running and current state
        */
        m_gsn = 0;
        masterData.gsn = 0;
      }
    
    /* next time to report backup status */
    Uint64 m_next_report;

    Uint32 m_gsn;
    CompoundState slaveState; 
    
    Uint32 clientRef;
    Uint32 clientData;
    Uint32 flags;
    Uint32 signalNo;
    Uint32 backupId;
    Uint32 backupKey[2];
    Uint32 masterRef;
    Uint32 errorCode;
    NdbNodeBitmask nodes;
    
    Uint64 noOfBytes;
    Uint64 noOfRecords;
    Uint64 noOfLogBytes;
    Uint64 noOfLogRecords;
    
    Uint32 startGCP;
    Uint32 currGCP;
    Uint32 stopGCP;
    DLCFifoList<Table> tables;
    SLList<TriggerRecord> triggers;
    
    SLList<BackupFile> files; 
    Uint32 ctlFilePtr;  // Ptr.i to ctl-file
    Uint32 logFilePtr;  // Ptr.i to log-file
    Uint32 dataFilePtr; // Ptr.i to first data-file
    
    Uint32 backupDataLen;  // Used for (un)packing backup request
    SimpleProperties props;// Used for (un)packing backup request

    struct SlaveData {
      SignalCounter trigSendCounter;
      Uint32 gsn;
      struct {
	Uint32 tableId;
      } createTrig;
      struct {
	Uint32 tableId;
      } dropTrig;
    } slaveData;

    struct MasterData {
      MasterData(Backup & b) 
	{
	}
      MutexHandle2<BACKUP_DEFINE_MUTEX> m_defineBackupMutex;
      MutexHandle2<DICT_COMMIT_TABLE_MUTEX> m_dictCommitTableMutex;

      Uint32 gsn;
      SignalCounter sendCounter;
      Uint32 errorCode;
      union {
        struct {
          Uint32 retriesLeft;
        } sequence;
	struct {
	  Uint32 startBackup;
	} waitGCP;
	struct {
	  Uint32 signalNo;
	  Uint32 noOfSignals;
	  Uint32 tablePtr;
	} startBackup;
	struct {
	  Uint32 dummy;
	} stopBackup;
      };
    } masterData;
    
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };

    void setErrorCode(Uint32 errCode){
      if(errorCode == 0)
	errorCode = errCode;
    }

    bool checkError() const {
      return errorCode != 0;
    }

    bool is_lcp() const {
      return backupDataLen == ~(Uint32)0;
    }

    Backup & backup;
    BlockNumber number() const { return backup.number(); }
    EmulatedJamBuffer *jamBuffer() const { return backup.jamBuffer(); }
    void progError(int line, int cause, const char * extra) { 
      backup.progError(line, cause, extra); 
    }
  };
  friend struct BackupRecord;
  typedef Ptr<BackupRecord> BackupRecordPtr;

  struct Config {
    Uint32 m_dataBufferSize;
    Uint32 m_logBufferSize;
    Uint32 m_minWriteSize;
    Uint32 m_maxWriteSize;
    Uint32 m_lcp_buffer_size;
    
    Uint32 m_disk_write_speed_sr;
    Uint32 m_disk_write_speed;
    Uint32 m_disk_synch_size;
    Uint32 m_diskless;
    Uint32 m_o_direct;
    Uint32   m_compressed_backup;
    Uint32 m_compressed_lcp;
  };
  
  /**
   * Variables
   */
  Uint32 * c_startOfPages;
  NodeId c_masterNodeId;
  SLList<Node> c_nodes;
  NdbNodeBitmask c_aliveNodes;
  DLList<BackupRecord> c_backups;
  Config c_defaults;

  /*
    Variables that control checkpoint to disk speed
  */
  Uint32 m_curr_disk_write_speed;
  Uint32 m_words_written_this_period;
  Uint32 m_overflow_disk_write;
  Uint32 m_reset_delay_used;
  NDB_TICKS m_reset_disk_speed_time;
  static const int  DISK_SPEED_CHECK_DELAY = 100;
  
  STATIC_CONST(NO_OF_PAGES_META_FILE = 
	       (2*MAX_WORDS_META_FILE + BACKUP_WORDS_PER_PAGE - 1) / 
	       BACKUP_WORDS_PER_PAGE);

  Uint32 m_backup_report_frequency;

  /**
   * Pools
   */
  ArrayPool<Table> c_tablePool;
  ArrayPool<BackupRecord> c_backupPool;
  ArrayPool<BackupFile> c_backupFilePool;
  ArrayPool<Page32> c_pagePool;
  ArrayPool<Fragment> c_fragmentPool;
  ArrayPool<Node> c_nodePool;
  ArrayPool<TriggerRecord> c_triggerPool;

  void checkFile(Signal*, BackupFilePtr);
  void checkScan(Signal*, BackupFilePtr);
  void fragmentCompleted(Signal*, BackupFilePtr);
  
  void backupAllData(Signal* signal, BackupRecordPtr);
  
  void getFragmentInfo(Signal*, BackupRecordPtr, TablePtr, Uint32 fragNo);
  void getFragmentInfoDone(Signal*, BackupRecordPtr);
  
  void openFiles(Signal* signal, BackupRecordPtr ptr);
  void openFilesReply(Signal*, BackupRecordPtr ptr, BackupFilePtr);
  void closeFiles(Signal*, BackupRecordPtr ptr);
  void closeFile(Signal*, BackupRecordPtr, BackupFilePtr);
  void closeFilesDone(Signal*, BackupRecordPtr ptr);  
  
  void sendDefineBackupReq(Signal *signal, BackupRecordPtr ptr);

  void defineBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId);
  void createTrigReply(Signal* signal, BackupRecordPtr ptr);
  void alterTrigReply(Signal* signal, BackupRecordPtr ptr);
  void startBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32);
  void stopBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId);
  
  void defineBackupRef(Signal*, BackupRecordPtr, Uint32 errCode = 0);
  void backupFragmentRef(Signal * signal, BackupFilePtr filePtr);

  void nextFragment(Signal*, BackupRecordPtr);
  
  void sendCreateTrig(Signal*, BackupRecordPtr ptr, TablePtr tabPtr);
  void createAttributeMask(TablePtr tab, Bitmask<MAXNROFATTRIBUTESINWORDS>&);
  void sendStartBackup(Signal*, BackupRecordPtr, TablePtr);
  void sendAlterTrig(Signal*, BackupRecordPtr ptr);

  void sendScanFragReq(Signal*,
                       BackupRecordPtr,
                       BackupFilePtr,
                       TablePtr,
                       FragmentPtr,
                       Uint32 delay);

  void sendDropTrig(Signal*, BackupRecordPtr ptr);
  void sendDropTrig(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr);
  void dropTrigReply(Signal*, BackupRecordPtr ptr);
  
  void sendSignalAllWait(BackupRecordPtr ptr, Uint32 gsn, Signal *signal, 
			 Uint32 signalLength,
			 bool executeDirect = false);
  bool haveAllSignals(BackupRecordPtr ptr, Uint32 gsn, Uint32 nodeId);

  void sendStopBackup(Signal*, BackupRecordPtr ptr);
  void sendAbortBackupOrd(Signal* signal, BackupRecordPtr ptr, Uint32 errCode);
  void sendAbortBackupOrdSlave(Signal* signal, BackupRecordPtr ptr, 
			       Uint32 errCode);
  void masterAbort(Signal*, BackupRecordPtr ptr);
  void masterSendAbortBackup(Signal*, BackupRecordPtr ptr);
  void slaveAbort(Signal*, BackupRecordPtr ptr);
  
  void abortFile(Signal* signal, BackupRecordPtr ptr, BackupFilePtr filePtr);
  void abortFileHook(Signal* signal, BackupFilePtr filePtr, bool scanDone);
  
  bool verifyNodesAlive(BackupRecordPtr, const NdbNodeBitmask& aNodeBitMask);
  bool checkAbort(BackupRecordPtr ptr);
  void checkNodeFail(Signal* signal,
		     BackupRecordPtr ptr,
		     NodeId newCoord,
		     Uint32 theFailedNodes[NdbNodeBitmask::Size]);
  void masterTakeOver(Signal* signal, BackupRecordPtr ptr);


  NodeId getMasterNodeId() const { return c_masterNodeId; }
  bool findTable(const BackupRecordPtr &, TablePtr &, Uint32 tableId) const;
  bool parseTableDescription(Signal*, BackupRecordPtr ptr, TablePtr, const Uint32*, Uint32);
  
  bool insertFileHeader(BackupFormat::FileType, BackupRecord*, BackupFile*);
  void sendBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errorCode);
  void sendBackupRef(BlockReference ref, Uint32 flags, Signal *signal,
		     Uint32 senderData, Uint32 errorCode);
  void dumpUsedResources();
  void cleanup(Signal*, BackupRecordPtr ptr);
  void abort_scan(Signal*, BackupRecordPtr ptr);
  void removeBackup(Signal*, BackupRecordPtr ptr);

  void sendUtilSequenceReq(Signal*, BackupRecordPtr ptr, Uint32 delay = 0);

  /*
    For periodic backup status reporting and explicit backup status reporting
  */
  /* Init at start of backup, timers etc... */
  void initReportStatus(Signal* signal, BackupRecordPtr ptr);
  /* Sheck timers for reporting at certain points */
  void checkReportStatus(Signal* signal, BackupRecordPtr ptr);
  /* Send backup status, invoked either periodically, or explicitly */
  void reportStatus(Signal* signal, BackupRecordPtr ptr,
                    BlockReference ref = CMVMI_REF);

  void sendSTTORRY(Signal*);
  void createSequence(Signal* signal);
  void createSequenceReply(Signal*, class UtilSequenceConf *);

  void lcp_open_file(Signal* signal, BackupRecordPtr ptr);
  void lcp_open_file_done(Signal*, BackupRecordPtr);
  void lcp_close_file_conf(Signal* signal, BackupRecordPtr);
  void read_lcp_descriptor(Signal*, BackupRecordPtr, TablePtr);

  bool ready_to_write(bool ready, Uint32 sz, bool eof, BackupFile *fileP);

  void afterGetTabinfoLockTab(Signal *signal,
                              BackupRecordPtr ptr, TablePtr tabPtr);
  void cleanupNextTable(Signal *signal, BackupRecordPtr ptr, TablePtr tabPtr);

  BackupFormat::LogFile::LogEntry* get_log_buffer(Signal*,TriggerPtr, Uint32);

  /*
   * MT LQH.  LCP runs separately in each instance number.
   * BACKUP uses instance key 1 (real instance 0 or 1).
  */
  STATIC_CONST( UserBackupInstanceKey = 1 );
  Uint32 instanceKey(BackupRecordPtr ptr) {
    return ptr.p->is_lcp() ? instance() : UserBackupInstanceKey;
  }
};

inline
void
Backup::OperationRecord::newRecord(Uint32 * p)
{
  dst = p;
  scanStop = p;
}

inline
void
Backup::OperationRecord::finished(Uint32 len)
{
  opLen += len;
  opNoDone++;
  noOfRecords++;
}

#endif
