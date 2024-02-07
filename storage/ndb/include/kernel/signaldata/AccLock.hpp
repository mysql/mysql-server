/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
  friend bool printACC_LOCKREQ(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  enum RequestType {  // first byte
    LockShared = 1,
    LockExclusive = 2,
    Unlock = 3,
    Abort = 4,
    AbortWithConf = 5
  };
  enum RequestFlag {  // second byte
  };
  enum ReturnCode {
    Success = 0,
    IsBlocked = 1,   // was put in lock queue
    WouldBlock = 2,  // if we add non-blocking option
    Refused = 3,
    NoFreeOp = 4
  };
  static constexpr Uint32 LockSignalLength = 14;
  static constexpr Uint32 UndoSignalLength = 3;

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
  Uint32 isCopyFragScan;
};

#undef JAM_FILE_ID

#endif
