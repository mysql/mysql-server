/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef GCP_HPP
#define GCP_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 131


/**
 * Sent as a distributed signal DIH-DIH.
 *
 * Also sent locally from DIH to SUMA.
 */
struct GCPPrepare
{
  static constexpr Uint32 SignalLength = 3;

  Uint32 nodeId;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPPrepareConf // Distr. DIH-DIH
{
  static constexpr Uint32 SignalLength = 3;

  Uint32 nodeId;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPCommit // Distr. DIH-DIH
{
  static constexpr Uint32 SignalLength = 3;

  Uint32 nodeId;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPNoMoreTrans // Local DIH/TC
{
  static constexpr Uint32 SignalLength = 4;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct GCPTCFinished // Local TC-DIH
{
  static constexpr Uint32 SignalLength = 4;

  Uint32 senderData;
  Uint32 gci_hi;
  Uint32 gci_lo;
  Uint32 tcFailNo;
};

struct GCPNodeFinished // Distr. DIH-DIH
{
  static constexpr Uint32 SignalLength = 4;

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
  static constexpr Uint32 SignalLength = 3;

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
  static constexpr Uint32 SignalLength = 4;

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
  static constexpr Uint32 SignalLength = 3;

private:
  Uint32 dihPtr;
  Uint32 nodeId;
  Uint32 gci;
};


#undef JAM_FILE_ID

#endif
