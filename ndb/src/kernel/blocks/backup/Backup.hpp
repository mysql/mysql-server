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

#ifndef BACKUP_H
#define BACKUP_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>

#include "FsBuffer.hpp"
#include "BackupFormat.hpp"

#include <NodeBitmask.hpp>
#include <SimpleProperties.hpp>

#include <SLList.hpp>
#include <ArrayList.hpp>
#include <SignalCounter.hpp>
#include <blocks/mutexes.hpp>

#include <NdbTCP.h>

/**
 * Backup - This block manages database backup and restore
 */
class Backup : public SimulatedBlock
{
public:
  Backup(const Configuration & conf);
  virtual ~Backup();
  BLOCK_DEFINES(Backup);
  
protected:

  void execSTTOR(Signal* signal);
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
  void execSTOP_BACKUP_REQ(Signal* signal);
  void execBACKUP_STATUS_REQ(Signal* signal);
  void execABORT_BACKUP_ORD(Signal* signal);
 
  /**
   * The actual scan
   */
  void execSCAN_HBREP(Signal* signal);
  void execTRANSID_AI(Signal* signal);
  void execKEYINFO20(Signal* signal);
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
  void execCREATE_TRIG_REF(Signal* signal);
  void execCREATE_TRIG_CONF(Signal* signal);
  void execALTER_TRIG_REF(Signal* signal);
  void execALTER_TRIG_CONF(Signal* signal);
  void execDROP_TRIG_REF(Signal* signal);
  void execDROP_TRIG_CONF(Signal* signal);

  /**
   * DIH signals
   */
  void execDI_FCOUNTCONF(Signal* signal);
  void execDIGETPRIMCONF(Signal* signal);

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
    Uint32 data[BACKUP_WORDS_PER_PAGE];
    Uint32 nextPool;
  };
  typedef Ptr<Page32> Page32Ptr;

  struct Attribute {
    struct Data {
      Uint8 nullable;
      Uint8 fixed;
      Uint8 key; 
      Uint8 unused;
      Uint32 sz32;       // No of 32 bit words
      Uint32 offset;     // Relative DataFixedAttributes/DataFixedKeys
      Uint32 offsetNull; // In NullBitmask
    } data;
    Uint32 nextPool;
  };
  typedef Ptr<Attribute> AttributePtr;
  
  struct Fragment {
    Uint32 tableId;
    Uint32 node;
    Uint16 scanned;  // 0 = not scanned x = scanned by node x
    Uint16 scanning; // 0 = not scanning x = scanning on node x
    Uint32 nextPool;
  };
  typedef Ptr<Fragment> FragmentPtr;

  struct Table {
    Table(ArrayPool<Attribute> &, ArrayPool<Fragment> &);
    
    Uint32 tableId;
    Uint32 schemaVersion;
    Uint32 frag_mask;
    Uint32 tableType;
    Uint32 noOfNull;
    Uint32 noOfKeys;
    Uint32 noOfAttributes;
    Uint32 noOfVariable;
    Uint32 sz_FixedKeys;
    Uint32 sz_FixedAttributes;
    Uint32 variableKeyId;
    Uint32 triggerIds[3];
    bool   triggerAllocated[3];
    
    Array<Attribute> attributes;
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
    inline Uint32 getFixedKeySize() const { return sz_FixedKeys; }
    
    /**
     * Once per fragment
     */
    bool newFragment(Uint32 tableId, Uint32 fragNo);
    bool fragComplete(Uint32 tableId, Uint32 fragNo);
    
    /**
     * Once per scan frag (next) req/conf
     */
    bool newScan();
    bool scanConf(Uint32 noOfOps, Uint32 opLen);
    
    /**
     * Per record
     */
    void newRecord(Uint32 * base);
    bool finished();
    
    /**
     * Per attribute
     */
    Uint32 * newKey();
    void     nullAttribute(Uint32 nullOffset);
    Uint32 * newNullable(Uint32 attrId, Uint32 sz);
    Uint32 * newAttrib(Uint32 offset, Uint32 sz);
    Uint32 * newVariable(Uint32 id, Uint32 sz);
    Uint32 * newVariableKey(Uint32 sz);
    
  private:
    Uint32* base; 
    Uint32* dst_Length;
    Uint32* dst_Bitmask;
    Uint32* dst_FixedKeys;
    Uint32* dst_FixedAttribs;
    BackupFormat::DataFile::VariableData* dst_VariableData;
    
    Uint32 noOfAttributes; // No of Attributes
    Uint32 variableKeyId;  // Id of variable key
    Uint32 attrLeft;       // No of attributes left

    Uint32 opNoDone;
    Uint32 opNoConf;
    Uint32 opLen;

  public:
    Uint32* dst;
    Uint32 attrSzLeft;  // No of words missing for current attribute
    Uint32 attrSzTotal; // No of AI words received
    Uint32 tablePtr;    // Ptr.i to current table

    FsBuffer dataBuffer;
    Uint32 noOfRecords;
    Uint32 noOfBytes;
    Uint32 maxRecordSize;
    
  private:
    Uint32* scanStart;
    Uint32* scanStop;

    /**
     * sizes of part
     */
    Uint32 sz_Bitmask;
    Uint32 sz_FixedKeys;
    Uint32 sz_FixedAttribs;

  public:
    union { Uint32 nextPool; Uint32 nextList; };
    Uint32 prevList;
  private:

    Backup & backup;
    BlockNumber number() const { return backup.number(); }
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
      : operation(backup),  pages(pp) {}
    
    Uint32 backupPtr; // Pointer to backup record
    Uint32 tableId;
    Uint32 fragmentNo;
    Uint32 filePointer;
    Uint32 errorCode;
    BackupFormat::FileType fileType;
    OperationRecord operation;
    
    Array<Page32> pages;
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };
    
    Uint8 fileOpened;
    Uint8 fileRunning;
    Uint8 fileDone;
    Uint8 scanRunning;
  }; 
  typedef Ptr<BackupFile> BackupFilePtr;
 

  /**
   * State for BackupRecord
   */
  enum State {
    INITIAL,
    DEFINING, // Defining backup content and parameters
    DEFINED,  // DEFINE_BACKUP_CONF sent in slave, received all in master
    STARTED,  // Creating triggers
    SCANNING, // Scanning fragments
    STOPPING, // Closing files
    CLEANING, // Cleaning resources
    ABORTING  // Aborting backup
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
    BackupRecord(Backup& b, ArrayPool<Page32> & pp, 
		 ArrayPool<Table> & tp, 
		 ArrayPool<BackupFile> & bp,
		 ArrayPool<TriggerRecord> & trp) 
      : slaveState(b, validSlaveTransitions, validSlaveTransitionsCount,1)
      , tables(tp), triggers(trp), files(bp), pages(pp)
      , masterData(b, validMasterTransitions, validMasterTransitionsCount)
	, backup(b)
    {
      closingFiles    = false;
      okToCleanMaster = true;
    }
    
    CompoundState slaveState; 
    
    Uint32 clientRef;
    Uint32 clientData;
    Uint32 backupId;
    Uint32 backupKey[2];
    Uint32 masterRef;
    Uint32 errorCode;
    NdbNodeBitmask nodes;
    
    bool okToCleanMaster;
    bool closingFiles;

    Uint64 noOfBytes;
    Uint64 noOfRecords;
    Uint64 noOfLogBytes;
    Uint64 noOfLogRecords;
    
    Uint32 startGCP;
    Uint32 currGCP;
    Uint32 stopGCP;
    DLList<Table> tables;
    SLList<TriggerRecord> triggers;
    
    SLList<BackupFile> files; 
    Uint32 ctlFilePtr;  // Ptr.i to ctl-file
    Uint32 logFilePtr;  // Ptr.i to log-file
    Uint32 dataFilePtr; // Ptr.i to first data-file
    
    Uint32 backupDataLen;  // Used for (un)packing backup request
    Array<Page32> pages;   // Used for (un)packing backup request
    SimpleProperties props;// Used for (un)packing backup request
    
    struct MasterData {
      MasterData(Backup & b, const State valid[], Uint32 count) 
	: state(b, valid, count, 0) 
      {
      }
      MutexHandle2<BACKUP_DEFINE_MUTEX> m_defineBackupMutex;
      MutexHandle2<DICT_COMMIT_TABLE_MUTEX> m_dictCommitTableMutex;

      Uint32 gsn;
      CompoundState state;
      SignalCounter sendCounter;
      Uint32 errorCode;
      struct {
	Uint32 tableId;
      } createTrig;
      struct {
	Uint32 tableId;
      } dropTrig;
      struct {
	Uint32 tableId;
      } alterTrig;
      union {
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

    Backup & backup;
    BlockNumber number() const { return backup.number(); }
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
  Uint32 m_diskless;

  STATIC_CONST(NO_OF_PAGES_META_FILE = 2);

  /**
   * Pools
   */
  ArrayPool<Table> c_tablePool;
  ArrayPool<Attribute> c_attributePool;  
  ArrayPool<BackupRecord> c_backupPool;
  ArrayPool<BackupFile> c_backupFilePool;
  ArrayPool<Page32> c_pagePool;
  ArrayPool<Fragment> c_fragmentPool;
  ArrayPool<Node> c_nodePool;
  ArrayPool<TriggerRecord> c_triggerPool;

  Uint32 calculate_frag_mask(Uint32);

  void checkFile(Signal*, BackupFilePtr);
  void checkScan(Signal*, BackupFilePtr);
  void fragmentCompleted(Signal*, BackupFilePtr);
  
  void backupAllData(Signal* signal, BackupRecordPtr);
  
  void getFragmentInfo(Signal*, BackupRecordPtr, TablePtr, Uint32 fragNo);
  void getFragmentInfoDone(Signal*, BackupRecordPtr);
  
  void openFiles(Signal* signal, BackupRecordPtr ptr);
  void openFilesReply(Signal*, BackupRecordPtr ptr, BackupFilePtr);
  void closeFiles(Signal*, BackupRecordPtr ptr);
  void closeFilesDone(Signal*, BackupRecordPtr ptr);  
  
  void sendDefineBackupReq(Signal *signal, BackupRecordPtr ptr);

  void defineBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId);
  void createTrigReply(Signal* signal, BackupRecordPtr ptr);
  void alterTrigReply(Signal* signal, BackupRecordPtr ptr);
  void startBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32, Uint32);
  void stopBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId);
  
  void defineBackupRef(Signal*, BackupRecordPtr, Uint32 errCode = 0);
  
  void nextFragment(Signal*, BackupRecordPtr);
  
  void sendCreateTrig(Signal*, BackupRecordPtr ptr, TablePtr tabPtr);
  void createAttributeMask(TablePtr tab, Bitmask<MAXNROFATTRIBUTESINWORDS>&);
  void sendStartBackup(Signal*, BackupRecordPtr, TablePtr);
  void sendAlterTrig(Signal*, BackupRecordPtr ptr);

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
  void masterAbort(Signal*, BackupRecordPtr ptr, bool controlledAbort);
  void masterSendAbortBackup(Signal*, BackupRecordPtr ptr);
  void slaveAbort(Signal*, BackupRecordPtr ptr);
  
  void abortFile(Signal* signal, BackupRecordPtr ptr, BackupFilePtr filePtr);
  void abortFileHook(Signal* signal, BackupFilePtr filePtr, bool scanDone);
  
  bool verifyNodesAlive(const NdbNodeBitmask& aNodeBitMask);
  bool checkAbort(BackupRecordPtr ptr);
  void checkNodeFail(Signal* signal,
		     BackupRecordPtr ptr,
		     NodeId newCoord,
		     Uint32 theFailedNodes[NodeBitmask::Size]);
  void masterTakeOver(Signal* signal, BackupRecordPtr ptr);


  NodeId getMasterNodeId() const { return c_masterNodeId; }
  bool findTable(const BackupRecordPtr &, TablePtr &, Uint32 tableId) const;
  TablePtr parseTableDescription(Signal*, BackupRecordPtr ptr, Uint32 len);
  
  bool insertFileHeader(BackupFormat::FileType, BackupRecord*, BackupFile*);
  void sendBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errorCode);
  void sendBackupRef(BlockReference ref, Signal *signal,
		     Uint32 senderData, Uint32 errorCode);
  void dumpUsedResources();
  void cleanupMasterResources(BackupRecordPtr ptr);
  void cleanupSlaveResources(BackupRecordPtr ptr);
  void cleanupFinalResources(BackupRecordPtr ptr);
  void removeBackup(Signal*, BackupRecordPtr ptr);

  void sendSTTORRY(Signal*);
  void createSequence(Signal* signal);
  void createSequenceReply(Signal*, class UtilSequenceConf *);
};

inline
void
Backup::OperationRecord::newRecord(Uint32 * p){
  base = p;
  dst_Length       = p; p += 1;
  dst_Bitmask      = p; p += sz_Bitmask;
  dst_FixedKeys    = p; p += sz_FixedKeys;
  dst_FixedAttribs = p; p += sz_FixedAttribs;
  dst_VariableData = (BackupFormat::DataFile::VariableData*)p;
  BitmaskImpl::clear(sz_Bitmask, dst_Bitmask);
  attrLeft = noOfAttributes;
  attrSzLeft = attrSzTotal = 0;
}

inline
Uint32 *
Backup::OperationRecord::newAttrib(Uint32 offset, Uint32 sz){
  attrLeft--;
  attrSzLeft = sz;
  dst = dst_FixedAttribs + offset;
  return dst;
}

inline
Uint32 *
Backup::OperationRecord::newKey(){
  attrLeft --;
  attrSzLeft = 0;
  return dst_FixedKeys;
}

inline
void
Backup::OperationRecord::nullAttribute(Uint32 offsetNull){
  attrLeft --;
  BitmaskImpl::set(sz_Bitmask, dst_Bitmask, offsetNull);
}

inline
Uint32 *
Backup::OperationRecord::newNullable(Uint32 id, Uint32 sz){
  attrLeft--;
  attrSzLeft = sz;
  
  dst = &dst_VariableData->Data[0];
  dst_VariableData->Sz = htonl(sz);
  dst_VariableData->Id = htonl(id);
  
  dst_VariableData = (BackupFormat::DataFile::VariableData *)(dst + sz);
  
  // Clear all bits on newRecord -> dont need to clear this
  // BitmaskImpl::clear(sz_Bitmask, dst_Bitmask, offsetNull);
  return dst;
}

inline
Uint32 *
Backup::OperationRecord::newVariable(Uint32 id, Uint32 sz){
  attrLeft--;
  attrSzLeft = sz;
  
  dst = &dst_VariableData->Data[0];
  dst_VariableData->Sz = htonl(sz);
  dst_VariableData->Id = htonl(id);
  
  dst_VariableData = (BackupFormat::DataFile::VariableData *)(dst + sz);
  return dst;
}

inline
Uint32 *
Backup::OperationRecord::newVariableKey(Uint32 sz){
  attrLeft--;
  attrSzLeft = 0;
  attrSzTotal += sz;
  
  dst = &dst_VariableData->Data[0];
  dst_VariableData->Sz = htonl(sz);
  dst_VariableData->Id = htonl(variableKeyId);
  
  dst_VariableData = (BackupFormat::DataFile::VariableData *)(dst + sz);
  return dst;
}

inline
bool
Backup::OperationRecord::finished(){
  if(attrLeft != 0 || attrSzLeft != 0){
    return false;
  }
  
  opLen += attrSzTotal + sz_FixedKeys;
  opNoDone++;
  
  scanStop = dst = (Uint32 *)dst_VariableData;
  
  const Uint32 len = (dst - base - 1);
  * dst_Length = htonl(len);
  
  noOfRecords++;
  
  return true;
}

#endif
