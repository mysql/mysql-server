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

#ifndef NDB_DBTUP_PROXY
#define NDB_DBTUP_PROXY

#include <LocalProxy.hpp>
#include <signaldata/DropTab.hpp>

class DbtupProxy : public LocalProxy {
public:
  DbtupProxy(Block_context& ctx);
  virtual ~DbtupProxy();
  BLOCK_DEFINES(DbtupProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // GSN_DROP_TAB_REQ
  struct Ss_DROP_TAB_REQ : SsParallel {
    DropTabReq m_req;
    Ss_DROP_TAB_REQ() {
      m_sendREQ = (SsFUNC)&DbtupProxy::sendDROP_TAB_REQ;
      m_sendCONF = (SsFUNC)&DbtupProxy::sendDROP_TAB_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_DROP_TAB_REQ>& pool(LocalProxy* proxy) {
      return ((DbtupProxy*)proxy)->c_ss_DROP_TAB_REQ;
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
