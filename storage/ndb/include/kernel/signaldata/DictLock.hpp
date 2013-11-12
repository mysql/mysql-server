/*
   Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DICT_LOCK_HPP
#define DICT_LOCK_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 133


// see comments in Dbdict.hpp

class DictLockReq {
  friend class Dbdict;
  friend class Dbdih;
  friend class Suma;
public:
  STATIC_CONST( SignalLength = 3 );
  enum LockType {
    NoLock = 0
    ,NodeRestartLock = 1 // S-lock
    ,NodeFailureLock = 2 // S-lock
    ,SchemaTransLock = 3
    // non-trans op locks
    ,CreateFileLock  = 8
    ,CreateFilegroupLock = 9
    ,DropFileLock    = 10
    ,DropFilegroupLock = 11
    ,SumaStartMe = 12
    ,SumaHandOver = 13
  };
private:
  Uint32 userPtr;
  Uint32 lockType;
  Uint32 userRef;
};

class DictLockConf {
  friend class Dbdict;
  friend class Dbdih;
  friend class Suma;
public:
  STATIC_CONST( SignalLength = 3 );
private:
  Uint32 userPtr;
  Uint32 lockType;
  Uint32 lockPtr;
};

class DictLockRef {
  friend class Dbdict;
  friend class Dbdih;
  friend class Suma;
public:
  STATIC_CONST( SignalLength = 3 );
  enum ErrorCode {
    NotMaster = 1,
    InvalidLockType = 2,
    BadUserRef = 3,
    TooLate = 4,
    TooManyRequests = 5
  };
private:
  Uint32 userPtr;
  Uint32 lockType;
  Uint32 errorCode;
};

class DictUnlockOrd {
  friend class Dbdict;
  friend class Dbdih;
  friend class Suma;
public:
  STATIC_CONST( SignalLength = 4 );

  Uint32 lockPtr;
  Uint32 lockType;
  Uint32 senderData;
  Uint32 senderRef;
};


#undef JAM_FILE_ID

#endif
