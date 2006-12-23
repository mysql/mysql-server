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
  STATIC_CONST( SignalLength = 2 );
  STATIC_CONST( ExtraLength = 1 + NdbNodeBitmask::Size );
  
  enum FailCause {
    ZOWN_FAILURE=0,
    ZOTHER_NODE_WHEN_WE_START=1,
    ZIN_PREP_FAIL_REQ=2,
    ZSTART_IN_REGREQ=3,
    ZHEARTBEAT_FAILURE=4,
    ZLINK_FAILURE=5,
    ZOTHERNODE_FAILED_DURING_START=6,
    ZMULTI_NODE_SHUTDOWN = 7,
    ZPARTITIONED_CLUSTER = 8
  };
  
private:
  
  Uint32 failNodeId;
  Uint32 failCause;
  /**
   * Used when failCause == ZPARTITIONED_CLUSTER
   */
  Uint32 president;
  Uint32 partition[NdbNodeBitmask::Size];
};


#endif
