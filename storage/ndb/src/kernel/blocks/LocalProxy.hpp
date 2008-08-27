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

#ifndef NDB_LOCAL_PROXY_HPP
#define NDB_LOCAL_PROXY_HPP

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <Bitmask.hpp>
#include <DLFifoList.hpp>
#include <signaldata/ReadConfig.hpp>
#include <signaldata/NdbSttor.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrigImpl.hpp>

/*
 * Proxy blocks for MT LQH.
 *
 * The LQH proxy is the LQH block seen by other nodes and blocks,
 * unless by-passed for efficiency.  Real LQH instances (workers)
 * run behind it.
 *
 * There are also ACC,TUP,TUX,BACKUP,RESTORE proxies and workers.
 * All proxy classes are subclasses of LocalProxy.
 */

class LocalProxy : public SimulatedBlock {
public:
  LocalProxy(BlockNumber blockNumber, Block_context& ctx);
  virtual ~LocalProxy();
  BLOCK_DEFINES(LocalProxy);

protected:
  enum { MaxWorkers = MAX_NDBMT_LQH_WORKERS };
  typedef Bitmask<MaxWorkers> WorkerMask;
  Uint32 c_workers;
  SimulatedBlock* c_worker[MaxWorkers];

  virtual SimulatedBlock* newWorker(Uint32 instanceNo) = 0;
  virtual void loadWorkers();

  // worker index to worker instance
  Uint32 workerInstance(Uint32 i) {
    ndbrequire(i < c_workers);
    return 1 + i;
  }

  // worker index to worker ref
  BlockReference workerRef(Uint32 i) {
    ndbrequire(i < c_workers);
    return numberToRef(number(), 1 + i, getOwnNodeId());
  }

  // support routines and classes ("Ss" = signal state)

  typedef void (LocalProxy::*SsFUNC)(Signal*, Uint32 ssId);

  struct SsCommon {
    Uint32 m_ssId;      // unique id in SsPool (below)
    SsFUNC m_sendREQ;   // from proxy to worker
    SsFUNC m_sendCONF;  // from proxy to caller
    Uint32 m_worker;    // current worker
    Uint32 m_error;
    SsCommon() {
      m_ssId = 0;
      m_sendREQ = 0;
      m_sendCONF = 0;
      m_worker = 0;
      m_error = 0;
    }
  };

  // run workers sequentially
  struct SsSequential : SsCommon {
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

  // run workers in parallel
  struct SsParallel : SsCommon {
    WorkerMask m_workerMask;
    SsParallel() {
      m_workerMask.clear();
    }
  };
  void sendREQ(Signal*, SsParallel& ss);
  void recvCONF(Signal*, SsParallel& ss);
  void recvREF(Signal*, SsParallel& ss, Uint32 error);
  // for use in sendREQ
  void skipReq(SsParallel& ss);
  void skipConf(SsParallel& ss);
  // for use in sendCONF
  bool firstReply(const SsParallel& ss);
  bool lastReply(const SsParallel& ss);

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

  // convenient for adding non-zero high nibble
  enum { SsIdBase = (1 << 28) };

  template <class Ss>
  Ss& ssSeize() {
    const Uint32 base = (1 << 28);
    const Uint32 mask = (1 << 28) - 1;
    Uint32 ssId = base | c_ssIdSeq;
    c_ssIdSeq = (c_ssIdSeq + 1) & mask;
    return ssSeize<Ss>(ssId);
  }

  template <class Ss>
  Ss& ssSeize(Uint32 ssId) {
    SsPool<Ss>& sp = Ss::pool(this);
    ndbrequire(ssId != 0);
    ndbrequire(sp.m_usage < Ss::poolSize);
    Ss* ssptr = 0;
    for (Uint32 i = 0; i < Ss::poolSize; i++) {
      Ss& ss = sp.m_pool[i];
      ndbrequire(ss.m_ssId != ssId);
      if (ss.m_ssId == 0 && ssptr == 0) {
        new (&ss) Ss;
        ss.m_ssId = ssId;
        ssptr = &ss;
        // keep looping to verify ssId is unique
      }
    }
    ndbrequire(ssptr != 0);
    sp.m_usage++;
    return *ssptr;
  }

  template <class Ss>
  Ss& ssFind(Uint32 ssId) {
    SsPool<Ss>& sp = Ss::pool(this);
    ndbrequire(ssId != 0);
    Ss* ssptr = 0;
    for (Uint32 i = 0; i < Ss::poolSize; i++) {
      Ss& ss = sp.m_pool[i];
      if (ss.m_ssId == ssId) {
        ssptr = &ss;
        break;
      }
    }
    ndbrequire(ssptr != 0);
    return *ssptr;
  }

  template <class Ss>
  void ssRelease(Uint32 ssId) {
    SsPool<Ss>& sp = Ss::pool(this);
    ndbrequire(sp.m_usage != 0);
    ndbrequire(ssId != 0);
    Ss* ssptr = 0;
    for (Uint32 i = 0; i < Ss::poolSize; i++) {
      Ss& ss = sp.m_pool[i];
      if (ss.m_ssId == ssId) {
        ss.m_ssId = 0;
        ssptr = &ss;
        break;
      }
    }
    ndbrequire(ssptr != 0);
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
    ndbrequire(signal->getNoOfSections() == 0);
    GlobalSignalNumber gsn = signal->header.theVerId_signalNumber & 0xFFFF;
    sendSignalWithDelay(reference(), gsn,
                        signal, 10, signal->length());
    return true;
  }

  // system info

  Uint32 c_typeOfStart;
  Uint32 c_masterNodeId;

  struct Node {
    Uint32 m_nodeId;
    bool m_alive;
    Node() {
      m_nodeId = 0;
      m_alive = false;
    }
    Uint32 nextList;
    union {
    Uint32 prevList;
    Uint32 nextPool;
    };
  };
  typedef Ptr<Node> NodePtr;
  ArrayPool<Node> c_nodePool;
  DLFifoList<Node> c_nodeList;

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
  void sendREAD_CONFIG_REQ(Signal*, Uint32 ssId);
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
  void sendSTTOR(Signal*, Uint32 ssId);
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
  void sendNDB_STTOR(Signal*, Uint32 ssId);
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
  void sendNODE_FAILREP(Signal*, Uint32 ssId);
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
  void sendINCL_NODEREQ(Signal*, Uint32 ssId);
  void execINCL_NODECONF(Signal*);
  void sendINCL_NODECONF(Signal*, Uint32 ssId);

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
  void sendDUMP_STATE_ORD(Signal*, Uint32 ssId);

  // GSN_NDB_TAMPER
  struct Ss_NDB_TAMPER : SsParallel {
    Uint32 m_errorInsert;
    Ss_NDB_TAMPER() {
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
  void sendNDB_TAMPER(Signal*, Uint32 ssId);

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
  void sendTIME_SIGNAL(Signal*, Uint32 ssId);

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
  void sendCREATE_TRIG_IMPL_REQ(Signal*, Uint32 ssId);
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
    enum { poolSize = 3 };
    static SsPool<Ss_DROP_TRIG_IMPL_REQ>& pool(LocalProxy* proxy) {
      return proxy->c_ss_DROP_TRIG_IMPL_REQ;
    }
  };
  SsPool<Ss_DROP_TRIG_IMPL_REQ> c_ss_DROP_TRIG_IMPL_REQ;
  void execDROP_TRIG_IMPL_REQ(Signal*);
  void sendDROP_TRIG_IMPL_REQ(Signal*, Uint32 ssId);
  void execDROP_TRIG_IMPL_CONF(Signal*);
  void execDROP_TRIG_IMPL_REF(Signal*);
  void sendDROP_TRIG_IMPL_CONF(Signal*, Uint32 ssId);
};

#endif
