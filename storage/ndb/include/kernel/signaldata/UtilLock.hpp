/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef UTIL_LOCK_HPP
#define UTIL_LOCK_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 34


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
  static constexpr Uint32 SignalLength = 5;

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
  static constexpr Uint32 SignalLength = 4;
  
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
  static constexpr Uint32 SignalLength = 5;
  
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
  static constexpr Uint32 SignalLength = 3;
  
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
  static constexpr Uint32 SignalLength = 3;
  
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
  static constexpr Uint32 SignalLength = 4;
  
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

  static constexpr Uint32 SignalLength = 4;

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

  static constexpr Uint32 SignalLength = 4;

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
  static constexpr Uint32 SignalLength = 3;

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
  static constexpr Uint32 SignalLength = 4;

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

  static constexpr Uint32 SignalLength = 4;

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
  static constexpr Uint32 SignalLength = 3;

public:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 lockId;
};


#undef JAM_FILE_ID

#endif
