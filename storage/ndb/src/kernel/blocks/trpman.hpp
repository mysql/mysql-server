/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TRPMAN_H
#define TRPMAN_H

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <LocalProxy.hpp>
#include <signaldata/EnableCom.hpp>
#include <signaldata/CloseComReqConf.hpp>
#include <signaldata/SyncThreadViaReqConf.hpp>
#include <ndb_limits.h>

#define JAM_FILE_ID 334


class Trpman : public SimulatedBlock
{
public:
  Trpman(Block_context& ctx, Uint32 instanceNumber = 0);
  BLOCK_DEFINES(Trpman);

  void execCLOSE_COMREQ(Signal *signal);
  void execCLOSE_COMCONF(Signal * signal);
  void execOPEN_COMORD(Signal *signal);
  void execENABLE_COMREQ(Signal *signal);
  void execDISCONNECT_REP(Signal *signal);
  void execCONNECT_REP(Signal *signal);
  void execROUTE_ORD(Signal* signal);
  void execACTIVATE_TRP_REQ(Signal*);
  void execUPD_QUERY_DIST_ORD(Signal*);

  void sendSYNC_THREAD_VIA_CONF(Signal*, Uint32, Uint32);
  void execSYNC_THREAD_VIA_REQ(Signal*);

  void execDBINFO_SCANREQ(Signal*);

  void execNDB_TAMPER(Signal*);
  void execDUMP_STATE_ORD(Signal*);
public:
  Uint32 distribute_signal(SignalHeader * const header,
                           const Uint32 instance);
  DistributionHandler m_distribution_handle;
  bool m_distribution_handler_inited;

protected:
  bool getParam(const char* name, Uint32* count) override;
private:
  bool handles_this_node(Uint32 nodeId, bool all = false);
  void close_com_failed_node(Signal*, Uint32);
  void enable_com_node(Signal*, Uint32);

};

class TrpmanProxy : public LocalProxy
{
public:
  TrpmanProxy(Block_context& ctx);
  ~TrpmanProxy() override;
  BLOCK_DEFINES(TrpmanProxy);

  // GSN_OPEN_COMORD
  void execOPEN_COMORD(Signal *signal);

  // GSN_CLOSE_COMREQ
  struct Ss_CLOSE_COMREQ : SsParallel {
    CloseComReqConf m_req;
    Ss_CLOSE_COMREQ() {
      m_sendREQ = (SsFUNCREQ)&TrpmanProxy::sendCLOSE_COMREQ;
      m_sendCONF = (SsFUNCREP)&TrpmanProxy::sendCLOSE_COMCONF;
    }
    enum { poolSize = MAX_NODES };
    static SsPool<Ss_CLOSE_COMREQ>& pool(LocalProxy* proxy) {
      return ((TrpmanProxy*)proxy)->c_ss_CLOSE_COMREQ;
    }
  };
  SsPool<Ss_CLOSE_COMREQ> c_ss_CLOSE_COMREQ;
  void execCLOSE_COMREQ(Signal *signal);
  void sendCLOSE_COMREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execCLOSE_COMCONF(Signal *signal);
  void sendCLOSE_COMCONF(Signal*, Uint32 ssId);

  // GSN_ENABLE_COMREQ
  struct Ss_ENABLE_COMREQ : SsParallel {
    EnableComReq m_req;
    Ss_ENABLE_COMREQ() {
      m_sendREQ = (SsFUNCREQ)&TrpmanProxy::sendENABLE_COMREQ;
      m_sendCONF = (SsFUNCREP)&TrpmanProxy::sendENABLE_COMCONF;
    }
    enum { poolSize = MAX_NODES };
    static SsPool<Ss_ENABLE_COMREQ>& pool(LocalProxy* proxy) {
      return ((TrpmanProxy*)proxy)->c_ss_ENABLE_COMREQ;
    }
  };
  SsPool<Ss_ENABLE_COMREQ> c_ss_ENABLE_COMREQ;
  void execENABLE_COMREQ(Signal *signal);
  void sendENABLE_COMREQ(Signal*, Uint32 ssId, SectionHandle*);
  void execENABLE_COMCONF(Signal *signal);
  void sendENABLE_COMCONF(Signal*, Uint32 ssId);

  // GSN_SYNC_THREAD_VIA
  struct Ss_SYNC_THREAD_VIA : SsParallel {
    SyncThreadViaReqConf m_req;
    Ss_SYNC_THREAD_VIA() {
      m_sendREQ = (SsFUNCREQ)&TrpmanProxy::sendSYNC_THREAD_VIA_REQ;
      m_sendCONF = (SsFUNCREP)&TrpmanProxy::sendSYNC_THREAD_VIA_CONF;
    }
    enum { poolSize = MAX_DATA_NODE_ID }; // Qmgr::MAX_DATA_NODE_FAILURES
    static SsPool<Ss_SYNC_THREAD_VIA>& pool(LocalProxy* proxy) {
      return ((TrpmanProxy*)proxy)->c_ss_SYNC_THREAD_VIA;
    }
  };
  SsPool<Ss_SYNC_THREAD_VIA> c_ss_SYNC_THREAD_VIA;
  void execSYNC_THREAD_VIA_REQ(Signal*);
  void sendSYNC_THREAD_VIA_REQ(Signal*, Uint32, SectionHandle*);
  void execSYNC_THREAD_VIA_CONF(Signal*);
  void sendSYNC_THREAD_VIA_CONF(Signal*, Uint32);

  void execROUTE_ORD(Signal* signal);
  void execNDB_TAMPER(Signal*);
  void execDUMP_STATE_ORD(Signal*);
  void execACTIVATE_TRP_REQ(Signal*);
protected:
  SimulatedBlock* newWorker(Uint32 instanceNo) override;
};


#undef JAM_FILE_ID

#endif
