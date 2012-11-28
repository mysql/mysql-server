/* Copyright (C) 2008 MySQL AB

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

#ifndef NDB_BACKUP_PROXY_HPP
#define NDB_BACKUP_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/UtilSequence.hpp>

class BackupProxy : public LocalProxy {
public:
  BackupProxy(Block_context& ctx);
  virtual ~BackupProxy();
  BLOCK_DEFINES(BackupProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

  // GSN_STTOR
  virtual void callSTTOR(Signal*);
  void sendUTIL_SEQUENCE_REQ(Signal*);
  void execUTIL_SEQUENCE_CONF(Signal*);
  void execUTIL_SEQUENCE_REF(Signal*);

  struct Ss_SUM_DUMP_STATE_ORD : SsParallel {
    static const int MAX_REQ_SIZE = 2;
    static const int MAX_REP_SIZE = 11;
    Uint32 m_request[ MAX_REQ_SIZE ];
    Uint32 m_report[ MAX_REP_SIZE ];
    
    Ss_SUM_DUMP_STATE_ORD() {
      m_sendREQ = (SsFUNCREQ)&BackupProxy::sendSUM_DUMP_STATE_ORD;
      m_sendCONF = (SsFUNCREP)&BackupProxy::sendSUM_EVENT_REP;
    }
    enum { poolSize = 1 };
    static SsPool<Ss_SUM_DUMP_STATE_ORD>& pool(LocalProxy* proxy) {
      return ((BackupProxy*)proxy)->c_ss_SUM_DUMP_STATE_ORD;
    }
  };
  SsPool<Ss_SUM_DUMP_STATE_ORD> c_ss_SUM_DUMP_STATE_ORD;

  // DUMP_STATE_ORD
  void execDUMP_STATE_ORD(Signal* );
  void sendSUM_DUMP_STATE_ORD(Signal*, Uint32 ssId, SectionHandle*);
  void execEVENT_REP(Signal* );
  void sendSUM_EVENT_REP(Signal*, Uint32 ssId);
};

#endif
