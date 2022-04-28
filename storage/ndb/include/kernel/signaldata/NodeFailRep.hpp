/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef NODE_FAILREP_HPP
#define NODE_FAILREP_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 59


/**
 * This signals is sent by Qmgr to NdbCntr
 *   and then from NdbCntr sent to: dih, dict, lqh, tc, API
 *   and others
 */
struct NodeFailRep {
  static constexpr Uint32 SignalLength = 3;
  static constexpr Uint32 SignalLengthLong = 3;

  static constexpr Uint32 SignalLength_v1 = 3 + NdbNodeBitmask48::Size;
  static constexpr Uint32 SignalLengthLong_v1 = 3 + NodeBitmask::Size;

  Uint32 failNo;

  /**
   * Note: This field is only set when signals is sent FROM Ndbcntr
   *       (not when signal is sent from Qmgr)
   */
  Uint32 masterNodeId;

  Uint32 noOfNodes;
  union
  {
    Uint32 theNodes[NdbNodeBitmask::Size]; // data nodes 8.0.17 and older
    Uint32 theAllNodes[NodeBitmask::Size]; // api nodes 8.0.17 and older
  };

  static Uint32 getNodeMaskLength(Uint32 signalLength) {
    assert(signalLength == SignalLength ||
           signalLength == SignalLengthLong ||
           signalLength == SignalLength_v1 ||
           signalLength == SignalLengthLong_v1);
    return signalLength - 3;
  }
};


#undef JAM_FILE_ID

#endif
