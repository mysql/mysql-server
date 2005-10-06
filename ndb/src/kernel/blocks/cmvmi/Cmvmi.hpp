/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef Cmvmi_H_
#define Cmvmi_H_

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <LogLevel.hpp>

#include <ArrayList.hpp>

/**
 * Cmvmi class
 */
class Cmvmi : public SimulatedBlock {
public:
  Cmvmi(const Configuration & conf);
  virtual ~Cmvmi();
  
private:
  /**
   * These methods used to be reportXXX
   *
   * But they in a nasty way intefere with the execution model
   * they been turned in to exec-Method used via prio A signals
   */
  void execDISCONNECT_REP(Signal*);
  void execCONNECT_REP(Signal*);
  
private:
  BLOCK_DEFINES(Cmvmi);

  // The signal processing functions
  void execNDB_TAMPER(Signal* signal);
  void execSET_LOGLEVELORD(Signal* signal);
  void execEVENT_REP(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execCLOSE_COMREQ(Signal* signal);
  void execENABLE_COMORD(Signal* signal);
  void execOPEN_COMREQ(Signal* signal);
  void execSIZEALT_ACK(Signal* signal);
  void execTEST_ORD(Signal* signal);

  void execSTATISTICS_REQ(Signal* signal);
  void execSTOP_ORD(Signal* signal);
  void execSTART_ORD(Signal* signal);
  void execTAMPER_ORD(Signal* signal);
  void execSET_VAR_REQ(Signal* signal);
  void execSET_VAR_CONF(Signal* signal);
  void execSET_VAR_REF(Signal* signal);

  void execDUMP_STATE_ORD(Signal* signal);

  void execEVENT_SUBSCRIBE_REQ(Signal *);
  void cancelSubscription(NodeId nodeId);
  
  void handleSET_VAR_REQ(Signal* signal);

  void execTESTSIG(Signal* signal);

  char theErrorMessage[256];
  void sendSTTORRY(Signal* signal);

  LogLevel clogLevel;
  NdbNodeBitmask c_dbNodes;

  class Configuration & theConfig;

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
  ArrayList<EventRepSubscriber> subscribers;

private:
  // Declared but not defined
  Cmvmi(const Cmvmi &obj);
  void operator = (const Cmvmi &);

  void sendFragmentedComplete(Signal* signal, Uint32 data, Uint32 returnCode);
};

#endif
