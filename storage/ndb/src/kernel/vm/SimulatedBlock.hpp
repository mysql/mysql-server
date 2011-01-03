/*
   Copyright (C) 2003 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef SIMULATEDBLOCK_H
#define SIMULATEDBLOCK_H

#include <NdbTick.h>
#include <kernel_types.h>
#include <util/version.h>
#include <ndb_limits.h>

#include "VMSignal.hpp"
#include <RefConvert.hpp>
#include <BlockNumbers.h>
#include <GlobalSignalNumbers.h>

#include "pc.hpp"
#include "Pool.hpp"
#include <NodeInfo.hpp>
#include <NodeState.hpp>
#include "GlobalData.hpp"
#include "LongSignal.hpp"
#include <SignalLoggerManager.hpp>

#include <ErrorReporter.hpp>
#include <ErrorHandlingMacros.hpp>

#include "DLList.hpp"
#include "ArrayPool.hpp"
#include "DLHashTable.hpp"
#include "WOPool.hpp"
#include "RWPool.hpp"
#include "Callback.hpp"
#include "SafeCounter.hpp"

#include <mgmapi.h>
#include <mgmapi_config_parameters.h>
#include <mgmapi_config_parameters_debug.h>
#include <kernel_config_parameters.h>
#include <Configuration.hpp>

#include <signaldata/ReadConfig.hpp>
#include <signaldata/UpgradeStartup.hpp>
#include "ndbd_malloc_impl.hpp"
#include <blocks/record_types.hpp>

/**
 * Something for filesystem access
 */
struct  NewBaseAddrBits              /* 32 bits */
{
  unsigned int     q               : 4;    /* Highest index - 2log */
  /* Strings are treated as 16 bit indexed        */
  /* variables with the number of characters in   */
  /* index 0, byte 0                              */
  unsigned int     v               : 3;    /* Size in bits - 2log */
  unsigned int     unused : 25 ;
};

typedef struct NewVar
{
  Uint32 *              WA;
  Uint32                nrr;
  Uint32                ClusterSize;    /* Real Cluster size    */
  NewBaseAddrBits       bits;
} NewVARIABLE;  /* 128 bits */

struct Block_context
{
  Block_context(class Configuration& cfg, class Ndbd_mem_manager& mm)
    : m_config(cfg), m_mm(mm) {}
  class Configuration& m_config;
  class Ndbd_mem_manager& m_mm;
};

class SimulatedBlock {
  friend class TraceLCP;
  friend class SafeCounter;
  friend class SafeCounterManager;
  friend struct UpgradeStartup;
  friend class AsyncFile;
  friend class PosixAsyncFile; // FIXME
  friend class Pgman;
  friend class Page_cache_client;
  friend class Lgman;
  friend class Logfile_client;
  friend struct Pool_context;
  friend struct SectionHandle;
  friend class LockQueue;
public:
  friend class BlockComponent;
  virtual ~SimulatedBlock();
  
protected:
  /**
   * Constructor
   */
  SimulatedBlock(BlockNumber blockNumber,
		 struct Block_context & ctx); 
  
  /**********************************************************
   * Handling of execFunctions
   */
  typedef void (SimulatedBlock::* ExecFunction)(Signal* signal);
  void addRecSignalImpl(GlobalSignalNumber g, ExecFunction fun, bool f =false);
  void installSimulatedBlockFunctions();
  ExecFunction theExecArray[MAX_GSN+1];

  void initCommon();
public:
  /**
   * 
   */
  inline void executeFunction(GlobalSignalNumber gsn, Signal* signal);
public:
  typedef void (SimulatedBlock::* CallbackFunction)(class Signal*, 
						    Uint32 callbackData,
						    Uint32 returnCode);
  struct Callback {
    CallbackFunction m_callbackFunction;
    Uint32 m_callbackData;
  };
  
  virtual const char* get_filename(Uint32 fd) const { return "";}
protected:
  static Callback TheEmptyCallback;
  void TheNULLCallbackFunction(class Signal*, Uint32, Uint32);
  static Callback TheNULLCallback;
  void execute(Signal* signal, Callback & c, Uint32 returnCode);
  
  
  /**********************************************************
   * Send signal - dialects
   */

  void sendSignal(BlockReference ref, 
		  GlobalSignalNumber gsn, 
                  Signal* signal, 
		  Uint32 length, 
		  JobBufferLevel jbuf ) const ;

  void sendSignal(NodeReceiverGroup rg,
		  GlobalSignalNumber gsn, 
                  Signal* signal, 
		  Uint32 length, 
		  JobBufferLevel jbuf ) const ;

  void sendSignal(BlockReference ref, 
		  GlobalSignalNumber gsn, 
                  Signal* signal, 
		  Uint32 length, 
		  JobBufferLevel jbuf,
		  SectionHandle* sections) const;

  void sendSignal(NodeReceiverGroup rg,
		  GlobalSignalNumber gsn,
                  Signal* signal,
		  Uint32 length,
		  JobBufferLevel jbuf,
		  SectionHandle* sections) const;

  void sendSignal(BlockReference ref,
		  GlobalSignalNumber gsn,
                  Signal* signal,
		  Uint32 length,
		  JobBufferLevel jbuf,
		  LinearSectionPtr ptr[3],
		  Uint32 noOfSections) const ;
  
  void sendSignal(NodeReceiverGroup rg, 
		  GlobalSignalNumber gsn, 
                  Signal* signal, 
		  Uint32 length, 
		  JobBufferLevel jbuf,
		  LinearSectionPtr ptr[3],
		  Uint32 noOfSections) const ;

  // Send multiple signal with delay. In this VM the jobbufffer level has 
  // no effect on on delayed signals
  //
  void sendSignalWithDelay(BlockReference ref, 
			   GlobalSignalNumber gsn, 
                           Signal* signal,
                           Uint32 delayInMilliSeconds, 
			   Uint32 length) const ;

  void sendSignalWithDelay(BlockReference ref,
			   GlobalSignalNumber gsn,
                           Signal* signal,
                           Uint32 delayInMilliSeconds,
			   Uint32 length,
			   SectionHandle* sections) const;

  void EXECUTE_DIRECT(Uint32 block, 
		      Uint32 gsn, 
		      Signal* signal, 
		      Uint32 len);
  
  class SectionSegmentPool& getSectionSegmentPool();
  void releaseSections(struct SectionHandle&);

  void handle_invalid_sections_in_send_signal(Signal*) const;
  void handle_lingering_sections_after_execute(Signal*) const;
  void handle_lingering_sections_after_execute(SectionHandle*) const;
  void handle_invalid_fragmentInfo(Signal*) const;
  void handle_send_failed(SendStatus, Signal*) const;
  void handle_out_of_longsignal_memory(Signal*) const;

  /**
   * Send routed signals (ONLY LOCALLY)
   *
   * NOTE: Only localhost is allowed!
   */
  struct RoutePath
  {
    Uint32 ref;
    JobBufferLevel prio;
  };
  void sendRoutedSignal(RoutePath path[],
                        Uint32 pathcnt,      // #hops
                        Uint32 dst[],        // Final destination(s)
                        Uint32 dstcnt,       // #final destination(s)
                        Uint32 gsn,          // Final GSN
                        Signal*,
                        Uint32 len,
                        JobBufferLevel prio, // Final prio
                        SectionHandle * handle = 0);


  /**
   * Check that signal sent from remote node
   *   is guaranteed to be correctly serialized wrt to NODE_FAILREP
   */
  bool checkNodeFailSequence(Signal*);

  /**********************************************************
   * Fragmented signals
   */
  
  /**
   * Assemble fragments
   *
   * @return true if all fragments has arrived
   *         false otherwise
   */
  bool assembleFragments(Signal * signal);
  
  void sendFragmentedSignal(BlockReference ref, 
			    GlobalSignalNumber gsn, 
			    Signal* signal, 
			    Uint32 length, 
			    JobBufferLevel jbuf,
			    SectionHandle * sections,
			    Callback & = TheEmptyCallback,
			    Uint32 messageSize = 240);

  void sendFragmentedSignal(NodeReceiverGroup rg, 
			    GlobalSignalNumber gsn, 
			    Signal* signal, 
			    Uint32 length, 
			    JobBufferLevel jbuf,
			    SectionHandle * sections,
			    Callback & = TheEmptyCallback,
			    Uint32 messageSize = 240);

  void sendFragmentedSignal(BlockReference ref, 
			    GlobalSignalNumber gsn, 
			    Signal* signal, 
			    Uint32 length, 
			    JobBufferLevel jbuf,
			    LinearSectionPtr ptr[3],
			    Uint32 noOfSections,
			    Callback & = TheEmptyCallback,
			    Uint32 messageSize = 240);

  void sendFragmentedSignal(NodeReceiverGroup rg, 
			    GlobalSignalNumber gsn, 
			    Signal* signal, 
			    Uint32 length, 
			    JobBufferLevel jbuf,
			    LinearSectionPtr ptr[3],
			    Uint32 noOfSections,
			    Callback & = TheEmptyCallback,
			    Uint32 messageSize = 240);

  /**
   * simBlockNodeFailure
   *
   * Method must be called by blocks that send or receive 
   * remote Fragmented Signals when they detect a node 
   * (NDBD or API) failure.
   * If the block needs to acknowledge or perform further
   * processing after completing block-level node failure 
   * handling, it can supply a Callback which will be invoked 
   * when block-level node failure handling has completed.
   * Otherwise TheEmptyCallback is used.
   * If TheEmptyCallback is used, all failure handling is
   * performed in the current timeslice, to avoid any
   * races.
   * 
   * Parameters
   *   signal       : Current signal*
   *   failedNodeId : Node id of failed node
   *   cb           : Callback to be executed when block-level
   *                  node failure handling completed.
   *                  TheEmptyCallback is passed if no further
   *                  processing is required.
   * Returns
   *   Number of 'resources' cleaned up in call.
   *   Callback return code is total resources cleaned up.
   *   
   */
  Uint32 simBlockNodeFailure(Signal* signal,
                             Uint32 failedNodeId,
                             Callback& cb = TheEmptyCallback);

  /**********************************************************
   * Fragmented signals structures
   */

  /**
   * Struct used when assembling fragmented long signals at receiver side
   */
  struct FragmentInfo {
    FragmentInfo(Uint32 fragId, Uint32 sender);
    
    Uint32 m_senderRef;
    Uint32 m_fragmentId;
    Uint32 m_sectionPtrI[3];
    union {
      Uint32 nextPool;
      Uint32 nextHash;
    };
    Uint32 prevHash;
    
    inline bool equal(FragmentInfo & p) const {
      return m_senderRef == p.m_senderRef && m_fragmentId == p.m_fragmentId;
    }
    
    inline Uint32 hashValue() const {
      return m_senderRef + m_fragmentId ;
    }
  }; // sizeof() = 32 bytes
  
  /**
   * Struct used when sending fragmented signals
   */
  struct FragmentSendInfo {
    FragmentSendInfo();
    
    enum Status {
      SendNotComplete = 0,
      SendComplete    = 1,
      SendCancelled   = 2
    };
    Uint8  m_status;
    Uint8  m_prio;
    Uint16 m_fragInfo;
    Uint16 m_gsn;
    Uint16 m_messageSize; // Size of each fragment
    Uint32 m_fragmentId;
    union {
      Ptr<struct SectionSegment> m_segmented;
      LinearSectionPtr m_linear;
    } m_sectionPtr[3];
    LinearSectionPtr m_theDataSection;
    NodeReceiverGroup m_nodeReceiverGroup; // 3
    Callback m_callback;
    union  {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  
  /**
   * setupFragmentSendInfo
   *   Setup a struct to be used with sendSignalFragment
   *   Used by sendFragmentedSignal
   */
  bool sendFirstFragment(FragmentSendInfo & info,
			 NodeReceiverGroup rg, 
			 GlobalSignalNumber gsn, 
			 Signal* signal, 
			 Uint32 length, 
			 JobBufferLevel jbuf,
			 SectionHandle * sections,
			 Uint32 messageSize = 240);
  
  bool sendFirstFragment(FragmentSendInfo & info,
			 NodeReceiverGroup rg, 
			 GlobalSignalNumber gsn, 
			 Signal* signal, 
			 Uint32 length, 
			 JobBufferLevel jbuf,
			 LinearSectionPtr ptr[3],
			 Uint32 noOfSections,
			 Uint32 messageSize = 240);
  
  /**
   * Send signal fragment
   *
   * @see sendFragmentedSignal
   */
  void sendNextSegmentedFragment(Signal* signal, FragmentSendInfo & info);

  /**
   * Send signal fragment
   *
   * @see sendFragmentedSignal
   */
  void sendNextLinearFragment(Signal* signal, FragmentSendInfo & info);
  
  BlockNumber    number() const;
  BlockReference reference() const;
  NodeId         getOwnNodeId() const;

  /**
   * Refresh Watch Dog in initialising code
   *
   */
  void refresh_watch_dog(Uint32 place = 1);
  void update_watch_dog_timer(Uint32 interval);

  /**
   * Prog error
   * This function should be called when this node should be shutdown
   * If the cause of the shutdown is known use extradata to add an 
   * errormessage describing the problem
   */
  void progError(int line, int err_code, const char* extradata=NULL) const
    ATTRIBUTE_NORETURN;
private:
  void  signal_error(Uint32, Uint32, Uint32, const char*, int) const
    ATTRIBUTE_NORETURN;
  const NodeId         theNodeId;
  const BlockNumber    theNumber;
  const BlockReference theReference;

  Uint32 doNodeFailureCleanup(Signal* signal,
                              Uint32 failedNodeId,
                              Uint32 resource,
                              Uint32 cursor,
                              Uint32 elementsCleaned,
                              Callback& cb);

  bool doCleanupFragInfo(Uint32 failedNodeId,
                         Uint32& cursor,
                         Uint32& rtUnitsUsed,
                         Uint32& elementsCleaned);

  bool doCleanupFragSend(Uint32 failedNodeId,
                         Uint32& cursor,
                         Uint32& rtUnitsUsed,
                         Uint32& elementsCleaned);
  
protected:
  Block_context m_ctx;
  NewVARIABLE* allocateBat(int batSize);
  void freeBat();
  static const NewVARIABLE* getBat    (BlockNumber blockNo);
  static Uint16             getBatSize(BlockNumber blockNo);
  
  static BlockReference calcTcBlockRef   (NodeId aNode);
  static BlockReference calcLqhBlockRef  (NodeId aNode);
  static BlockReference calcAccBlockRef  (NodeId aNode);
  static BlockReference calcTupBlockRef  (NodeId aNode);
  static BlockReference calcTuxBlockRef  (NodeId aNode);
  static BlockReference calcDihBlockRef  (NodeId aNode);
  static BlockReference calcQmgrBlockRef (NodeId aNode);
  static BlockReference calcDictBlockRef (NodeId aNode);
  static BlockReference calcNdbCntrBlockRef (NodeId aNode);
  static BlockReference calcTrixBlockRef (NodeId aNode);
  static BlockReference calcBackupBlockRef (NodeId aNode);
  static BlockReference calcSumaBlockRef (NodeId aNode);

  static BlockReference calcApiClusterMgrBlockRef (NodeId aNode);

  /** 
   * allocRecord
   * Allocates memory for the datastructures where ndb keeps the data
   *
   */
  void* allocRecord(const char * type, size_t s, size_t n, bool clear = true, Uint32 paramId = 0);
  void* allocRecordAligned(const char * type, size_t s, size_t n, void **unaligned_buffer, Uint32 align = NDB_O_DIRECT_WRITE_ALIGNMENT, bool clear = true, Uint32 paramId = 0);
  
  /**
   * Deallocate record
   *
   * NOTE: Also resets pointer
   */
  void deallocRecord(void **, const char * type, size_t s, size_t n);
  
  /**
   * Allocate memory from global pool,
   *   returns #chunks used
   *
   * Typically used by part of code, not converted to use global pool
   *   directly, but allocates everything during startup
   */
  struct AllocChunk
  {
    Uint32 ptrI;
    Uint32 cnt;
  };
  Uint32 allocChunks(AllocChunk dst[], Uint32 /* size of dst */ arraysize,
                     Uint32 /* resource group */ rg,
                     Uint32 /* no of pages to allocate */ pages,
                     Uint32 paramId /* for error message if failing */);

  static int sortchunks(const void*, const void*);

  /**
   * General info event (sent to cluster log)
   */
  void infoEvent(const char * msg, ...) const
    ATTRIBUTE_FORMAT(printf, 2, 3);
  void warningEvent(const char * msg, ...) const
    ATTRIBUTE_FORMAT(printf, 2, 3);
  
  /**
   * Get node state
   */
  const NodeState & getNodeState() const;

  /**
   * Get node info
   */
  const NodeInfo & getNodeInfo(NodeId nodeId) const;
  NodeInfo & setNodeInfo(NodeId);

  const NodeVersionInfo& getNodeVersionInfo() const;
  NodeVersionInfo& setNodeVersionInfo();
  
  /**********************
   * Xfrm stuff
   */
  
  /**
   * @return length
   */
  Uint32 xfrm_key(Uint32 tab, const Uint32* src, 
		  Uint32 *dst, Uint32 dstSize,
		  Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX]) const;

  Uint32 xfrm_attr(Uint32 attrDesc, CHARSET_INFO* cs,
                   const Uint32* src, Uint32 & srcPos,
                   Uint32* dst, Uint32 & dstPos, Uint32 dstSize) const;
  
  /**
   *
   */
  Uint32 create_distr_key(Uint32 tableId,
			  Uint32 *data, 
			  const Uint32 keyPaLen[MAX_ATTRIBUTES_IN_INDEX])const;
  
  /**
   * if ndbd,
   *   wakeup main-loop if sleeping on IO
   * if ndbmtd
   *   wakeup thread running block-instance
   */
  void wakeup();

  /**
   * setup struct for wakeup
   */
  void setup_wakeup();

private:
  NewVARIABLE* NewVarRef;      /* New Base Address Table for block  */
  Uint16       theBATSize;     /* # entries in BAT */

protected:  
  ArrayPool<GlobalPage>& m_global_page_pool;
  ArrayPool<GlobalPage>& m_shared_page_pool;
  
  void execNDB_TAMPER(Signal * signal);
  void execNODE_STATE_REP(Signal* signal);
  void execCHANGE_NODE_STATE_REQ(Signal* signal);

  void execSIGNAL_DROPPED_REP(Signal* signal);
  void execCONTINUE_FRAGMENTED(Signal* signal);
  void execAPI_START_REP(Signal* signal);
  void execNODE_START_REP(Signal* signal);

  void execLOCAL_ROUTE_ORD(Signal*);
private:
  /**
   * Node state
   */
  NodeState theNodeState;

  Uint32 c_fragmentIdCounter;
  ArrayPool<FragmentInfo> c_fragmentInfoPool;
  DLHashTable<FragmentInfo> c_fragmentInfoHash;
  
  bool c_fragSenderRunning;
  ArrayPool<FragmentSendInfo> c_fragmentSendPool;
  DLList<FragmentSendInfo> c_linearFragmentSendList;
  DLList<FragmentSendInfo> c_segmentedFragmentSendList;

protected:
  Uint32 debugPrintFragmentCounts();
  
public: 
  class MutexManager {
    friend class Mutex;
    friend class SimulatedBlock;
    friend class DbUtil;
  public:
    MutexManager(class SimulatedBlock &);
    
    bool setSize(Uint32 maxNoOfActiveMutexes);
    Uint32 getSize() const ; // Get maxNoOfActiveMutexes
    
  private:
    /**
     * core interface
     */
    struct ActiveMutex {
      ActiveMutex() {}
      Uint32 m_gsn; // state
      Uint32 m_mutexId;
      Callback m_callback;
      union {
	Uint32 nextPool;
	Uint32 nextList;
      };
      Uint32 prevList;
    };
    typedef Ptr<ActiveMutex> ActiveMutexPtr;
    
    bool seize(ActiveMutexPtr& ptr);
    void release(Uint32 activeMutexPtrI);
    
    void getPtr(ActiveMutexPtr& ptr);
    
    void create(Signal*, ActiveMutexPtr&);
    void destroy(Signal*, ActiveMutexPtr&);
    void lock(Signal*, ActiveMutexPtr&, Uint32 flags);
    void unlock(Signal*, ActiveMutexPtr&);
    
  private:
    void execUTIL_CREATE_LOCK_REF(Signal* signal);
    void execUTIL_CREATE_LOCK_CONF(Signal* signal);
    void execUTIL_DESTORY_LOCK_REF(Signal* signal);
    void execUTIL_DESTORY_LOCK_CONF(Signal* signal);
    void execUTIL_LOCK_REF(Signal* signal);
    void execUTIL_LOCK_CONF(Signal* signal);
    void execUTIL_UNLOCK_REF(Signal* signal);
    void execUTIL_UNLOCK_CONF(Signal* signal);
    
    SimulatedBlock & m_block;
    ArrayPool<ActiveMutex> m_mutexPool;
    DLList<ActiveMutex> m_activeMutexes;
    
    BlockReference reference() const;
    void progError(int line, int err_code, const char* extra = 0);
  };
  
  friend class MutexManager;
  MutexManager c_mutexMgr;

  void ignoreMutexUnlockCallback(Signal* signal, Uint32 ptrI, Uint32 retVal);
  virtual bool getParam(const char * param, Uint32 * retVal) { return false;}

  SafeCounterManager c_counterMgr;
private:
  void execUTIL_CREATE_LOCK_REF(Signal* signal);
  void execUTIL_CREATE_LOCK_CONF(Signal* signal);
  void execUTIL_DESTORY_LOCK_REF(Signal* signal);
  void execUTIL_DESTORY_LOCK_CONF(Signal* signal);
  void execUTIL_LOCK_REF(Signal* signal);
  void execUTIL_LOCK_CONF(Signal* signal);
  void execUTIL_UNLOCK_REF(Signal* signal);
  void execUTIL_UNLOCK_CONF(Signal* signal);

protected:
  void execUPGRADE(Signal* signal);

  void fsRefError(Signal* signal, Uint32 line, const char *msg);
  void execFSWRITEREF(Signal* signal);
  void execFSREADREF(Signal* signal);
  void execFSOPENREF(Signal* signal);
  void execFSCLOSEREF(Signal* signal);
  void execFSREMOVEREF(Signal* signal);
  void execFSSYNCREF(Signal* signal);
  void execFSAPPENDREF(Signal* signal);

  // Variable for storing inserted errors, see pc.H
  ERROR_INSERT_VARIABLE;

#ifdef VM_TRACE_TIME
public:
  void clearTimes();
  void printTimes(FILE * output);
  void addTime(Uint32 gsn, Uint64 time);
  void subTime(Uint32 gsn, Uint64 time);
  struct TimeTrace {
    Uint32 cnt;
    Uint64 sum, sub;
  } m_timeTrace[MAX_GSN+1];
  Uint32 m_currentGsn;
#endif

#ifdef VM_TRACE
  Ptr<void> **m_global_variables, **m_global_variables_save;
  void clear_global_variables();
  void init_globals_list(void ** tmp, size_t cnt);
  void disable_global_variables();
  void enable_global_variables();
#endif
};

inline 
void 
SimulatedBlock::executeFunction(GlobalSignalNumber gsn, Signal* signal){
  ExecFunction f = theExecArray[gsn];
  if(gsn <= MAX_GSN && f != 0){
#ifdef VM_TRACE
    clear_global_variables();
#endif
    (this->*f)(signal);

    if (unlikely(signal->header.m_noOfSections))
    {
      handle_lingering_sections_after_execute(signal);
    }
    return;
  }

  /**
   * This point only passed if an error has occurred
   */
  char errorMsg[255];
  if (!(gsn <= MAX_GSN)) {
    BaseString::snprintf(errorMsg, 255, "Illegal signal received (GSN %d too high)", gsn);
    ERROR_SET(fatal, NDBD_EXIT_PRGERR, errorMsg, errorMsg);
  }
  if (!(theExecArray[gsn] != 0)) {
    BaseString::snprintf(errorMsg, 255, "Illegal signal received (GSN %d not added)", gsn);
    ERROR_SET(fatal, NDBD_EXIT_PRGERR, errorMsg, errorMsg);
  }
  ndbrequire(false);
}

inline
void
SimulatedBlock::execute(Signal* signal, Callback & c, Uint32 returnCode){
  CallbackFunction fun = c.m_callbackFunction; 
  if (fun == TheNULLCallback.m_callbackFunction)
    return;
  ndbrequire(fun != 0);
  c.m_callbackFunction = NULL;
  (this->*fun)(signal, c.m_callbackData, returnCode);
}

inline 
BlockNumber
SimulatedBlock::number() const {
   return theNumber;
}

inline
BlockReference
SimulatedBlock::reference() const {
   return theReference;
}

inline
NodeId
SimulatedBlock::getOwnNodeId() const {
  return theNodeId;
}

inline
BlockReference
SimulatedBlock::calcTcBlockRef   (NodeId aNodeId){
  return numberToRef(DBTC, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcLqhBlockRef  (NodeId aNodeId){
return numberToRef(DBLQH, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcAccBlockRef  (NodeId aNodeId){
  return numberToRef(DBACC, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcTupBlockRef  (NodeId aNodeId){
  return numberToRef(DBTUP, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcTuxBlockRef  (NodeId aNodeId){
  return numberToRef(DBTUX, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcDihBlockRef  (NodeId aNodeId){
  return numberToRef(DBDIH, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcDictBlockRef (NodeId aNodeId){
  return numberToRef(DBDICT, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcQmgrBlockRef (NodeId aNodeId){
  return numberToRef(QMGR, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcNdbCntrBlockRef (NodeId aNodeId){
  return numberToRef(NDBCNTR, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcTrixBlockRef (NodeId aNodeId){
  return numberToRef(TRIX, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcBackupBlockRef (NodeId aNodeId){
  return numberToRef(BACKUP, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcSumaBlockRef (NodeId aNodeId){
  return numberToRef(SUMA, aNodeId);
}

inline
BlockReference
SimulatedBlock::calcApiClusterMgrBlockRef (NodeId aNodeId){
  return numberToRef(API_CLUSTERMGR, aNodeId);
}

inline
const NodeState &
SimulatedBlock::getNodeState() const {
  return theNodeState;
}

inline
const NodeInfo &
SimulatedBlock::getNodeInfo(NodeId nodeId) const {
  ndbrequire(nodeId > 0 && nodeId < MAX_NODES);
  return globalData.m_nodeInfo[nodeId];
}

inline
const NodeVersionInfo &
SimulatedBlock::getNodeVersionInfo() const {
  return globalData.m_versionInfo;
}

inline
NodeVersionInfo &
SimulatedBlock::setNodeVersionInfo() {
  return globalData.m_versionInfo;
}

inline
void
SimulatedBlock::EXECUTE_DIRECT(Uint32 block, 
			       Uint32 gsn, 
			       Signal* signal, 
			       Uint32 len){
  signal->setLength(len);
#ifdef VM_TRACE
  if(globalData.testOn){
    signal->header.theVerId_signalNumber = gsn;
    signal->header.theReceiversBlockNumber = block;
    signal->header.theSendersBlockRef = reference();
    globalSignalLoggers.executeDirect(signal->header,
				      0,        // in
				      &signal->theData[0],
                                      globalData.ownId);
  }
#endif
  SimulatedBlock* b = globalData.getBlock(block);
#ifdef VM_TRACE_TIME
  Uint32 us1, us2;
  Uint64 ms1, ms2;
  NdbTick_CurrentMicrosecond(&ms1, &us1);
  Uint32 tGsn = m_currentGsn;
  b->m_currentGsn = gsn;
#endif
  b->executeFunction(gsn, signal);
#ifdef VM_TRACE_TIME
  NdbTick_CurrentMicrosecond(&ms2, &us2);
  Uint64 diff = ms2;
  diff -= ms1;
  diff *= 1000000;
  diff += us2;
  diff -= us1;
  b->addTime(gsn, diff);
  m_currentGsn = tGsn;
  subTime(tGsn, diff);
#endif
#ifdef VM_TRACE
  if(globalData.testOn){
    signal->header.theVerId_signalNumber = gsn;
    signal->header.theReceiversBlockNumber = block;
    signal->header.theSendersBlockRef = reference();
    globalSignalLoggers.executeDirect(signal->header,
				      1,        // out
				      &signal->theData[0],
                                      globalData.ownId);
  }
#endif
}

#ifdef VM_TRACE_TIME
inline
void
SimulatedBlock::addTime(Uint32 gsn, Uint64 time){
  m_timeTrace[gsn].cnt ++;
  m_timeTrace[gsn].sum += time;
}

inline
void
SimulatedBlock::subTime(Uint32 gsn, Uint64 time){
  m_timeTrace[gsn].sub += time;
}
#endif

/**
 * Defines for backward compatiblility
 */

#define BLOCK_DEFINES(BLOCK) \
  typedef void (BLOCK::* ExecSignalLocal) (Signal* signal); \
  typedef void (BLOCK::* BlockCallback)(Signal*, Uint32 callb, Uint32 retCode); \
  inline CallbackFunction safe_cast(BlockCallback f){ \
    return static_cast<CallbackFunction>(f); \
  } \
public:\
private: \
  void addRecSignal(GlobalSignalNumber gsn, ExecSignalLocal f, bool force = false)

#define BLOCK_CONSTRUCTOR(BLOCK) do { SimulatedBlock::initCommon(); } while(0)

#define BLOCK_FUNCTIONS(BLOCK) \
void \
BLOCK::addRecSignal(GlobalSignalNumber gsn, ExecSignalLocal f, bool force){ \
  addRecSignalImpl(gsn, (ExecFunction)f, force);\
}

#include "Mutex.hpp"

inline
SectionHandle::~SectionHandle()
{
  if (unlikely(m_cnt))
  {
    m_block->handle_lingering_sections_after_execute(this);
  }
}

#ifdef ERROR_INSERT
#define RSS_AP_SNAPSHOT(x) Uint32 rss_##x
#define RSS_AP_SNAPSHOT_SAVE(x) rss_##x = x.getNoOfFree()
#define RSS_AP_SNAPSHOT_CHECK(x) ndbrequire(rss_##x == x.getNoOfFree())
#define RSS_AP_SNAPSHOT_SAVE2(x,y) rss_##x = x.getNoOfFree()+(y)
#define RSS_AP_SNAPSHOT_CHECK2(x,y) ndbrequire(rss_##x == x.getNoOfFree()+(y))

#define RSS_OP_COUNTER(x) Uint32 x
#define RSS_OP_COUNTER_INIT(x) x = 0
#define RSS_OP_ALLOC(x) x ++
#define RSS_OP_FREE(x) x --
#define RSS_OP_ALLOC_X(x,n) x += n
#define RSS_OP_FREE_X(x,n) x -= n

#define RSS_OP_SNAPSHOT(x) Uint32 rss_##x
#define RSS_OP_SNAPSHOT_SAVE(x) rss_##x = x
#define RSS_OP_SNAPSHOT_CHECK(x) ndbrequire(rss_##x == x)
#else
#define RSS_AP_SNAPSHOT(x) struct rss_dummy0_##x { int dummy; }
#define RSS_AP_SNAPSHOT_SAVE(x)
#define RSS_AP_SNAPSHOT_CHECK(x)
#define RSS_AP_SNAPSHOT_SAVE2(x,y)
#define RSS_AP_SNAPSHOT_CHECK2(x,y)

#define RSS_OP_COUNTER(x) struct rss_dummy1_##x { int dummy; }
#define RSS_OP_COUNTER_INIT(x)
#define RSS_OP_ALLOC(x)
#define RSS_OP_FREE(x)
#define RSS_OP_ALLOC_X(x,n)
#define RSS_OP_FREE_X(x,n)

#define RSS_OP_SNAPSHOT(x) struct rss_dummy2_##x { int dummy; }
#define RSS_OP_SNAPSHOT_SAVE(x)
#define RSS_OP_SNAPSHOT_CHECK(x)

#endif

#endif

