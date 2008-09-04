/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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

#ifdef VM_TRACE
#define D(x) \
  do { \
    char buf[200]; \
    if (!debugOutOn()) break; \
    debugOut << debugOutTag(buf, __LINE__) << x << endl; \
  } while (0)
#define V(x) " " << #x << ":" << (x)
#else
#define D(x)
#undef V
#endif

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
		 struct Block_context & ctx,
                 Uint32 instanceNumber = 0);
  
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

  /* Multiple block instances */
  Uint32 instance() const {
    return theInstance;
  }
  Uint32 getWorkerCount() const {
    ndbrequire(theInstance == 0); // valid only on main instance
    ndbrequire(theInstanceCount >= 1);
    return theInstanceCount - 1;
  }
  SimulatedBlock* getInstance(Uint32 instanceNumber) {
    ndbrequire(theInstance == 0);
    if (instanceNumber == 0)
      return this;
    if (instanceNumber < theInstanceCount) {
      ndbrequire(theInstanceList != 0);
      return theInstanceList[instanceNumber];
    }
    return 0;
  }
  virtual void loadWorkers() {}

  /* Setup state of a block object for executing in a particular thread. */
  void assignToThread(Uint32 threadId, EmulatedJamBuffer *jamBuffer,
                      Uint32 *watchDogCounter);
  /* For multithreaded ndbd, get the id of owning thread. */
  uint32 getThreadId() const { return m_threadId; }
  static bool isMultiThreaded();

  /* Configuration based alternative.  Applies only to this node */
  static bool isNdbMt() { return globalData.isNdbMt; }
  static bool isNdbMtLqh() { return globalData.isNdbMtLqh; }
  static Uint32 getLqhWorkers() { return globalData.ndbMtLqhWorkers; }

  /*
   * Instance key (1-4) is used only when sending a signal.  Receiver
   * maps it to actual instance (0, if receiver is not MT LQH).
   *
   * For performance reason, DBTC gets instance key directly from DBDIH
   * via DI*GET*NODES*REQ signals.
   */
  static Uint32 getInstanceKey(Uint32 tabId, Uint32 fragId);

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

  /* NoRelease sendSignal variants do not release sections as
   * a side-effect of sending.  This requires extra
   * copying for local sends
   */
  void sendSignalNoRelease(BlockReference ref, 
                           GlobalSignalNumber gsn, 
                           Signal* signal, 
                           Uint32 length, 
                           JobBufferLevel jbuf,
                           SectionHandle* sections) const;

  void sendSignalNoRelease(NodeReceiverGroup rg,
                           GlobalSignalNumber gsn,
                           Signal* signal,
                           Uint32 length,
                           JobBufferLevel jbuf,
                           SectionHandle* sections) const;

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

  /*
   * Instance defaults to instance of sender.  Using explicit
   * instance argument asserts that the call is thread-safe.
   */
  void EXECUTE_DIRECT(Uint32 block, 
		      Uint32 gsn, 
		      Signal* signal, 
		      Uint32 len,
                      Uint32 givenInstanceNo = ZNIL);
  
  class SectionSegmentPool& getSectionSegmentPool();
  void releaseSections(struct SectionHandle&);

  void handle_invalid_sections_in_send_signal(Signal*) const;
  void handle_lingering_sections_after_execute(Signal*) const;
  void handle_lingering_sections_after_execute(SectionHandle*) const;

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
  
  /* If send size is > FRAGMENT_WORD_SIZE, fragments of this size
   * will be sent by the sendFragmentedSignal variants
   */
  STATIC_CONST( FRAGMENT_WORD_SIZE = 240 );

  void sendFragmentedSignal(BlockReference ref, 
			    GlobalSignalNumber gsn, 
			    Signal* signal, 
			    Uint32 length, 
			    JobBufferLevel jbuf,
			    SectionHandle * sections,
			    Callback & = TheEmptyCallback,
			    Uint32 messageSize = FRAGMENT_WORD_SIZE);

  void sendFragmentedSignal(NodeReceiverGroup rg, 
			    GlobalSignalNumber gsn, 
			    Signal* signal, 
			    Uint32 length, 
			    JobBufferLevel jbuf,
			    SectionHandle * sections,
			    Callback & = TheEmptyCallback,
			    Uint32 messageSize = FRAGMENT_WORD_SIZE);

  void sendFragmentedSignal(BlockReference ref, 
			    GlobalSignalNumber gsn, 
			    Signal* signal, 
			    Uint32 length, 
			    JobBufferLevel jbuf,
			    LinearSectionPtr ptr[3],
			    Uint32 noOfSections,
			    Callback & = TheEmptyCallback,
			    Uint32 messageSize = FRAGMENT_WORD_SIZE);

  void sendFragmentedSignal(NodeReceiverGroup rg, 
			    GlobalSignalNumber gsn, 
			    Signal* signal, 
			    Uint32 length, 
			    JobBufferLevel jbuf,
			    LinearSectionPtr ptr[3],
			    Uint32 noOfSections,
			    Callback & = TheEmptyCallback,
			    Uint32 messageSize = FRAGMENT_WORD_SIZE);

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
      SendComplete    = 1
    };
    Uint8  m_status;
    Uint8  m_prio;
    Uint8  m_fragInfo;
    enum Flags {
      SendNoReleaseSeg = 0x1
    };
    Uint8  m_flags;
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
   * sendFirstFragment
   *   Used by sendFragmentedSignal
   *   noRelease can only be used if the caller can guarantee
   *   not to free the supplied sections until all fragments 
   *   have been sent.
   */
  bool sendFirstFragment(FragmentSendInfo & info,
			 NodeReceiverGroup rg, 
			 GlobalSignalNumber gsn, 
			 Signal* signal, 
			 Uint32 length, 
			 JobBufferLevel jbuf,
			 SectionHandle * sections,
                         bool noRelease,
			 Uint32 messageSize = FRAGMENT_WORD_SIZE);
  
  bool sendFirstFragment(FragmentSendInfo & info,
			 NodeReceiverGroup rg, 
			 GlobalSignalNumber gsn, 
			 Signal* signal, 
			 Uint32 length, 
			 JobBufferLevel jbuf,
			 LinearSectionPtr ptr[3],
			 Uint32 noOfSections,
			 Uint32 messageSize = FRAGMENT_WORD_SIZE);
  
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
public:
  /* Must be public so that we can jam() outside of block scope. */
  EmulatedJamBuffer *jamBuffer() const;
protected:
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
  void progError(int line, int err_code, const char* extradata=NULL) const ;
private:
  void  signal_error(Uint32, Uint32, Uint32, const char*, int) const ;
  const NodeId         theNodeId;
  const BlockNumber    theNumber;
  const Uint32 theInstance;
  const BlockReference theReference;
  /*
   * Instance 0 is the main instance.  It creates/owns other instances.
   * In MT LQH main instance is the LQH proxy and the others ("workers")
   * are real LQHs run by multiple threads.
   */
  enum { MaxInstances = 1 + MAX_NDBMT_LQH_WORKERS };
  Uint32 theInstanceCount;          // set in main
  SimulatedBlock** theInstanceList; // set in main, indexed by instance
  SimulatedBlock* theMainInstance;  // set in all
  /*
    Thread id currently executing this block.
    Not used in singlethreaded ndbd.
  */
  Uint32 m_threadId;
  /*
    Jam buffer reference.
    In multithreaded ndbd, this is different in each thread, and must be
    updated if migrating the block to another thread.
  */
  EmulatedJamBuffer *m_jamBuffer;
  /* For multithreaded ndb, the thread-specific watchdog counter. */
  Uint32 *m_watchDogCounter;
protected:
  Block_context m_ctx;
  NewVARIABLE* allocateBat(int batSize);
  void freeBat();
  static const NewVARIABLE* getBat    (BlockNumber blockNo,
                                       Uint32 instanceNo);
  static Uint16             getBatSize(BlockNumber blockNo,
                                       Uint32 instanceNo);
  
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

  // matching instance on same node e.g. LQH-ACC-TUP
  BlockReference calcInstanceBlockRef(BlockNumber aBlock);

  // matching instance on another node e.g. LQH-LQH
  BlockReference calcInstanceBlockRef(BlockNumber aBlock, NodeId aNode);

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
   * General info event (sent to cluster log)
   */
  void infoEvent(const char * msg, ...) const ;
  void warningEvent(const char * msg, ...) const ;
  
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
			  const Uint32* src,
                          Uint32 *dst, 
			  const Uint32 keyPaLen[MAX_ATTRIBUTES_IN_INDEX])const;
  
private:
  NewVARIABLE* NewVarRef;      /* New Base Address Table for block  */
  Uint16       theBATSize;     /* # entries in BAT */

protected:  
  SafeArrayPool<GlobalPage>& m_global_page_pool;
  ArrayPool<GlobalPage>& m_shared_page_pool;
  
  void execNDB_TAMPER(Signal * signal);
  void execNODE_STATE_REP(Signal* signal);
  void execCHANGE_NODE_STATE_REQ(Signal* signal);

  void execSIGNAL_DROPPED_REP(Signal* signal);
  void execCONTINUE_FRAGMENTED(Signal* signal);
  void execSTOP_FOR_CRASH(Signal* signal);
  void execAPI_START_REP(Signal* signal);
  void execNODE_START_REP(Signal* signal);
  void execSEND_PACKED(Signal* signal);
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
  Ptr<void> **m_global_variables;
  void clear_global_variables();
  void init_globals_list(void ** tmp, size_t cnt);
#endif

#ifdef VM_TRACE
  NdbOut debugOut;
  bool debugOutOn() const;
  const char* debugOutTag(char* buf, int line);
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
EmulatedJamBuffer *
SimulatedBlock::jamBuffer() const {
   return m_jamBuffer;
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
BlockReference
SimulatedBlock::calcInstanceBlockRef(BlockNumber aBlock){
  return numberToRef(aBlock, instance(), getOwnNodeId());
}

inline
BlockReference
SimulatedBlock::calcInstanceBlockRef(BlockNumber aBlock, NodeId aNodeId){
  return numberToRef(aBlock, instance(), aNodeId);
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

inline
void
SimulatedBlock::EXECUTE_DIRECT(Uint32 block, 
			       Uint32 gsn, 
			       Signal* signal, 
			       Uint32 len,
                               Uint32 givenInstanceNo)
{
  signal->setLength(len);
  SimulatedBlock* b = globalData.getBlock(block);
  ndbassert(b != 0);
  /**
   * In multithreaded NDB, blocks run in different threads, and EXECUTE_DIRECT
   * (unlike sendSignal) is generally not thread-safe.
   * So only allow EXECUTE_DIRECT between blocks that run in the same thread,
   * unless caller explicitly marks it as being thread safe (eg NDBFS),
   * by using an explicit instance argument.
   * By default instance of sender is used.  This is automatically thread-safe
   * for worker instances (instance != 0).
   */
  Uint32 instanceNo = givenInstanceNo;
  if (instanceNo == ZNIL)
    instanceNo = instance();
  if (instanceNo != 0)
    b = b->getInstance(instanceNo);
  ndbassert(b != 0);
  ndbassert(givenInstanceNo != ZNIL || b->getThreadId() == getThreadId());
#ifdef VM_TRACE
  if(globalData.testOn){
    signal->header.theVerId_signalNumber = gsn;
    signal->header.theReceiversBlockNumber = numberToBlock(block, instanceNo);
    signal->header.theSendersBlockRef = reference();
    globalSignalLoggers.executeDirect(signal->header,
				      0,        // in
				      &signal->theData[0],
                                      globalData.ownId);
  }
#endif
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
    signal->header.theReceiversBlockNumber = numberToBlock(block, instanceNo);
    signal->header.theSendersBlockRef = reference();
    globalSignalLoggers.executeDirect(signal->header,
				      1,        // out
				      &signal->theData[0],
                                      globalData.ownId);
  }
#endif
}

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
#define RSS_AP_SNAPSHOT(x)
#define RSS_AP_SNAPSHOT_SAVE(x)
#define RSS_AP_SNAPSHOT_CHECK(x)

#define RSS_OP_COUNTER(x)
#define RSS_OP_COUNTER_INIT(x)
#define RSS_OP_ALLOC(x)
#define RSS_OP_FREE(x)
#define RSS_OP_ALLOC_X(x,n)
#define RSS_OP_FREE_X(x,n)

#define RSS_OP_SNAPSHOT(x)
#define RSS_OP_SNAPSHOT_SAVE(x)
#define RSS_OP_SNAPSHOT_CHECK(x)

#endif

struct Hash2FragmentMap
{
  STATIC_CONST( MAX_MAP = 240 );
  Uint32 m_cnt;
  Uint32 m_fragments;
  Uint8 m_map[MAX_MAP];
  Uint32 nextPool;
  Uint32 m_object_id;
};

extern ArrayPool<Hash2FragmentMap> g_hash_map;

#endif

