/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef BACKUP_H
#define BACKUP_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>

#include "FsBuffer.hpp"
#include "BackupFormat.hpp"

#include <NodeBitmask.hpp>
#include <SimpleProperties.hpp>

#include <IntrusiveList.hpp>
#include <SignalCounter.hpp>
#include <blocks/mutexes.hpp>

#include <NdbTick.h>
#include <Array.hpp>
#include <Mutex.hpp>

#include <signaldata/RedoStateRep.hpp>
#include "../dblqh/Dblqh.hpp"
#include <signaldata/BackupSignalData.hpp>

#define JAM_FILE_ID 474


/**
 * Backup - This block manages database backup and restore
 */
class Backup : public SimulatedBlock
{
  friend class BackupProxy;

  bool m_is_query_block;
public:
  Backup(Block_context& ctx,
         Uint32 instanceNumber = 0,
         Uint32 blockNo = BACKUP);
  ~Backup() override;
  BLOCK_DEFINES(Backup);
 
  class Dblqh* c_lqh;
  class Dbtup* c_tup;
  class Lgman* c_lgman;
  class Pgman* c_pgman;

  enum CallbackIndex {
    // lgman
    SYNC_LOG_LCP_LSN = 1,
    // pgman
    COUNT_CALLBACKS = 2
  };
  CallbackEntry m_callbackEntry[COUNT_CALLBACKS];
  CallbackTable m_callbackTable;

protected:

  void execSTTOR(Signal* signal);
  void execREDO_STATE_REP(Signal*);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execNODE_START_REP(Signal* signal);
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
  void execCHECK_NODE_RESTARTCONF(Signal*);

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

  void execFSREADREF(Signal* signal);
  void execFSREADCONF(Signal* signal);

  void execFSWRITEREF(Signal* signal);
  void execFSWRITECONF(Signal* signal);

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
  void execSYNC_PAGE_WAIT_REP(Signal*);
  void execSYNC_PAGE_CACHE_CONF(Signal*);
  void execSYNC_EXTENT_PAGES_CONF(Signal*);
  void execEND_LCPREQ(Signal* signal);
  void execINFORM_BACKUP_DROP_TAB_REQ(Signal*);
  void execWAIT_LCP_IDLE_REQ(Signal*);

  void execDBINFO_SCANREQ(Signal *signal);

  void execRESTORABLE_GCI_REP(Signal *signal);

  void execLCP_STATUS_REQ(Signal* signal);

private:
  void defineBackupMutex_locked(Signal* signal, Uint32 ptrI,Uint32 retVal);
  void dictCommitTableMutex_locked(Signal* signal, Uint32 ptrI,Uint32 retVal);
  void startDropTrig_synced(Signal* signal, Uint32 ptrI, Uint32 retVal);
  Uint32 validateEncryptionPassword(const EncryptionKeyMaterial* epd);

 public:
  struct Node {
    Uint32 nodeId;
    Uint32 alive;
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };
  };
  typedef Ptr<Node> NodePtr;
  typedef ArrayPool<Node> Node_pool;
  typedef SLList<Node_pool> Node_list;

  void update_lcp_pages_scanned(Signal *signal,
                                Uint32 filePtrI,
                                Uint32 scanned_pages,
                                Uint32 & scanGCI,
                                bool & changed_row_page_flag);

  bool is_partial_lcp_enabled()
  {
    return m_enable_partial_lcp != 0;
  }
  bool is_redo_control_enabled()
  {
    return m_enable_redo_control != 0;
  }

  Uint32 get_recovery_work()
  {
    return m_recovery_work;
  }
  Uint32 get_insert_recovery_work()
  {
    return m_insert_recovery_work;
  }
  void init_extended_lcp_stat();
  void print_extended_lcp_stat();
  void alloc_page_after_lcp_start(Uint32 page_no);
  void alloc_dropped_page_after_lcp_start(bool is_change_page);
  void dropped_page_after_lcp_start(bool is_change_page,
                                    bool is_last_lcp_state_A);
  void skip_page_lcp_scanned_bit();
  void skip_no_change_page();
  void skip_empty_page_lcp();
  void record_dropped_empty_page_lcp();
  void record_late_alloc_page_lcp();
  void page_to_skip_lcp(bool is_last_lcp_state_A);
  void lcp_keep_row();
  void lcp_keep_delete_row();
  void lcp_keep_delete_by_page_id();

  void init_lcp_scan(Uint32 & scanGCI,
                     bool & changed_row_page_flag);
  void end_lcp_scan(Uint32 number_of_pages);

  void record_deleted_rowid(Uint32 pageNo,
                            Uint32 pageIndex,
                            Uint32 gci);

  void record_deleted_pageid(Uint32 pageNo, Uint32 record_size);

  int is_page_lcp_scanned(Uint32 page_id, bool & all_part);
  Uint32 hash_lcp_part(Uint32 page_id) const;

#define BACKUP_WORDS_PER_PAGE 8191
  struct Page32 {
    Uint32 data[BACKUP_WORDS_PER_PAGE];
    Uint32 nextPool;
  };
  typedef Ptr<Page32> Page32Ptr;
  typedef ArrayPool<Page32> Page32_pool;

  struct Fragment {
    Uint64 noOfRecords;
    Uint32 tableId;
    Uint32 createGci;
    Uint16 node;
    Uint16 fragmentId;
    Uint32 lqhInstanceKey;
    Uint8 scanned;  // 0 = not scanned x = scanned by node x
    Uint8 scanning; // 0 = not scanning x = scanning on node x
    Uint8 firstFragment;
    Uint32 nextPool;
  };
  typedef Ptr<Fragment> FragmentPtr;
  typedef ArrayPool<Fragment> Fragment_pool;

  struct DeleteLcpFile
  {
    Uint64 lcpLsn;
    Uint32 tableId;
    Uint32 fragmentId;
    Uint32 firstFileId;
    Uint32 lastFileId;
    Uint32 waitCompletedGci;
    Uint32 lcpCtlFileNumber;
    Uint32 validFlag;
    union
    {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<DeleteLcpFile> DeleteLcpFilePtr;
  typedef ArrayPool<DeleteLcpFile> DeleteLcpFile_pool;
  typedef DLCFifoList<DeleteLcpFile_pool> DeleteLcpFile_list;
  typedef LocalDLCFifoList<DeleteLcpFile_pool>
    LocalDeleteLcpFile_list;
  DeleteLcpFile_list::Head m_delete_lcp_file_head;

  Uint32 m_newestRestorableGci;
  bool m_delete_lcp_files_ongoing;
  Uint32 m_enable_redo_control;
  Uint32 m_enable_partial_lcp;
  Uint32 m_recovery_work;
  Uint32 m_insert_recovery_work;

  Uint32 m_cfg_mt_backup;
  bool m_skew_disk_speed;

  struct Table {
    Table(Fragment_pool &);
    
    Uint64 noOfRecords;

    Uint32 tableId;
    Uint32 backupPtrI;
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
    /**
     * Pointer used by c_tableMap
     */
    Uint32 nextMapTable;
  };
  typedef Ptr<Table> TablePtr;
  typedef ArrayPool<Table> Table_pool;
  typedef SLList<Table_pool> Table_list;
  typedef DLCFifoList<Table_pool> Table_fifo;

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
    void scanConf(Uint32 noOfOps, Uint32 opLen, Uint32 buffer_data_len);
    Uint32 publishBufferData();
    void closeScan();
    
    /**
     * Per record
     */
    void newRecord(Uint32 * base);
    void finished(Uint32 len);
    void set_scanned_pages(Uint32 num_scanned_pages);
    
  private:
    Uint32* base; 
    Uint32 opNoDone;
    Uint32 opNoConf;
    Uint32 opLen;

  public:
    Uint32* dst;
    Uint32 tablePtr;    // Ptr.i to current table

    FsBuffer dataBuffer;
    Uint64 noOfRecords;
    Uint64 noOfBytes;
    Uint32 maxRecordSize;
    Uint32 lcpScannedPages;
    
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
    [[noreturn]] void progError(int line,
                                int cause,
                                const char * extra,
                                const char * check)
    {
      backup.progError(line, cause, extra, check);
    }
  };
  friend struct OperationRecord;

  struct TriggerRecord {
    TriggerRecord() { event = ~0;}
    OperationRecord * operation;
    BackupFormat::LogFile::LogEntry * logEntry;
    Uint32 tableId;
    Uint32 tab_ptr_i;
    Uint32 event;
    Uint32 backupPtr;
    Uint32 errorCode;
    union { Uint32 nextPool; Uint32 nextList; };
  };
  typedef Ptr<TriggerRecord> TriggerPtr;
  typedef ArrayPool<TriggerRecord> TriggerRecord_pool;
  typedef SLList<TriggerRecord_pool> TriggerRecord_list;

  /**
   * BackupFile - At least 3 per backup
   */
  struct BackupFile {
    BackupFile(Backup & backup, Page32_pool& pp)
      : operation(backup),  pages(pp) { m_retry_count = 0; }
    
    Uint32 backupPtr; // Pointer to backup record
    Uint32 tableId;
    Uint32 fragmentNo;
    Uint32 filePointer;
    Uint32 bytesRead;

    Uint32 m_retry_count;
    Uint32 errorCode;
    BackupFormat::FileType fileType;
    OperationRecord operation;

    Uint64 m_lcp_inserts;
    Uint64 m_lcp_writes;
    Uint64 m_lcp_delete_by_pageids;
    Uint64 m_lcp_delete_by_rowids;
    
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
      ,BF_HEADER_FILE = 0x40
      ,BF_READING     = 0x80
      ,BF_WRITING     = 0x100
      ,BF_REMOVING    = 0x200
    };
    Uint32 m_flags;
    Uint32 m_pos;
  }; 
  typedef Ptr<BackupFile> BackupFilePtr;
  typedef ArrayPool<BackupFile> BackupFile_pool;
  typedef SLList<BackupFile_pool> BackupFile_list;

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

  enum PrepareState
  {
    NOT_ACTIVE  = 0,
    PREPARE_READ_CTL_FILES  = 1,
    PREPARE_OPEN_DATA_FILE = 2,
    PREPARE_READ_TABLE_DESC = 3,
    PREPARED = 4,
    PREPARE_ABORTING = 5,
    PREPARE_DROP = 6,
    PREPARE_DROP_CLOSE = 7
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
    [[noreturn]] void progError(int line,
                                int cause,
                                const char * extra,
                                const char * check)
    {
      backup.progError(line, cause, extra, check);
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
                 Table_pool& tp,
                 BackupFile_pool& bp,
                 TriggerRecord_pool& trp)
      : slaveState(b, validSlaveTransitions, validSlaveTransitionsCount,1)
      , m_first_fragment(false)
      , m_num_fragments(0)
      , prepare_table(tp)
      , tables(tp)
      , triggers(trp), files(bp)
      , ctlFilePtr(RNIL)
      ,logFilePtr(RNIL)
      , masterData(b)
      , backup(b)
      , m_encrypted_file(false)
      {
        m_wait_end_lcp = false;
        m_wait_empty_queue = false;
        m_initial_lcp_started = false;
        m_wait_gci_to_delete = 0;
        localLcpId = 0;
        m_wait_data_file_close = false;
        m_disk_data_exist = false;
        m_wait_sync_extent = false;
        m_wait_final_sync_extent = false;
        m_wait_disk_data_sync = false;
        m_num_sync_pages_waiting = 0;
        m_num_sync_extent_pages_written = 0;
        /*
          report of backup status uses these variables to keep track
          if backup ia running and current state
        */
        m_gsn = 0;
        masterData.gsn = 0;
        m_informDropTabTableId = Uint32(~0);
        m_informDropTabReference = Uint32(~0);
        currentDeleteLcpFile = RNIL;
        noOfRecords = 0;
        noOfBytes = 0;
        for (Uint32 i = 0; i < BackupFormat::NDB_MAX_FILES_PER_LCP; i++)
        {
          dataFilePtr[i] = RNIL;
          prepareDataFilePtr[i] = RNIL;
        }
        idleFragWorkerCount = 0;
      }
    
    /* prev time backup status was reported */
    NDB_TICKS m_prev_report;

    bool m_wait_end_lcp;
    /**
     * DBLQH have requested us to report when LCP activity ceases.
     * If this variable is true we are waiting for delete file
     * queue to become empty to respond that LCP activity is idle.
     */
    bool m_wait_empty_queue;
    bool m_initial_lcp_started;
    Uint32 m_gsn;
    Uint32 m_lastSignalId;
    Uint32 m_prioA_scan_batches_to_execute;
    CompoundState slaveState; 

    bool m_first_fragment;
    Uint32 m_num_fragments;
    /**
     * Which header file is used for this LCP, there are only two 0 and 1.
     *
     * We record the startPart and the number of parts used in this LCP.
     *
     * We record the data file number used for this LCP.
     *
     * We record the newestGci that was part of this LCP, this value is
     * recorded in the header file to ensure that we use the proper set
     * of LCP files to restore.
     */
    Uint32 prepareNextLcpCtlFileNumber;
    Uint32 prepareDeleteCtlFileNumber;
    Uint32 prepareMaxGciWritten;
    PrepareState prepareState;
    Uint32 prepareFirstDataFileNumber;
    Uint32 preparePrevLcpId;
    Uint32 preparePrevLocalLcpId;
    Uint32 prepareErrorCode;
    Table_fifo prepare_table;
    Uint32 m_prepare_scan_change_gci;
    Uint32 m_prepare_first_start_part_in_lcp;
    Uint32 m_prepare_num_parts_in_lcp;
    Uint32 m_prepare_max_parts_in_lcp;
    BackupFormat::PartPair
      m_prepare_part_info[BackupFormat::NDB_MAX_LCP_PARTS];

    struct LcpScanInfo
    {
      Uint32 m_start_all_part;
      Uint32 m_num_all_parts;
      Uint32 m_start_change_part;
      Uint32 m_num_change_parts;
    };
    /**
     * Handle later, LCP processing.
     */
    Uint64 m_row_count;
    Uint64 m_prev_row_count;
    Uint64 m_row_change_count;
    Uint64 m_memory_used_in_bytes;
    bool   m_empty_lcp;
    bool   m_is_lcp_scan_active;
    bool   m_working_changed_row_page_flag;
    bool   m_current_changed_row_page_flag;
    Uint32 m_outstanding_operations;
    Uint32 m_first_start_part_in_lcp;
    Uint32 m_num_parts_in_this_lcp;
    Uint32 m_num_parts_in_lcp;
    Uint32 m_max_parts_in_lcp;
    Uint32 m_lcp_current_page_scanned;
    Uint32 m_lcp_max_page_cnt;
    Uint32 m_scan_change_gci;
    Uint32 m_lcp_remove_files;
    Uint32 m_num_lcp_files;
    Uint32 m_num_lcp_data_files_open;
    Uint32 m_first_data_file_number;
    Uint32 m_last_data_file_number;
    Uint32 m_current_data_file_ptr;
    Uint32 m_working_data_file_ptr;
    Uint64 m_current_lcp_lsn;

    Uint32 m_save_error_code;
    Uint32 m_change_page_alloc_after_start;
    Uint32 m_all_page_alloc_after_start;
    Uint32 m_change_page_alloc_dropped_after_start;
    Uint32 m_all_page_alloc_dropped_after_start;
    Uint32 m_change_page_dropped_A_after_start;
    Uint32 m_all_page_dropped_A_after_start;
    Uint32 m_change_page_dropped_D_after_start;
    Uint32 m_all_page_dropped_D_after_start;
    Uint32 m_skip_change_page_no_change;
    Uint32 m_skip_change_page_lcp_scanned_bit;
    Uint32 m_skip_all_page_lcp_scanned_bit;
    Uint32 m_skip_empty_change_page;
    Uint32 m_skip_empty_all_page;
    Uint32 m_record_empty_change_page_A;
    Uint32 m_record_late_alloc_change_page_A;
    Uint32 m_skip_late_alloc_change_page_D;
    Uint32 m_skip_late_alloc_all_page_A;
    Uint32 m_skip_late_alloc_all_page_D;
    Uint64 m_lcp_keep_row_change_pages;
    Uint64 m_lcp_keep_row_all_pages;
    Uint64 m_lcp_keep_delete_row_change_pages;
    Uint64 m_lcp_keep_delete_row_all_pages;
    Uint32 m_lcp_keep_delete_change_pages;
    Uint32 m_lcp_keep_delete_all_pages;

    Uint64 m_last_recorded_bytes_written;
    Uint64 m_pause_counter;
    Uint64 m_row_scan_counter;
    NDB_TICKS m_last_delay_scan_timer;
    NDB_TICKS m_scan_start_timer;

    Uint32 m_num_scan_req_on_prioa;

    bool m_any_lcp_page_ops;

    BackupFormat::PartPair m_part_info[BackupFormat::NDB_MAX_LCP_PARTS];
    LcpScanInfo m_scan_info[BackupFormat::NDB_MAX_FILES_PER_LCP];

    Uint32 m_informDropTabTableId;
    BlockReference m_informDropTabReference;

    Uint32 newestGci;
    Uint32 m_wait_gci_to_delete;
    Uint32 deleteCtlFileNumber;
    Uint32 deleteDataFileNumber;

    /* State variables for finalisation of LCP processing of a fragment. */
    bool m_disk_data_exist;
    bool m_wait_data_file_close;
    bool m_wait_disk_data_sync;
    bool m_wait_sync_extent;
    bool m_wait_final_sync_extent;
    bool m_lcp_lsn_synced;

    Uint32 m_num_sync_pages_waiting;
    Uint32 m_num_sync_extent_pages_written;
    /* Data for delete LCP file process */
    Uint32 deleteFilePtr;
    Uint32 currentDeleteLcpFile;
    bool m_delete_data_file_ongoing;

    Uint32 backupId; /* LCP id for LCPs, backupId for backups */
    Uint32 localLcpId; /* Local LCP id for local LCP (0 otherwise) */

    /**
     * Error code for backup and for execution phase of LCP.
     */
    Uint32 errorCode;
    /**
     * List of tables for backups, used during LCP execution phase, for
     * LCP it only contains one table, so can always be fetched using
     * the first call.
     */
    Table_fifo tables;

    /**
     * The below set of variables are only used by backups.
     */
    Uint32 signalNo;
 
    Uint32 clientRef;
    Uint32 clientData;
    Uint32 senderRef;
    Uint32 senderData;
    Uint32 flags;
    Uint32 backupKey[2];
    Uint32 masterRef;
    NdbNodeBitmask nodes;

    Bitmask<(Uint32)(MAX_NDBMT_LQH_THREADS/sizeof(Uint32))> fragWorkers[MAX_NDB_NODES];
    Uint32 idleFragWorkerCount;

    /**
     * Statistical variables for LCP and Backup, initialised when
     * starting LCP and incremented during LCP execution phase.
     */
    Uint64 noOfBytes;
    Uint64 noOfRecords;
    /* m_bytes_written is used for scheduling of LCP and Backups */
    Uint64 m_bytes_written;

    /**
     * Statistical variables for backups.
     */
    Uint64 noOfLogBytes;
    Uint64 noOfLogRecords;

    /**
     * Backup variables
     */
    Uint32 startGCP;
    Uint32 currGCP;
    Uint32 stopGCP;
    TriggerRecord_list triggers;


    /**
     * The list of file records used in this block by both
     * backups and LCPs.
     */
    BackupFile_list files;
    Uint32 ctlFilePtr;  // Ptr.i to ctl-file (LCP and Backup)
    Uint32 logFilePtr;  // Ptr.i to log-file (Only backup)
    Uint32 dataFilePtr[BackupFormat::NDB_MAX_FILES_PER_LCP];
      // Ptr.i to first data-file (LCP and Backup)
    Uint32 prepareDataFilePtr[BackupFormat::NDB_MAX_FILES_PER_LCP]; // Only LCP
    Uint32 prepareCtlFilePtr[2];  // Ptr.i to ctl-file for LCP prepare
    
    Uint32 backupDataLen;  // Used for (un)packing backup request
    SimpleProperties props;// Used for (un)packing backup request

    NDB_TICKS m_start_sync_op;
    NDB_TICKS m_high_res_lcp_start_time;

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

    void setPrepareErrorCode(Uint32 errCode){
      if(prepareErrorCode == 0)
	prepareErrorCode = errCode;
    }

    bool checkError() const {
      return errorCode != 0;
    }

    bool checkPrepareError() const {
      return prepareErrorCode != 0;
    }

    bool is_lcp() const {
      return backupDataLen == ~(Uint32)0;
    }

    Backup & backup;
    BlockNumber number() const { return backup.number(); }
    EmulatedJamBuffer *jamBuffer() const { return backup.jamBuffer(); }
    [[noreturn]] void progError(int line,
                                int cause,
                                const char * extra,
                                const char * check)
    {
      backup.progError(line, cause, extra, check);
    }

    bool m_encrypted_file;
    EncryptionKeyMaterial m_encryption_password_data;
  };
  friend struct BackupRecord;
  typedef Ptr<BackupRecord> BackupRecordPtr;
  typedef ArrayPool<BackupRecord> BackupRecord_pool;
  typedef SLList<BackupRecord_pool> BackupRecord_sllist;
  typedef DLList<BackupRecord_pool> BackupRecord_dllist;

/**
 * Number of words needed in buff to start a new scan batch
 * (Which can directly write a number of rows of max size
 *  into the buffer)
 */
#define BACKUP_MIN_BUFF_WORDS (ZRESERVED_SCAN_BATCH_SIZE *   \
                               (MAX_TUPLE_SIZE_IN_WORDS +    \
                                MAX_ATTRIBUTES_IN_TABLE +    \
                                128))

#define MAX_BUFFER_USED_WITHOUT_REDO_ALERT (512 * 1024)
#define BACKUP_DEFAULT_WRITE_SIZE (256 * 1024)
#define BACKUP_DEFAULT_BUFFER_SIZE (2 * 1024 * 1024)
#define BACKUP_DEFAULT_LOGBUFFER_SIZE (16 * 1024 * 1024)

  struct Config {
    Uint32 m_dataBufferSize;
    Uint32 m_logBufferSize;
    Uint32 m_minWriteSize;
    Uint32 m_maxWriteSize;
    Uint32 m_lcp_buffer_size;
    
    Uint64 m_disk_write_speed_min;
    Uint64 m_disk_write_speed_max;
    Uint64 m_disk_write_speed_max_other_node_restart;
    Uint64 m_disk_write_speed_max_own_restart;
    Uint32 m_backup_disk_write_pct;
    Uint32 m_disk_synch_size;
    Uint32 m_diskless;
    Uint32 m_o_direct;
    Uint32 m_compressed_backup;
    Uint32 m_compressed_lcp;
    Uint32 m_encryption_required;
  };
  
  /**
   * Variables
   */
  Uint32 * c_startOfPages;
  /**
   * Map from tableId to tabPtr.i to speed up findTable
   * If the same table is mapped to several backups we will
   * look for the table with the correct backupPtr.
   */
  Uint32 * c_tableMap;
  Uint32 c_tableMapSize;
  NodeId c_masterNodeId;
  Node_list c_nodes;
  NdbNodeBitmask c_aliveNodes;
  BackupRecord_dllist c_backups;
  Config c_defaults;

  bool c_encrypted_filesystem;

  /*
    Variables that control checkpoint to disk speed
  */
  bool m_is_lcp_running;
  bool m_is_backup_running;
  bool m_is_any_node_restarting;
  bool m_node_restart_check_sent;
  bool m_our_node_started;
  Uint64 m_curr_disk_write_speed;
  Uint64 m_curr_backup_disk_write_speed;
  Uint64 m_words_written_this_period;
  Uint64 m_backup_words_written_this_period;
  Uint64 m_overflow_disk_write;
  Uint64 m_backup_overflow_disk_write;
  Uint32 m_reset_delay_used;
  NDB_TICKS m_reset_disk_speed_time;

//#ifdef VM_TRACE
  Uint64 m_debug_redo_log_count;
//#endif

  /* Keep track of disk data usage in checkpoints */
  Uint64 m_current_dd_time_us;
  Uint32 m_last_lcp_dd_percentage;
  Uint32 m_undo_log_level_percentage;
  Uint32 m_max_undo_log_level_percentage;

  RedoStateRep::RedoAlertState m_redo_alert_state;
  RedoStateRep::RedoAlertState m_local_redo_alert_state;
  RedoStateRep::RedoAlertState m_global_redo_alert_state;
  Uint32 m_redo_alert_factor;
  BackupRecordPtr m_lcp_ptr;

  NDB_TICKS m_lcp_start_time;
  NDB_TICKS m_prev_lcp_start_time;
  NDB_TICKS m_lcp_current_cut_point;
  Uint64 m_last_redo_used_in_bytes;
  Uint64 m_last_lcp_exec_time_in_ms;
  Uint64 m_max_redo_speed_per_sec;
  Uint64 m_update_size_lcp[2];
  Uint64 m_update_size_lcp_last;
  Uint64 m_insert_size_lcp[2];
  Uint64 m_insert_size_lcp_last;
  Uint64 m_delete_size_lcp[2];
  Uint64 m_delete_size_lcp_last;
  Uint64 m_proposed_disk_write_speed;
  Uint64 m_lcp_change_rate;
  Uint64 m_lcp_timing_factor;
  Int64 m_lcp_lag[2];
  Uint32 m_lcp_timing_counter;
  Uint32 m_redo_percentage;
  Uint32 m_max_redo_percentage;
  bool m_first_lcp_started;

  void init_lcp_timers(Uint64);
  void calculate_seconds_since_lcp_cut(Uint64& seconds_since_lcp_cut);
  Uint64 init_change_size(Uint64 update_size,
                          Uint64 insert_size,
                          Uint64 delete_size,
                          Uint64 total_memory);
  Uint64 modify_change_size(Uint64 update_size,
                            Uint64 insert_size,
                            Uint64 delete_size,
                            Uint64 total_size,
                            Uint64 change_size);
  Uint32 calculate_parts(Uint64 total_size,
                         Uint64 total_memory);
  Uint64 calculate_change_rate(Uint64 change_size,
                               Uint64& seconds_since_lcp_cut);
  Uint64 calculate_checkpoint_rate(Uint64 update_size,
                                   Uint64 insert_size,
                                   Uint64 delete_size,
                                   Uint64 total_memory,
                                   Uint64& seconds_since_lcp_cut);
  void calculate_redo_parameters(Uint64 redo_usage,
                                 Uint64 redo_size,
                                 Uint64 redo_written_since_last_call,
                                 Uint64 millis_since_last_call,
                                 Uint64& redo_percentage,
                                 Uint64& max_redo_used_before_cut,
                                 Uint64& mean_redo_used_before_cut,
                                 Uint64& mean_redo_speed_per_sec,
                                 Uint64& current_redo_speed_per_sec,
                                 Uint64& redo_available);
  void change_alert_state_redo_percent(Uint64 redo_percentage);
  void change_alert_state_redo_usage(Uint64 max_redo_used_before_cut,
                                     Uint64 mean_redo_used_before_cut,
                                     Uint64 redo_available);
  void handle_global_alert_state(Signal *signal,
    RedoStateRep::RedoAlertState save_redo_alert_state);
  void set_redo_alert_factor(Uint64 redo_percentage);
  void set_lcp_timing_factors(Uint64 seconds_since_lcp_cut);
  void reset_lcp_timing_factors();
  void set_proposed_disk_write_speed(Uint64 current_redo_speed_per_sec,
                                     Uint64 mean_redo_speed_per_sec,
                                     Uint64 seconds_since_lcp_cut);
  void measure_change_speed(Signal*, Uint64 millis_since_last_call);
  void debug_report_redo_control(Uint32);
  void lcp_start_point(Signal*);
  bool lcp_end_point(BackupRecordPtr);
  Uint64 calculate_proposed_disk_write_speed();

  Uint32 m_curr_lcp_id;

  /**
   * We check the use of disk write speed limits every 100 milliseconds. The
   * speed check parameters is also in words, so this means to get the current
   * speed in bytes per second we need to multiply with 40.
   */
  static const int  DISK_SPEED_CHECK_DELAY = 100;
  static const int CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS = 40;
  
  Uint64 m_monitor_words_written;
  Uint64 m_backup_monitor_words_written;
  Uint32 m_periods_passed_in_monitor_period;
  NDB_TICKS m_monitor_snapshot_start;

  /**
   * A number of statistical variables that keep track of
   * various events and how often they happen.
   */
  Uint64 slowdowns_due_to_io_lag;
  Uint64 slowdowns_due_to_high_cpu;
  Uint64 slowdown_backups_due_to_high_cpu;
  Uint64 disk_write_speed_set_to_min;
  Uint64 backup_disk_write_speed_set_to_min;

  /**
   * Variables used to keep stats on disk write speeds for
   * reporting in checkpoint_speed ndbinfo table.
   * We keep the last 60 seconds of stats and use this to
   * calculate various aggregates reported in the ndbinfo
   * table.
   *
   * The idea is that next_disk_write_speed_report specifies
   * the next entry to fill in a speed report into. The
   * last_disk_write_speed_report points to the oldest one
   * that we have written so far. At first we write into
   * index 0, so in the beginning is last_disk_write_speed_report
   * equal to 0 and next_disk_write_speed_report is pointing to
   * the next one to write into. When we write into the last
   * entry (index = 60) then we have written in all entries and
   * we move the last forward. After that we will always have
   * last one ahead of next. Since this means that the next
   * to write isn't available (although it isn't written yet)
   * we have 61 entries in the array to cover 60 seconds of
   * time.
   */
#define DISK_WRITE_SPEED_REPORT_SIZE 61

#define MILLIS_IN_A_SECOND 1000
#define MILLIS_ADJUST_FOR_EARLY_REPORT 20
  struct DiskWriteSpeedReport
  {
    Uint64 backup_lcp_bytes_written;
    Uint64 backup_bytes_written;
    Uint64 redo_bytes_written;
    Uint64 target_disk_write_speed;
    Uint64 target_backup_disk_write_speed;
    Uint64 millis_passed;
  };
  DiskWriteSpeedReport disk_write_speed_rep[DISK_WRITE_SPEED_REPORT_SIZE];
  Uint32 last_disk_write_speed_report;
  Uint32 next_disk_write_speed_report;

  /**
   * Methods used in control of checkpoint speed
   */
  void handle_overflow(Uint64& overflow_disk_write,
                       Uint64& words_written_this_period,
                       Uint64& curr_disk_write_speed);
  void calculate_next_delay(const NDB_TICKS curr_time);
  void monitor_disk_write_speed(const NDB_TICKS curr_time,
                                const Uint64 millisPassed);
  void calculate_current_speed_bounds(Uint64& max_speed,
                                      Uint64& max_backup_speed,
                                      Uint64& min_speed);
  void adjust_disk_write_speed_down(Uint64& curr_disk_write_speed,
                                    Uint64& disk_speed_set_to_min,
                                    Uint64 min_speed,
                                    int adjust_speed);
  void adjust_disk_write_speed_up(Uint64& curr_disk_write_speed,
                                  Uint64 max_speed,
                                  int adjust_speed);
  void calculate_disk_write_speed(Signal *signal);
  void send_next_reset_disk_speed_counter(Signal *signal);

  void restore_disk_write_speed_numbers(void);
  void calculate_real_disk_write_speed_parameters(void);
  Uint64 get_new_speed_val32(Signal *signal);
  Uint64 get_new_speed_val64(Signal *signal);

  /**
   * Methods used in ndbinfo reporting of checkpoint speed.
   */
  void report_disk_write_speed_report(Uint64 bytes_written_this_period,
                                      Uint64 backup_bytes_written_this_period,
                                      Uint64 millis_passed);
  Uint32 get_disk_write_speed_record(Uint32 start_index);
  Uint64 calculate_millis_since_finished(Uint32 start_index);
  void calculate_disk_write_speed_seconds_back(Uint32 seconds_back,
                                       Uint64 & millis_passed,
                                       Uint64 & backup_lcp_bytes_written,
                                       Uint64 & backup_bytes_written,
                                       Uint64 & redo_bytes_written,
                                       bool at_least_one = false);
  void calculate_std_disk_write_speed_seconds_back(Uint32 seconds_back,
                             Uint64 millis_passed_total,
                             Uint64 backup_lcp_bytes_written_total,
                             Uint64 backup_bytes_written_total,
                             Uint64 redo_bytes_written_total,
                             Uint64 & std_dev_backup_lcp_in_bytes_per_sec,
                             Uint64 & std_dev_backup_in_bytes_per_sec,
                             Uint64 & std_dev_redo_in_bytes_per_sec);


  static constexpr Uint32 NO_OF_PAGES_META_FILE = 
	       (2*MAX_WORDS_META_FILE + BACKUP_WORDS_PER_PAGE - 1) /
	       BACKUP_WORDS_PER_PAGE;

  Uint32 m_backup_report_frequency;

  bool c_initial_start_lcp_not_done_yet;

  unsigned char c_part_array[BackupFormat::NDB_MAX_LCP_PARTS * 4];
  /**
   * Pools
   */
  Table_pool c_tablePool;
  BackupRecord_pool c_backupPool;
  BackupFile_pool c_backupFilePool;
  Page32_pool c_pagePool;
  Fragment_pool c_fragmentPool;
  Node_pool c_nodePool;
  TriggerRecord_pool c_triggerPool;
  ArrayPool<DeleteLcpFile> c_deleteLcpFilePool;

  void checkFile(Signal*, BackupFilePtr);
  void checkScan(Signal*, BackupRecordPtr, BackupFilePtr, bool);
  bool check_new_scan(BackupRecordPtr ptr, OperationRecord &op, bool);
  bool check_min_buf_size(BackupRecordPtr ptr, OperationRecord &op);
  bool check_frag_complete(BackupRecordPtr ptr, BackupFilePtr filePtr);
  bool check_error(BackupRecordPtr ptr, BackupFilePtr filePtr);
  void fragmentCompleted(Signal*, BackupFilePtr, Uint32 errCode = 0);
  
  void backupAllData(Signal* signal, BackupRecordPtr);
  
  void getFragmentInfo(Signal*, BackupRecordPtr, TablePtr, Uint32 fragNo);
  void getFragmentInfoDone(Signal*, BackupRecordPtr);
  
  void openFiles(Signal* signal, BackupRecordPtr ptr);
  void openFilesReply(Signal*, BackupRecordPtr ptr, BackupFilePtr);
  void openFilesReplyLCP(Signal*, BackupRecordPtr ptr, BackupFilePtr);
  void closeFiles(Signal*, BackupRecordPtr ptr);
  void closeFile(Signal*,
                 BackupRecordPtr,
                 BackupFilePtr,
                 bool prepare_phase = false,
                 bool remove_flag = false);
  void closeFilesDone(Signal*, BackupRecordPtr ptr);  

  void init_file(BackupFilePtr, Uint32);

  void sendDefineBackupReq(Signal *signal, BackupRecordPtr ptr);

  void defineBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId);
  void createTrigReply(Signal* signal, BackupRecordPtr ptr);
  void alterTrigReply(Signal* signal, BackupRecordPtr ptr);
  void startBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32);
  void stopBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId);
  
  void defineBackupRef(Signal*, BackupRecordPtr, Uint32 errCode = 0);
  void backupFragmentRef(Signal * signal, BackupFilePtr filePtr);

  void nextFragment(Signal*, BackupRecordPtr);
  void release_tables(BackupRecordPtr);
  
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

  void init_scan_prio_level(Signal *signal, BackupRecordPtr ptr);
  bool check_scan_if_raise_prio(Signal *signal, BackupRecordPtr ptr);

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
  bool findTable(const BackupRecordPtr &, TablePtr &, Uint32 tableId);
  void insertTableMap(TablePtr &, Uint32 backupPtrI, Uint32 tableId);
  void removeTableMap(TablePtr &, Uint32 backupPtrI, Uint32 tableId);
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
  /* Check timers for reporting at certain points */
  void checkReportStatus(Signal* signal, BackupRecordPtr ptr);
  /* Send backup status, invoked either periodically, or explicitly */
  void reportStatus(Signal* signal, BackupRecordPtr ptr,
                    BlockReference ref = CMVMI_REF);

  void sendSTTORRY(Signal*);
  void createSequence(Signal* signal);
  void createSequenceReply(Signal*, class UtilSequenceConf *);

  void lcp_close_ctl_file_drop_case(Signal*, BackupRecordPtr);
  void finish_end_lcp(Signal*, BackupRecordPtr);
  bool check_if_in_page_range(Uint32 part_id,
                              Uint32 start_part,
                              Uint32 num_parts);
  void lcp_read_ctl_file(Page32Ptr, Uint32, BackupRecordPtr);
  void lcp_write_ctl_file(Signal*, BackupRecordPtr);
  void lcp_write_ctl_file_to_disk(Signal*, BackupFilePtr, Page32Ptr);
  void lcp_init_ctl_file(Page32Ptr pagePtr);
  Uint32 compress_part_pairs(struct BackupFormat::LCPCtlFile*,
                             Uint32 numPartPairs,
                             Uint32 file_size);
  Uint32 decompress_part_pairs(struct BackupFormat::LCPCtlFile*,
                               Uint32,
                               struct BackupFormat::PartPair*);

  bool convert_ctl_page_to_host(struct BackupFormat::LCPCtlFile*);
  void convert_ctl_page_to_network(Uint32*, Uint32 file_size);

  void handle_idle_lcp(Signal*, BackupRecordPtr);
  Uint64 get_total_memory();
  Uint64 calculate_row_change_count(BackupRecordPtr);
  Uint32 calculate_min_parts(Uint64 row_count,
                             Uint64 row_change_count,
                             Uint64 mem_used,
                             Uint64 total_mem);
  void calculate_number_of_parts(BackupRecordPtr);
  void copy_lcp_info_from_prepare(BackupRecordPtr);
  void copy_prev_lcp_info(BackupRecordPtr,
                          struct BackupFormat::LCPCtlFile *lcpCtlFilePtr);
  void start_lcp_scan(Signal *signal,
                      BackupRecordPtr ptr,
                      TablePtr tabPtr,
                      Uint32 ptrI,
                      Uint32 fragNo);
  Uint32 get_part_add(Uint32 start_part, Uint32 num_parts);
  Uint32 get_file_add(Uint32 start_file, Uint32 num_files);
  Uint32 get_file_sub(Uint32 start_file, Uint32 num_files);


  void prepare_ranges_for_parts(BackupRecordPtr, Uint32 parts);
  void prepare_parts_for_lcp(Signal*, BackupRecordPtr);
  void prepare_new_part_info(BackupRecordPtr, Uint32);
  void lcp_swap_tables(BackupRecordPtr, TablePtr&, Uint32);
  void lcp_swap_data_file(BackupRecordPtr);
  void lcp_swap_ctl_file(BackupRecordPtr);
  void lcp_set_lcp_id(BackupRecordPtr,
                      struct BackupFormat::LCPCtlFile*);
  void lcp_copy_ctl_page(BackupRecordPtr);
  void lcp_update_ctl_page(BackupRecordPtr,
                               Page32Ptr&,
                               BackupFilePtr&);
  Uint32 lcp_pre_sync_lsn(BackupRecordPtr);
  void start_execute_lcp(Signal* signal,
                         BackupRecordPtr ptr,
                         TablePtr & tabPtr,
                         Uint32 tableId);
  void lcp_open_ctl_file(Signal*, BackupRecordPtr, Uint32 lcpNo);
  void lcp_open_ctl_file_done(Signal*, BackupRecordPtr, BackupFilePtr);
  void lcp_read_ctl_file_done(Signal* signal, BackupRecordPtr);
  void lcp_close_ctl_file(Signal*, BackupRecordPtr, Uint32 closeLcpNo);
  void lcp_close_prepare_ctl_file_done(Signal*, BackupRecordPtr);
  void lcp_read_ctl_page(BackupFilePtr, Page32Ptr&);
  void lcp_open_data_file(Signal*, BackupRecordPtr);
  void lcp_open_data_file_late(Signal*, BackupRecordPtr, Uint32 index);
  void lcp_open_data_file_done(Signal*, BackupRecordPtr);
  void lcp_close_data_file(Signal*, BackupRecordPtr, bool remove_flag);
  void lcp_close_data_file_conf(Signal* signal, BackupRecordPtr);
  void read_lcp_descriptor(Signal*, BackupRecordPtr, TablePtr);
  void lcp_start_complete_processing(Signal *signal, BackupRecordPtr ptr);
  void sync_page_cache_callback(Signal*, Uint32 ptrI, Uint32 res);
  void finalize_lcp_processing(Signal*, BackupRecordPtr);
  void lcp_one_part_completed(Signal*, BackupRecordPtr);
  void lcp_write_undo_log(Signal *signal, BackupRecordPtr);

  void check_wait_end_lcp(Signal*, BackupRecordPtr ptr);
  void check_empty_queue_waiters(Signal*, BackupRecordPtr ptr);
  void delete_lcp_file_processing(Signal*);
  void finished_removing_files(Signal*, BackupRecordPtr);
  void handleEND_LCPCONF(Signal*, BackupRecordPtr);
  void sendEND_LCPCONF(Signal*, BackupRecordPtr);
  void checkEND_LCPCONF(Signal*, BackupRecordPtr, bool ready);
  void check_pgman_prep_lcp_ready(Signal *signal, Uint32 ptrI);
  void send_firstSYNC_EXTENT_PAGES_REQ(Signal*, BackupRecordPtr);
  void sendINFORM_BACKUP_DROP_TAB_CONF(Signal*, BackupRecordPtr);

  void sync_log_lcp_lsn(Signal*, DeleteLcpFilePtr, Uint32 ptrI);
  void sync_log_lcp_lsn_callback(Signal*, Uint32 ptrI, Uint32 unused);
  void lcp_open_ctl_file_for_rewrite(Signal*,
                                     DeleteLcpFilePtr,
                                     BackupRecordPtr);
  void lcp_open_ctl_file_for_rewrite_done(Signal*, BackupFilePtr);
  void lcp_read_ctl_file_for_rewrite(Signal*, BackupFilePtr);
  void lcp_read_ctl_file_for_rewrite_done(Signal*, BackupFilePtr);
  void lcp_update_ctl_file_for_rewrite(Signal*, BackupFilePtr, Page32Ptr);
  void lcp_update_ctl_file_for_rewrite_done(Signal*,
                                            BackupRecordPtr,
                                            BackupFilePtr);
  void lcp_close_ctl_file_for_rewrite(Signal*, BackupRecordPtr, BackupFilePtr);
  void lcp_close_ctl_file_for_rewrite_done(Signal*,
                                           BackupRecordPtr,
                                           BackupFilePtr);

  void lcp_remove_file(Signal*,
                       BackupRecordPtr,
                       DeleteLcpFilePtr);
  void lcp_remove_file_conf(Signal*, BackupRecordPtr ptr);

  bool ready_to_write(bool ready,
                      Uint32 sz,
                      bool eof,
                      BackupFile *fileP,
                      BackupRecord* ptrP);

  void afterGetTabinfoLockTab(Signal *signal,
                              BackupRecordPtr ptr, TablePtr tabPtr);
  void cleanupNextTable(Signal *signal, BackupRecordPtr ptr, TablePtr tabPtr);

  BackupFormat::LogFile::LogEntry* get_log_buffer(Signal*,TriggerPtr, Uint32);

  /*
   * MT LQH.  LCP runs separately in each instance number.
   * BACKUP uses instance key 1 (real instance 0 or 1) as master.
  */
  static constexpr Uint32 NdbdInstanceKey = 0;
  static constexpr Uint32 BackupProxyInstanceKey = 0;
  static constexpr Uint32 UserBackupInstanceKey = 1;
  /*
   * instanceNo() is used for routing backup control signals and has 3
   * use cases:
   * - LCP: return own instance ID, i.e route signal to self
   * - multi-threaded backup: return instance of BackupProxy, which
           forwards signal to all instances, i.e. route signal to all instances
   * - single-threaded backup: return instance 1, i.e. route signal to LDM1
   */
  Uint32 instanceNo(BackupRecordPtr ptr)
  {
     return ptr.p->is_lcp() ?
            instance() : (ptr.p->flags & BackupReq::MT_BACKUP) ?
            BackupProxyInstanceKey : UserBackupInstanceKey;
  }

  /* map a fragment to an LDM
   * single-threaded backup: assign fragment to LDM1
   * multithreaded backup: assign fragment to LDM which owns it
   */
  Uint32 mapFragToLdm(BackupRecordPtr ptr,
                      Uint32 ownerNode,
                      Uint32 instanceKey)
  {
    // instance key is 1..n and may be larger than actual number of ldms.
    // To ensure we only schedule one fragment per actual ldm at a time, we
    // use node information to determine actual ldm which will process request.
    Uint32 instanceNo = getInstanceNo(ownerNode, instanceKey);
    return (ptr.p->flags & BackupReq::MT_BACKUP) ?
            instanceNo : UserBackupInstanceKey;
  }

  bool is_backup_worker()
  {
    return isNdbMtLqh() ? (instance() == UserBackupInstanceKey) :  true;
  }
  /*
   * Select master instance on any node: LDM1 for ndbmtd, LDM0 for ndbd
   * Used in node-failure aborts when a participant node is promoted to master
   */
  Uint32 masterInstanceKey(BackupRecordPtr ptr) {
     return isNdbMtLqh() ?
            UserBackupInstanceKey : NdbdInstanceKey;
  }


  /**
   * Ugly shared state to allow different worker instances
   * to detect that a backup is going, although they are
   * not participating.
   * Modified by the instance performing backup
   */  
  static bool g_is_single_thr_backup_running;

  void get_page_info(BackupRecordPtr,
                     Uint32 part_id,
                     Uint32 & scanGCI,
                     bool & changed_row_page_flag);
  void set_working_file(BackupRecordPtr,
                        Uint32 part_id,
                        bool is_all_rows_page);
  void init_file_for_lcp(Signal*, Uint32 index, BackupRecordPtr, Uint32 ptrI);
  bool is_all_rows_page(BackupRecordPtr, Uint32 part_id);
  void change_current_page_temp(Uint32 page_no);
  void restore_current_page(BackupRecordPtr ptr);

  void setRestorableGci(Uint32);
  Uint32 getRestorableGci();

  void set_undo_log_level(Uint32 percentage_used);

  bool check_pause_lcp_backup(BackupRecordPtr ptr,
                              bool is_lcp,
                              bool is_send_scan_next_req);
  bool check_pause_lcp_backup(BackupRecordPtr ptr);
  bool check_pause_lcp();
  void update_pause_lcp_counter(Uint32 loop_count);
  void pausing_lcp(Uint32 place, Uint32 val);
  void get_lcp_record(BackupRecordPtr &ptr);
  bool get_backup_record(BackupRecordPtr &ptr);
public:
  bool is_change_part_state(Uint32 page_id);
  Uint32 get_max_words_per_scan_batch(Uint32, Uint32&, Uint32, Uint32);
};

inline
Uint32
Backup::getRestorableGci()
{
  return m_newestRestorableGci;
}

inline
void
Backup::OperationRecord::set_scanned_pages(Uint32 num_pages_scanned)
{
  lcpScannedPages = num_pages_scanned;
}

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

 
#define ZMAX_WORDS_PER_SCAN_BATCH_LOW_PRIO 1600
#define ZMAX_WORDS_PER_SCAN_BATCH_HIGH_PRIO 8000
inline
bool
Backup::check_pause_lcp()
{
  return check_pause_lcp_backup(m_lcp_ptr, true, false);
}

inline
bool
Backup::check_pause_lcp_backup(BackupRecordPtr ptr)
{
  return check_pause_lcp_backup(ptr, ptr.p->is_lcp(), true);
}

inline
Uint32
Backup::get_max_words_per_scan_batch(Uint32 prioAFlag,
                                     Uint32 & wordsWritten,
                                     Uint32 is_lcp,
                                     Uint32 ptrI)
{
  if (prioAFlag == 0)
    return (wordsWritten >= ZMAX_WORDS_PER_SCAN_BATCH_LOW_PRIO);
  else
  {
    bool ret_val;
    if (is_lcp)
      ret_val = check_pause_lcp();
    else
      ret_val = (wordsWritten >= ZMAX_WORDS_PER_SCAN_BATCH_HIGH_PRIO);
    if (ret_val)
      wordsWritten = 0;
    return ret_val;
  }
}

#undef JAM_FILE_ID

#endif
