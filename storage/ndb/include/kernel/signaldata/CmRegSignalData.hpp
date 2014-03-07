/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CM_REG_HPP
#define CM_REG_HPP

#include <NodeBitmask.hpp>

#define JAM_FILE_ID 65


/**
 * This is the first distributed signal 
 *   (the node tries to register in the cluster)
 */
class CmRegReq {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 6 + NdbNodeBitmask::Size );
private:
  
  Uint32 blockRef;
  Uint32 nodeId;
  Uint32 version;    // See ndb_version.h
  Uint32 mysql_version;

  Uint32 start_type; // As specified by cmd-line or mgm, NodeState::StartType
  Uint32 latest_gci; // 0 means no fs
  Uint32 skip_nodes[NdbNodeBitmask::Size]; // Nodes that does not _need_ 
                                           // to be part of restart
};

/**
 * The node receving this signal has been accepted into the cluster
 */
class CmRegConf {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 5 + NdbNodeBitmask::Size );
private:
  
  Uint32 presidentBlockRef;
  Uint32 presidentNodeId;
  Uint32 presidentVersion;
  Uint32 presidentMysqlVersion;

  /**
   * The dynamic id that the node reciving this signal has
   */
  Uint32 dynamicId;
  Uint32 allNdbNodes[NdbNodeBitmask::Size];  
};

/**
 * 
 */
class CmRegRef {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 7 + NdbNodeBitmask::Size );
  
  enum ErrorCode {
    ZBUSY = 0,          /* Only the president can send this */
    ZBUSY_PRESIDENT = 1,/* Only the president can send this */
    ZBUSY_TO_PRES = 2,  /* Only the president can send this */
    ZNOT_IN_CFG = 3,    /* Only the president can send this */
    ZELECTION = 4,      /* Receiver is definitely not president,
                         * but we are not sure if sender ends up
                         * as president. */
    ZNOT_PRESIDENT = 5, /* We are not president */
    ZNOT_DEAD = 6,       /* We are not dead when we are starting  */
    ZINCOMPATIBLE_VERSION = 7,
    ZINCOMPATIBLE_START_TYPE = 8,
    ZSINGLE_USER_MODE = 9, /* The cluster is in single user mode,
			    * data node is not allowed to get added
			    * in the cluster while in single user mode */
    ZGENERIC = 100 /* The generic error code */
  };
private:
  
  Uint32 blockRef;
  Uint32 nodeId;
  Uint32 errorCode;
  /**
   * Applicable if ZELECTION
   */
  Uint32 presidentCandidate;
  Uint32 candidate_latest_gci; // 0 means non

  /**
   * Data for sending node sending node
   */
  Uint32 latest_gci; 
  Uint32 start_type; 
  Uint32 skip_nodes[NdbNodeBitmask::Size]; // Nodes that does not _need_ 
                                           // to be part of restart
};

class CmAdd {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 4 );
  
private:
  enum RequestType {
    Prepare   = 0,
    AddCommit = 1,
    CommitNew = 2
  };
  
  Uint32 requestType;
  Uint32 startingNodeId;
  Uint32 startingVersion;
  Uint32 startingMysqlVersion;
};

class CmAckAdd {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 3 );
  
private:
  Uint32 senderNodeId;
  Uint32 requestType; // see CmAdd::RequestType
  Uint32 startingNodeId;
};

class CmNodeInfoReq {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 5 );
  
private:
  /**
   * This is information for sending node (starting node)
   */
  Uint32 nodeId;
  Uint32 dynamicId;
  Uint32 version;
  Uint32 mysql_version;
  Uint32 lqh_workers;   // added in telco-6.4
};

class CmNodeInfoRef {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 3 );

  enum ErrorCode {
    NotRunning = 1
  };
  
private:
  Uint32 nodeId;
  Uint32 errorCode;
};

class CmNodeInfoConf {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 5 );
  
private:
  Uint32 nodeId;
  Uint32 dynamicId;
  Uint32 version;
  Uint32 mysql_version;
  Uint32 lqh_workers;   // added in telco-6.4
};


#undef JAM_FILE_ID

#endif
