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

#ifndef BACKUP_HPP
#define BACKUP_HPP

#include <NodeBitmask.hpp>
#include "SignalData.hpp"

#define JAM_FILE_ID 101

/**
 * Request to start a backup
 */
class BackupReq {
  /**
   * Sender(s)
   */
  friend class MgmtSrvr;

  /**
   * Reciver(s)
   */
  friend class Backup;

  friend bool printBACKUP_REQ(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 4;
  static constexpr Uint32 WAITCOMPLETED = 0x3;
  static constexpr Uint32 USE_UNDO_LOG = 0x4;
  static constexpr Uint32 MT_BACKUP = 0x8;
  static constexpr Uint32 ENCRYPTED_BACKUP = 0x10;
  static constexpr Uint32 NOWAIT_REPLY = 0x20;

 private:
  Uint32 senderData;
  Uint32 backupDataLen;
  /* & 0x3 - waitCompleted
   * & 0x4 - use undo log
   */
  Uint32 flags;
  Uint32 inputBackupId;
};

class BackupData {
  /**
   * Sender(s)
   */
  friend class BackupMaster;

  /**
   * Reciver(s)
   */
  friend class Backup;

  friend bool printBACKUP_DATA(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 25;

  enum KeyValues {
    /**
     * Buffer(s) and stuff
     */
    BufferSize = 1,  // In MB
    BlockSize = 2,   // Write in chunks of this (in bytes)
    MinWrite = 3,    // Minimum write as multiple of blocksize
    MaxWrite = 4,    // Maximum write as multiple of blocksize

    // Max throughput
    // Parallel files

    NoOfTables = 1000,
    TableName = 1001  // char*
  };

 private:
  enum RequestType { ClientToMaster = 1, MasterToSlave = 2 };
  Uint32 requestType;

  union {
    Uint32 backupPtr;
    Uint32 senderData;
  };
  Uint32 backupId;

  /**
   * totalLen = totalLen_offset >> 16
   * offset = totalLen_offset & 0xFFFF
   */
  Uint32 totalLen_offset;

  /**
   * Length in this = signal->length() - 3
   * Sender block ref = signal->senderBlockRef()
   */
  Uint32 backupData[21];
};

/**
 * The request to start a backup was refused
 */
class BackupRef {
  /**
   * Sender(s)
   */
  friend class Backup;

  /**
   * Reciver(s)
   */
  friend class MgmtSrvr;

  friend bool printBACKUP_REF(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 3;

 private:
  enum ErrorCodes {
    Undefined = 1300,
    IAmNotMaster = 1301,
    OutOfBackupRecord = 1302,
    OutOfResources = 1303,
    SequenceFailure = 1304,
    BackupDefinitionNotImplemented = 1305,
    CannotBackupDiskless = 1306,
    EncryptionNotSupported = 1307,
    EncryptionPasswordMissing = 1308,
    BadEncryptionPassword = 1309,
    EncryptionPasswordTooLong = 1310,
    EncryptionPasswordZeroLength = 1311,
    BackupDuringUpgradeUnsupported = 1329
  };
  Uint32 senderData;
  Uint32 errorCode;
  union {
    Uint32 masterRef;
  };
};

/**
 * The backup has started
 */
class BackupConf {
  /**
   * Sender(s)
   */
  friend class Backup;

  /**
   * Reciver(s)
   */
  friend class MgmtSrvr;

  friend bool printBACKUP_CONF(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 2;

 private:
  Uint32 senderData;
  Uint32 backupId;
};

/**
 * A backup has been aborted
 */
class BackupAbortRep {
  /**
   * Sender(s)
   */
  friend class Backup;

  /**
   * Reciver(s)
   */
  friend class MgmtSrvr;

  friend bool printBACKUP_ABORT_REP(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 3;

 private:
  Uint32 senderData;
  Uint32 backupId;
  Uint32 reason;
};

/**
 * A backup has been completed
 */
class BackupCompleteRep {
  /**
   * Sender(s)
   */
  friend class Backup;

  /**
   * Reciver(s)
   */
  friend class MgmtSrvr;

  friend bool printBACKUP_COMPLETE_REP(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 12;

 private:
  Uint32 senderData;
  Uint32 backupId;
  Uint32 startGCP;
  Uint32 stopGCP;
  Uint32 noOfBytesLow;
  Uint32 noOfRecordsLow;
  Uint32 noOfLogBytes;
  Uint32 noOfLogRecords;
  Uint32 unused[2];
  Uint32 noOfBytesHigh;
  Uint32 noOfRecordsHigh;
};

/**
 * A master has finished taking-over backup responsiblility
 */
class BackupNFCompleteRep {
  friend bool printBACKUP_NF_COMPLETE_REP(FILE *, const Uint32 *, Uint32,
                                          Uint16);
};

/**
 * Abort of backup
 */
class AbortBackupOrd {
  /**
   * Sender / Reciver
   */
  friend class Backup;
  friend class BackupProxy;
  friend class MgmtSrvr;

  friend bool printABORT_BACKUP_ORD(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 4;

  enum RequestType {
    ClientAbort = 1321,
    BackupComplete = 1322,
    BackupFailure = 1323,    // General backup failure coordinator -> slave
    LogBufferFull = 1324,    //                        slave -> coordinator
    FileOrScanError = 1325,  //                       slave -> coordinator
    BackupFailureDueToNodeFail = 1326,  //             slave -> slave
    OkToClean = 1327                    //             master -> slave

    ,
    AbortScan = 1328,
    IncompatibleVersions = 1329
  };

 private:
  Uint32 requestType;
  Uint32 backupId;
  union {
    Uint32 backupPtr;
    Uint32 senderData;
  };
  Uint32 senderRef;
};

#undef JAM_FILE_ID

#endif
