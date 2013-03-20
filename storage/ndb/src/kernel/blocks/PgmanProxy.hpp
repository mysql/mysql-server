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

#ifndef NDB_PGMAN_PROXY_HPP
#define NDB_PGMAN_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/ReleasePages.hpp>
#include "pgman.hpp"

class PgmanProxy : public LocalProxy {
public:
  PgmanProxy(Block_context& ctx);
  virtual ~PgmanProxy();
  BLOCK_DEFINES(PgmanProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // GSN_LCP_FRAG_ORD
  struct Ss_LCP_FRAG_ORD : SsParallel {
    /*
     * Sent once from LQH proxy (at LCP) and LGMAN (at SR).
     * The pgman instances only set a flag and do not reply.
     */
    static const char* name() { return "LCP_FRAG_ORD"; }
    LcpFragOrd m_req;
    Ss_LCP_FRAG_ORD() {
      m_sendREQ = (SsFUNCREQ)&PgmanProxy::sendLCP_FRAG_ORD;
      m_sendCONF = (SsFUNCREP)0;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_LCP_FRAG_ORD>& pool(LocalProxy* proxy) {
      return ((PgmanProxy*)proxy)->c_ss_LCP_FRAG_ORD;
    }
  };
  SsPool<Ss_LCP_FRAG_ORD> c_ss_LCP_FRAG_ORD;
  static Uint32 getSsId(const LcpFragOrd* req) {
    return SsIdBase | (req->lcpId & 0xFFFF);
  }
  void execLCP_FRAG_ORD(Signal*);
  void sendLCP_FRAG_ORD(Signal*, Uint32 ssId, SectionHandle*);

  // GSN_END_LCP_REQ
  struct Ss_END_LCP_REQ : SsParallel {
    /*
     * Sent once from LQH proxy (at LCP) and LGMAN (at SR).
     * Each pgman instance runs LCP before we send a CONF.
     */
    static const char* name() { return "END_LCP_REQ"; }
    EndLcpReq m_req;
    Ss_END_LCP_REQ() {
      m_sendREQ = (SsFUNCREQ)&PgmanProxy::sendEND_LCP_REQ;
      m_sendCONF = (SsFUNCREP)&PgmanProxy::sendEND_LCP_CONF;
      // extra worker (for extent pages) must run after others
      m_extraLast = true;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_END_LCP_REQ>& pool(LocalProxy* proxy) {
      return ((PgmanProxy*)proxy)->c_ss_END_LCP_REQ;
    }
  };
  SsPool<Ss_END_LCP_REQ> c_ss_END_LCP_REQ;
  static Uint32 getSsId(const EndLcpReq* req) {
    return SsIdBase | (req->backupId & 0xFFFF);
  }
  static Uint32 getSsId(const EndLcpConf* conf) {
    return conf->senderData;
  }
  static Uint32 getSsId(const ReleasePagesConf* conf) {
    return conf->senderData;
  }
  void execEND_LCP_REQ(Signal*);
  void sendEND_LCP_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execEND_LCP_CONF(Signal*);
  void sendEND_LCP_CONF(Signal*, Uint32 ssId);
  void execRELEASE_PAGES_CONF(Signal*);

  // client methods
  friend class Page_cache_client;

  int get_page(Page_cache_client& caller,
               Signal*, Page_cache_client::Request& req, Uint32 flags);

  void update_lsn(Page_cache_client& caller,
                  Local_key key, Uint64 lsn);

  int drop_page(Page_cache_client& caller,
                Local_key key, Uint32 page_id);

  Uint32 create_data_file(Signal*);

  Uint32 alloc_data_file(Signal*, Uint32 file_no);

  void map_file_no(Signal*, Uint32 file_no, Uint32 fd);

  void free_data_file(Signal*, Uint32 file_no, Uint32 fd);

  void send_data_file_ord(Signal*, Uint32 i, Uint32 ret,
                          Uint32 cmd, Uint32 file_no = RNIL, Uint32 fd = RNIL);
};

#endif
