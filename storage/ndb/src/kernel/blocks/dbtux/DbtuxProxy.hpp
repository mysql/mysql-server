/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NDB_DBTUX_PROXY_HPP
#define NDB_DBTUX_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/IndexStatSignal.hpp>

#define JAM_FILE_ID 376


class DbtuxProxy : public LocalProxy {
public:
  DbtuxProxy(Block_context& ctx);
  ~DbtuxProxy() override;
  BLOCK_DEFINES(DbtuxProxy);

protected:
  SimulatedBlock* newWorker(Uint32 instanceNo) override;

  // GSN_ALTER_INDX_IMPL_REQ
  struct Ss_ALTER_INDX_IMPL_REQ : SsParallel {
    AlterIndxImplReq m_req;
    Ss_ALTER_INDX_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtuxProxy::sendALTER_INDX_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtuxProxy::sendALTER_INDX_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_ALTER_INDX_IMPL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtuxProxy*)proxy)->c_ss_ALTER_INDX_IMPL_REQ;
    }
  };
  SsPool<Ss_ALTER_INDX_IMPL_REQ> c_ss_ALTER_INDX_IMPL_REQ;
  void execALTER_INDX_IMPL_REQ(Signal*);
  void sendALTER_INDX_IMPL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execALTER_INDX_IMPL_CONF(Signal*);
  void execALTER_INDX_IMPL_REF(Signal*);
  void sendALTER_INDX_IMPL_CONF(Signal*, Uint32 ssId);

  // GSN_INDEX_STAT_IMPL_REQ
  struct Ss_INDEX_STAT_IMPL_REQ : SsParallel {
    IndexStatImplReq m_req;
    Ss_INDEX_STAT_IMPL_REQ() {
      m_sendREQ = (SsFUNCREQ)&DbtuxProxy::sendINDEX_STAT_IMPL_REQ;
      m_sendCONF = (SsFUNCREP)&DbtuxProxy::sendINDEX_STAT_IMPL_CONF;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_INDEX_STAT_IMPL_REQ>& pool(LocalProxy* proxy) {
      return ((DbtuxProxy*)proxy)->c_ss_INDEX_STAT_IMPL_REQ;
    }
  };
  SsPool<Ss_INDEX_STAT_IMPL_REQ> c_ss_INDEX_STAT_IMPL_REQ;
  void execINDEX_STAT_IMPL_REQ(Signal*);
  void sendINDEX_STAT_IMPL_REQ(Signal*, Uint32 ssId, SectionHandle*);
  void execINDEX_STAT_IMPL_CONF(Signal*);
  void execINDEX_STAT_IMPL_REF(Signal*);
  void sendINDEX_STAT_IMPL_CONF(Signal*, Uint32 ssId);

  // GSN_INDEX_STAT_REP
  void execINDEX_STAT_REP(Signal*);
};


#undef JAM_FILE_ID

#endif
