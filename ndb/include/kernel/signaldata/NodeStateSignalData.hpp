/* Copyright (C) 2003 MySQL AB

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

#ifndef NODE_STATE_SIGNAL_DATA_HPP
#define NODE_STATE_SIGNAL_DATA_HPP

#include <NodeState.hpp>

/**
 * NodeStateRep
 *
 * Sent so that all blocks will update their NodeState
 */
class NodeStateRep {
  /**
   * Sender(s)
   */
  friend class Ndbcntr;
  
  /**
   * Reciver
   */
  friend class SimulatedBlock;
  
public:
  STATIC_CONST( SignalLength = NodeState::DataLength );
private:
  
  NodeState nodeState;
};

/**
 * ChangeNodeStateReq
 *
 * Sent by NdbCntr when synchronous NodeState updates are needed
 */
class ChangeNodeStateReq {
  /**
   * Sender(s)
   */
  friend class Ndbcntr;
  
  /**
   * Reciver
   */
  friend class SimulatedBlock;
  
public:
  STATIC_CONST( SignalLength = 2 + NodeState::DataLength );
public:
  
  Uint32 senderRef;
  Uint32 senderData;
  NodeState nodeState;
};

/**
 * ChangeNodeStateConf
 *
 * Sent by SimulatedBlock as a confirmation to ChangeNodeStateReq
 */
class ChangeNodeStateConf {
  /**
   * Sender(s)
   */
  friend class SimulatedBlock;
  
  /**
   * Reciver
   */
  friend class NdbCntr;
  
public:
  STATIC_CONST( SignalLength = 1 );
private:
  
  Uint32 senderData;
};


#endif
