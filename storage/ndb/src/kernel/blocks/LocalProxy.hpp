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
  Uint32 c_threads;
  SimulatedBlock* c_worker[MaxWorkers];

  virtual SimulatedBlock* newWorker(Uint32 instanceNo) = 0;

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
  }

  template <class Ss>
  void ssRelease(Ss& ss) {
    ssRelease<Ss>(ss.m_ssId);
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
};

#endif
