/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_BACKUP_PROXY_HPP
#define NDB_BACKUP_PROXY_HPP

#include <LocalProxy.hpp>
#include <signaldata/UtilSequence.hpp>

#define JAM_FILE_ID 478


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
  void execRESTORABLE_GCI_REP(Signal*);
};


#undef JAM_FILE_ID

#endif
