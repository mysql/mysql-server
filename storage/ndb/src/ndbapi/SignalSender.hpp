/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SIGNAL_SENDER_HPP
#define SIGNAL_SENDER_HPP

#include <ndb_global.h>
#include <Vector.hpp>
#include "NdbApiSignal.hpp"
#include "TransporterFacade.hpp"
#include "trp_client.hpp"

#include <signaldata/AllocNodeId.hpp>
#include <signaldata/ApiVersion.hpp>
#include <signaldata/BackupSignalData.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/EventSubscribeReq.hpp>
#include <signaldata/ResumeReq.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/TamperOrd.hpp>
#include <signaldata/TestOrd.hpp>

struct trp_node;

struct SimpleSignal {
 public:
  SimpleSignal(bool dealloc = false);
  SimpleSignal(const SimpleSignal &src);
  ~SimpleSignal();

  void set(class SignalSender &, Uint8 trace, Uint16 recBlock, Uint16 gsn,
           Uint32 len);

  NdbApiSignal header;
  LinearSectionPtr ptr[3];

  int readSignalNumber() const { return header.readSignalNumber(); }
  Uint32 *getDataPtrSend() { return header.getDataPtrSend(); }
  const Uint32 *getDataPtr() const { return header.getDataPtr(); }
  Uint32 getLength() const { return header.getLength(); }

  /**
   * Fragmentation
   */
  bool isFragmented() const { return header.isFragmented(); }
  bool isFirstFragment() const { return header.isFirstFragment(); }
  bool isLastFragment() const { return header.isLastFragment(); }
  Uint32 getFragmentId() const { return header.getFragmentId(); }

  void print(FILE *out = stdout) const;
  SimpleSignal &operator=(const SimpleSignal &);

 private:
  bool deallocSections;
};

class SignalSender : public trp_client {
 public:
  /**
   * deliverAll option set to true also delivers signals which may be
   * frequent.
   * Care should be taken to ensure that these are handled promptly to
   * avoid excessive buffering
   */
  SignalSender(TransporterFacade *facade, int blockNo = -1,
               bool deliverAll = false);
  SignalSender(Ndb_cluster_connection *connection, bool deliverAll = false);
  ~SignalSender() override;

  int lock();
  int unlock();

  Uint32 getOwnRef() const;

  NodeId find_confirmed_node(const NodeBitmask &mask);
  NodeId find_connected_node(const NodeBitmask &mask);
  NodeId find_alive_node(const NodeBitmask &mask);

  SendStatus sendSignal(Uint16 nodeId, const SimpleSignal *);
  SendStatus sendSignal(Uint16 nodeId, SimpleSignal &sig, Uint16 recBlock,
                        Uint16 gsn, Uint32 len);
  int sendFragmentedSignal(Uint16 nodeId, SimpleSignal &sig, Uint16 recBlock,
                           Uint16 gsn, Uint32 len);
  NodeBitmask broadcastSignal(NodeBitmask mask, SimpleSignal &sig,
                              Uint16 recBlock, Uint16 gsn, Uint32 len);

  SimpleSignal *waitFor(Uint32 timeOutMillis = 0);

  Uint32 get_an_alive_node() const { return theFacade->get_an_alive_node(); }
  Uint32 getAliveNode() const { return get_an_alive_node(); }
  bool get_node_alive(Uint32 n) const { return getNodeInfo(n).m_alive; }

 private:
  int m_blockNo;
  TransporterFacade *theFacade;
  bool m_locked;
  bool m_deliverAll;

  NodeId find_node(const NodeBitmask &mask, bool (*cond)(const trp_node &));

 public:
  /**
   * trp_client interface
   */
  void trp_deliver_signal(const NdbApiSignal *signal,
                          const struct LinearSectionPtr ptr[3]) override;

  Vector<SimpleSignal *> m_jobBuffer;
  Vector<SimpleSignal *> m_usedBuffer;

  template <class T>
  SimpleSignal *waitFor(Uint32 timeOutMillis, T &t);
};

#endif
