/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef Cmvmi_H_
#define Cmvmi_H_

#include <SimulatedBlock.hpp>
#include <LogLevel.hpp>

#include <DLList.hpp>

/**
 * Cmvmi class
 */
class Cmvmi : public SimulatedBlock {
public:
  Cmvmi(Block_context&);
  virtual ~Cmvmi();
  
private:
  BLOCK_DEFINES(Cmvmi);

  // The signal processing functions
  void execNDB_TAMPER(Signal* signal);
  void execSET_LOGLEVELORD(Signal* signal);
  void execEVENT_REP(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execSIZEALT_ACK(Signal* signal);
  void execTEST_ORD(Signal* signal);

  void execSTOP_ORD(Signal* signal);
  void execSTART_ORD(Signal* signal);
  void execTAMPER_ORD(Signal* signal);

  void execDUMP_STATE_ORD(Signal* signal);

  void execEVENT_SUBSCRIBE_REQ(Signal *);
  void execCANCEL_SUBSCRIPTION_REQ(Signal *);

  void execTESTSIG(Signal* signal);
  void execNODE_START_REP(Signal* signal);

  void execCONTINUEB(Signal* signal);

  void execDBINFO_SCANREQ(Signal *signal);

  void execALLOC_MEM_REF(Signal*);
  void execALLOC_MEM_CONF(Signal*);

  void execGET_CONFIG_REQ(Signal*);

  char theErrorMessage[256];
  void sendSTTORRY(Signal* signal);

  LogLevel clogLevel;
  NdbNodeBitmask c_dbNodes;

  /**
   * This struct defines the data needed for a EVENT_REP subscriber
   */
  struct EventRepSubscriber {
    /**
     * What log level is the subscriber using
     */
    LogLevel       logLevel;

    /**
     * What block reference does he use
     *   (Where should the EVENT_REP's be forwarded)
     */
    BlockReference blockRef;

    /**
     * Next ptr (used in pool/list)
     */
    union { Uint32 nextPool; Uint32 nextList; };
    Uint32 prevList;
  };
  typedef Ptr<EventRepSubscriber> SubscriberPtr;
  
  /**
   * Pool of EventRepSubscriber record
   */
  ArrayPool<EventRepSubscriber> subscriberPool;
  
  /**
   * List of current subscribers
   */
  DLList<EventRepSubscriber> subscribers;

private:
  // Declared but not defined
  Cmvmi(const Cmvmi &obj);
  void operator = (const Cmvmi &);

  void startFragmentedSend(Signal* signal, Uint32 variant, Uint32 numSigs, NodeReceiverGroup rg);
  void testNodeFailureCleanupCallback(Signal* signal, Uint32 variant, Uint32 elementsCleaned);
  void testFragmentedCleanup(Signal* signal, SectionHandle* handle, Uint32 testType, Uint32 variant);
  void sendFragmentedComplete(Signal* signal, Uint32 data, Uint32 returnCode);

  Uint32 c_memusage_report_frequency;
  void reportDMUsage(Signal* signal, int incDec,
                     BlockReference ref = CMVMI_REF);
  void reportIMUsage(Signal* signal, int incDec,
                     BlockReference ref = CMVMI_REF);

  NDB_TICKS m_start_time;

  struct SyncRecord
  {
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_prio;
    Uint32 m_cnt;
    Uint32 m_error;
    Uint32 nextPool;
  };
  ArrayPool<SyncRecord> c_syncReqPool;

  void execSYNC_REQ(Signal*);
  void execSYNC_REF(Signal*);
  void execSYNC_CONF(Signal*);
  void sendSYNC_REP(Signal * signal, Ptr<SyncRecord> ptr);

  void init_global_page_pool();
};

#endif
