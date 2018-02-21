/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "IntrusiveList.hpp"
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
#include "ndbd_malloc_impl.hpp"
#include <blocks/record_types.hpp>

#include "Ndbinfo.hpp"

struct CHARSET_INFO;

#define JAM_FILE_ID 248


#ifdef VM_TRACE
#define D(x) \
  do { \
    char buf[200]; \
    if (!debugOutOn()) break; \
    debugOutLock(); \
    debugOutStream() << debugOutTag(buf, __LINE__) << x << dec << "\n"; \
    debugOutUnlock(); \
  } while (0)
#define V(x) " " << #x << ":" << (x)
#else
#define D(x) do { } while(0)
#undef V
#endif

/**
 * LCP scans and Backup scans always use batch size 16, there are even
 * optimisations in allocation and handling LCP scans and Backup scans
 * keeping proper rates using this particular batch size. This is also
 * true for Node recovery scans as started by COPY_FRAGREQ.
 */
#define ZRESERVED_SCAN_BATCH_SIZE 16
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

struct PackedWordsContainer
{
  BlockReference hostBlockRef;
  Uint32 noOfPackedWords;
  Uint32 packedWords[30];
}; // 128 bytes

#define LIGHT_LOAD_CONST 0
#define MEDIUM_LOAD_CONST 1
#define OVERLOAD_CONST 2
enum OverloadStatus
{
  LIGHT_LOAD = LIGHT_LOAD_CONST,
  MEDIUM_LOAD = MEDIUM_LOAD_CONST,
  OVERLOAD = OVERLOAD_CONST
};

/**
  Description of NDB Software Architecture
  ----------------------------------------

  The NDB software architecture has two foundations, blocks and signals.
  The base object for the blocks is the below SimulatedBlock class and
  the signal object is the base class Signal defined in VMSignal.hpp.

  Blocks are intended as software units that owns its own data and it
  communicates with other blocks only through signals. Each block owns
  its own set of data which it entirely controls. There has been some
  optimisations where blocks always executing in the same thread can do
  some shortcuts by calling functions in a different block directly.
  There is even some code to call functions in a block in a different
  thread, in this case however some mutex is required to protect the
  data.

  Blocks are gathered together in threads. Threads are gathered into nodes.
  So when sending a signal you need to send it to an address. The address is
  a 32-bit word. It is a bit similar to IPv4 addresses. The address is
  setup in the following manner:

  -- Bit 0-8 ------- Bit 9-15 ------ Bit 16-31 ------
  | Block number  | Thread id   |       NodeId      |
  ---------------------------------------------------

  So when delivering a signal we start by checking the node id. If the node
  id is our own node id, then we will continue checking thread id. If it
  is destined to another node, then we move the signal sending to the module
  that takes care of transporting the signal to another node in the cluster.
  
  Each other node is found using a socket over TCP/IP. The architecture
  supports also other ways to transport signals such as using some form
  of shared memory between processes on the same or different machines.
  It would also be possible to extend the architecture such that we
  might use different sockets for different threads in the node.

  If the signal is destined for a different thread then we transport the
  signal to that thread, we use a separate memory buffer for each two
  threads that communicate such that the communication between threads is
  completely lock-free.

  One block number can be used in several threads. So e.g. the LDM threads
  all contain its own instance of the DBLQH block. The method instance()
  gets the instance number of the currently executing block. The method
  reference() gets the block reference of the currently executing block.

  If we send to ourselves we put the signal in the memory buffer for
  communication with our own thread.

  The current limits of the architecture is a maximum of 512 block numbers.
  We currently use less than 25 of those numbers. The maximum number of
  threads are 128 threads. We currently can use at most 72 threads.
  The current node limit is 255 nodes and node id 0 is a special case.

  So there is still a lot of space in the addressing mechanism for growth
  in terms of number of threads, blocks and nodes and even for introduction
  of new addressable units like subthreads or similar things.

  The software architecture also contains a structure for how signals are
  structured. Each signal is sent at a certain priority level. Finally also
  there is a concept of sending delayed signals to blocks within the same
  thread.

  Priority level on signals
  -------------------------
  So starting with priority level a signal can be sent on high priority
  (JBA) and normal priority (JBB). The priority level can be used also when
  sending to other nodes. The priority will however not be used to prioritise
  the signal in sending it over the socket to the receiving node.

  Each thread has its own buffer for A-priority signals. In the scheduler
  we will always execute all signals in the A-priority buffer first. If
  new A-priority signals are sent during these signals, then they will also
  be executed until no more signals exist on A-priority level. So it's not
  allowed to have a flow of signals all executing at A-level. We always have
  to insert a signal in the flow that either goes down to B-level or use some
  form of delayed signal.

  If an A-level signal is sent from a B-level signal it will be handled
  differently in the single threaded ndbd and the multi-threaded ndbmtd. In
  ndbmtd it will be executed after executing up to 128 B-level signals. In
  ndbd it will be executed as the next signal. So one cannot assume that an
  A-level signal will be executed before a specific B-level signal. A B-level
  signal can even be executed before an A-level signal although it was sent
  after the A-level signal.

  Delayed signals
  ---------------
  Delayed signals are used to create threads of activities that execute without
  consuming too much CPU activity. Delayed signals can only be sent internally
  within the same thread. When the signal has been delayed and is taken out of
  its timer queue its inserted into the priority A buffer.

  Bounded delay signals
  ---------------------
  A special form of delayed signal also exists, this is sent with delay equal to
  the constant BOUNDED_DELAY. This means that the signal will be executed as a
  priority A task as soon as the current set of B-level tasks are done. This is
  similar to sending an A-level signal from a B-level job in ndbmtd. However for
  ndbd it's not the same thing and also when sending an A-level signal from an
  A-level signal it is also not the same thing.

  So a delayed signal with delay BOUNDED_DELAY is a special type of signal
  with a bounded delay. The bound is that no more than 100 B-level signals will
  be executed before this signal is executed. Given our design requirements
  a B-level signal should mostly be executed within at most 5-10 microseconds
  or so, mostly much shorter than this even, so a normal execution time of
  a signal would be below 1 microsecond. So 100 signals should almost never
  execute for more than 1000 microseconds and rarely go beyond even 100
  microseconds.

  So these bounded delay signals are a good tool to ensure that activitites
  such as backups, checkpoints, node recovery activities, altering of tables
  and similar things gets executed at a certain rate. Without any possibility
  of bounded delay signals it is very hard to implement an activity that gets
  executed at a certain rate.

  So in a sense we're using the bounded delay signals to implement a form of
  time-sharing priority, certain activities are allowed to use a proportion
  of the available CPU resources, not too much, but also not too little. If
  an LCP gets bogged down by user transactions then the system will eventually
  run out of REDO log space. If a node recovery activity gets bogged down by
  user transactions then we will run for too long with only one replica in the
  node group which is bad for system availability.

  Execute direct signals
  ----------------------
  If the receiving block is within the same thread, then it is possible to
  send the signal using the method EXECUTE_DIRECT. This execution will
  happen immediately and won't be scheduled for later, it will be done in
  the same fashion as a function call.

  Signals
  -------
  Signals are carried with a certain structure:
  1) Each signal has a signal number. This number also is mapped to a name.
     When executing a signal with a certain number which e.g. has the name
     TCKEYREQ, then this signal is implemented by a method called
     execTCKEYREQ in the receiving block. More than one block could have
     such a method since a signal is not tied to a certain block.

  2) Each signal has 4 areas that can be sent in the signal. The first is
     always sent in the signal, this is the fixed part. The fixed part
     consists of at least 1 and at most 25 32-bit words. Many signals have
     a class that defines this part of the signal. This is however not
     absolutely necessary. Then there are up to 3 sections that can carry
     longer information bits. So e.g. a TCKEYREQ has one section that contains
     the primary key and another part that contains the attribute information.
     The attribute information could be seen as a program sent to MySQL
     Cluster data nodes to read, update the row specified in the key
     section. The attribute information could also contain interpreted
     programs that can do things like increment, decrement, conditional
     update and so forth.

   3) As mentioned above each signal carries a certain priority level to
      execute it on. It is currently not possible to check the prio
      level you're currently executing on, but it would be real simple
      to add this capability if necessary.

   4) When executing a certain signal it gets a signal id, this id is
      local to the thread and is incremented by one each new signal that
      is executed. This signal id is available in the Signal class and
      can e.g. be used to deduce if the thread is currently at high load.

   A signal is sent over the socket using a special protocol that is called
   Protocol6. This is not discussed more here, it is a feature of the
   transport mechanisms of the NDB Software Architecture.

   CONTINUEB
   ---------
   CONTINUEB is a special signal used by almost all blocks. This signal is
   used to handle background thread activities. Often the CONTINUEB signals
   are used as part of implementing a more complex action. One example is
   when DBDIH starts up a new LCP. It sends various forms of CONTINUEB
   signals to itself to move ahead through the LCP actions it needs to do
   as part of starting up a new LCP. The first word contains the type of
   CONTINUEB signal, so this is in a sense a bit like a second level of
   signal number. Based on this number the CONTINUEB data is treated
   differently.

   Common patterns of signals
   --------------------------
   There is no absolute patterns for how signal data looks like. But it is
   very common that a signal at least contains the destination object id,
   the senders object id and the senders block reference. The senders block
   reference is actually also available in the Signal class when executing
   a signal. But we can use various forms of software routing of the
   signal, so the senders block reference is the originator of the signal,
   not necessarily the same as the sender of the signal since it could be
   routed through several blocks on the way.

   The basic data type in the NDB signals are unsigned 32-bit integers. So
   all addressing is using a special form of pointers. The pointers always
   refers to a special class of objects and the pointer is the index in an
   array of objects of this kind. So we can have up to 4 billion objects of
   most kinds. If one needs to send strings and 64-bit integers one follows
   various techniques to do this. Signals are sent in the endian order of
   the machine they were generated, so machines in a cluster has to be
   of the same type of endian.

   ROUTE_SIGNAL
   ------------
   ROUTE_SIGNAL is a special signal that can be used to carry a signal
   in a special path to ensure that it arrives in the correct order to
   the receiving block.

   Signal order guarantees
   -----------------------
   The following signal order guarantees are maintained.

   1) Sending signals at the same priority level to the same block reference
      from one block will arrive in the order they were sent.

      It is not guaranteed if the priority level is different for the signals,
      it is also not guaranteed if they are sent through different paths.
      Not even sending in the following pattern has a guarantee on the
      delivery order. Signal 1: Block A -> Block B, Signal 2: Block A ->
      Block C -> Block B. Although the signal 2 uses a longer path and is
      destined to the same block it can still arrive before signal 1 at
      Block B. The reason is that we execute signals from one sender at a
      time, so we might be executing in Block C very quickly whereas the
      thread executing Block B might be stalled and then when Block C has
      sent its signal the thread executing Block B wakes up and decides
      to execute signals from Block C before signals from Block A.

   So as can be seen there is very little support for signal orders in the
   NDB software architecture and so most protocols have to take into
   account that signals can arrive in many different orders.

   Fragmented signals
   ------------------
   It is possible to send really long signals. These signals cannot be
   sent as one signal though. They are sent as one signal, then they will
   be split up into multiple signals. The fixed part is the same in all
   signals. What mainly differs is that they each contain a part of each
   segment.

   When receiving such a signal one should always call assembleFragments()
   at first to see if the entire signal has arrived first. The signal
   executor method is executed once for each signal fragment that is sent.
   When all fragments have arrived then they will contain the full signal
   with up to 3 sections that can much longer than the normal sized signals
   that have limitations on the size of the signals.

   Tracing infrastructure
   ----------------------
   All signals are sent through memory buffers. At crashes these memory
   buffers can be used to print the last executed signals in each thread.
   This will aid in looking for reasons for the crash. There will be one
   file generated for each thread in the ndbmtd, in the case of ndbd there
   will be only one file since there is only one file.

   Jams
   ----
   jam() and its cousins is a set of macros used for tracing what happened
   at the point of a crash. Each jam call executes a set of instructions
   that inserts the line number of the jam macro into an array kept per
   thread. There is some overhead in the jams, but it helps quite a lot in
   debugging crashes of the NDB data nodes. At crash time we can see a few
   thousand of the last decisions made just before the crash. This together
   with the signal logs makes for a powerful tool to root out bugs in NDB
   data nodes.

   Trace Id
   --------
   Each signal also carries a signal id, this id can be used to trace certain
   activities that go on for a longer time. This tracing can happen even in a
   live system.
*/

class SimulatedBlock :
  public SegmentUtils  /* SimulatedBlock implements the Interface */
{
  friend class TraceLCP;
  friend class SafeCounter;
  friend class SafeCounterManager;
  friend class AsyncFile;
  friend class PosixAsyncFile; // FIXME
  friend class Win32AsyncFile;
  friend class Pgman;
  friend class Page_cache_client;
  friend class Lgman;
  friend class Logfile_client;
  friend class Tablespace_client;
  friend class Dbtup_client;
  friend struct Pool_context;
  friend struct SectionHandle;
  friend class LockQueue;
  friend class SimplePropertiesSectionWriter;
  friend class SegmentedSectionGuard;
  friend class DynArr256Pool; // for cerrorInsert
public:
  friend class BlockComponent;
  virtual ~SimulatedBlock();

  static const Uint32 BOUNDED_DELAY = 0xFFFFFF00;
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
  void handle_execute_error(GlobalSignalNumber gsn);

  void initCommon();

  inline void executeFunction(GlobalSignalNumber gsn,
                              Signal* signal,
                              ExecFunction f);

  inline void executeFunction(GlobalSignalNumber gsn,
                              Signal* signal,
                              ExecFunction f,
                              BlockReference ref,
                              Uint32 len);

public:
  typedef void (SimulatedBlock::* CallbackFunction)(class Signal*,
						    Uint32 callbackData,
						    Uint32 returnCode);
  struct Callback {
    CallbackFunction m_callbackFunction;
    Uint32 m_callbackData;
  };

  inline void executeFunction(GlobalSignalNumber gsn, Signal* signal);
  inline void executeFunction_async(GlobalSignalNumber gsn, Signal* signal);

  /* Multiple block instances */
  Uint32 instance() const {
    return theInstance;
  }

  ExecFunction getExecuteFunction(GlobalSignalNumber gsn)
  {
    return theExecArray[gsn];
  }
 
  SimulatedBlock* getInstance(Uint32 instanceNumber) {
    ndbrequire(theInstance == 0); // valid only on main instance
    if (instanceNumber == 0)
      return this;
    ndbrequire(instanceNumber < MaxInstances);
    if (theInstanceList != 0)
      return theInstanceList[instanceNumber];
    return 0;
  }
  void addInstance(SimulatedBlock* b, Uint32 theInstanceNo);
  virtual void loadWorkers() {}

  struct ThreadContext
  {
    Uint32 threadId;
    EmulatedJamBuffer* jamBuffer;
    Uint32 * watchDogCounter;
    SectionSegmentPool::Cache * sectionPoolCache;
    NDB_TICKS* pHighResTimer;
  };
  /* Setup state of a block object for executing in a particular thread. */
  void assignToThread(ThreadContext ctx);
  /* For multithreaded ndbd, get the id of owning thread. */
  uint32 getThreadId() const { return m_threadId; }
  /**
   * To call EXECUTE_DIRECT on THRMAN we need to get its instance number.
   * Its instance number is always 1 higher than the thread id since 0
   * is used for the proxy instance and then there is one instance per
   * thread.
   */
  Uint32 getThrmanInstance() const
  {
    if (isNdbMt())
    {
      return m_threadId + 1;
    }
    else
    {
      return 0;
    }
  }
  static bool isMultiThreaded();

  /* Configuration based alternative.  Applies only to this node */
  static bool isNdbMt() { return globalData.isNdbMt; }
  static bool isNdbMtLqh() { return globalData.isNdbMtLqh; }
  static Uint32 getLqhWorkers() { return globalData.ndbMtLqhWorkers; }

  /**
   * Assert that thread calling this function is "owner" of block instance
   */
#ifdef VM_TRACE
  void assertOwnThread();
#else
  void assertOwnThread(){ }
#endif

  /*
   * Instance key (1-4) is used only when sending a signal.  Receiver
   * maps it to actual instance (0, if receiver is not MT LQH).
   *
   * For performance reason, DBTC gets instance key directly from DBDIH
   * via DI*GET*NODES*REQ signals.
   */
  static Uint32 getInstanceKey(Uint32 tabId, Uint32 fragId);
  static Uint32 getInstanceKeyCanFail(Uint32 tabId, Uint32 fragId);
  static Uint32 getInstanceFromKey(Uint32 instanceKey); // local use only

  /**
   * This method will make sure that when callback in called each
   *   thread running an instance any of the threads in blocks[]
   *   will have executed a signal
   */
  void synchronize_threads_for_blocks(Signal*, const Uint32 blocks[],
                                      const Callback&, JobBufferLevel = JBB);
  
  /**
   * This method make sure that the path specified in blocks[]
   *   will be traversed before returning
   */
  void synchronize_path(Signal*, const Uint32 blocks[],
                        const Callback&, JobBufferLevel = JBB);

  /**
   * These methods are used to assist blocks to use the TIME_SIGNAL to
   * generate drum beats with a regular delay. elapsed_time will report
   * back the elapsed time since last call but will never report more
   * than max delay. max_delay = 2 * delay here.
   */
  void init_elapsed_time(Signal *signal,
                         NDB_TICKS &latestTIME_SIGNAL);
  void sendTIME_SIGNAL(Signal *signal,
                       const NDB_TICKS currentTime,
                       Uint32 delay);
  Uint64 elapsed_time(Signal *signal,
                      const NDB_TICKS currentTime,
                      NDB_TICKS &latestTIME_SIGNAL,
                      Uint32 max_delay);

private:
  struct SyncThreadRecord
  {
    Callback m_callback;
    Uint32 m_cnt;
    Uint32 nextPool;
  };
  typedef ArrayPool<SyncThreadRecord> SyncThreadRecord_pool;

  SyncThreadRecord_pool c_syncThreadPool;
  void execSYNC_THREAD_REQ(Signal*);
  void execSYNC_THREAD_CONF(Signal*);

  void execSYNC_REQ(Signal*);

  void execSYNC_PATH_REQ(Signal*);
  void execSYNC_PATH_CONF(Signal*);
public:
  virtual const char* get_filename(Uint32 fd) const { return "";}

  void EXECUTE_DIRECT_FN(ExecFunction f,
                         Signal *signal);

protected:
  static Callback TheEmptyCallback;
  void TheNULLCallbackFunction(class Signal*, Uint32, Uint32);
  static Callback TheNULLCallback;
  void execute(Signal* signal, Callback & c, Uint32 returnCode);
  

  /**
   * Various methods to get data from ndbd/ndbmtd such as time
   * spent in sleep, sending and executing, number of signals
   * in queue, and send buffer level.
   *
   * Also retrieving a thread name (this name must be pointing to a
   * static pointer since it will be stored and kept for a long
   * time. So the pointer cannot be changed.
   *
   * Finally also the ability to query for send thread information.
   */
  void getSendBufferLevel(NodeId node, SB_LevelType &level);
  Uint32 getSignalsInJBB();
  void setOverloadStatus(OverloadStatus new_status);
  void setWakeupThread(Uint32 wakeup_instance);
  void setNodeOverloadStatus(OverloadStatus new_status);
  void setSendNodeOverloadStatus(OverloadStatus new_status);
  void setNeighbourNode(NodeId node);
  void getPerformanceTimers(Uint64 &micros_sleep,
                            Uint64 &spin_time,
                            Uint64 &buffer_full_micros_sleep,
                            Uint64 &micros_send);
  void getSendPerformanceTimers(Uint32 send_instance,
                                Uint64 & exec_time,
                                Uint64 & sleep_time,
                                Uint64 & spin_time,
                                Uint64 & user_time_os,
                                Uint64 & kernel_time_os,
                                Uint64 & elapsed_time_os);
  Uint32 getSpintime();
  Uint32 getNumSendThreads();
  Uint32 getNumThreads();
  const char * getThreadName();
  const char * getThreadDescription();

  NDB_TICKS getHighResTimer() const 
  {
    return *m_pHighResTimer;
  }

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

  /**
   * EXECUTE_DIRECT comes in four variants.
   *
   * EXECUTE_DIRECT_FN/2 with explicit function, not signal number, see above.
   *
   * EXECUTE_DIRECT/4 calls another block within same thread.
   *
   * EXECUTE_DIRECT_MT/5 used when other block may be in another thread.
   *
   * EXECUTE_DIRECT_SS/5 can pass sections in call to block in same thread.
   */
  void EXECUTE_DIRECT(Uint32 block,
		      Uint32 gsn,
		      Signal* signal,
		      Uint32 len);
  /*
   * Instance defaults to instance of sender.  Using explicit
   * instance argument asserts that the call is thread-safe.
   */
  void EXECUTE_DIRECT_MT(Uint32 block,
		         Uint32 gsn,
		         Signal* signal,
		         Uint32 len,
                         Uint32 givenInstanceNo);
  void EXECUTE_DIRECT_SS(Uint32 block,
		         Uint32 gsn,
		         Signal* signal,
		         Uint32 len,
                         SectionHandle* sections);

  class SectionSegmentPool& getSectionSegmentPool();
  void release(SegmentedSectionPtr & ptr);
  void release(SegmentedSectionPtrPOD & ptr) {
    SegmentedSectionPtr tmp(ptr);
    release(tmp);
    ptr.setNull();
  }
  void releaseSection(Uint32 firstSegmentIVal);
  void releaseSections(struct SectionHandle&);

  bool import(Ptr<SectionSegment> & first, const Uint32 * src, Uint32 len);
  bool import(SegmentedSectionPtr& ptr, const Uint32* src, Uint32 len);
  bool import(SectionHandle * dst, LinearSectionPtr src[3],Uint32 cnt);

  bool appendToSection(Uint32& firstSegmentIVal, const Uint32* src, Uint32 len);
  bool dupSection(Uint32& copyFirstIVal, Uint32 srcFirstIVal);
  bool writeToSection(Uint32 firstSegmentIVal, Uint32 offset, const Uint32* src, Uint32 len);

  void handle_invalid_sections_in_send_signal(const Signal*) const;
  void handle_lingering_sections_after_execute(const Signal*) const;
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

#ifdef ERROR_INSERT
  void setDelayedPrepare();
#endif

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
  
  /**
   * Assemble dropped fragments
   *
   * Should be called at the start of a Dropped Signal Report 
   * (GSN_DROPPED_SIGNAL_REP) handler when it is expected that
   * the block could receive fragmented signals.
   * No dropped signal handling should be done until this method
   * returns true.
   * 
   * @return true if all fragments has arrived and dropped signal
   *              handling can proceed.
   *         false otherwise
   */
  bool assembleDroppedFragments(Signal * signal);
  
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
    
    inline bool equal(FragmentInfo const & p) const {
      return m_senderRef == p.m_senderRef && m_fragmentId == p.m_fragmentId;
    }
    
    inline Uint32 hashValue() const {
      return m_senderRef + m_fragmentId ;
    }

    inline bool isDropped() const {
      /* IsDropped when entry in hash, but no segments stored */
      return (( m_sectionPtrI[0] == RNIL ) &&
              ( m_sectionPtrI[1] == RNIL ) &&
              ( m_sectionPtrI[2] == RNIL ) );
    }
  }; // sizeof() = 32 bytes
  typedef ArrayPool<FragmentInfo> FragmentInfo_pool;
  typedef DLHashTable<FragmentInfo_pool, FragmentInfo> FragmentInfo_hash;

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
    Uint8  m_fragInfo;
    enum Flags {
      SendNoReleaseSeg = 0x1
    };
    Uint8  m_flags;
    Uint16 m_gsn;
    Uint16 m_messageSize; // Size of each fragment
    Uint32 m_fragmentId;
    union {
      // Similar to Ptr<SectionSegment> but a POD, as needed in a union.
      struct {
        SectionSegment* p;
        Uint32 i;
      } m_segmented;
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
  typedef ArrayPool<FragmentSendInfo> FragmentSendInfo_pool;
  typedef DLList<FragmentSendInfo_pool> FragmentSendInfo_list;
  
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
  [[noreturn]] void progError(int line,
                              int err_code,
                              const char* extradata=NULL,
                              const char* check="") const;
private:
  [[noreturn]] void signal_error(Uint32,
                                 Uint32,
                                 Uint32,
                                 const char*,
                                 int) const;
  const NodeId         theNodeId;
  const BlockNumber    theNumber;
  const Uint32 theInstance;
  const BlockReference theReference;
  /*
   * Instance 0 is the main instance.  It creates/owns other instances.
   * In MT LQH main instance is the LQH proxy and the others ("workers")
   * are real LQHs run by multiple threads.
   */
protected:
  enum { MaxInstances = NDBMT_MAX_BLOCK_INSTANCES };
private:
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

  /* Read-only high res timer pointer */
  const NDB_TICKS* m_pHighResTimer;

  SectionSegmentPool::Cache * m_sectionPoolCache;
  
  
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
  // valid only if receiver has same number of workers
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

  Uint32 change_and_get_io_laggers(int change);
  /**********************
   * Xfrm stuff
   *
   * xfrm the attr / key for **hash** generation.
   * - Keys being equal should generate identical xfrm'ed strings.
   * - Uniquenes of two non equal keys are preferred, but not required.
   */
  
  /**
   * @return length
   */
  Uint32 xfrm_key_hash(Uint32 tab, const Uint32* src,
		       Uint32 *dst, Uint32 dstSize,
		       Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX]) const;

  Uint32 xfrm_attr_hash(Uint32 attrDesc, const CHARSET_INFO* cs,
                        const Uint32* src, Uint32 & srcPos,
                        Uint32* dst, Uint32 & dstPos, Uint32 dstSize) const;


  /*******************
   * Compare either a full (non-NULL) key, or a single attr.
   *
   * Character strings are compared taking their normalized
   * 'weight' into considderation, as defined by their collation.
   *
   * No intermediate xfrm'ed string are produced during the compare.
   *
   * return '<0', '==0' or '>0' for 's1<s2', s1==s2, 's2>s2' resp.
   */
  int cmp_key(Uint32 tab, const Uint32* s1, const Uint32 *s2) const;

  int cmp_attr(Uint32 attrDesc, const CHARSET_INFO* cs,
	       const Uint32 *s1, Uint32 s1Len,
	       const Uint32 *s2, Uint32 s2Len) const;
  
  /**
   *
   */
  Uint32 create_distr_key(Uint32 tableId,
			  const Uint32* src,
                          Uint32 *dst, 
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

  /**
   * Get receiver thread index for node
   * MAX_NODES == no receiver thread
   */
  Uint32 get_recv_thread_idx(NodeId nodeId);

private:
  NewVARIABLE* NewVarRef;      /* New Base Address Table for block  */
  Uint16       theBATSize;     /* # entries in BAT */

protected:  
  GlobalPage_safepool& m_global_page_pool;
  GlobalPage_pool& m_shared_page_pool;
  
  void execNDB_TAMPER(Signal * signal);
  void execNODE_STATE_REP(Signal* signal);
  void execCHANGE_NODE_STATE_REQ(Signal* signal);

  void execSIGNAL_DROPPED_REP(Signal* signal);
  void execCONTINUE_FRAGMENTED(Signal* signal);
  void execSTOP_FOR_CRASH(Signal* signal);
  void execAPI_START_REP(Signal* signal);
  void execNODE_START_REP(Signal* signal);
  void execSEND_PACKED(Signal* signal);
  void execLOCAL_ROUTE_ORD(Signal*);
private:
  /**
   * Node state
   */
  NodeState theNodeState;

  Uint32 c_fragmentIdCounter;
  FragmentInfo_pool c_fragmentInfoPool;
  FragmentInfo_hash c_fragmentInfoHash;
  
  bool c_fragSenderRunning;
  FragmentSendInfo_pool c_fragmentSendPool;
  FragmentSendInfo_list c_linearFragmentSendList;
  FragmentSendInfo_list c_segmentedFragmentSendList;

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
    typedef ArrayPool<ActiveMutex> ActiveMutex_pool;
    typedef DLList<ActiveMutex_pool> ActiveMutex_list;
    
    bool seize(ActiveMutexPtr& ptr);
    void release(Uint32 activeMutexPtrI);
    
    void getPtr(ActiveMutexPtr& ptr) const;
    
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
    ActiveMutex_pool m_mutexPool;
    ActiveMutex_list m_activeMutexes;
    
    BlockReference reference() const;
    [[noreturn]] void progError(int line,
                                int err_code,
                                const char* extra = 0,
                                const char* check = "");
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

  void check_sections(Signal* signal, 
                      Uint32 oldSecCount, 
                      Uint32 newSecCount) const;
protected:

  void fsRefError(Signal* signal, Uint32 line, const char *msg);
  void execFSWRITEREF(Signal* signal);
  void execFSREADREF(Signal* signal);
  void execFSOPENREF(Signal* signal);
  void execFSCLOSEREF(Signal* signal);
  void execFSREMOVEREF(Signal* signal);
  void execFSSYNCREF(Signal* signal);
  void execFSAPPENDREF(Signal* signal);

  // MT LQH callback CONF via signal
public:
  struct CallbackPtr {
    Uint32 m_callbackIndex;
    Uint32 m_callbackData;
  };
protected:
  enum CallbackFlags {
    CALLBACK_DIRECT = 0x0001, // use EXECUTE_DIRECT (assumed thread safe)
    CALLBACK_ACK    = 0x0002  // send ack at the end of callback timeslice
  };

  struct CallbackEntry {
    CallbackFunction m_function;
    Uint32 m_flags;
  };

  struct CallbackTable {
    Uint32 m_count;
    CallbackEntry* m_entry; // array
  };

  CallbackTable* m_callbackTableAddr; // set by block if used

  enum {
    THE_NULL_CALLBACK = 0 // must assign TheNULLCallbackFunction
  };

  void block_require(void);
  void execute(Signal* signal, CallbackPtr & cptr, Uint32 returnCode);
  const CallbackEntry& getCallbackEntry(Uint32 ci);
  void sendCallbackConf(Signal* signal, Uint32 fullBlockNo,
                        CallbackPtr& cptr,
                        Uint32 senderData, Uint32 callbackInfo,
                        Uint32 returnCode);
  void execCALLBACK_CONF(Signal* signal);

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

#ifdef VM_TRACE
public:
  NdbOut debugOut;
  NdbOut& debugOutStream() { return debugOut; };
  bool debugOutOn();
  void debugOutLock() { globalSignalLoggers.lock(); }
  void debugOutUnlock() { globalSignalLoggers.unlock(); }
  const char* debugOutTag(char* buf, int line);
#endif

  const char* getPartitionBalanceString(Uint32 fct)
  {
    switch (fct)
    {
      case NDB_PARTITION_BALANCE_SPECIFIC:
        return "NDB_PARTITION_BALANCE_SPECIFIC";
      case NDB_PARTITION_BALANCE_FOR_RA_BY_NODE:
        return "NDB_PARTITION_BALANCE_FOR_RA_BY_NODE";
      case NDB_PARTITION_BALANCE_FOR_RP_BY_NODE:
        return "NDB_PARTITION_BALANCE_FOR_RP_BY_NODE";
      case NDB_PARTITION_BALANCE_FOR_RP_BY_LDM:
        return "NDB_PARTITION_BALANCE_FOR_RP_BY_LDM";
      case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM:
        return "NDB_PARTITION_BALANCE_FOR_RA_BY_LDM";
      case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_2:
        return "NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_2";
      case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_3:
        return "NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_3";
      case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_4:
        return "NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_4";
      default:
        ndbabort();
    }
    return NULL;
  }

  void ndbinfo_send_row(Signal* signal,
                        const DbinfoScanReq& req,
                        const Ndbinfo::Row& row,
                        Ndbinfo::Ratelimit& rl) const;

  void ndbinfo_send_scan_break(Signal* signal,
                               DbinfoScanReq& req,
                               const Ndbinfo::Ratelimit& rl,
                               Uint32 data1, Uint32 data2 = 0,
                               Uint32 data3 = 0, Uint32 data4 = 0) const;

  void ndbinfo_send_scan_conf(Signal* signal,
                              DbinfoScanReq& req,
                              const Ndbinfo::Ratelimit& rl) const;

#ifdef NDB_DEBUG_RES_OWNERSHIP
  /* Utils to lock and unlock the global section segment pool */
  void lock_global_ssp();
  void unlock_global_ssp();
#endif


protected:
  /**
   * SegmentUtils methods
   */
  virtual SectionSegment* getSegmentPtr(Uint32 iVal);
  virtual bool seizeSegment(Ptr<SectionSegment>& p);
  virtual void releaseSegment(Uint32 iVal);

  virtual void releaseSegmentList(Uint32 firstSegmentIVal);

  /** End of SegmentUtils methods */
};

// outside blocks e.g. within a struct
#ifdef VM_TRACE
#define DEBUG_OUT_DEFINES(blockNo) \
static SimulatedBlock* debugOutBlock() \
  { return globalData.getBlock(blockNo); } \
static NdbOut& debugOutStream() \
  { return debugOutBlock()->debugOutStream(); } \
static bool debugOutOn() \
  { return debugOutBlock()->debugOutOn(); } \
static void debugOutLock() \
  { debugOutBlock()->debugOutLock(); } \
static void debugOutUnlock() \
  { debugOutBlock()->debugOutUnlock(); } \
static const char* debugOutTag(char* buf, int line) \
  { return debugOutBlock()->debugOutTag(buf, line); } \
static void debugOutDefines()
#else
#define DEBUG_OUT_DEFINES(blockNo) \
static void debugOutDefines()
#endif

inline
void
SimulatedBlock::executeFunction(GlobalSignalNumber gsn,
                                Signal *signal)
{
  ExecFunction f = theExecArray[gsn];
  if (unlikely(gsn > MAX_GSN))
  {
    handle_execute_error(gsn);
    return;
  }
  executeFunction(gsn, signal, f);
}

inline
void
SimulatedBlock::executeFunction_async(GlobalSignalNumber gsn,
                                      Signal *signal)
{
  ExecFunction f = theExecArray[gsn];
#ifdef VM_TRACE
  clear_global_variables();
#endif
  if (unlikely(gsn > MAX_GSN))
  {
    handle_execute_error(gsn);
    return;
  }
  executeFunction(gsn, signal, f);
}

inline
void
SimulatedBlock::executeFunction(GlobalSignalNumber gsn,
                                Signal* signal,
                                ExecFunction f,
                                BlockReference ref,
                                Uint32 len)
{
  if (unlikely(gsn > MAX_GSN))
  {
    handle_execute_error(gsn);
    return;
  }
  signal->setLength(len);
  signal->header.theSendersBlockRef = ref;
  executeFunction(gsn, signal, f);
}

inline 
void 
SimulatedBlock::executeFunction(GlobalSignalNumber gsn,
                                Signal* signal,
                                ExecFunction f)
{
#ifdef NDB_DEBUG_RES_OWNERSHIP
  /* Use block num + gsn composite as owner id by default */
  setResOwner((Uint32(refToBlock(reference())) << 16) | gsn);
#endif
  if (likely(f != 0))
  {
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
  handle_execute_error(gsn);
}

inline
void SimulatedBlock::block_require(void)
{
  ndbabort();
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
void
SimulatedBlock::execute(Signal* signal, CallbackPtr & cptr, Uint32 returnCode){
  const CallbackEntry& ce = getCallbackEntry(cptr.m_callbackIndex);
  cptr.m_callbackIndex = ZNIL;
  Callback c;
  c.m_callbackFunction = ce.m_function;
  c.m_callbackData = cptr.m_callbackData;
  execute(signal, c, returnCode);
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
Uint32 SimulatedBlock::change_and_get_io_laggers(Int32 change)
{
  globalData.lock_IO_lag();
  Int32 io_laggers = Int32(globalData.get_io_laggers());
  require((io_laggers + change) >= 0);
  Uint32 new_io_laggers = Uint32(io_laggers + change);
  globalData.set_io_laggers(new_io_laggers);
  globalData.unlock_IO_lag();
  return new_io_laggers;
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
SimulatedBlock::EXECUTE_DIRECT_FN(ExecFunction f,
                                  Signal *signal)
{
  (this->*f)(signal);
}

inline
void
SimulatedBlock::EXECUTE_DIRECT_MT(Uint32 block, 
			          Uint32 gsn, 
			          Signal* signal, 
			          Uint32 len,
                                  Uint32 instanceNo)
{
  SimulatedBlock* rec_block;
  SimulatedBlock* main_block = globalData.getBlock(block);
  ndbassert(main_block != 0);
  /**
   * In multithreaded NDB, blocks run in different threads, and EXECUTE_DIRECT
   * (unlike sendSignal) is generally not thread-safe.
   * So only allow EXECUTE_DIRECT between blocks that run in the same thread,
   * unless caller explicitly marks it as being thread safe (eg NDBFS),
   * by using an explicit instance argument.
   * By default instance of sender is used.  This is automatically thread-safe
   * for worker instances (instance != 0).
   *
   * We also need to use this function when calling blocks that don't belong
   * to the same module, so e.g. LDM blocks can all call each other without
   * using this method. But e.g. no block can call THRMAN using implicit
   * instance id since the instance numbers of the LDM blocks and the THRMAN
   * blocks are not the same. There is one THRMAN instance for each thread,
   * not only for the LDM threads.
   */
  signal->header.theSendersBlockRef = reference();
  signal->setLength(len);
  ndbassert(instanceNo < MaxInstances);
  rec_block = main_block->getInstance(instanceNo);
  ndbassert(rec_block != 0);
#ifdef VM_TRACE
  if(globalData.testOn){
    signal->header.theVerId_signalNumber = gsn;
    signal->header.theReceiversBlockNumber = numberToBlock(block, instanceNo);
    globalSignalLoggers.executeDirect(signal->header,
				      0,        // in
				      &signal->theData[0],
                                      globalData.ownId);
  }
#endif
#ifdef VM_TRACE_TIME
  const NDB_TICKS t1 = NdbTick_getCurrentTicks();
  Uint32 tGsn = m_currentGsn;
  rec_block->m_currentGsn = gsn;
#endif
  rec_block->executeFunction(gsn, signal);
#ifdef VM_TRACE_TIME
  const NDB_TICKS t2 = NdbTick_getCurrentTicks();
  const Uint64 diff = NdbTick_Elapsed(t1, t2).microSec();
  rec_block->addTime(gsn, diff);
  m_currentGsn = tGsn;
  subTime(tGsn, diff);
#endif
}

/**
  We implement the normal EXECUTE_DIRECT in a special function
  since this is performance-critical although it introduces a
  little bit of code duplication.
*/
inline
void
SimulatedBlock::EXECUTE_DIRECT(Uint32 block, 
			       Uint32 gsn, 
			       Signal* signal, 
			       Uint32 len)
{
  /**
    globalData.getBlock(block) gives us the pointer to the block of
    the receiving class, it gives the pointer to instance 0. This
    instance has all the function pointers that all other instances
    also have, so we can reuse this block object to get the execute
    function. This means that we will use less cache lines over the
    system.
  */
  SimulatedBlock* main_block = globalData.getBlock(block);
  Uint32 instanceNo = instance();
  BlockReference ref = reference();
  SimulatedBlock* rec_block;
  signal->setLength(len);
  ndbassert(main_block != 0);
  ndbassert(main_block->theInstance == 0);
  ndbassert(instanceNo < MaxInstances);
  rec_block = main_block->theInstanceList[instanceNo];
  if (unlikely(gsn > MAX_GSN))
  {
    handle_execute_error(gsn);
    return;
  }
  ExecFunction f = rec_block->theExecArray[gsn];
  signal->header.theSendersBlockRef = ref;
  /**
   * In this function we only allow function calls within the same thread.
   *
   * No InstanceList leads to immediate Segmentation Fault,
   * so not necessary to check with ndbrequire for this.
   */
  ndbassert(rec_block != 0);
  ndbassert(rec_block->getThreadId() == getThreadId());
#ifdef VM_TRACE
  if(globalData.testOn){
    signal->header.theVerId_signalNumber = gsn;
    signal->header.theReceiversBlockNumber = numberToBlock(block, instanceNo);
    globalSignalLoggers.executeDirect(signal->header,
				      0,        // in
				      &signal->theData[0],
                                      globalData.ownId);
  }
#endif
#ifdef VM_TRACE_TIME
  const NDB_TICKS t1 = NdbTick_getCurrentTicks();
  Uint32 tGsn = m_currentGsn;
  rec_block->m_currentGsn = gsn;
#endif
  rec_block->executeFunction(gsn, signal, f);
#ifdef VM_TRACE_TIME
  const NDB_TICKS t2 = NdbTick_getCurrentTicks();
  const Uint64 diff = NdbTick_Elapsed(t1,t2).microSec();
  rec_block->addTime(gsn, diff);
  m_currentGsn = tGsn;
  subTime(tGsn, diff);
#endif
}

inline
void
SimulatedBlock::EXECUTE_DIRECT_SS(Uint32 block, 
			          Uint32 gsn, 
			          Signal* signal, 
			          Uint32 len,
                                  SectionHandle* sections)
{
  signal->header.m_noOfSections = sections->m_cnt;
  for (Uint32 i = 0; i < sections->m_cnt; i++)
  {
    signal->m_sectionPtrI[i] = sections->m_ptr[i].i;
  }
  sections->clear();
  EXECUTE_DIRECT(block, gsn, signal, len);
}

// Do a consictency check before reusing a signal.
inline void 
SimulatedBlock::check_sections(Signal* signal, 
                               Uint32 oldSecCount, 
                               Uint32 newSecCount) const
{ 
  // Sections from previous use should have been consumed by now.
  if (unlikely(oldSecCount != 0))
  { 
    handle_invalid_sections_in_send_signal(signal); 
  } 
  else if (unlikely(newSecCount == 0 &&
                    signal->header.m_fragmentInfo != 0 && 
                    signal->header.m_fragmentInfo != 3))
  { 
    handle_invalid_fragmentInfo(signal); 
  }
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

#ifdef ERROR_INSERT
#define RSS_AP_SNAPSHOT(x) Uint32 rss_##x
#define RSS_AP_SNAPSHOT_SAVE(x) rss_##x = x.getUsed()
#define RSS_AP_SNAPSHOT_CHECK(x) ndbrequire(rss_##x == x.getUsed())
#define RSS_AP_SNAPSHOT_SAVE2(x,y) rss_##x = x.getUsed()-(y)
#define RSS_AP_SNAPSHOT_CHECK2(x,y) ndbrequire(rss_##x == x.getUsed()-(y))

#define RSS_OP_COUNTER(x) Uint32 x
#define RSS_OP_COUNTER_INIT(x) x = 0
#define RSS_OP_ALLOC(x) x ++
#define RSS_OP_FREE(x) x --
#define RSS_OP_ALLOC_X(x,n) x += n
#define RSS_OP_FREE_X(x,n) x -= n

#define RSS_OP_SNAPSHOT(x) Uint32 rss_##x
#define RSS_OP_SNAPSHOT_SAVE(x) rss_##x = x
#define RSS_OP_SNAPSHOT_CHECK(x) ndbrequire(rss_##x == x)

#define RSS_DA256_SNAPSHOT(x) Uint32 rss_##x
#define RSS_DA256_SNAPSHOT_SAVE(x) rss_##x = x.m_high_pos
#define RSS_DA256_SNAPSHOT_CHECK(x) ndbrequire(x.m_high_pos <= rss_##x)
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

#define RSS_DA256_SNAPSHOT(x)
#define RSS_DA256_SNAPSHOT_SAVE(x)
#define RSS_DA256_SNAPSHOT_CHECK(x)
#endif

struct Hash2FragmentMap
{
  STATIC_CONST( MAX_MAP = NDB_MAX_HASHMAP_BUCKETS );
  Uint32 m_cnt;
  Uint32 m_fragments;
  Uint16 m_map[MAX_MAP];
  Uint32 nextPool;
  Uint32 m_object_id;
};
typedef ArrayPool<Hash2FragmentMap> Hash2FragmentMap_pool;

extern Hash2FragmentMap_pool g_hash_map;

/**
 * Guard class for auto release of segmentedsectionptr's
 */
class SegmentedSectionGuard
{
  Uint32 cnt;
  Uint32 ptr[3];
  SimulatedBlock * block;

public:
  SegmentedSectionGuard(SimulatedBlock* b) : cnt(0), block(b) { }
  SegmentedSectionGuard(SimulatedBlock* b, Uint32 ptrI) : cnt(1), block(b) {
    ptr[0] = ptrI;
  }

  void add(Uint32 ptrI) {
    if (ptrI != RNIL)
    {
      assert(cnt < NDB_ARRAY_SIZE(ptr));
      ptr[cnt] = ptrI;
      cnt++;
    }
  }

  void release() {
    for (Uint32 i = 0; i < cnt; i++) {
      if (ptr[i] != RNIL)
        block->releaseSection(ptr[i]);
    }
    cnt = 0;
  }

  void clear() {
    cnt = 0;
  }

  ~SegmentedSectionGuard() {
    release();
  }
};

#undef JAM_FILE_ID

#endif

