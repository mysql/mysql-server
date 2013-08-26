/*
   Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TRPMAN_H
#define TRPMAN_H

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <LocalProxy.hpp>
#include <signaldata/EnableCom.hpp>
#include <signaldata/CloseComReqConf.hpp>

#define JAM_FILE_ID 334


class Trpman : public SimulatedBlock
{
public:
  Trpman(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Trpman();
  BLOCK_DEFINES(Trpman);

  void execCLOSE_COMREQ(Signal *signal);
  void execCLOSE_COMCONF(Signal * signal);
  void execOPEN_COMORD(Signal *signal);
  void execENABLE_COMREQ(Signal *signal);
  void execDISCONNECT_REP(Signal *signal);
  void execCONNECT_REP(Signal *signal);
  void execROUTE_ORD(Signal* signal);

  void execDBINFO_SCANREQ(Signal*);

  void execNDB_TAMPER(Signal*);
  void execDUMP_STATE_ORD(Signal*);
protected:
private:
  bool handles_this_node(Uint32 nodeId);
};

class TrpmanProxy : public LocalProxy
{
public:
  TrpmanProxy(Block_context& ctx);
  virtual ~TrpmanProxy();
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

  void execROUTE_ORD(Signal* signal);
  void execNDB_TAMPER(Signal*);
  void execDUMP_STATE_ORD(Signal*);
protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);
};


#undef JAM_FILE_ID

#endif
