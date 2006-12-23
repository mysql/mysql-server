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

#ifndef CHECKNODEGROUPS_H
#define CHECKNODEGROUPS_H

#include <string.h>
#include <NodeBitmask.hpp>
#include "SignalData.hpp"
#include "SignalDataPrint.hpp"

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
  Uint32 blockRef;              // sender's node id
  union {
    Uint32 requestType;           // direct flag, output code
    Uint32 output;
  };

  Uint32 nodeId;             // nodeId input for GetNodeGroupMembers
  NodeBitmask mask;             /* set of NDB nodes, input for ArbitCheck,
        			   * output for GetNodeGroupMembers
				   */
  enum RequestType {
    Direct              = 0x1,
    ArbitCheck          = 0x2,
    GetNodeGroup        = 0x4,
    GetNodeGroupMembers = 0x8
  };

  enum Output {
    Lose = 1,                   // we cannot survive
    Win = 2,                    // we and only we can survive
    Partitioning = 3            // possible network partitioning
  };

  STATIC_CONST( SignalLength = 3 + NodeBitmask::Size );
};

#endif
