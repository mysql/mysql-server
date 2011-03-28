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

#ifndef WAIT_GCP_HPP
#define WAIT_GCP_HPP

/**
 * This signal is sent by anyone to local DIH
 *
 * If local DIH is not master, it forwards it to master DIH
 *   and start acting as a proxy
 *
 */
class WaitGCPReq {
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Sender
   */
  friend class Ndbcntr;
  friend class Dbdict;
  friend class Backup;
  friend class Trix;

public:
  STATIC_CONST( SignalLength = 3 );
public:
  enum RequestType {
    Complete = 1,           ///< Wait for a GCP to complete
    CompleteForceStart = 2, ///< Wait for a GCP to complete start one if needed
    CompleteIfRunning = 3,  ///< Wait for ongoing GCP
    CurrentGCI        = 8,  ///< Immediately return current GCI
    BlockStartGcp     = 9,
    UnblockStartGcp   = 10,
    WaitEpoch         = 11, // If GCP is blocked, wait for epoch to not start
    RestartGCI        = 12  // Return restart GCI
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
};

class WaitGCPConf {

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;
  friend class Dbdict;
  friend class Backup;
  friend class Trix;

public:
  STATIC_CONST( SignalLength = 4 );
  
public:
  Uint32 senderData;
  Uint32 gci_hi;
  Uint32 blockStatus;
  Uint32 gci_lo;
};

class WaitGCPRef {
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;
  friend class Dbdict;
  friend class Backup;
  friend class Trix;
  friend class NdbDictInterface;

public:
  STATIC_CONST( SignalLength = 2 );

  enum ErrorCode {
    StopOK = 0,
    NF_CausedAbortOfProcedure = 1,
    NoWaitGCPRecords = 2,
    NF_MasterTakeOverInProgress = 3
  };
  
private:
  Uint32 errorCode;
  Uint32 senderData;
};

#endif
