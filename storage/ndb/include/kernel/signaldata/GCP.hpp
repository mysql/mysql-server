/*
   Copyright (C) 2003, 2005-2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef GCP_HPP
#define GCP_HPP

#include "SignalData.hpp"

/**
 * Sent as a distributed signal DIH-DIH.
 *
 * Also sent locally from DIH to SUMA.
 */
struct GCPPrepare
{
  STATIC_CONST( SignalLength = 3 );

  Uint32 nodeId;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPPrepareConf // Distr. DIH-DIH
{
  STATIC_CONST( SignalLength = 3 );

  Uint32 nodeId;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPCommit // Distr. DIH-DIH
{
  STATIC_CONST( SignalLength = 3 );

  Uint32 nodeId;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPNoMoreTrans // Local DIH/TC
{
  STATIC_CONST( SignalLength = 4 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPTCFinished // Local TC-DIH
{
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderData;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPNodeFinished // Distr. DIH-DIH
{
  STATIC_CONST( SignalLength = 4 );

  Uint32 nodeId;
  Uint32 gci_hi;
  Uint32 failno;
  Uint32 gci_lo;
};

/**
 * GCPSaveReq / (Ref/Conf) is sent as part of GCP
 */
class GCPSaveReq // Distr. DIH-LQH
{
  /**
   * Sender(s)
   */
  friend class Dbdih;
  
  /**
   * Reciver(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

  friend bool printGCPSaveReq(FILE * output, const Uint32 * theData, 
			      Uint32 len, Uint16 receiverBlockNo);
public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 dihBlockRef;
  Uint32 dihPtr;
  Uint32 gci;
};

class GCPSaveRef // Distr. LQH-DIH
{
  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;
  
  /**
   * Reciver(s)
   */
  friend class Dbdih;
  
  friend bool printGCPSaveRef(FILE * output, const Uint32 * theData, 
			      Uint32 len, Uint16 receiverBlockNo);
public:
  STATIC_CONST( SignalLength = 4 );

  enum ErrorCode {
    NodeShutdownInProgress = 1,
    FakedSignalDueToNodeFailure = 2,
    NodeRestartInProgress = 3
  };
  
private:
  Uint32 dihPtr;
  Uint32 nodeId;
  Uint32 gci;
  Uint32 errorCode;
};

class GCPSaveConf // Distr. LQH-DIH
{
  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;
  
  /**
   * Reciver(s)
   */
  friend class Dbdih;

  friend bool printGCPSaveConf(FILE * output, const Uint32 * theData, 
			       Uint32 len, Uint16 receiverBlockNo);
public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 dihPtr;
  Uint32 nodeId;
  Uint32 gci;
};

#endif
