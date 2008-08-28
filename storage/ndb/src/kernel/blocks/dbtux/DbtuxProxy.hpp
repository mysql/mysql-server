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

#ifndef NDB_DBTUX_PROXY_HPP
#define NDB_DBTUX_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/DropTab.hpp>

class DbtuxProxy : public LocalProxy {
public:
  DbtuxProxy(Block_context& ctx);
  virtual ~DbtuxProxy();
  BLOCK_DEFINES(DbtuxProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // GSN_ALTER_INDX_IMPL_REQ
  struct Ss_ALTER_INDX_IMPL_REQ : SsParallel {
    AlterIndxImplReq m_req;
    Ss_ALTER_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNC)&DbtuxProxy::sendALTER_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNC)&DbtuxProxy::sendALTER_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_INDX_IMPL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtuxProxy*)proxy)->c_ss_ALTER_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_ALTER_INDX_IMPL_REQ> c_ss_ALTER_INDX_IMPL_REQ;
  void execALTER_INDX_IMPL_REQ(Signal*);
  void sendALTER_INDX_IMPL_REQ(Signal*, Uint32 ssId);
  void execALTER_INDX_IMPL_CONF(Signal*);
  void execALTER_INDX_IMPL_REF(Signal*);
  void sendALTER_INDX_IMPL_CONF(Signal*, Uint32 ssId);

  // GSN_DROP_TAB_REQ
  struct Ss_DROP_TAB_REQ : SsParallel {
    DropTabReq m_req;
    Ss_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DbtuxProxy::sendDROP_TAB_REQ;
      m_sendCONF = (SsFUNC)&DbtuxProxy::sendDROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtuxProxy*)proxy)->c_ss_DROP_TAB_REQ;
    }
  };
  SsPool<Ss_DROP_TAB_REQ> c_ss_DROP_TAB_REQ;
  Uint32 getSsId(const DropTabReq* req) {
    return SsIdBase | req->tableId;
  }
  Uint32 getSsId(const DropTabConf* conf) {
    return SsIdBase | conf->tableId;
  }
  void execDROP_TAB_REQ(Signal*);
  void sendDROP_TAB_REQ(Signal*, Uint32 ssId);
  void execDROP_TAB_CONF(Signal*);
  void sendDROP_TAB_CONF(Signal*, Uint32 ssId);
};

#endif
