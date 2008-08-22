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

#ifndef NDB_DBLQH_PROXY_HPP
#define NDB_DBLQH_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/LqhFrag.hpp>
#include <signaldata/TabCommit.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/GCP.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/StartRec.hpp>

class DblqhProxy : public LocalProxy {
public:
  DblqhProxy(Block_context& ctx);
  virtual ~DblqhProxy();
  BLOCK_DEFINES(DblqhProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // GSN_NDB_STTOR
  virtual void callNDB_STTOR(Signal*);

  // GSN_CREATE_TAB_REQ
  struct Ss_CREATE_TAB_REQ : SsParallel {
    CreateTabReq m_req;
    Uint32 m_lqhConnectPtr[MaxWorkers];
    Ss_CREATE_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendCREATE_TAB_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendCREATE_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CREATE_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_CREATE_TAB_REQ;
    }
  };
  SsPool<Ss_CREATE_TAB_REQ> c_ss_CREATE_TAB_REQ;
  void execCREATE_TAB_REQ(Signal*);
  void sendCREATE_TAB_REQ(Signal*, Uint32 ssId);
  void execCREATE_TAB_CONF(Signal*);
  void execCREATE_TAB_REF(Signal*);
  void sendCREATE_TAB_CONF(Signal*, Uint32 ssId);

  // GSN_LQHADDATTREQ [ sub-op ]
  struct Ss_LQHADDATTREQ : SsParallel {
    LqhAddAttrReq m_req;
    Uint32 m_reqlength;
    Ss_LQHADDATTREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendLQHADDATTREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendLQHADDATTCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LQHADDATTREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_LQHADDATTREQ;
    }
  };
  SsPool<Ss_LQHADDATTREQ> c_ss_LQHADDATTREQ;
  void execLQHADDATTREQ(Signal*);
  void sendLQHADDATTREQ(Signal*, Uint32 ssId);
  void execLQHADDATTCONF(Signal*);
  void execLQHADDATTREF(Signal*);
  void sendLQHADDATTCONF(Signal*, Uint32 ssId);

  // GSN_LQHFRAGREQ [ sub-op ]
  struct Ss_LQHFRAGREQ : SsParallel {
    LqhFragReq m_req;
    Ss_LQHFRAGREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendLQHFRAGREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendLQHFRAGCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LQHFRAGREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_LQHFRAGREQ;
    }
  };
  SsPool<Ss_LQHFRAGREQ> c_ss_LQHFRAGREQ;
  void execLQHFRAGREQ(Signal*);
  void sendLQHFRAGREQ(Signal*, Uint32 ssId);
  void execLQHFRAGCONF(Signal*);
  void execLQHFRAGREF(Signal*);
  void sendLQHFRAGCONF(Signal*, Uint32 ssId);

  // GSN_TAB_COMMITREQ [ sub-op ]
  struct Ss_TAB_COMMITREQ : SsParallel {
    TabCommitReq m_req;
    Ss_TAB_COMMITREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendTAB_COMMITREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendTAB_COMMITCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TAB_COMMITREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_TAB_COMMITREQ;
    }
  };
  SsPool<Ss_TAB_COMMITREQ> c_ss_TAB_COMMITREQ;
  void execTAB_COMMITREQ(Signal*);
  void sendTAB_COMMITREQ(Signal*, Uint32 ssId);
  void execTAB_COMMITCONF(Signal*);
  void execTAB_COMMITREF(Signal*);
  void sendTAB_COMMITCONF(Signal*, Uint32 ssId);

  // GSN_LCP_FRAG_ORD
  void execLCP_FRAG_ORD(Signal*);

  // GSN_LCP_COMPLETE_ORD [ fictional gsn ]
  struct Ss_LCP_COMPLETE_ORD : SsParallel {
    LcpFragOrd m_req;
    Ss_LCP_COMPLETE_ORD(){
      m_sendREQ = (SsFUNC)&DblqhProxy::sendLCP_COMPLETE_ORD;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendLCP_COMPLETE_REP;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LCP_COMPLETE_ORD>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_LCP_COMPLETE_ORD;
    }
  };
  SsPool<Ss_LCP_COMPLETE_ORD> c_ss_LCP_COMPLETE_ORD;
  static Uint32 getSsId(const LcpFragOrd* req) {
    return SsIdBase | (req->lcpId & 0xFFFF);
  }
  static Uint32 getSsId(const LcpCompleteRep* conf) {
    return SsIdBase | (conf->lcpId & 0xFFFF);
  }
  void execLCP_COMPLETE_ORD(Signal*);
  void sendLCP_COMPLETE_ORD(Signal*, Uint32 ssId);
  void execLCP_COMPLETE_REP(Signal*);
  void sendLCP_COMPLETE_REP(Signal*, Uint32 ssId);

  // GSN_GCP_SAVEREQ
  struct Ss_GCP_SAVEREQ : SsParallel {
    GCPSaveReq m_req;
    Ss_GCP_SAVEREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendGCP_SAVEREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendGCP_SAVECONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_GCP_SAVEREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_GCP_SAVEREQ;
    }
  };
  SsPool<Ss_GCP_SAVEREQ> c_ss_GCP_SAVEREQ;
  Uint32 getSsId(const GCPSaveReq* req) {
    return SsIdBase | (req->gci & 0xFFFF);
  }
  Uint32 getSsId(const GCPSaveConf* conf) {
    return SsIdBase | (conf->gci & 0xFFFF);
  }
  Uint32 getSsId(const GCPSaveRef* ref) {
    return SsIdBase | (ref->gci & 0xFFFF);
  }
  void execGCP_SAVEREQ(Signal*);
  void sendGCP_SAVEREQ(Signal*, Uint32 ssId);
  void execGCP_SAVECONF(Signal*);
  void execGCP_SAVEREF(Signal*);
  void sendGCP_SAVECONF(Signal*, Uint32 ssId);

  // GSN_PREP_DROP_TAB_REQ
  struct Ss_PREP_DROP_TAB_REQ : SsParallel {
    PrepDropTabReq m_req;
    Ss_PREP_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendPREP_DROP_TAB_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendPREP_DROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_PREP_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_PREP_DROP_TAB_REQ;
    }
  };
  SsPool<Ss_PREP_DROP_TAB_REQ> c_ss_PREP_DROP_TAB_REQ;
  Uint32 getSsId(const PrepDropTabReq* req) {
    return SsIdBase | req->tableId;
  }
  Uint32 getSsId(const PrepDropTabConf* conf) {
    return SsIdBase | conf->tableId;
  }
  Uint32 getSsId(const PrepDropTabRef* ref) {
    return SsIdBase | ref->tableId;
  }
  void execPREP_DROP_TAB_REQ(Signal*);
  void sendPREP_DROP_TAB_REQ(Signal*, Uint32 ssId);
  void execPREP_DROP_TAB_CONF(Signal*);
  void execPREP_DROP_TAB_REF(Signal*);
  void sendPREP_DROP_TAB_CONF(Signal*, Uint32 ssId);

  // GSN_DROP_TAB_REQ
  struct Ss_DROP_TAB_REQ : SsParallel {
    DropTabReq m_req;
    Ss_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendDROP_TAB_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendDROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_DROP_TAB_REQ;
    }
  };
  SsPool<Ss_DROP_TAB_REQ> c_ss_DROP_TAB_REQ;
  Uint32 getSsId(const DropTabReq* req) {
    return SsIdBase | req->tableId;
  }
  Uint32 getSsId(const DropTabConf* conf) {
    return SsIdBase | conf->tableId;
  }
  Uint32 getSsId(const DropTabRef* ref) {
    return SsIdBase | ref->tableId;
  }
  void execDROP_TAB_REQ(Signal*);
  void sendDROP_TAB_REQ(Signal*, Uint32 ssId);
  void execDROP_TAB_CONF(Signal*);
  void execDROP_TAB_REF(Signal*);
  void sendDROP_TAB_CONF(Signal*, Uint32 ssId);

  // GSN_START_RECREQ
  struct Ss_START_RECREQ : SsParallel {
    StartRecReq m_req;
    Ss_START_RECREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendSTART_RECREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendSTART_RECCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_START_RECREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_START_RECREQ;
    }
  };
  SsPool<Ss_START_RECREQ> c_ss_START_RECREQ;
  void execSTART_RECREQ(Signal*);
  void sendSTART_RECREQ(Signal*, Uint32 ssId);
  void execSTART_RECCONF(Signal*);
  void sendSTART_RECCONF(Signal*, Uint32 ssId);
};

#endif
