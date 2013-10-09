/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_DBTC_PROXY_HPP
#define NDB_DBTC_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/TabCommit.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/GCP.hpp>

#include <signaldata/CreateIndxImpl.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/DropIndxImpl.hpp>

class DbtcProxy : public LocalProxy {
public:
  DbtcProxy(Block_context& ctx);
  virtual ~DbtcProxy();
  BLOCK_DEFINES(DbtcProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // GSN_NDB_STTOR
  virtual void callNDB_STTOR(Signal*);

  // GSN_TC_SCHVERREQ
  struct Ss_TC_SCHVERREQ : SsParallel {
    TcSchVerReq m_req;
    Ss_TC_SCHVERREQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendTC_SCHVERREQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendTC_SCHVERCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TC_SCHVERREQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_TC_SCHVERREQ;
    }
  };
  SsPool<Ss_TC_SCHVERREQ> c_ss_TC_SCHVERREQ;
  void execTC_SCHVERREQ(Signal*);
  void sendTC_SCHVERREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execTC_SCHVERCONF(Signal*);
  void sendTC_SCHVERCONF(Signal*, Uint32 ssId);

  // GSN_TAB_COMMITREQ [ sub-op ]
  struct Ss_TAB_COMMITREQ : SsParallel {
    TabCommitReq m_req;
    Ss_TAB_COMMITREQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendTAB_COMMITREQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendTAB_COMMITCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TAB_COMMITREQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_TAB_COMMITREQ;
    }
  };
  SsPool<Ss_TAB_COMMITREQ> c_ss_TAB_COMMITREQ;
  void execTAB_COMMITREQ(Signal*);
  void sendTAB_COMMITREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execTAB_COMMITCONF(Signal*);
  void execTAB_COMMITREF(Signal*);
  void sendTAB_COMMITCONF(Signal*, Uint32 ssId);

  // GSN_PREP_DROP_TAB_REQ
  struct Ss_PREP_DROP_TAB_REQ : SsParallel {
    PrepDropTabReq m_req;
    Ss_PREP_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendPREP_DROP_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendPREP_DROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_PREP_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_PREP_DROP_TAB_REQ;
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
  void sendPREP_DROP_TAB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execPREP_DROP_TAB_CONF(Signal*);
  void execPREP_DROP_TAB_REF(Signal*);
  void sendPREP_DROP_TAB_CONF(Signal*, Uint32 ssId);

  // GSN_DROP_TAB_REQ
  struct Ss_DROP_TAB_REQ : SsParallel {
    DropTabReq m_req;
    Ss_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendDROP_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendDROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_DROP_TAB_REQ;
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
  void sendDROP_TAB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execDROP_TAB_CONF(Signal*);
  void execDROP_TAB_REF(Signal*);
  void sendDROP_TAB_CONF(Signal*, Uint32 ssId);

  // GSN_ALTER_TAB_REQ
  struct Ss_ALTER_TAB_REQ : SsParallel {
    AlterTabReq m_req;
    Ss_ALTER_TAB_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendALTER_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendALTER_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_ALTER_TAB_REQ;
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
  void sendALTER_TAB_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execALTER_TAB_CONF(Signal*);
  void execALTER_TAB_REF(Signal*);
  void sendALTER_TAB_CONF(Signal*, Uint32 ssId);

  /**
   * TCSEIZEREQ
   */
  Uint32 m_tc_seize_req_instance; // round robin
  void execTCSEIZEREQ(Signal* signal);

  /**
   * TCGETOPSIZEREQ
   */
  struct Ss_TCGETOPSIZEREQ : SsParallel {
    Uint32 m_sum;
    Uint32 m_req[2];
    Ss_TCGETOPSIZEREQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendTCGETOPSIZEREQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendTCGETOPSIZECONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TCGETOPSIZEREQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_TCGETOPSIZEREQ;
    }
  };
  SsPool<Ss_TCGETOPSIZEREQ> c_ss_TCGETOPSIZEREQ;
  void execTCGETOPSIZEREQ(Signal*);
  void sendTCGETOPSIZEREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execTCGETOPSIZECONF(Signal*);
  void sendTCGETOPSIZECONF(Signal*, Uint32 ssId);

  /**
   * TC_CLOPSIZEREQ
   */
  struct Ss_TC_CLOPSIZEREQ : SsParallel {
    Uint32 m_req[2];
    Ss_TC_CLOPSIZEREQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendTC_CLOPSIZEREQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendTC_CLOPSIZECONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TC_CLOPSIZEREQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_TC_CLOPSIZEREQ;
    }
  };
  SsPool<Ss_TC_CLOPSIZEREQ> c_ss_TC_CLOPSIZEREQ;
  void execTC_CLOPSIZEREQ(Signal*);
  void sendTC_CLOPSIZEREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execTC_CLOPSIZECONF(Signal*);
  void sendTC_CLOPSIZECONF(Signal*, Uint32 ssId);

  // GSN_GCP_NOMORETRANS
  struct Ss_GCP_NOMORETRANS : SsParallel {
    GCPNoMoreTrans m_req;
    Ss_GCP_NOMORETRANS() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendGCP_NOMORETRANS;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendGCP_TCFINISHED;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_GCP_NOMORETRANS>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_GCP_NOMORETRANS;
    }
  };
  SsPool<Ss_GCP_NOMORETRANS> c_ss_GCP_NOMORETRANS;
  void execGCP_NOMORETRANS(Signal*);
  void sendGCP_NOMORETRANS(Signal*, Uint32 ssId, SectionHandle*);
  void execGCP_TCFINISHED(Signal*);
  void sendGCP_TCFINISHED(Signal*, Uint32 ssId);

  // GSN_API_FAILREQ
  struct Ss_API_FAILREQ : SsParallel {
    Uint32 m_ref; //
    Ss_API_FAILREQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendAPI_FAILREQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendAPI_FAILCONF;
    }
    enum { poolSize = MAX_NDB_NODES };
    static SsPool<Ss_API_FAILREQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_API_FAILREQ;
    }
  };
  SsPool<Ss_API_FAILREQ> c_ss_API_FAILREQ;
  void execAPI_FAILREQ(Signal*);
  void sendAPI_FAILREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execAPI_FAILCONF(Signal*);
  void sendAPI_FAILCONF(Signal*, Uint32 ssId);

  // GSN_CREATE_INDX_IMPL_REQ
  struct Ss_CREATE_INDX_IMPL_REQ : SsParallel {
    CreateIndxImplReq m_req;

    Ss_CREATE_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendCREATE_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendCREATE_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_CREATE_INDX_IMPL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_CREATE_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_CREATE_INDX_IMPL_REQ> c_ss_CREATE_INDX_IMPL_REQ;
  void execCREATE_INDX_IMPL_REQ(Signal*);
  void sendCREATE_INDX_IMPL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execCREATE_INDX_IMPL_CONF(Signal*);
  void execCREATE_INDX_IMPL_REF(Signal*);
  void sendCREATE_INDX_IMPL_CONF(Signal*, Uint32 ssId);

  // GSN_ALTER_INDX_IMPL_REQ
  struct Ss_ALTER_INDX_IMPL_REQ : SsParallel {
    AlterIndxImplReq m_req;
    Ss_ALTER_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendALTER_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendALTER_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_INDX_IMPL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_ALTER_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_ALTER_INDX_IMPL_REQ> c_ss_ALTER_INDX_IMPL_REQ;
  void execALTER_INDX_IMPL_REQ(Signal*);
  void sendALTER_INDX_IMPL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execALTER_INDX_IMPL_CONF(Signal*);
  void execALTER_INDX_IMPL_REF(Signal*);
  void sendALTER_INDX_IMPL_CONF(Signal*, Uint32 ssId);

  // GSN_DROP_INDX_IMPL_REQ
  struct Ss_DROP_INDX_IMPL_REQ : SsParallel {
    DropIndxImplReq m_req;
    Ss_DROP_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendDROP_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendDROP_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_INDX_IMPL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_DROP_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_DROP_INDX_IMPL_REQ> c_ss_DROP_INDX_IMPL_REQ;
  void execDROP_INDX_IMPL_REQ(Signal*);
  void sendDROP_INDX_IMPL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execDROP_INDX_IMPL_CONF(Signal*);
  void execDROP_INDX_IMPL_REF(Signal*);
  void sendDROP_INDX_IMPL_CONF(Signal*, Uint32 ssId);

  // GSN_TAKE_OVERTCCONF
  void execTAKE_OVERTCCONF(Signal*);
};

#endif
