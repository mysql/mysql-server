/*
   Copyright (C) 2003-2007 MySQL AB
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

#ifndef UTIL_LOCK_HPP
#define UTIL_LOCK_HPP

#include "SignalData.hpp"

class UtilLockReq {
  
  /**
   * Receiver
   */
  friend class DbUtil;
  
  /**
   * Sender
   */
  friend class Dbdih;
  friend class MutexManager;

  friend bool printUTIL_LOCK_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );

  enum RequestInfo {
    TryLock    = 1,
    SharedLock = 2,
    Notify     = 4,
    Granted    = 8
  };

public:
  Uint32 senderData;  
  Uint32 senderRef;
  Uint32 lockId;
  Uint32 requestInfo;
  Uint32 extra;
};

class UtilLockConf {
  
  /**
   * Receiver
   */
  friend class Dbdih;
  friend class MutexManager;  

  /**
   * Sender
   */
  friend class DbUtil;

  friend bool printUTIL_LOCK_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
  Uint32 extra;
};

class UtilLockRef {
  
  /**
   * Reciver
   */
  friend class Dbdih;
  friend class MutexManager;
  
  /**
   * Sender
   */
  friend class DbUtil;
  
  friend bool printUTIL_LOCK_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  
  enum ErrorCode {
    OK = 0,
    NoSuchLock = 1,
    OutOfLockRecords = 2,
    DistributedLockNotSupported = 3,
    LockAlreadyHeld = 4,
    InLockQueue = 5 // lock + notify
  };
public:

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
  Uint32 errorCode;
  Uint32 extra;
};

class UtilUnlockReq {
  
  /**
   * Receiver
   */
  friend class DbUtil;
  
  /**
   * Sender
   */
  friend class Dbdih;
  friend class MutexManager;

  friend bool printUTIL_UNLOCK_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
  
public:
  Uint32 senderData;  
  Uint32 senderRef;
  Uint32 lockId;
};

class UtilUnlockConf {
  
  /**
   * Receiver
   */
  friend class Dbdih;
  friend class MutexManager;  

  /**
   * Sender
   */
  friend class DbUtil;

  friend bool printUTIL_UNLOCK_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
  
public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
};

class UtilUnlockRef {
  
  /**
   * Reciver
   */
  friend class Dbdih;
  friend class MutexManager;
  
  /**
   * Sender
   */
  friend class DbUtil;
  
  friend bool printUTIL_UNLOCK_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
  enum ErrorCode {
    OK = 0,
    NoSuchLock = 1,
    NotLockOwner = 2,
    NotInLockQueue = 3
  };
public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
  Uint32 errorCode;
};

/**
 * Creating a lock
 */
class UtilCreateLockReq {
  /**
   * Receiver
   */
  friend class DbUtil;
  
  /**
   * Sender
   */
  friend class MutexManager;
  
  friend bool printUTIL_CREATE_LOCK_REQ(FILE *, const Uint32*, Uint32, Uint16);
public:
  enum LockType {
    Mutex = 0 // Lock with only exclusive locks
  };

  STATIC_CONST( SignalLength = 4 );

public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
  Uint32 lockType;
};

class UtilCreateLockRef {
  /**
   * Sender
   */
  friend class DbUtil;
  
  /**
   * Receiver
   */
  friend class MutexManager;

  friend bool printUTIL_CREATE_LOCK_REF(FILE *, const Uint32*, Uint32, Uint16);
public:
  enum ErrorCode {
    OK = 0,
    OutOfLockQueueRecords = 1,
    LockIdAlreadyUsed = 2,
    UnsupportedLockType = 3
  };

  STATIC_CONST( SignalLength = 4 );

public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
  Uint32 errorCode;
};

class UtilCreateLockConf {
  /**
   * Sender
   */
  friend class DbUtil;
  
  /**
   * Receiver
   */
  friend class MutexManager;

  friend bool printUTIL_CREATE_LOCK_CONF(FILE*, const Uint32*, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );

public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
};

/**
 * Creating a lock
 */
class UtilDestroyLockReq {
  /**
   * Receiver
   */
  friend class DbUtil;
  
  /**
   * Sender
   */
  friend class MutexManager;
  
  friend bool printUTIL_DESTROY_LOCK_REQ(FILE *, const Uint32*, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
};

class UtilDestroyLockRef {
  /**
   * Sender
   */
  friend class DbUtil;
  
  /**
   * Receiver
   */
  friend class MutexManager;

  friend bool printUTIL_DESTROY_LOCK_REF(FILE *, const Uint32*, Uint32, Uint16);
public:
  enum ErrorCode {
    OK = 0,
    NoSuchLock = 1,
    NotLockOwner = 2
  };

  STATIC_CONST( SignalLength = 4 );

public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
  Uint32 errorCode;
};

class UtilDestroyLockConf {
  /**
   * Sender
   */
  friend class DbUtil;
  
  /**
   * Receiver
   */
  friend class MutexManager;

  friend bool printUTIL_DESTROY_LOCK_CONF(FILE*, const Uint32*, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );

public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
};

#endif
