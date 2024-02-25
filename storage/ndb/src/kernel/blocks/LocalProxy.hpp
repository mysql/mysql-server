/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_LOCAL_PROXY_HPP
#define NDB_LOCAL_PROXY_HPP

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <Bitmask.hpp>
#include <IntrusiveList.hpp>
#include <signaldata/ReadConfig.hpp>
#include <signaldata/NdbSttor.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/Sync.hpp>

#define JAM_FILE_ID 438


/*
 * Proxy blocks for MT LQH.
 *
 * The LQH proxy is the LQH block seen by other nodes and blocks,
 * unless by-passed for efficiency.  Real LQH instances (workers)
 * run behind it.  The instance number is 1 + worker index.
 *
 * There are also proxies and workers for ACC, TUP, TUX, BACKUP,
 * RESTORE, and PGMAN.  Proxy classes are subclasses of LocalProxy.
 * Workers with same instance number (one from each class) run in
 * same thread.
 *
 * After LQH workers there is an optional extra worker.  It runs
 * in the thread of the main block (i.e. the proxy).  Its instance
 * number is fixed as 1 + MaxLqhWorkers (currently 5) i.e. it skips
 * over any unused LQH instance numbers.
 */

class LocalProxy : public SimulatedBlock {
public:
  LocalProxy(BlockNumber blockNumber, Block_context& ctx);
  ~LocalProxy() override;
  BLOCK_DEFINES(LocalProxy);

protected:
  enum { MaxWorkers = SimulatedBlock::MaxInstances };
  typedef Bitmask<(MaxWorkers+31)/32> WorkerMask;
  Uint32 c_workers;
  // no gaps - extra worker has index c_lqhWorkers (not MaxLqhWorkers)
  SimulatedBlock* c_worker[MaxWorkers];
  Uint32 c_anyWorkerCounter;

  virtual SimulatedBlock* newWorker(Uint32 instanceNo) = 0;
  void loadWorkers() override;

  // get worker block by index (not by instance)

  SimulatedBlock* workerBlock(Uint32 i) {
    ndbrequire(i < c_workers);
    ndbrequire(c_worker[i] != 0);
    return c_worker[i];
  }

  // get worker block reference by index (not by instance)

  BlockReference workerRef(Uint32 i) {
    return numberToRef(number(), workerInstance(i), getOwnNodeId());
  }

  // convert between worker index and worker instance

  Uint32 workerInstance(Uint32 i) const {
    ndbrequire(i < c_workers);
    return i + 1;
  }

  Uint32 workerIndex(Uint32 ino) const {
    ndbrequire(ino != 0);
    return ino - 1;
  }

  // Get a worker index - will balance round robin across
  // workers over time.
  Uint32 getAnyWorkerIndex()
  {
    return (c_anyWorkerCounter++) % c_workers;
  }

  // Statelessly forward a signal (including any sections) 
  // to the worker with the supplied index.
  void forwardToWorkerIndex(Signal* signal, Uint32 index);
  
  // Statelessly forward the signal (including any sections)
  // to one of the workers, load balancing.
  // Requires no arrival order constraints between signals.
  void forwardToAnyWorker(Signal* signal);

  // support routines and classes ("Ss" = signal state)

  typedef void (LocalProxy::*SsFUNCREQ)(Signal*, Uint32 ssId, SectionHandle*);
  typedef void (LocalProxy::*SsFUNCREP)(Signal*, Uint32 ssId);

  struct SsCommon {
    Uint32 m_ssId;      // unique id in SsPool (below)
    SsFUNCREQ m_sendREQ;   // from proxy to worker
    SsFUNCREP m_sendCONF;  // from proxy to caller
    Uint32 m_worker;    // current worker
    Uint32 m_error;
    Uint32 m_sec_cnt;
    Uint32 m_sec_ptr[3];
    static const char* name() { return "UNDEF"; }
    SsCommon() {
      m_ssId = 0;
      m_sendREQ = 0;
      m_sendCONF = 0;
      m_worker = 0;
      m_error = 0;
      m_sec_cnt = 0;
    }
  };

  // run workers sequentially
  struct SsSequential : SsCommon {
    SsSequential() {}
  };
  void sendREQ(Signal*, SsSequential& ss);
  void recvCONF(Signal*, SsSequential& ss);
  void recvREF(Signal*, SsSequential& ss, Uint32 error);
  // for use in sendREQ
  void skipReq(SsSequential& ss);
  void skipConf(SsSequential& ss);
  // for use in sendCONF
  bool firstReply(const SsSequential& ss);
  bool lastReply(const SsSequential& ss);

  void saveSections(SsCommon&ss, SectionHandle&);
  void restoreHandle(SectionHandle&, SsCommon&);

  // run workers in parallel
  struct SsParallel : SsCommon {
    WorkerMask m_workerMask;
    SsParallel() {
    }
  };
  void sendREQ(Signal*, SsParallel& ss, bool skipLast = false);
  void recvCONF(Signal*, SsParallel& ss);
  void recvREF(Signal*, SsParallel& ss, Uint32 error);
  // for use in sendREQ
  void skipReq(SsParallel& ss);
  void skipConf(SsParallel& ss);
  // for use in sendCONF
  bool firstReply(const SsParallel& ss);
  bool lastReply(const SsParallel& ss);
  bool lastExtra(Signal* signal, SsParallel& ss);
  // set all or given bits in worker mask
  void setMask(SsParallel& ss);
  void setMask(SsParallel& ss, const WorkerMask& mask);

  /*
   * Ss instances are seized from a pool.  Each pool is simply an array
   * of Ss instances.  Usually poolSize is 1.  Some signals need a few
   * more but the heavy stuff (query/DML) by-passes the proxy.
   *
   * Each Ss instance has a unique Uint32 ssId.  If there are multiple
   * instances then ssId must be computable from signal data.  One option
   * often is to use a generated ssId and set it as senderData,
   */

  template <class Ss>
  struct SsPool {
    Ss m_pool[Ss::poolSize];
    Uint32 m_usage;
    SsPool() {
      m_usage = 0;
    }
  };

  Uint32 c_ssIdSeq;

  // convenient for adding non-zero high bit
  enum { SsIdBase = (1u << 31) };

  template <class Ss>
  Ss* ssSearch(Uint32 ssId)
  {
    SsPool<Ss>& sp = Ss::pool(this);
    Ss* ssptr = 0;
    for (Uint32 i = 0; i < Ss::poolSize; i++) {
      if (sp.m_pool[i].m_ssId == ssId) {
        ssptr = &sp.m_pool[i];
        break;
      }
    }
    return ssptr;
  }

  template <class Ss>
  Ss& ssSeize() {
    SsPool<Ss>& sp = Ss::pool(this);
    Ss* ssptr = ssSearch<Ss>(0);
    ndbrequire(ssptr != 0);
    // Use position in array as ssId
    UintPtr pos = ssptr - sp.m_pool;
    Uint32 ssId = Uint32(pos) + 1;
    new (ssptr) Ss;
    ssptr->m_ssId = ssId;
    sp.m_usage++;
    //D("ssSeize()" << V(sp.m_usage) << hex << V(ssId) << " " << Ss::name());
    return *ssptr;
  }

  template <class Ss>
  Ss& ssSeize(Uint32 ssId) {
    SsPool<Ss>& sp = Ss::pool(this);
    ndbrequire(sp.m_usage < Ss::poolSize);
    ndbrequire(ssId != 0);
    Ss* ssptr;
    // check for duplicate
    ssptr = ssSearch<Ss>(ssId);
    ndbrequire(ssptr == 0);
    // search for free
    ssptr = ssSearch<Ss>(0);
    ndbrequire(ssptr != 0);
    // set methods, clear bitmasks, etc
    new (ssptr) Ss;
    ssptr->m_ssId = ssId;
    sp.m_usage++;
    //D("ssSeize" << V(sp.m_usage) << hex << V(ssId) << " " << Ss::name());
    return *ssptr;
  }

  template <class Ss>
  Ss& ssFind(Uint32 ssId) {
    ndbrequire(ssId != 0);
    Ss* ssptr = ssSearch<Ss>(ssId);
    ndbrequire(ssptr != 0);
    return *ssptr;
  }

  /*
   * In some cases it may not be known if this is first request.
   * This situation should be avoided by adding signal data or
   * by keeping state in the proxy instance.
   */
  template <class Ss>
  Ss& ssFindSeize(Uint32 ssId, bool* found) {
    ndbrequire(ssId != 0);
    Ss* ssptr = ssSearch<Ss>(ssId);
    if (ssptr != 0) {
      if (found)
        *found = true;
      return *ssptr;
    }
    if (found)
      *found = false;
    return ssSeize<Ss>(ssId);
  }

  template <class Ss>
  void ssRelease(Uint32 ssId) {
    SsPool<Ss>& sp = Ss::pool(this);
    ndbrequire(sp.m_usage != 0);
    ndbrequire(ssId != 0);
    //D("ssRelease" << V(sp.m_usage) << hex << V(ssId) << " " << Ss::name());
    Ss* ssptr = ssSearch<Ss>(ssId);
    ndbrequire(ssptr != 0);
    ssptr->m_ssId = 0;
    ndbrequire(sp.m_usage > 0);
    sp.m_usage--;
  }

  template <class Ss>
  void ssRelease(Ss& ss) {
    ssRelease<Ss>(ss.m_ssId);
  }

  /*
   * In some cases handle pool full via delayed signal.
   * wl4391_todo maybe use CONTINUEB and guard against infinite loop.
   */
  template <class Ss>
  bool ssQueue(Signal* signal) {
    SsPool<Ss>& sp = Ss::pool(this);
    if (sp.m_usage < Ss::poolSize)
      return false;

    SectionHandle handle(this, signal);
    GlobalSignalNumber gsn = signal->header.theVerId_signalNumber & 0xFFFF;
    sendSignalWithDelay(reference(), gsn,
                        signal, 10, signal->length(), &handle);
    return true;
  }

  // system info

  Uint32 c_typeOfStart;
  Uint32 c_masterNodeId;

  // GSN_READ_CONFIG_REQ
  struct Ss_READ_CONFIG_REQ : SsSequential {
    ReadConfigReq m_req;
    Ss_READ_CONFIG_REQ() {
      m_sendREQ = &LocalProxy::sendREAD_CONFIG_REQ;
      m_sendCONF = &LocalProxy::sendREAD_CONFIG_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_READ_CONFIG_REQ>& pool(LocalProxy* proxy) {
      return proxy->c_ss_READ_CONFIG_REQ;
    }
  };
  SsPool<Ss_READ_CONFIG_REQ> c_ss_READ_CONFIG_REQ;
  void execREAD_CONFIG_REQ(Signal*);
  virtual void callREAD_CONFIG_REQ(Signal*);
  void backREAD_CONFIG_REQ(Signal*);
  void sendREAD_CONFIG_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execREAD_CONFIG_CONF(Signal*);
  void sendREAD_CONFIG_CONF(Signal*, Uint32 ssId);

  // GSN_STTOR
  struct Ss_STTOR : SsParallel {
    Uint32 m_reqlength;
    Uint32 m_reqdata[25];
    Uint32 m_conflength;
    Uint32 m_confdata[25];
    Ss_STTOR() {
      m_sendREQ = &LocalProxy::sendSTTOR;
      m_sendCONF = &LocalProxy::sendSTTORRY;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_STTOR>& pool(LocalProxy* proxy) {
      return proxy->c_ss_STTOR;
    }
  };
  SsPool<Ss_STTOR> c_ss_STTOR;
  void execSTTOR(Signal*);
  virtual void callSTTOR(Signal*);
  void backSTTOR(Signal*);
  void sendSTTOR(Signal*, Uint32 ssId, SectionHandle*);
  void execSTTORRY(Signal*);
  void sendSTTORRY(Signal*, Uint32 ssId);

  // GSN_NDB_STTOR
  struct Ss_NDB_STTOR : SsParallel {
    NdbSttor m_req;
    enum { m_reqlength = sizeof(NdbSttor) >> 2 };
    Ss_NDB_STTOR() {
      m_sendREQ = &LocalProxy::sendNDB_STTOR;
      m_sendCONF = &LocalProxy::sendNDB_STTORRY;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_NDB_STTOR>& pool(LocalProxy* proxy) {
      return proxy->c_ss_NDB_STTOR;
    }
  };
  SsPool<Ss_NDB_STTOR> c_ss_NDB_STTOR;
  void execNDB_STTOR(Signal*);
  virtual void callNDB_STTOR(Signal*);
  void backNDB_STTOR(Signal*);
  void sendNDB_STTOR(Signal*, Uint32 ssId, SectionHandle*);
  void execNDB_STTORRY(Signal*);
  void sendNDB_STTORRY(Signal*, Uint32 ssId);

  // GSN_READ_NODESREQ
  struct Ss_READ_NODES_REQ {
    GlobalSignalNumber m_gsn; // STTOR or NDB_STTOR
    Ss_READ_NODES_REQ() {
      m_gsn = 0;
    }
  };
  Ss_READ_NODES_REQ c_ss_READ_NODESREQ;
  void sendREAD_NODESREQ(Signal*);
  void execREAD_NODESCONF(Signal*);
  void execREAD_NODESREF(Signal*);

  // GSN_NODE_FAILREP
  struct Ss_NODE_FAILREP : SsParallel {
    NodeFailRep m_req;
    // REQ sends NdbNodeBitmask but CONF sends nodeId at a time
    NdbNodeBitmask m_waitFor[MaxWorkers];
    Ss_NODE_FAILREP() {
      m_sendREQ = &LocalProxy::sendNODE_FAILREP;
      m_sendCONF = &LocalProxy::sendNF_COMPLETEREP;
    }
    // some blocks do not reply
    static bool noReply(BlockNumber blockNo) {
      return
        blockNo == BACKUP;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_NODE_FAILREP>& pool(LocalProxy* proxy) {
      return proxy->c_ss_NODE_FAILREP;
    }
  };
  SsPool<Ss_NODE_FAILREP> c_ss_NODE_FAILREP;
  void execNODE_FAILREP(Signal*);
  void sendNODE_FAILREP(Signal*, Uint32 ssId, SectionHandle*);
  void execNF_COMPLETEREP(Signal*);
  void sendNF_COMPLETEREP(Signal*, Uint32 ssId);

  // GSN_INCL_NODEREQ
  struct Ss_INCL_NODEREQ : SsParallel {
    // future-proof by allocating max length
    struct Req {
      Uint32 senderRef;
      Uint32 inclNodeId;
      Uint32 word[23];
    };
    struct Conf {
      Uint32 inclNodeId;
      Uint32 senderRef;
    };
    Uint32 m_reqlength;
    Req m_req;
    Ss_INCL_NODEREQ() {
      m_sendREQ = &LocalProxy::sendINCL_NODEREQ;
      m_sendCONF = &LocalProxy::sendINCL_NODECONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_INCL_NODEREQ>& pool(LocalProxy* proxy) {
      return proxy->c_ss_INCL_NODEREQ;
    }
  };
  SsPool<Ss_INCL_NODEREQ> c_ss_INCL_NODEREQ;
  void execINCL_NODEREQ(Signal*);
  void sendINCL_NODEREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execINCL_NODECONF(Signal*);
  void sendINCL_NODECONF(Signal*, Uint32 ssId);

  // GSN_NODE_STATE_REP
  struct Ss_NODE_STATE_REP : SsParallel {
    Ss_NODE_STATE_REP() {
      m_sendREQ = &LocalProxy::sendNODE_STATE_REP;
      m_sendCONF = 0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_NODE_STATE_REP>& pool(LocalProxy* proxy) {
      return proxy->c_ss_NODE_STATE_REP;
    }
  };
  SsPool<Ss_NODE_STATE_REP> c_ss_NODE_STATE_REP;
  void execNODE_STATE_REP(Signal*);
  void sendNODE_STATE_REP(Signal*, Uint32 ssId, SectionHandle*);

  // GSN_CHANGE_NODE_STATE_REQ
  struct Ss_CHANGE_NODE_STATE_REQ : SsParallel {
    ChangeNodeStateReq m_req;
    Ss_CHANGE_NODE_STATE_REQ() {
      m_sendREQ = &LocalProxy::sendCHANGE_NODE_STATE_REQ;
      m_sendCONF = &LocalProxy::sendCHANGE_NODE_STATE_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CHANGE_NODE_STATE_REQ>& pool(LocalProxy* proxy) {
      return proxy->c_ss_CHANGE_NODE_STATE_REQ;
    }
  };
  SsPool<Ss_CHANGE_NODE_STATE_REQ> c_ss_CHANGE_NODE_STATE_REQ;
  void execCHANGE_NODE_STATE_REQ(Signal*);
  void sendCHANGE_NODE_STATE_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execCHANGE_NODE_STATE_CONF(Signal*);
  void sendCHANGE_NODE_STATE_CONF(Signal*, Uint32 ssId);

  // GSN_DUMP_STATE_ORD
  struct Ss_DUMP_STATE_ORD : SsParallel {
    Uint32 m_reqlength;
    Uint32 m_reqdata[25];
    Ss_DUMP_STATE_ORD() {
      m_sendREQ = &LocalProxy::sendDUMP_STATE_ORD;
      m_sendCONF = 0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DUMP_STATE_ORD>& pool(LocalProxy* proxy) {
      return proxy->c_ss_DUMP_STATE_ORD;
    }
  };
  SsPool<Ss_DUMP_STATE_ORD> c_ss_DUMP_STATE_ORD;
  void execDUMP_STATE_ORD(Signal*);
  void sendDUMP_STATE_ORD(Signal*, Uint32 ssId, SectionHandle*);

  // GSN_NDB_TAMPER
  struct Ss_NDB_TAMPER : SsParallel
  {
    Uint32 m_errorInsert;
    Uint32 m_errorInsertExtra;
    bool m_haveErrorInsertExtra;
    Ss_NDB_TAMPER()
    {
      m_sendREQ = &LocalProxy::sendNDB_TAMPER;
      m_sendCONF = 0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_NDB_TAMPER>& pool(LocalProxy* proxy) {
      return proxy->c_ss_NDB_TAMPER;
    }
  };
  SsPool<Ss_NDB_TAMPER> c_ss_NDB_TAMPER;
  void execNDB_TAMPER(Signal*);
  void sendNDB_TAMPER(Signal*, Uint32 ssId, SectionHandle*);

  // GSN_TIME_SIGNAL
  struct Ss_TIME_SIGNAL : SsParallel {
    Ss_TIME_SIGNAL() {
      m_sendREQ = &LocalProxy::sendTIME_SIGNAL;
      m_sendCONF = 0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TIME_SIGNAL>& pool(LocalProxy* proxy) {
      return proxy->c_ss_TIME_SIGNAL;
    }
  };
  SsPool<Ss_TIME_SIGNAL> c_ss_TIME_SIGNAL;
  void execTIME_SIGNAL(Signal*);
  void sendTIME_SIGNAL(Signal*, Uint32 ssId, SectionHandle*);

  // GSN_CREATE_TRIG_IMPL_REQ
  struct Ss_CREATE_TRIG_IMPL_REQ : SsParallel {
    CreateTrigImplReq m_req;
    Ss_CREATE_TRIG_IMPL_REQ() {
      m_sendREQ = &LocalProxy::sendCREATE_TRIG_IMPL_REQ;
      m_sendCONF = &LocalProxy::sendCREATE_TRIG_IMPL_CONF;
    }
    enum { poolSize = 3 };
    static SsPool<Ss_CREATE_TRIG_IMPL_REQ>& pool(LocalProxy* proxy) {
      return proxy->c_ss_CREATE_TRIG_IMPL_REQ;
    }
  };
  SsPool<Ss_CREATE_TRIG_IMPL_REQ> c_ss_CREATE_TRIG_IMPL_REQ;
  void execCREATE_TRIG_IMPL_REQ(Signal*);
  void sendCREATE_TRIG_IMPL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execCREATE_TRIG_IMPL_CONF(Signal*);
  void execCREATE_TRIG_IMPL_REF(Signal*);
  void sendCREATE_TRIG_IMPL_CONF(Signal*, Uint32 ssId);

  // GSN_DROP_TRIG_IMPL_REQ
  struct Ss_DROP_TRIG_IMPL_REQ : SsParallel {
    DropTrigImplReq m_req;
    Ss_DROP_TRIG_IMPL_REQ() {
      m_sendREQ = &LocalProxy::sendDROP_TRIG_IMPL_REQ;
      m_sendCONF = &LocalProxy::sendDROP_TRIG_IMPL_CONF;
    }
    enum { poolSize = NDB_MAX_PROXY_DROP_TRIG_IMPL_REQ };
    static SsPool<Ss_DROP_TRIG_IMPL_REQ>& pool(LocalProxy* proxy) {
      return proxy->c_ss_DROP_TRIG_IMPL_REQ;
    }
  };
  SsPool<Ss_DROP_TRIG_IMPL_REQ> c_ss_DROP_TRIG_IMPL_REQ;
  void execDROP_TRIG_IMPL_REQ(Signal*);
  void sendDROP_TRIG_IMPL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execDROP_TRIG_IMPL_CONF(Signal*);
  void execDROP_TRIG_IMPL_REF(Signal*);
  void sendDROP_TRIG_IMPL_CONF(Signal*, Uint32 ssId);

  // GSN_DBINFO_SCANREQ
  bool find_next(Ndbinfo::ScanCursor* cursor) const;
  void execDBINFO_SCANREQ(Signal*);
  void execDBINFO_SCANCONF(Signal*);

  // GSN_SYNC_REQ
  void execSYNC_REQ(Signal*);
  void execSYNC_REF(Signal*);
  void execSYNC_CONF(Signal*);
  void sendSYNC_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void sendSYNC_CONF(Signal*, Uint32 ssId);
  struct Ss_SYNC_REQ : SsParallel {
    SyncReq m_req;
    Ss_SYNC_REQ() {
      m_sendREQ = &LocalProxy::sendSYNC_REQ;
      m_sendCONF = &LocalProxy::sendSYNC_CONF;
    }
    enum { poolSize = 4 };
    static SsPool<Ss_SYNC_REQ>& pool(LocalProxy* proxy) {
      return proxy->c_ss_SYNC_REQ;
    }
  };
  SsPool<Ss_SYNC_REQ> c_ss_SYNC_REQ;

  void execSYNC_PATH_REQ(Signal*);

  // GSN_API_FAILREQ
  struct Ss_API_FAILREQ : SsParallel {
    Uint32 m_ref; //
    Ss_API_FAILREQ() {
      m_sendREQ = (SsFUNCREQ)&LocalProxy::sendAPI_FAILREQ;
      m_sendCONF = (SsFUNCREP)&LocalProxy::sendAPI_FAILCONF;
    }
    enum { poolSize = MAX_NODES };
    static SsPool<Ss_API_FAILREQ>& pool(LocalProxy* proxy) {
      return proxy->c_ss_API_FAILREQ;
    }
  };
  SsPool<Ss_API_FAILREQ> c_ss_API_FAILREQ;
  void execAPI_FAILREQ(Signal*);
  void sendAPI_FAILREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execAPI_FAILCONF(Signal*);
  void sendAPI_FAILCONF(Signal*, Uint32 ssId);
};


#undef JAM_FILE_ID

#endif
