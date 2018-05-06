/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef LCP_SIGNAL_DATA_HPP
#define LCP_SIGNAL_DATA_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 170


struct StartLcpReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;
  
  /**
   * Sender(s) / Receiver(s)
   */
  
  /**
   * Receiver(s)
   */
  friend class Dblqh;

  friend bool printSTART_LCP_REQ(FILE *, const Uint32 *, Uint32, Uint16);  
public:

  STATIC_CONST( SignalLength = 2 + 2 * NdbNodeBitmask::Size + 1 );
  Uint32 senderRef;
  Uint32 lcpId;
  
  NdbNodeBitmask participatingDIH;
  NdbNodeBitmask participatingLQH;

  enum PauseStart
  {
    NormalLcpStart = 0,
    PauseLcpStartFirst = 1,
    PauseLcpStartSecond = 2
  };

  /**
   * pauseStart = 0 normal start
   * pauseStart = 1 starting node into already running LCP,
   *                bitmasks contains participants
   * pauseStart = 2 starting node into already running LCP,
   *                bitmasks contains completion bitmasks
   * pauseStart = 1 requires no response since pauseStart = 2 will arrive
   *                immediately after it.
   */
  PauseStart pauseStart;
};

class StartLcpConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;
  
  /**
   * Sender(s) / Receiver(s)
   */
  
  /**
   * Receiver(s)
   */
  friend class Dbdih;

  friend bool printSTART_LCP_CONF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 senderRef;
  Uint32 lcpId;
};

/**
 * This signals is sent by Dbdih to Dblqh
 * to order checkpointing of a certain
 * fragment.
 */
class LcpFragOrd {
  /**
   * Sender(s)
   */
  friend class Dbdih;
  friend class Lgman;
  friend class Pgman;
  friend class Dbtup;
  friend class Backup;

  /**
   * Sender(s) / Receiver(s)
   */
  
  /**
   * Receiver(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;
  friend class PgmanProxy;

  friend bool printLCP_FRAG_ORD(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 6 );
private:
  
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 lcpNo;
  Uint32 lcpId;
  Uint32 lastFragmentFlag;
  Uint32 keepGci;
};


struct LcpFragRep {
  /**
   * Sender(s) and receiver(s)
   */
  friend class Dbdih;

  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

  friend bool printLCP_FRAG_REP(FILE *, const Uint32 *, Uint32, Uint16);  

  STATIC_CONST( SignalLength = 7 );
  STATIC_CONST( SignalLengthTQ = 8 );
  STATIC_CONST( BROADCAST_REQ = 0 );

  Uint32 nodeId;
  Uint32 lcpId;
  Uint32 lcpNo;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 maxGciCompleted;
  Uint32 maxGciStarted;
  Uint32 fromTQ;
};

class LcpCompleteRep {
  /**
   * Sender(s) and receiver(s)
   */
  friend class Dbdih;
  
  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

  friend bool printLCP_COMPLETE_REP(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 3 );
  STATIC_CONST( SignalLengthTQ = 4 );
  
private:
  Uint32 nodeId;
  Uint32 blockNo;
  Uint32 lcpId;
  Uint32 fromTQ;
};

struct LcpPrepareReq 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lcpNo;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 lcpId;
  Uint32 backupPtr;
  Uint32 backupId;
  Uint32 createGci;
  Uint32 localLcpId;

  STATIC_CONST( SignalLength = 10 );
};

struct LcpPrepareRef
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 errorCode;
  
  STATIC_CONST( SignalLength = 5 );
};

struct LcpPrepareConf 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragmentId;
  
  STATIC_CONST( SignalLength = 4 );
};

struct SyncPageCacheReq
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragmentId;

  STATIC_CONST( SignalLength = 4 );
};

struct SyncPageCacheConf
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 diskDataExistFlag;

  STATIC_CONST( SignalLength = 5 );
};

struct SyncExtentPagesReq
{
  enum LcpOrder
  {
    FIRST_LCP = 0,
    INTERMEDIATE_LCP = 1,
    END_LCP = 2
  };
  Uint32 senderData;
  Uint32 senderRef;
  LcpOrder lcpOrder;

  STATIC_CONST( SignalLength = 3 );
};

struct SyncExtentPagesConf
{
  Uint32 senderData;
  Uint32 senderRef;

  STATIC_CONST( SignalLength = 2 );
};

struct EndLcpReq 
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 backupPtr;
  Uint32 backupId;
  // extra word for LQH worker to proxy
  Uint32 proxyBlockNo;

  STATIC_CONST( SignalLength = 4 );
};

struct EndLcpConf
{
  Uint32 senderData;
  Uint32 senderRef;
  
  STATIC_CONST( SignalLength = 2 );
};

struct LcpStatusReq
{
  /** 
   * Sender(s)
   */
  friend class Dblqh;
  
  /**
   * Sender(s) / Receiver(s)
   */

  /**
   * Receiver(s)
   */
  friend class Backup;

  friend bool printLCP_STATUS_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 senderRef;
  Uint32 senderData;
};

struct LcpStatusConf
{
  /** 
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Sender(s) / Receiver(s)
   */

  /**
   * Receiver(s)
   */
  friend class Dblqh;

  friend bool printLCP_STATUS_CONF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 12 );

  enum LcpState
  {
    LCP_IDLE       = 0,
    LCP_PREPARED   = 1,
    LCP_SCANNING   = 2,
    LCP_SCANNED    = 3,
    LCP_PREPARE_READ_CTL_FILES = 4,
    LCP_PREPARE_OPEN_DATA_FILE = 5,
    LCP_PREPARE_READ_TABLE_DESC = 6,
    LCP_PREPARE_ABORTING = 7,
    LCP_WAIT_END_LCP = 8,
    LCP_PREPARE_WAIT_DROP_CASE = 9,
    LCP_WAIT_SYNC_DISK = 10,
    LCP_WAIT_SYNC_EXTENT = 11,
    LCP_WAIT_WRITE_CTL_FILE = 12,
    LCP_WAIT_CLOSE_EMPTY = 13,
    LCP_WAIT_FINAL_SYNC_EXTENT = 14
  };
private:
  Uint32 senderRef;
  Uint32 senderData;
  /* Backup stuff */
  Uint32 lcpState;
  /* In lcpState == LCP_IDLE, refers to prev LCP
   * otherwise, refers to current running LCP
   */
  Uint32 lcpDoneRowsHi;
  Uint32 lcpDoneRowsLo;
  Uint32 lcpDoneBytesHi;
  Uint32 lcpDoneBytesLo;
  
  Uint32 tableId;
  Uint32 fragId;
  /* Backup stuff valid iff lcpState == LCP_SCANNING or
   * LCP_SCANNED
   * For LCP_SCANNING contains row count of rows scanned
   *  (Increases as scan proceeds)
   * For LCP_SCANNED contains bytes remaining to be flushed
   * to file.
   *  (Decreases as buffer drains to file)
   *
   * lcpScannedPages is number of pages scanned by TUP, it is possible
   * to scan for a long while only finding LCP_SKIP records, so this
   * is necessary to check as well for progress.
   */
  Uint32 completionStateHi;
  Uint32 completionStateLo;

  Uint32 lcpScannedPages;
};

struct LcpStatusRef
{
  /** 
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Sender(s) / Receiver(s)
   */

  /**
   * Receiver(s)
   */
  friend class Dblqh;

  friend bool printLCP_STATUS_REF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 3 );
  
  enum StatusFailCodes
  {
    NoLCPRecord    = 1,
    NoTableRecord  = 2,
    NoFileRecord   = 3
  };

private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 error;
};

class PauseLcpReq
{
public:
  STATIC_CONST (SignalLength = 3 );

  enum PauseAction
  {
    NoAction = 0,
    Pause = 1,
    UnPauseIncludedInLcp = 2,
    UnPauseNotIncludedInLcp = 3
  };
  Uint32 senderRef;
  Uint32 pauseAction;
  Uint32 startNodeId;
};

class PauseLcpConf
{
public:
  STATIC_CONST (SignalLength = 2 );

  Uint32 senderRef;
  Uint32 startNodeId;
};

class FlushLcpRepReq
{
public:
  STATIC_CONST (SignalLength = 2 );

  Uint32 senderRef;
  Uint32 startNodeId;
};

class FlushLcpRepConf
{
public:
  STATIC_CONST (SignalLength = 2 );

  Uint32 senderRef;
  Uint32 startNodeId;
};

class WaitCompleteLcpConf
{
public:
  STATIC_CONST (SignalLength = 5);

  Uint32 senderRef;
  Uint32 lcpId;
  Uint32 localLcpId;
  Uint32 maxGciInLcp;
  Uint32 maxKeepGci;
};
class LcpAllCompleteReq
{
public:
  STATIC_CONST (SignalLength = 4);

  Uint32 senderRef;
  Uint32 lcpId;
  Uint32 maxGciInLcp;
  Uint32 maxKeepGci;
};

class GetLocalLcpIdConf
{
public:
  STATIC_CONST (SignalLength = 2);

  Uint32 lcpId;
  Uint32 localLcpId;
};
#undef JAM_FILE_ID

#endif
