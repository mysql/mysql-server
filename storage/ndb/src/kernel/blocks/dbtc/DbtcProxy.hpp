/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "../dbgdm/DbgdmProxy.hpp"

#include <signaldata/GCP.hpp>

#include <signaldata/CreateIndxImpl.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/DropIndxImpl.hpp>
#include <signaldata/AbortAll.hpp>


class DbtcProxy : public DbgdmProxy {
public:
  DbtcProxy(Block_context& ctx);
  virtual ~DbtcProxy();
  BLOCK_DEFINES(DbtcProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // GSN_NDB_STTOR
  virtual void callNDB_STTOR(Signal*);

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

  // GSN_ABORT_ALL_REQ
  struct Ss_ABORT_ALL_REQ : SsParallel {
    AbortAllReq m_req;
    Ss_ABORT_ALL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtcProxy::sendABORT_ALL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtcProxy::sendABORT_ALL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ABORT_ALL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtcProxy*)proxy)->c_ss_ABORT_ALL_REQ;
    }
  };
  SsPool<Ss_ABORT_ALL_REQ> c_ss_ABORT_ALL_REQ;
  void execABORT_ALL_REQ(Signal*);
  void sendABORT_ALL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execABORT_ALL_REF(Signal*);
  void execABORT_ALL_CONF(Signal*);
  void sendABORT_ALL_CONF(Signal*, Uint32 ssId);
};

#endif
