/*
   Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NODEPING_HPP
#define NODEPING_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 213



/*
 * NodePingReq/Conf is sent between QMGR nodes to help determine the
 * available connectivity in a cluster experiencing heartbeat problems
 *
 * When a node detects that it has not received a heartbeat from a
 * connected node for the heartbeat period, it initiates a global
 * connectivity check protocol by sending a NODE_PING_REQ signal to all
 * nodes considered to be running.
 *
 * On receiving this signal, a node will respond with NODE_PING_CONF to
 * the sender, and begin its own connectivity check, if it is not
 * already involved in one.
 *
 * In this way, all nodes reachable within some latency n will begin
 * a connectivity check.  If they do not receive a NODE_PING_CONF from a
 * peer node within some further latency m, then they consider it to
 * be suspect, and after a further latency p they consider it failed.
 *
 * In environments where latency between nodes fluctuates, but
 * connectivity is maintained (for example where TCP connections observe
 * latency due to underlying IP re-routing/failover), the connectivity
 * check allows nodes to arm themselves in preparation for the potential
 * race of FAIL_REP signals that can arise in these situations, by marking
 * connections experiencing latency as SUSPECT.  Once a node is marked as
 * SUSPECT, FAIL_REP signals originating from it may not be trusted or
 * acted upon.
 */

class NodePingReq {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class Qmgr;
public:
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderData;
  Uint32 senderRef;
};

class NodePingConf {
  /**
   * Sender(s) / Receiver(s)
   */
  friend class Qmgr;
public:
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderData;
  Uint32 senderRef;
};


#undef JAM_FILE_ID

#endif
