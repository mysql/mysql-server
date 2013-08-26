/*
   Copyright (c) 2004, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CNTR_START_HPP
#define CNTR_START_HPP

#include <NodeBitmask.hpp>

#define JAM_FILE_ID 191


/**
 * 
 */
class CntrStartReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  friend bool printCNTR_START_REQ(FILE*, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 3 );
private:
  
  Uint32 nodeId;
  Uint32 startType;
  Uint32 lastGci;
};

class CntrStartRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  friend bool printCNTR_START_REF(FILE*, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

  enum ErrorCode {
    OK = 0,
    NotMaster = 1,
    StopInProgress = 2
  };
private:
  
  Uint32 errorCode;
  Uint32 masterNodeId;
};

class CntrStartConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  friend struct UpgradeStartup;
  
  friend bool printCNTR_START_CONF(FILE*, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 4 + 2 * NdbNodeBitmask::Size );
  
private:
  
  Uint32 startType;
  Uint32 startGci;
  Uint32 masterNodeId;
  Uint32 noStartNodes;
  Uint32 startedNodes[NdbNodeBitmask::Size];
  Uint32 startingNodes[NdbNodeBitmask::Size];
};

struct CntrWaitRep
{
  Uint32 nodeId;
  Uint32 waitPoint;
  Uint32 request;
  Uint32 sp;

  enum Request
  {
    WaitFor = 1,
    Grant = 2
  };

  STATIC_CONST( SignalLength = 4 );

  enum WaitPos
  {
    ZWAITPOINT_4_1  = 1
    ,ZWAITPOINT_4_2 = 2
    ,ZWAITPOINT_5_1 = 3
    ,ZWAITPOINT_5_2 = 4
    ,ZWAITPOINT_6_1 = 5
    ,ZWAITPOINT_6_2 = 6
    ,ZWAITPOINT_7_1 = 7
    ,ZWAITPOINT_7_2 = 8
    ,ZWAITPOINT_4_2_TO = 9 // We are forced to TO (during SR)
  };
};


#undef JAM_FILE_ID

#endif
