/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CM_REG_HPP
#define CM_REG_HPP

#include <NodeBitmask.hpp>

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
  STATIC_CONST( SignalLength = 3 );
private:
  
  Uint32 blockRef;
  Uint32 nodeId;
  Uint32 version; // See ndb_version.h
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
  STATIC_CONST( SignalLength = 4 + NdbNodeBitmask::Size );
private:
  
  Uint32 presidentBlockRef;
  Uint32 presidentNodeId;
  Uint32 presidentVersion;

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
  STATIC_CONST( SignalLength = 4 );
  
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
    ZINCOMPATIBLE_VERSION = 7
  };
private:
  
  Uint32 blockRef;
  Uint32 nodeId;
  Uint32 errorCode;
  Uint32 presidentCandidate;
};

class CmAdd {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 3 );
  
private:
  enum RequestType {
    Prepare   = 0,
    AddCommit = 1,
    CommitNew = 2
  };
  
  Uint32 requestType;
  Uint32 startingNodeId;
  Uint32 startingVersion;
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
  STATIC_CONST( SignalLength = 3 );
  
private:
  /**
   * This is information for sending node (starting node)
   */
  Uint32 nodeId;
  Uint32 dynamicId;
  Uint32 version;
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
  STATIC_CONST( SignalLength = 3 );
  
private:
  Uint32 nodeId;
  Uint32 dynamicId;
  Uint32 version;
};

#endif







