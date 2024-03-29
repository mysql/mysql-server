/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef __REQUEST_TRACKER_HPP
#define __REQUEST_TRACKER_HPP

#include "SafeCounter.hpp"

#define JAM_FILE_ID 328


class RequestTracker {
public:
  RequestTracker(){ init(); }

  void init() { m_confs.clear(); m_nRefs = 0; }

  template<typename SignalClass>
  bool init(SafeCounterManager& mgr,
	    NodeReceiverGroup rg, Uint16 GSN, Uint32 senderData)
  {
    init();
    SafeCounter tmp(mgr, m_sc);
    return tmp.init<SignalClass>(rg, GSN, senderData);
  }

  bool ignoreRef(SafeCounterManager& mgr, Uint32 nodeId)
  { return m_sc.clearWaitingFor(mgr, nodeId); }

  bool reportRef(SafeCounterManager& mgr, Uint32 nodeId)
  { m_nRefs++; return m_sc.clearWaitingFor(mgr, nodeId); }

  bool reportConf(SafeCounterManager& mgr, Uint32 nodeId)
  { m_confs.set(nodeId); return m_sc.clearWaitingFor(mgr, nodeId); }

  bool hasRef() { return m_nRefs != 0; }

  bool hasConf() { return !m_confs.isclear(); }

  bool done() { return m_sc.done(); }

  NdbNodeBitmask m_confs;

private:
  SafeCounterHandle m_sc;
  Uint8 m_nRefs;
};


#undef JAM_FILE_ID

#endif // __REQUEST_TRACKER_HPP
