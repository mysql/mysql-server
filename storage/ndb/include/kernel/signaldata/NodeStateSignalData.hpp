/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef NODE_STATE_SIGNAL_DATA_HPP
#define NODE_STATE_SIGNAL_DATA_HPP

#include <NodeState.hpp>

#define JAM_FILE_ID 165

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
  friend class Cmvmi;

  /**
   * Reciver
   */
  friend class SimulatedBlock;
  friend class Dbtux;
  friend class Dbacc;
  friend class Dbtup;

 public:
  static constexpr Uint32 SignalLength = NodeState::DataLength;

 private:
  NodeStatePOD nodeState;
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
  static constexpr Uint32 SignalLength = 2 + NodeState::DataLength;

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
  friend class LocalProxy;

 public:
  static constexpr Uint32 SignalLength = 1;

 private:
  Uint32 senderData;
};

#undef JAM_FILE_ID

#endif
