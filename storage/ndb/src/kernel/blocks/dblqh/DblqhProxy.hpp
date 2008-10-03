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
#include <signaldata/AlterTab.hpp>
#include <signaldata/StartRec.hpp>
#include <signaldata/LqhTransReq.hpp>
#include <signaldata/LqhTransConf.hpp>
#include <signaldata/EmptyLcp.hpp>

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

  // GSN_LQHFRAGREQ [ pass-through ]
  void execLQHFRAGREQ(Signal*);

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

  // GSN_ALTER_TAB_REQ
  struct Ss_ALTER_TAB_REQ : SsParallel {
    AlterTabReq m_req;
    Uint32 m_sections;
    // wl4391_todo check max length in various cases
    enum { MaxSection0 = 2 * MAX_ATTRIBUTES_IN_TABLE };
    Uint32 m_sz0;
    Uint32 m_section0[MaxSection0];
    Ss_ALTER_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendALTER_TAB_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendALTER_TAB_CONF;
      m_sections = 0;
      m_sz0 = 0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_ALTER_TAB_REQ;
    }
  };
  SsPool<Ss_ALTER_TAB_REQ> c_ss_ALTER_TAB_REQ;
  Uint32 getSsId(const AlterTabReq* req) {
    return SsIdBase | req->tableId;
  }
  Uint32 getSsId(const AlterTabConf* conf) {
    return conf->senderData;
  }
  Uint32 getSsId(const AlterTabRef* ref) {
    return ref->senderData;
  }
  void execALTER_TAB_REQ(Signal*);
  void sendALTER_TAB_REQ(Signal*, Uint32 ssId);
  void execALTER_TAB_CONF(Signal*);
  void execALTER_TAB_REF(Signal*);
  void sendALTER_TAB_CONF(Signal*, Uint32 ssId);

  /**
   * GSN_START_FRAGREQ needs to be serialized wrt START_RECREQ
   *   so send it via proxy, even if DIH knows where to send it...
   */
  void execSTART_FRAGREQ(Signal*);

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

  // GSN_LQH_TRANSREQ
  struct Ss_LQH_TRANSREQ : SsParallel {
    LqhTransReq m_req;
    LqhTransConf m_conf; // latest conf
    Ss_LQH_TRANSREQ() {
      m_sendREQ = (SsFUNC)&DblqhProxy::sendLQH_TRANSREQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendLQH_TRANSCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LQH_TRANSREQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_LQH_TRANSREQ;
    }
  };
  SsPool<Ss_LQH_TRANSREQ> c_ss_LQH_TRANSREQ;
  void execLQH_TRANSREQ(Signal*);
  void sendLQH_TRANSREQ(Signal*, Uint32 ssId);
  void execLQH_TRANSCONF(Signal*);
  void sendLQH_TRANSCONF(Signal*, Uint32 ssId);

  // GSN_EMPTY_LCP_REQ
  struct Ss_EMPTY_LCP_REQ : SsParallel {
    EmptyLcpReq m_req;
    EmptyLcpConf m_conf; // build final conf here
    Ss_EMPTY_LCP_REQ() {
      m_conf.idle = 1;
      m_sendREQ = (SsFUNC)&DblqhProxy::sendEMPTY_LCP_REQ;
      m_sendCONF = (SsFUNC)&DblqhProxy::sendEMPTY_LCP_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_EMPTY_LCP_REQ>& pool(LocalProxy* proxy) {
      return ((DblqhProxy*)proxy)->c_ss_EMPTY_LCP_REQ;
    }
  };
  SsPool<Ss_EMPTY_LCP_REQ> c_ss_EMPTY_LCP_REQ;
  void execEMPTY_LCP_REQ(Signal*);
  void sendEMPTY_LCP_REQ(Signal*, Uint32 ssId);
  void execEMPTY_LCP_CONF(Signal*);
  void execEMPTY_LCP_REF(Signal*);
  void sendEMPTY_LCP_CONF(Signal*, Uint32 ssId);
};

#endif
