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

#ifndef ACC_LOCK_HPP
#define ACC_LOCK_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 153


/*
 * Lock or unlock tuple.  If lock request is queued, the reply is later
 * via ACCKEYCONF.
 */
class AccLockReq {
  friend class Dbacc;
  friend class Dbtup;
  friend class Dbtux;
  friend bool printACC_LOCKREQ(FILE *, const Uint32*, Uint32, Uint16);
public:
  enum RequestType {    // first byte
    LockShared = 1,
    LockExclusive = 2,
    Unlock = 3,
    Abort = 4,
    AbortWithConf = 5
  };
  enum RequestFlag {    // second byte
  };
  enum ReturnCode {
    Success = 0,
    IsBlocked = 1,      // was put in lock queue
    WouldBlock = 2,     // if we add non-blocking option
    Refused = 3,
    NoFreeOp = 4
  };
  STATIC_CONST( LockSignalLength = 13 );
  STATIC_CONST( UndoSignalLength = 3 );
private:
  Uint32 returnCode;
  Uint32 requestInfo;
  Uint32 accOpPtr;
  // rest only if lock request
  Uint32 userPtr;
  Uint32 userRef;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 fragPtrI;
  Uint32 hashValue;
  Uint32 page_id;
  Uint32 page_idx;
  Uint32 transId1;
  Uint32 transId2;
};


#undef JAM_FILE_ID

#endif
