/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NODE_FAILREP_HPP
#define NODE_FAILREP_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

/**
 * This signals is sent by Qmgr to NdbCntr
 *   and then from NdbCntr sent to: dih, dict, lqh, tc, API
 *   and others
 */
struct NodeFailRep {
  STATIC_CONST( SignalLength = 3 + NdbNodeBitmask::Size );
  STATIC_CONST( SignalLengthLong = 3 + NodeBitmask::Size );
  Uint32 failNo;

  /**
   * Note: This field is only set when signals is sent FROM Ndbcntr
   *       (not when signal is sent from Qmgr)
   */
  Uint32 masterNodeId;

  Uint32 noOfNodes;
  Uint32 theNodes[NdbNodeBitmask::Size];
};

#endif
