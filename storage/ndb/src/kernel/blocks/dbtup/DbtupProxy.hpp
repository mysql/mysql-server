/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef NDB_DBTUP_PROXY
#define NDB_DBTUP_PROXY

#include <LocalProxy.hpp>
#include <signaldata/CreateTab.hpp>
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

  Uint32 c_tableRecSize;
  Uint8* c_tableRec;    // bool => table exists

  // GSN_READ_CONFIG_REQ
  virtual void callREAD_CONFIG_REQ(Signal*);

  // GSN_STTOR
  virtual void callSTTOR(Signal*);

  // GSN_CREATE_TAB_REQ
  void execCREATE_TAB_REQ(Signal*);
  // GSN_DROP_TAB_REQ
  void execDROP_TAB_REQ(Signal*);

  // GSN_BUILD_INDX_IMPL_REQ
  struct Ss_BUILD_INDX_IMPL_REQ : SsParallel {
    BuildIndxImplReq m_req;
    Ss_BUILD_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtupProxy::sendBUILD_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtupProxy::sendBUILD_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_BUILD_INDX_IMPL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtupProxy*)proxy)->c_ss_BUILD_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_BUILD_INDX_IMPL_REQ> c_ss_BUILD_INDX_IMPL_REQ;
  void execBUILD_INDX_IMPL_REQ(Signal*);
  void sendBUILD_INDX_IMPL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execBUILD_INDX_IMPL_CONF(Signal*);
  void execBUILD_INDX_IMPL_REF(Signal*);
  void sendBUILD_INDX_IMPL_CONF(Signal*, Uint32 ssId);

  // client methods
  friend class Dbtup_client;

  // LGMAN

  struct Proxy_undo {
    enum { MaxData = 20 + MAX_TUPLE_SIZE_IN_WORDS };
    Uint32 m_type;
    Uint32 m_len;
    const Uint32* m_ptr;
    Uint32 m_data[MaxData]; // copied from m_ptr at once
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
    bool m_in_use;
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
