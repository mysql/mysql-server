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
#include <signaldata/BuildIndxImpl.hpp>

class DbtupProxy : public LocalProxy {
public:
  DbtupProxy(Block_context& ctx);
  virtual ~DbtupProxy();
  BLOCK_DEFINES(DbtupProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  class Pgman* c_pgman; // PGMAN proxy

  // GSN_STTOR
  virtual void callSTTOR(Signal*);

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

  // GSN_BUILD_INDX_IMPL_REQ
  struct Ss_BUILD_INDX_IMPL_REQ : SsParallel {
    BuildIndxImplReq m_req;
    Ss_BUILD_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNC)&DbtupProxy::sendBUILD_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNC)&DbtupProxy::sendBUILD_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_BUILD_INDX_IMPL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtupProxy*)proxy)->c_ss_BUILD_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_BUILD_INDX_IMPL_REQ> c_ss_BUILD_INDX_IMPL_REQ;
  void execBUILD_INDX_IMPL_REQ(Signal*);
  void sendBUILD_INDX_IMPL_REQ(Signal*, Uint32 ssId);
  void execBUILD_INDX_IMPL_CONF(Signal*);
  void execBUILD_INDX_IMPL_REF(Signal*);
  void sendBUILD_INDX_IMPL_CONF(Signal*, Uint32 ssId);

  // client methods
  friend class Dbtup_client;

  // LGMAN

  enum { MaxUndoData = MAX_TUPLE_SIZE_IN_WORDS };
  Uint32 c_proxy_undo_data[MaxUndoData];

  struct Proxy_undo {
    Uint32 m_type;
    Uint32 m_len;
    const Uint32* m_ptr;
    Uint64 m_lsn;
    // from undo entry and page
    Local_key m_key;
    Uint32 m_page_id;
    Uint32 m_table_id;
    Uint32 m_fragment_id;
    Uint32 m_instance_no;
    enum {
      SendToAll = 1,
      ReadTupPage = 2,
      GetInstance = 4,
      NoExecute = 8,
      SendUndoNext = 16
    };
    Uint32 m_actions;
    Proxy_undo();
  };
  Proxy_undo c_proxy_undo;

  void disk_restart_undo(Signal*, Uint64 lsn,
                         Uint32 type, const Uint32 * ptr, Uint32 len);

  // next 3 are helper methods
  void disk_restart_undo_callback(Signal*, Uint32, Uint32 page_id);

  void disk_restart_undo_finish(Signal*);

  void disk_restart_undo_send(Signal*, Uint32 i);

  // TSMAN

  int disk_restart_alloc_extent(Uint32 tableId, Uint32 fragId, 
				const Local_key* key, Uint32 pages);
  void disk_restart_page_bits(Uint32 tableId, Uint32 fragId,
			      const Local_key* key, Uint32 bits);
};

#endif
