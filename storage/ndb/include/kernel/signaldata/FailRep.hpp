/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef FAIL_REP_HPP
#define FAIL_REP_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

/**
 * 
 */
class FailRep {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  friend class Ndbcntr;
  
  /**
   * For printing
   */
  friend bool printFAIL_REP(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( OrigSignalLength = 2 );
  STATIC_CONST( PartitionedExtraLength = 1 + NdbNodeBitmask::Size );
  STATIC_CONST( SourceExtraLength = 1 );
  STATIC_CONST( SignalLength = OrigSignalLength + SourceExtraLength );
  
  enum FailCause {
    ZOWN_FAILURE=0,
    ZOTHER_NODE_WHEN_WE_START=1,
    ZIN_PREP_FAIL_REQ=2,
    ZSTART_IN_REGREQ=3,
    ZHEARTBEAT_FAILURE=4,
    ZLINK_FAILURE=5,
    ZOTHERNODE_FAILED_DURING_START=6,
    ZMULTI_NODE_SHUTDOWN = 7,
    ZPARTITIONED_CLUSTER = 8,
    ZCONNECT_CHECK_FAILURE = 9
  };

  Uint32 getFailSourceNodeId(Uint32 sigLen) const
  {
    /* Get failSourceNodeId from signal given length
     * 2 cases of 2 existing cases : 
     *   1) Old node, no source id
     *   2) New node, source id
     *   a) ZPARTITIONED_CLUSTER, extra info
     *   b) Other error, no extra info
     */
    if (failCause == ZPARTITIONED_CLUSTER)
    {
      return (sigLen == (SignalLength + PartitionedExtraLength)) ?
        partitioned.partitionFailSourceNodeId : 
        0;
    }

    return (sigLen == SignalLength) ? failSourceNodeId :
      0;
  }

private:
  
  Uint32 failNodeId;
  Uint32 failCause;
  /**
   * Used when failCause == ZPARTITIONED_CLUSTER
   */
  union {
    struct
    {
      Uint32 president;
      Uint32 partition[NdbNodeBitmask::Size];
      Uint32 partitionFailSourceNodeId;
    } partitioned;
    Uint32 failSourceNodeId;
  };
};


#endif
