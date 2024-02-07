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

#ifndef CHECKNODEGROUPS_H
#define CHECKNODEGROUPS_H

#include <string.h>
#include <NodeBitmask.hpp>
#include "SignalData.hpp"
#include "SignalDataPrint.hpp"

#define JAM_FILE_ID 190

/**
 * Ask DIH to check if a node set can survive i.e. if it
 * has at least one node in every node group.  Returns one
 * of Win, Lose, Partitioning.
 *
 * Same class is used for REQ and CONF.  The REQ can also
 * be executed as a direct signal.
 */
class CheckNodeGroups {
 public:
  union {
    Uint32 blockRef;          // sender's node id
    Uint32 partitionBalance;  // For GetDefaultFragments
  };

  union {
    Uint32 requestType;  // direct flag, output code
    Uint32 output;
  };

  union {
    Uint32 nodeId;           // nodeId input for GetNodeGroupMembers
    Uint32 extraNodeGroups;  // For GetDefaultFragments
  };
  Uint32 senderData;      // Sender data, kept in return signal
  NdbNodeBitmaskPOD mask; /* set of NDB nodes, input for ArbitCheck,
                           * output for GetNodeGroupMembers
                           * Part of direct signal, but sent as first
                           * section for async signal.
                           */
  /**
   * The set of nodes before failure, this is useful to discover if any node
   * group is completely alive after the failure. Even if only one node in
   * a node group is only alive before failure, if this node is still up
   * after the failure we have a complete node group up and running.
   *
   * before_fail_mask is only used in Direct signal and in ArbitCheck.
   */
  NdbNodeBitmaskPOD before_fail_mask;

  enum RequestType {
    Direct = 0x1,
    ArbitCheck = 0x2,
    GetNodeGroup = 0x4,
    GetNodeGroupMembers = 0x8,
    GetDefaultFragments = 0x10,
    GetDefaultFragmentsFullyReplicated = 0x20,
    UseBeforeFailMask = 0x40
  };

  enum Output {
    Lose = 1,         // we cannot survive
    Win = 2,          // we and only we can survive
    Partitioning = 3  // possible network partitioning
  };

  static constexpr Uint32 SignalLength =
      4 + NdbNodeBitmask::Size;  // Only for direct signal.
  static constexpr Uint32 SignalLengthArbitCheckShort =
      4 + NdbNodeBitmask::Size;
  static constexpr Uint32 SignalLengthArbitCheckLong =
      4 + (2 * NdbNodeBitmask::Size);
  static constexpr Uint32 SignalLengthNoBitmask = 4;
};

#undef JAM_FILE_ID

#endif
