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

#ifndef BACKUP_IMPL_HPP
#define BACKUP_IMPL_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 78


/*
 * DefineBackupReq
 *
 * Global signal, but only between data nodes of same version since mixed
 * version backup is not allowed.
 * No logic for mixed versions is needed.
 */
class DefineBackupReq {
  /**
   * Sender(s)
   */
  friend class BackupMaster;
  
  /**
   * Reciver(s)
   */
  friend class Backup;
  friend class BackupProxy;
  friend class Dblqh;

  friend bool printDEFINE_BACKUP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength_v1 = 11 + NdbNodeBitmask48::Size;

private:
  /**
   * i - value of backup object
   */
  Uint32 backupPtr;

  Uint32 backupId;
  Uint32 clientRef;
  Uint32 clientData;
  Uint32 senderRef;

  /**
   * Which node(s) is participating in the backup
   * Note: Only to support versions < 8.0.18
   */
  NdbNodeBitmask48 nodes;
  
  /**
   * Generated random number
   */
  Uint32 backupKey[2];
  
  /**
   * Length of backup data
   */
  Uint32 backupDataLen;

  /**
   * Backup flags
   */
  /* & 0x3 - waitCompleted
   * & 0x4 - Use undo log
   */
  Uint32 flags;
  /**
   * Reference of block which controls backup across all nodes
   */
  Uint32 masterRef;
  Uint32 senderData;
};

class DefineBackupRef {
  /**
   * Sender(s)
   */
  friend class Backup;
  friend class BackupProxy;
  friend class Dblqh;

  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printDEFINE_BACKUP_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 4;
  
  enum ErrorCode {
    Undefined = 1340,
    FailedToAllocateBuffers = 1342,
    FailedToSetupFsBuffers = 1343,
    FailedToAllocateTables = 1344,
    FailedInsertFileHeader = 1345,
    FailedInsertTableList = 1346,
    FailedAllocateTableMem = 1347,
    FailedToAllocateFileRecord = 1348,
    FailedToAllocateAttributeRecord = 1349,
    FailedForBackupFilesAleadyExist = 1350
  };
private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 errorCode;
  Uint32 nodeId;
};

class DefineBackupConf {
  /**
   * Sender(s)
   */
  friend class Backup;
  friend class BackupProxy;
  friend class Dblqh;

  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printDEFINE_BACKUP_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 2;
  
private:
  Uint32 backupId;
  Uint32 backupPtr;
};

class StartBackupReq {
  /**
   * Sender(s)
   */
  friend class BackupMaster;
  
  /**
   * Reciver(s)
   */
  friend class Backup;
  friend class BackupProxy;

  friend bool printSTART_BACKUP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:

  static constexpr Uint32 SignalLength = 4;

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 senderRef;
  Uint32 senderData;
};

class StartBackupRef {
  /**
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;
  friend class BackupProxy;

  friend bool printSTART_BACKUP_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 4;

  enum ErrorCode {
    FailedToAllocateTriggerRecord = 1,
    FailedStartSinceDefineFailed = 1351
  };
private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 errorCode;
  Uint32 nodeId;
};

class StartBackupConf {
  /**
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printSTART_BACKUP_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 2;

private:
  Uint32 backupId;
  Uint32 backupPtr;
};

class BackupFragmentReq {
  /**
   * Sender(s)
   */
  friend class BackupMaster;
  friend class BackupProxy;
  
  /**
   * Reciver(s)
   */
  friend class Backup;
  friend class Dblqh;

  friend bool printBACKUP_FRAGMENT_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 7;

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 tableId;
  Uint32 fragmentNo;
  Uint32 count;
  Uint32 senderRef;
  Uint32 senderData;
};

class BackupFragmentRef {
  /**
   * Sender(s)
   */
  friend class Backup;
  friend class Dblqh;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;
  friend class BackupProxy;

  friend bool printBACKUP_FRAGMENT_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 5;

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 errorCode;
  Uint32 nodeId;
};

class BackupFragmentConf {
  /**
   * Sender(s)
   */
  friend class Backup;
  friend class Dblqh;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printBACKUP_FRAGMENT_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 8;

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 tableId;
  Uint32 fragmentNo;
  Uint32 noOfRecordsLow;
  Uint32 noOfBytesLow;
  Uint32 noOfRecordsHigh;
  Uint32 noOfBytesHigh;
};

class BackupFragmentCompleteRep {
public:
  static constexpr Uint32 SignalLength = 8;

  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 tableId;
  Uint32 fragmentNo;
  Uint32 noOfTableRowsLow;
  Uint32 noOfFragmentRowsLow;
  Uint32 noOfTableRowsHigh;
  Uint32 noOfFragmentRowsHigh;
};

class StopBackupReq {
  /**
   * Sender(s)
   */
  friend class BackupMaster;
  
  /**
   * Reciver(s)
   */
  friend class Backup;
  friend class BackupProxy;

  friend bool printSTOP_BACKUP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 6;

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 startGCP;
  Uint32 stopGCP;
  Uint32 senderRef;
  Uint32 senderData;
};

class StopBackupRef {
  /**
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;
  friend class BackupProxy;

  friend bool printSTOP_BACKUP_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 4;

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 errorCode;
  Uint32 nodeId;
};

class StopBackupConf {
  /**
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printSTOP_BACKUP_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 4;
  
private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 noOfLogBytes;
  Uint32 noOfLogRecords;
};

class BackupStatusReq {
  /**
   * Sender(s)
   */
  friend class BackupMaster;
  
  /**
   * Reciver(s)
   */
  friend class Backup;

  friend bool printBACKUP_STATUS_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 1;

private:
};

class BackupStatusConf {
  /**
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printBACKUP_STATUS_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 1;

private:
};



#undef JAM_FILE_ID

#endif
