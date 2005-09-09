/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef BACKUP_IMPL_HPP
#define BACKUP_IMPL_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

class DefineBackupReq {
  /**
   * Sender(s)
   */
  friend class BackupMaster;
  
  /**
   * Reciver(s)
   */
  friend class Backup;

  friend bool printDEFINE_BACKUP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 9 + NdbNodeBitmask::Size);

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
   */
  NdbNodeBitmask nodes;
  
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
   */
  Uint32 flags;
};

class DefineBackupRef {
  /**
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printDEFINE_BACKUP_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
  enum ErrorCode {
    Undefined = 1340,
    FailedToAllocateBuffers = 1342,
    FailedToSetupFsBuffers = 1343,
    FailedToAllocateTables = 1344,
    FailedInsertFileHeader = 1345,
    FailedInsertTableList = 1346,
    FailedAllocateTableMem = 1347,
    FailedToAllocateFileRecord = 1348,
    FailedToAllocateAttributeRecord = 1349
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
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printDEFINE_BACKUP_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );
  
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

  friend bool printSTART_BACKUP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:

  STATIC_CONST( MaxTableTriggers = 4 );
  STATIC_CONST( HeaderLength = 5 );
  STATIC_CONST( TableTriggerLength = 4);
  
private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 signalNo;
  Uint32 noOfSignals;
  Uint32 noOfTableTriggers;

  struct TableTriggers {
    Uint32 tableId;
    Uint32 triggerIds[3];
  } tableTriggers[MaxTableTriggers];
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

  friend bool printSTART_BACKUP_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );

  enum ErrorCode {
    FailedToAllocateTriggerRecord = 1
  };
private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 signalNo;
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
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 signalNo;
};

class BackupFragmentReq {
  /**
   * Sender(s)
   */
  friend class BackupMaster;
  
  /**
   * Reciver(s)
   */
  friend class Backup;

  friend bool printBACKUP_FRAGMENT_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 tableId;
  Uint32 fragmentNo;
  Uint32 count;
};

class BackupFragmentRef {
  /**
   * Sender(s)
   */
  friend class Backup;
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printBACKUP_FRAGMENT_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );

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
  
  /**
   * Reciver(s)
   */
  friend class BackupMaster;

  friend bool printBACKUP_FRAGMENT_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 6 );

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 tableId;
  Uint32 fragmentNo;
  Uint32 noOfRecords;
  Uint32 noOfBytes;
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

  friend bool printSTOP_BACKUP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

private:
  Uint32 backupId;
  Uint32 backupPtr;
  Uint32 startGCP;
  Uint32 stopGCP;
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

  friend bool printSTOP_BACKUP_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

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
  STATIC_CONST( SignalLength = 4 );
  
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
  STATIC_CONST( SignalLength = 1 );

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
  STATIC_CONST( SignalLength = 1 );

private:
};


#endif
