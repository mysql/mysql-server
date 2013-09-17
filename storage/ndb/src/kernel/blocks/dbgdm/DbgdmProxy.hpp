/*
  Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef NDB_DBGDM_PROXY_HPP
#define NDB_DBGDM_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/TabCommit.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>

#define JAM_FILE_ID 337


/**
 * The Global Dictionary Manager (GDB):
 *
 * Intended as a shared base class for the TC and SPJ
 * table dictionary which share lots of common
 * components in this area.
 */

class DbgdmProxy : public LocalProxy {
public:
  DbgdmProxy(BlockNumber blockNumber, Block_context& ctx);
  virtual ~DbgdmProxy();
  BLOCK_DEFINES(DbgdmProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo) = 0;

  // GSN_TC_SCHVERREQ
  struct Ss_TC_SCHVERREQ : SsParallel {
    TcSchVerReq m_req;
    Ss_TC_SCHVERREQ() {
      m_sendREQ = (SsFUNCREQ)&DbgdmProxy::sendTC_SCHVERREQ;
      m_sendCONF = (SsFUNCREP)&DbgdmProxy::sendTC_SCHVERCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TC_SCHVERREQ>& pool(LocalProxy* proxy) {
      return ((DbgdmProxy*)proxy)->c_ss_TC_SCHVERREQ;
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
      m_sendREQ = (SsFUNCREQ)&DbgdmProxy::sendTAB_COMMITREQ;
      m_sendCONF = (SsFUNCREP)&DbgdmProxy::sendTAB_COMMITCONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_TAB_COMMITREQ>& pool(LocalProxy* proxy) {
      return ((DbgdmProxy*)proxy)->c_ss_TAB_COMMITREQ;
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
      m_sendREQ = (SsFUNCREQ)&DbgdmProxy::sendPREP_DROP_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DbgdmProxy::sendPREP_DROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_PREP_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DbgdmProxy*)proxy)->c_ss_PREP_DROP_TAB_REQ;
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
      m_sendREQ = (SsFUNCREQ)&DbgdmProxy::sendDROP_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DbgdmProxy::sendDROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DbgdmProxy*)proxy)->c_ss_DROP_TAB_REQ;
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
      m_sendREQ = (SsFUNCREQ)&DbgdmProxy::sendALTER_TAB_REQ;
      m_sendCONF = (SsFUNCREP)&DbgdmProxy::sendALTER_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DbgdmProxy*)proxy)->c_ss_ALTER_TAB_REQ;
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
};


#undef JAM_FILE_ID

#endif // NDB_DBGDM_PROXY_HPP
