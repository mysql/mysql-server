/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <ndb_global.h>

#include <cstring>
#include <TransporterRegistry.hpp>
#include <FastScheduler.hpp>
#include <Emulator.hpp>
#include <ErrorHandlingMacros.hpp>

#include "LongSignal.hpp"
#include "LongSignalImpl.hpp"

#include <signaldata/EventReport.hpp>
#include <signaldata/TestOrd.hpp>
#include <signaldata/SignalDroppedRep.hpp>
#include <signaldata/DisconnectRep.hpp>

#include "VMSignal.hpp"
#include <NdbOut.hpp>
#include "TransporterCallbackKernel.hpp"
#include <DebuggerNames.hpp>
#include <trpman.hpp>

#include <EventLogger.hpp>

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
//#define DEBUG_MULTI_TRP 1
#endif

#ifdef DEBUG_MULTI_TRP
#define DEB_MULTI_TRP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_MULTI_TRP(arglist) do { } while (0)
#endif

#define JAM_FILE_ID 226


/**
 * The instance
 */
SectionSegmentPool g_sectionSegmentPool;

/* Instance debugging vars
 * Set from DBTC
 */
Uint32 ErrorSignalReceive= 0;      //Block to inject signal errors into
Uint32 ErrorMaxSegmentsToSeize= 0;

/**
 * This variable controls if ErrorSignalReceive/ErrorMaxSegmentsToSeize
 *   is active...This to make sure only received signals are affected
 *   and not long signals sent inside node
 */
extern bool ErrorImportActive;

struct ConnectionError
{
  enum TransporterError err;
  const char *text;
};

static const ConnectionError connectionError[] =
{
  { TE_NO_ERROR, "No error"},
  { TE_SHM_UNABLE_TO_CREATE_SEGMENT, "Unable to create shared memory segment"},
  { (enum TransporterError) -1, "No connection error message available (please report a bug)"}
};

const char *lookupConnectionError(Uint32 err)
{
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(connectionError); i++)
  {
    if ((Uint32)connectionError[i].err == err)
    {
      return connectionError[i].text;
    }
  }
  return "No connection error message available (please report a bug)";
}

#ifndef NDBD_MULTITHREADED

class TransporterCallbackKernelNonMT :
  public TransporterCallback,
  public TransporterSendBufferHandle,
  public TransporterReceiveHandleKernel
{
  void reportSendLen(NodeId nodeId, Uint32 count, Uint64 bytes) override;

public:
  TransporterCallbackKernelNonMT()
  : m_send_buffers(NULL), m_page_freelist(NULL), m_send_buffer_memory(NULL)
  {}

  ~TransporterCallbackKernelNonMT() override;

  /**
   * Allocate send buffer.
   *
   * Argument is the value of config parameter TotalSendBufferMemory. If 0,
   * a default will be used of sum(max send buffer) over all transporters.
   * The second is the config parameter ExtraSendBufferMemory
   */
  void allocate_send_buffers(Uint64 total_send_buffer,
                             Uint64 extra_send_buffer) override;

  /**
   * Implements TransporterCallback interface:
   */
  void enable_send_buffer(NodeId, TrpId) override;
  void disable_send_buffer(NodeId, TrpId) override;

  Uint32 get_bytes_to_send_iovec(NodeId node_id,
                                 TrpId trp_id,
                                 struct iovec *dst,
                                 Uint32 max) override;
  Uint32 bytes_sent(NodeId, TrpId, Uint32 bytes) override;

  /**
   * These are the TransporterSendBufferHandle methods used by the
   * single-threaded ndbd.
   */
  Uint32 *getWritePtr(NodeId,
                      TrpId,
                      Uint32 lenBytes,
                      Uint32 prio,
                      Uint32 max_use,
                      SendStatus *error) override;
  Uint32 updateWritePtr(NodeId, TrpId, Uint32 lenBytes, Uint32 prio) override;
  void getSendBufferLevel(NodeId node, SB_LevelType &level) override;
  bool forceSend(NodeId, TrpId) override;

private:
  /* Send buffer pages. */
  struct SendBufferPage {
    /* This is the number of words that will fit in one page of send buffer. */
    static const Uint32 PGSIZE = 32768;
    static Uint32 max_data_bytes()
    {
      return PGSIZE - offsetof(SendBufferPage, m_data);
    }

    /* Send buffer for one transporter is kept in a single-linked list. */
    struct SendBufferPage *m_next;

    /* Bytes of send data available in this page. */
    Uint16 m_bytes;
    /* Start of unsent data */
    Uint16 m_start;

    /* Data; real size is to the end of one page. */
    char m_data[2];
  };

  /* Send buffer for one transporter. */
  struct SendBuffer {
    bool m_enabled;
    /* Total size of data in buffer, from m_offset_start_data to end. */
    Uint32 m_used_bytes;
    /* Linked list of active buffer pages with first and last pointer. */
    SendBufferPage *m_first_page;
    SendBufferPage *m_last_page;
  };

  SendBufferPage *alloc_page();
  void release_page(SendBufferPage *page);
  void discard_send_buffer(TrpId trp_id);

  /* Send buffers. */
  SendBuffer *m_send_buffers;

  /* Linked list of free pages. */
  SendBufferPage *m_page_freelist;
  /* Original block of memory for pages (so we can free it at exit). */
  unsigned char *m_send_buffer_memory;
  
  Uint64 m_tot_send_buffer_memory;
  Uint64 m_tot_used_buffer_memory;
}; //class TransporterCallbackKernelNonMT

static TransporterCallbackKernelNonMT myTransporterCallback;

TransporterSendBufferHandle *getNonMTTransporterSendHandle()
{
  return &myTransporterCallback;
}

TransporterRegistry globalTransporterRegistry(&myTransporterCallback,
					      &myTransporterCallback);

#else

TransporterSendBufferHandle *getNonMTTransporterSendHandle()
{
  return NULL;
}
#endif // not NDBD_MULTITHREADED

#ifdef NDBD_MULTITHREADED
static struct ReceiverThreadCache
{
  SectionSegmentPool::Cache cache_instance;
  char pad[64 - sizeof(SectionSegmentPool::Cache)];
} g_receiver_thread_cache[MAX_NDBMT_RECEIVE_THREADS];

void
mt_init_receiver_cache()
{
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(g_receiver_thread_cache); i++)
  {
    g_receiver_thread_cache[i].cache_instance.init_cache(1024,1024);
  }
}

void
mt_set_section_chunk_size()
{
  g_sectionSegmentPool.setChunkSize(256);
}

#else
void mt_init_receiver_cache(){}
void mt_set_section_chunk_size(){}
#endif

bool
TransporterReceiveHandleKernel::deliver_signal(SignalHeader * const header,
                                               Uint8 prio,
                                               TransporterError &error_code,
                                               Uint32 * const theData,
                                               LinearSectionPtr ptr[3])
{
#ifdef NDBD_MULTITHREADED
  SectionSegmentPool::Cache & cache =
    g_receiver_thread_cache[m_receiver_thread_idx].cache_instance;
#endif

  const Uint32 secCount = header->m_noOfSections;
  const Uint32 length = header->theLength;

  // if this node is not MT LQH then instance bits are stripped at execute

#ifdef TRACE_DISTRIBUTED
  g_eventLogger->info("recv: %s(%d) from (%s, %d)",
                      getSignalName(header->theVerId_signalNumber),
                      header->theVerId_signalNumber,
                      getBlockName(refToBlock(header->theSendersBlockRef)),
                      refToNode(header->theSendersBlockRef));
#endif

  bool ok = true;
  Ptr<SectionSegment> secPtr[3];
  std::memset(secPtr, 0, sizeof(secPtr));
  secPtr[0].p = secPtr[1].p = secPtr[2].p = 0;

#ifdef NDB_DEBUG_RES_OWNERSHIP
  /**
   * Track sections seized as part of receiving signal with
   * 1 as 'special' block number for receiver
   */
  setResOwner(0x1 << 16 | header->theVerId_signalNumber);
#endif

#if defined(ERROR_INSERT)
  if (secCount > 0)
  {
    const Uint32 receiverBlock = blockToMain(header->theReceiversBlockNumber);
    if (unlikely(ErrorSignalReceive == receiverBlock))
    {
      ErrorImportActive = true;
    }
  }
#endif

  switch(secCount){
  case 3:
    ok &= import(SPC_CACHE_ARG secPtr[2], ptr[2].p, ptr[2].sz);
    [[fallthrough]];
  case 2:
    ok &= import(SPC_CACHE_ARG secPtr[1], ptr[1].p, ptr[1].sz);
    [[fallthrough]];
  case 1:
    ok &= import(SPC_CACHE_ARG secPtr[0], ptr[0].p, ptr[0].sz);
  }
#if defined(ERROR_INSERT)
  ErrorImportActive = false;
#endif

  /**
   * Check that we haven't received a too long signal
   */
  ok &= (length + secCount <= 25);
  
  Uint32 secPtrI[3];
  if(ok){
    /**
     * Normal path 
     */
    secPtrI[0] = secPtr[0].i;
    secPtrI[1] = secPtr[1].i;
    secPtrI[2] = secPtr[2].i;

#ifdef NDBD_MULTITHREADED
    const Uint32 receiverBlock = blockToMain(header->theReceiversBlockNumber);
    if (receiverBlock == V_QUERY)
    {
      /**
       * Signal was sent from remote DBTC or DBSPJ containing a
       * LQHKEYREQ or SCAN_FRAGREQ signal. These signals can be
       * executed by any LDM or Query thread as long as they are
       * COMMITTED READs.
       *
       * As reported in the paper "OLTP on Hardware Islands" in VLDB 2012
       * one can achieve much higher throughput with execution using
       * fine-grained shared nothing. This means here that the traditional
       * approach of executing COMMITTED READs in the LDM owning the data is the
       * preferred approach. What we have done to improve that here is that we
       * have introduced a query thread. In 2020 most CPU architectures
       * use 2 hyperthreads per CPU core. This means that we can have an LDM
       * thread and a query thread share the same CPU core. In this manner we
       * achieve the optimal performance for perfectly partitioned workloads.
       *
       * However not all workloads are perfectly partitionable. So e.g. in
       * TPC-H there is a higher cost for complex queries to execute when
       * all tables are fully partitioned. Many analytical schemas contain
       * one or more data tables that contain massive amounts of data.
       * These tablese often benefit from being fully partitioned. However
       * smaller dimension tables often benefit from a smaller amount of
       * partitioning.
       *
       * As noted in the paper above one wants to execute within a hardware
       * island, so at least we don't want to allow the query to use a
       * different CPU socket compared to where the owning LDM thread resides.
       * This would significantly increase contention around the fragment
       * lock as well as significantly increasing data movement between various
       * CPU caches in the server.
       *
       * In addition if we allow the round robin to use too many CPUs we can
       * easily create bursts of accesses to a fragment that makes too many
       * LDM and query threads become stalled. To avoid this we set a constant
       * limit to the round robin size. We want this size to be as big as
       * possible to achieve best scalability for dimension tables, but at the
       * same time we want to avoid using so many threads that we can easily
       * become stalled.
       *
       * The outcome of this discussion is that we will define a number of
       * Round Robin groups. Thus the LDM and query threads are placed into
       * individual round robin groups.
       *
       * One more problem we can have is data skew. For the tables that are
       * not fully partitioned this is solved by using the above approach
       * to use round robin.
       *
       * V_QUERY is a virtual block that doesn't actually exist. They are
       * discovered by the deliver signal module here.
       *
       * The responsibility to translate it is handled by the
       * TRPMAN block instance for this receive thread.
       * This instance receives a signal from the THRMAN instance
       * of the rep thread with a weight that each instance should
       * take in executing those signals. These weights are used to
       * create a round robin array of references that is used to
       * map these signals onto LDM threads and query threads.
       *
       * These weights are used both to create round robin schemas
       * for the round robin groups as well as for the smaller group
       * only containing one LDM thread with its companion query threads
       * (normally only 1 query thread per LDM threads). We could foresee
       * in the future with CPUs that employ 4 processors per CPU core that
       * we could have more than query thread per LDM thread.
       *
       * TC threads has a similar array used for local LQHKEYREQ
       * and SCAN_FRAGREQ signals. The THRMAN instance of the rep
       * thread distributes the same weight to all receive threads
       * and all TC threads. There is a weight for each LDM thread
       * and query thread. The weights can be between 0 and 16
       * where 0 means no signals will be sent to the instance and
       * 16 means the thread will take much responsibility in
       * handling the query load.
       *
       * This ensures that the LDM thread can be protected to handle
       * the responsibility of locking queries, write queries and
       * queries involving disk data. Thus when an LDM thread becomes
       * overloaded we can offload query handling to other LDM threads
       * and to query threads.
       */
      require(prio == JBB);
      const Uint32 instance_no =
        blockToInstance(header->theReceiversBlockNumber);
      Uint32 ref = ((Trpman*)m_trpman)->distribute_signal(header, instance_no);
      if (likely(ref != 0))
      {
        header->theReceiversBlockNumber = refToBlock(ref);
        sendlocal(m_thr_no, header, theData, secPtrI);
        return false;
      }
      error_code = TE_INVALID_SIGNAL;
      return true;
    }
#endif
#ifndef NDBD_MULTITHREADED
    globalScheduler.execute(header, prio, theData, secPtrI);  
#else
    if (prio == JBB)
      sendlocal(m_thr_no /* self */,
                header, theData, secPtrI);
    else
      sendprioa(m_thr_no /* self */,
                header, theData, secPtrI);

#endif
    return false;
  }
  
  /**
   * Out of memory
   */
  for(Uint32 i = 0; i<secCount; i++){
    if(secPtr[i].p != 0){
      g_sectionSegmentPool.releaseList(SPC_SEIZE_ARG
                                       relSz(secPtr[i].p->m_sz),
                                       secPtr[i].i, 
				       secPtr[i].p->m_lastSegment);
    }
  }

  SignalDroppedRep * rep = (SignalDroppedRep*)theData;
  Uint32 gsn = header->theVerId_signalNumber;
  Uint32 len = header->theLength;
  Uint32 newLen= (len > 22 ? 22 : len);
  memmove(rep->originalData, theData, (4 * newLen));
  rep->originalGsn = gsn;
  rep->originalLength = len;
  rep->originalSectionCount = secCount;
  header->theVerId_signalNumber = GSN_SIGNAL_DROPPED_REP;
  header->theLength = newLen + 3;
  header->m_noOfSections = 0;
#ifndef NDBD_MULTITHREADED
  globalScheduler.execute(header, prio, theData, secPtrI);    
#else
  if (prio == JBB)
    sendlocal(m_thr_no /* self */,
              header, theData, NULL);
  else
    sendprioa(m_thr_no /* self */,
              header, theData, NULL);
#endif
  return false;
}

NdbOut & 
operator<<(NdbOut& out, const SectionSegment & ss){
  out << "[ last= " << ss.m_lastSegment << " next= " << ss.nextPool << " ]";
  return out;
}

void
TransporterReceiveHandleKernel::reportError(NodeId nodeId,
                                            TransporterError errorCode,
                                            const char *info)
{
#ifdef DEBUG_TRANSPORTER
  g_eventLogger->info("reportError (%d, 0x%x) %s", nodeId, errorCode,
                      info ? info : "");
#endif

  DBUG_ENTER("reportError");
  DBUG_PRINT("info",("nodeId %d  errorCode: 0x%x  info: %s",
		     nodeId, errorCode, info));

  switch (errorCode)
  {
  case TE_SIGNAL_LOST_SEND_BUFFER_FULL:
  {
    char msg[64];
    BaseString::snprintf(msg, sizeof(msg), "Remote node id %d.%s%s", nodeId,
	     info ? " " : "", info ? info : "");
    ErrorReporter::handleError(NDBD_EXIT_SIGNAL_LOST_SEND_BUFFER_FULL,
			       msg, __FILE__, NST_ErrorHandler);
  }
  case TE_SIGNAL_LOST:
  {
    char msg[64];
    BaseString::snprintf(msg, sizeof(msg), "Remote node id %d,%s%s", nodeId,
	     info ? " " : "", info ? info : "");
    ErrorReporter::handleError(NDBD_EXIT_SIGNAL_LOST,
			       msg, __FILE__, NST_ErrorHandler);
  }
  case TE_SHM_IPC_PERMANENT:
  {
    char msg[128];
    BaseString::snprintf(msg, sizeof(msg),
	     "Remote node id %d.%s%s",
	     nodeId, info ? " " : "", info ? info : "");
    ErrorReporter::handleError(NDBD_EXIT_CONNECTION_SETUP_FAILED,
			       msg, __FILE__, NST_ErrorHandler);
  }
  default:
    break;
  }
 
  if(errorCode & TE_DO_DISCONNECT){
    reportDisconnect(nodeId, errorCode);
  }
  
  SignalT<3> signal;
  std::memset(&signal.header, 0, sizeof(signal.header));


  if(errorCode & TE_DO_DISCONNECT)
    signal.theData[0] = NDB_LE_TransporterError;
  else
    signal.theData[0] = NDB_LE_TransporterWarning;
  
  signal.theData[1] = nodeId;
  signal.theData[2] = errorCode;
  
  signal.header.theLength = 3;  
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.header.theReceiversBlockNumber = CMVMI;
  signal.header.theVerId_signalNumber = GSN_EVENT_REP;
#ifndef NDBD_MULTITHREADED
  Uint32 secPtr[3];
  globalScheduler.execute(&signal.header, JBA, signal.theData, secPtr);
#else
  sendprioa(m_thr_no /* self */,
            &signal.header, signal.theData, NULL);
#endif

  DBUG_VOID_RETURN;
}

/**
 * Report average send length in bytes (4096 last sends)
 */
#ifndef NDBD_MULTITHREADED
TransporterCallbackKernelNonMT::~TransporterCallbackKernelNonMT()
{
   m_page_freelist = NULL;
   delete[] m_send_buffers;
   delete[] m_send_buffer_memory;
}

void
TransporterCallbackKernelNonMT::reportSendLen(NodeId nodeId, Uint32 count,
                                              Uint64 bytes)
{
  SignalT<3> signal;
  std::memset(&signal.header, 0, sizeof(signal.header));

  signal.header.theLength = 3;
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.header.theReceiversBlockNumber = CMVMI;
  signal.header.theVerId_signalNumber = GSN_EVENT_REP;

  signal.theData[0] = NDB_LE_SendBytesStatistic;
  signal.theData[1] = nodeId;
  signal.theData[2] = Uint32(bytes/count);

  Uint32 secPtr[3];
  globalScheduler.execute(&signal.header, JBA, signal.theData, secPtr);
}


#define MIN_SEND_BUFFER_SIZE (4 * 1024 * 1024)

void
TransporterCallbackKernelNonMT::allocate_send_buffers(
                                           Uint64 total_send_buffer,
                                           Uint64 extra_send_buffer)
{
  const int maxTransporters = MAX_NTRANSPORTERS;
  const int nTransporters   = globalTransporterRegistry.get_transporter_count();

  if (total_send_buffer == 0)
    total_send_buffer = globalTransporterRegistry.get_total_max_send_buffer();

  total_send_buffer += extra_send_buffer;

  if (!extra_send_buffer)
  {
    /**
     * If extra send buffer memory is 0 it means we can decide on an
     * appropriate value for it. We select to always ensure that the
     * minimum send buffer memory is 4M, otherwise we simply don't
     * add any extra send buffer memory at all.
     */
    if (total_send_buffer < MIN_SEND_BUFFER_SIZE)
    {
      total_send_buffer = (Uint64)MIN_SEND_BUFFER_SIZE;
    }
  }

  if (m_send_buffers)
  {
    /* Send buffers already allocated -> resize the buffer pages */
    assert(m_send_buffer_memory);

    // TODO resize send buffer pages

    return;
  }

  /**
   * Initialize transporter send buffers (initially empty).
   * (Sparesely populated array of 'nTransporters')
   */
  assert(nTransporters <= maxTransporters);
  m_send_buffers = new SendBuffer[maxTransporters];
  for (int i = 0; i < maxTransporters; i++)
  {
    SendBuffer &b = m_send_buffers[i];
    b.m_first_page = NULL;
    b.m_last_page = NULL;
    b.m_used_bytes = 0;
    b.m_enabled = false;
  }

  /* Initialize the page freelist. */
  Uint64 send_buffer_pages =
    (total_send_buffer + SendBufferPage::PGSIZE - 1)/SendBufferPage::PGSIZE;
  /* Add one extra page of internal fragmentation overhead per transporter. */
  send_buffer_pages += nTransporters;

  m_send_buffer_memory =
    new unsigned char[UintPtr(send_buffer_pages * SendBufferPage::PGSIZE)];
  if (m_send_buffer_memory == NULL)
  {
    ndbout << "Unable to allocate "
           << send_buffer_pages * SendBufferPage::PGSIZE
           << " bytes of memory for send buffers, aborting." << endl;
    abort();
  }

  m_page_freelist = NULL;
  for (unsigned i = 0; i < send_buffer_pages; i++)
  {
    SendBufferPage *page =
      (SendBufferPage *)(m_send_buffer_memory + i * SendBufferPage::PGSIZE);
    page->m_bytes = 0;
    page->m_next = m_page_freelist;
    m_page_freelist = page;
  }
  m_tot_send_buffer_memory = SendBufferPage::PGSIZE * send_buffer_pages;
  m_tot_used_buffer_memory = 0;
}

TransporterCallbackKernelNonMT::SendBufferPage *
TransporterCallbackKernelNonMT::alloc_page()
{
  SendBufferPage *page = m_page_freelist;
  if (page != NULL)
  {
    m_tot_used_buffer_memory += SendBufferPage::PGSIZE;
    m_page_freelist = page->m_next;
    return page;
  }

  ndbout << "ERROR: out of send buffers in kernel." << endl;
  return NULL;
}

void
TransporterCallbackKernelNonMT::release_page(SendBufferPage *page)
{
  assert(page != NULL);
  page->m_next = m_page_freelist;
  m_tot_used_buffer_memory -= SendBufferPage::PGSIZE;
  m_page_freelist = page;
}

Uint32
TransporterCallbackKernelNonMT::get_bytes_to_send_iovec(NodeId node,
                                                        TrpId trp_id,
                                                        struct iovec *dst,
                                                        Uint32 max)
{
  (void)node;
  SendBuffer *b = m_send_buffers + trp_id;

  if (unlikely(!b->m_enabled))
  {
    discard_send_buffer(trp_id);
    return 0;
  }
  if (unlikely(max == 0))
    return 0;

  Uint32 count = 0;
  SendBufferPage *page = b->m_first_page;
  while (page != NULL && count < max)
  {
    dst[count].iov_base = page->m_data+page->m_start;
    dst[count].iov_len = page->m_bytes;
    assert(page->m_start + page->m_bytes <= page->max_data_bytes());
    page = page->m_next;
    count++;
  }

  return count;
}

Uint32
TransporterCallbackKernelNonMT::bytes_sent(NodeId nodeId,
                                           TrpId trp_id,
                                           Uint32 bytes)
{
  (void)nodeId;
  SendBuffer *b = m_send_buffers + trp_id;
  Uint32 used_bytes = b->m_used_bytes;

  if (bytes == 0)
    return used_bytes;

  used_bytes -= bytes;
  b->m_used_bytes = used_bytes;

  SendBufferPage *page = b->m_first_page;
  while (bytes && bytes >= page->m_bytes)
  {
    SendBufferPage * tmp = page;
    bytes -= page->m_bytes;
    page = page->m_next;
    release_page(tmp);
  }

  if (used_bytes == 0)
  {
    b->m_first_page = 0;
    b->m_last_page = 0;
  }
  else
  {
    page->m_start += bytes;
    page->m_bytes -= bytes;
    assert(page->m_start + page->m_bytes <= page->max_data_bytes());
    b->m_first_page = page;
  }

  return used_bytes;
}

void
TransporterCallbackKernelNonMT::enable_send_buffer(NodeId nodeId, TrpId trp_id)
{
  (void)nodeId;
  SendBuffer *b = m_send_buffers + trp_id;
  assert(b->m_enabled == false);
  assert(b->m_first_page == NULL);  //Disabled buffer is empty
  b->m_enabled = true;
}

void
TransporterCallbackKernelNonMT::disable_send_buffer(NodeId nodeId,
                                                    TrpId trp_id)
{
  (void)nodeId;
  SendBuffer *b = m_send_buffers + trp_id;
  b->m_enabled = false;
  discard_send_buffer(trp_id);
}

void
TransporterCallbackKernelNonMT::discard_send_buffer(TrpId trp_id)
{
  SendBuffer *b = m_send_buffers + trp_id;
  SendBufferPage *page = b->m_first_page;
  while (page != NULL)
  {
    SendBufferPage *next = page->m_next;
    release_page(page);
    page = next;
  }
  b->m_first_page = NULL;
  b->m_last_page = NULL;
  b->m_used_bytes = 0;
}

/**
 * These are the TransporterSendBufferHandle methods used by the
 * single-threaded ndbd.
 */
Uint32 *
TransporterCallbackKernelNonMT::getWritePtr(NodeId nodeId,
                                            TrpId trp_id,
                                            Uint32 lenBytes,
                                            Uint32 prio,
                                            Uint32 max_use,
                                            SendStatus *error)
{
  (void)nodeId;
  SendBuffer *b = m_send_buffers + trp_id;

  /* First check if we have room in already allocated page. */
  SendBufferPage *page = b->m_last_page;
  if (page != NULL &&
      page->m_bytes + page->m_start + lenBytes <= page->max_data_bytes())
  {
    return (Uint32 *)(page->m_data + page->m_start + page->m_bytes);
  }

  if (unlikely(b->m_used_bytes + lenBytes > max_use))
  {
    *error = SEND_BUFFER_FULL;
    return NULL;
  }

  if (unlikely(lenBytes > SendBufferPage::max_data_bytes()))
  {
    *error = SEND_MESSAGE_TOO_BIG;
    return NULL;
  }

  /* Allocate a new page. */
  page = alloc_page();
  if (unlikely(page == NULL))
  {
    *error = SEND_BUFFER_FULL;
    return NULL;
  }
  page->m_next = NULL;
  page->m_bytes = 0;
  page->m_start = 0;

  if (b->m_last_page == NULL)
  {
    b->m_first_page = page;
    b->m_last_page = page;
  }
  else
  {
    assert(b->m_first_page != NULL);
    b->m_last_page->m_next = page;
    b->m_last_page = page;
  }
  return (Uint32 *)(page->m_data);
}

Uint32
TransporterCallbackKernelNonMT::updateWritePtr(NodeId nodeId,
                                               TrpId trp_id,
                                               Uint32 lenBytes,
                                               Uint32 prio)
{
  (void)nodeId;
  SendBuffer *b = m_send_buffers + trp_id;
  SendBufferPage *page = b->m_last_page;
  assert(page != NULL);
  assert(page->m_bytes + lenBytes <= page->max_data_bytes());
  page->m_bytes += lenBytes;
  b->m_used_bytes += lenBytes;
  return b->m_used_bytes;
}

/**
 * This is used by the ndbd, so here only one thread is using this, so
 * values will always be consistent.
 */
void
TransporterCallbackKernelNonMT::getSendBufferLevel(NodeId nodeId,
                                                   SB_LevelType &level)
{
  TrpId trp_ids;
  Uint32 num_ids;
  globalTransporterRegistry.get_trps_for_node(nodeId,
                                              &trp_ids,
                                              num_ids,
                                              1);
  SendBuffer *b = m_send_buffers + trp_ids;
  calculate_send_buffer_level(b->m_used_bytes,
                              m_tot_send_buffer_memory,
                              m_tot_used_buffer_memory,
                              0,
                              level);
  return;
}

bool
TransporterCallbackKernelNonMT::forceSend(NodeId nodeId, TrpId trp_id)
{
  (void)nodeId;
  return globalTransporterRegistry.performSend(trp_id);
}

#endif //'not NDBD_MULTITHREADED'

/**
 * Report average receive length in bytes (4096 last receives)
 */
void
TransporterReceiveHandleKernel::reportReceiveLen(NodeId nodeId, Uint32 count,
                                            Uint64 bytes)
{

  SignalT<3> signal;
  std::memset(&signal.header, 0, sizeof(signal.header));

  signal.header.theLength = 3;  
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.header.theReceiversBlockNumber = CMVMI;
  signal.header.theVerId_signalNumber = GSN_EVENT_REP;

  signal.theData[0] = NDB_LE_ReceiveBytesStatistic;
  signal.theData[1] = nodeId;
  signal.theData[2] = Uint32(bytes/count);
#ifndef NDBD_MULTITHREADED
  Uint32 secPtr[3];
  globalScheduler.execute(&signal.header, JBA, signal.theData, secPtr);
#else
  sendprioa(m_thr_no /* self */,
            &signal.header, signal.theData, NULL);
#endif
}

/**
 * Report connection established
 */

void
TransporterReceiveHandleKernel::reportConnect(NodeId nodeId)
{

  SignalT<1> signal;
  std::memset(&signal.header, 0, sizeof(signal.header));

#ifndef NDBD_MULTITHREADED
  Uint32 trpman_instance = 1;
#else
  Uint32 trpman_instance = 1 /* proxy */ + m_receiver_thread_idx;
#endif
  signal.header.theLength = 1;
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.header.theReceiversBlockNumber = numberToBlock(TRPMAN,trpman_instance);
  signal.header.theVerId_signalNumber = GSN_CONNECT_REP;

  signal.theData[0] = nodeId;

#ifndef NDBD_MULTITHREADED
  Uint32 secPtr[3];
  globalScheduler.execute(&signal.header, JBA, signal.theData, secPtr);
#else
  /**
   * The first argument to sendprioa is from which thread number this
   * signal is sent, it is always sent from a receive thread
   */
  sendprioa(m_thr_no /* self */,
            &signal.header, signal.theData, NULL);
#endif
}

/**
 * Report connection broken
 */
void
TransporterReceiveHandleKernel::reportDisconnect(NodeId nodeId, Uint32 errNo)
{
  DBUG_ENTER("reportDisconnect");

  SignalT<DisconnectRep::SignalLength> signal;
  std::memset(&signal.header, 0, sizeof(signal.header));

#ifndef NDBD_MULTITHREADED
  Uint32 trpman_instance = 1;
#else
  Uint32 trpman_instance = 1 /* proxy */ + m_receiver_thread_idx;
#endif
  signal.header.theLength = DisconnectRep::SignalLength;
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.header.theTrace = TestOrd::TraceDisconnect;
  signal.header.theVerId_signalNumber = GSN_DISCONNECT_REP;
  signal.header.theReceiversBlockNumber = numberToBlock(TRPMAN,trpman_instance);

  DisconnectRep * rep = CAST_PTR(DisconnectRep, &signal.theData[0]);
  rep->nodeId = nodeId;
  rep->err = errNo;

#ifndef NDBD_MULTITHREADED
  Uint32 secPtr[3];
  globalScheduler.execute(&signal.header, JBA, signal.theData, secPtr);
#else
  sendprioa(m_thr_no /* self */,
            &signal.header, signal.theData, NULL);
#endif

  DBUG_VOID_RETURN;
}

void
SignalLoggerManager::printSegmentedSection(FILE * output,
                                           const SignalHeader & sh,
                                           const SegmentedSectionPtr ptr[3],
                                           unsigned i)
{
  fprintf(output, "SECTION %u type=segmented", i);
  if (i >= 3) {
    fprintf(output, " *** invalid ***\n");
    return;
  }
  const Uint32 len = ptr[i].sz;
  SectionSegment * ssp = ptr[i].p;
  Uint32 pos = 0;
  fprintf(output, " size=%u\n", (unsigned)len);
  while (pos < len) {
    if (pos > 0 && pos % SectionSegment::DataLength == 0) {
      ssp = g_sectionSegmentPool.getPtr(ssp->m_nextSegment);
    }
    printDataWord(output, pos, ssp->theData[pos % SectionSegment::DataLength]);
  }
  if (len > 0)
    putc('\n', output);
}

/**
 * Check to see if jobbbuffers are starting to get full
 * and if so call doJob
 */
int
TransporterReceiveHandleKernel::checkJobBuffer()
{
#ifndef NDBD_MULTITHREADED
  return globalScheduler.checkDoJob();
#else
  return mt_checkDoJob(m_receiver_thread_idx);
#endif
}

#ifdef NDBD_MULTITHREADED
void
TransporterReceiveHandleKernel::assign_trps(Uint32 *recv_thread_idx_array)
{
  m_transporters.clear(); /* Clear all first */
  for (Uint32 trp_id = 1; trp_id < MAX_NTRANSPORTERS; trp_id++)
  {
    if (recv_thread_idx_array[trp_id] == m_receiver_thread_idx)
    {
      DEB_MULTI_TRP(("trp_id %u assigned to recv thread %u",
                     trp_id, m_receiver_thread_idx));
      m_transporters.set(trp_id); /* Belongs to our receive thread */
    }
  }
  return;
}
#endif

void
TransporterReceiveHandleKernel::transporter_recv_from(NodeId nodeId)
{
  if (globalData.get_hb_count(nodeId) != 0)
  {
    globalData.set_hb_count(nodeId) = 0;
  }
}

#ifndef NDBD_MULTITHREADED
class TransporterReceiveHandle *
mt_get_trp_receive_handle(unsigned instance)
{
  assert(instance == 0);
  return &myTransporterCallback;
}
#endif

/** 
 * #undef is needed since this file is included by TransporterCallback_nonmt.cpp
 * and TransporterCallback_mt.cpp
 */
#undef JAM_FILE_ID
