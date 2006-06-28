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

#ifndef __REQUEST_TRACKER_HPP
#define __REQUEST_TRACKER_HPP

#include "SafeCounter.hpp"

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

private:
  SafeCounterHandle m_sc;
  NodeBitmask m_confs;
  Uint8 m_nRefs;
};

#endif // __REQUEST_TRACKER_HPP
