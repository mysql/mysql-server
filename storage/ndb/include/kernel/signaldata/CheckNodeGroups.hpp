/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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
    Uint32 blockRef;              // sender's node id
    Uint32 partitionBalance;     // For GetDefaultFragments
  };

  union {
    Uint32 requestType;           // direct flag, output code
    Uint32 output;
  };

  union {
    Uint32 nodeId;             // nodeId input for GetNodeGroupMembers
    Uint32 extraNodeGroups;    // For GetDefaultFragments
  };
  NdbNodeBitmaskPOD mask;         /* set of NDB nodes, input for ArbitCheck,
        			   * output for GetNodeGroupMembers
				   */
  Uint32 senderData;            // Sender data, kept in return signal

  enum RequestType {
    Direct              = 0x1,
    ArbitCheck          = 0x2,
    GetNodeGroup        = 0x4,
    GetNodeGroupMembers = 0x8,
    GetDefaultFragments = 0x10,
    GetDefaultFragmentsFullyReplicated = 0x20
  };

  enum Output {
    Lose = 1,                   // we cannot survive
    Win = 2,                    // we and only we can survive
    Partitioning = 3            // possible network partitioning
  };

  STATIC_CONST( SignalLength = 4 + NdbNodeBitmask::Size );
};


#undef JAM_FILE_ID

#endif
