/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef BACKUP_FORMAT_HPP
#define BACKUP_FORMAT_HPP

#include <ndb_types.h>

#define JAM_FILE_ID 473


static const char BACKUP_MAGIC[] = { 'N', 'D', 'B', 'B', 'C', 'K', 'U', 'P' };

struct BackupFormat {

  static const Uint32 NDB_MAX_LCP_PARTS = 2048;
  static const Uint32 NDB_MAX_FILES_PER_LCP = 8;
  static const Uint32 NDB_MAX_LCP_PARTS_PER_ROUND =
    NDB_MAX_LCP_PARTS / NDB_MAX_FILES_PER_LCP;
  static const Uint32 NDB_MAX_LCP_FILES = 2064;
  static const Uint32 NDB_LCP_CTL_FILE_SIZE_SMALL = 4096;
  static const Uint32 NDB_LCP_CTL_FILE_SIZE_BIG = 8192;

  enum RecordType
  {
    INSERT_TYPE            = 0,
    WRITE_TYPE             = 1,
    DELETE_BY_ROWID_TYPE   = 2,
    DELETE_BY_PAGEID_TYPE  = 3,
    DELETE_BY_ROWID_WRITE_TYPE = 4,
    NORMAL_DELETE_TYPE     = 5,
    END_TYPE               = 6
  };

  /**
   * Section types in file
   */
  enum SectionType {
    FILE_HEADER       = 1,
    FRAGMENT_HEADER   = 2,
    FRAGMENT_FOOTER   = 3,
    TABLE_LIST        = 4,
    TABLE_DESCRIPTION = 5,
    GCP_ENTRY         = 6,
    FRAGMENT_INFO     = 7,
    EMPTY_ENTRY       = 8
  };

  struct FileHeader {
    char Magic[8];
    Uint32 BackupVersion;

    Uint32 SectionType;
    Uint32 SectionLength;
    Uint32 FileType;
    Uint32 BackupId;
    Uint32 BackupKey_0;
    Uint32 BackupKey_1;
    Uint32 ByteOrder;
    Uint32 NdbVersion;
    Uint32 MySQLVersion;
  };

  struct FileHeader_pre_backup_version {
    char Magic[8];
    Uint32 NdbVersion;

    Uint32 SectionType;
    Uint32 SectionLength;
    Uint32 FileType;
    Uint32 BackupId;
    Uint32 BackupKey_0;
    Uint32 BackupKey_1;
    Uint32 ByteOrder;
  };

  /**
   * File types
   */
  enum FileType {
    CTL_FILE = 1,
    LOG_FILE = 2, //redo log file for backup.
    DATA_FILE = 3,
    LCP_FILE = 4,
    UNDO_FILE = 5,//undo log for backup.
    LCP_CTL_FILE = 6
  };

  struct PartPair
  {
    Uint16 startPart;
    Uint16 numParts;
  };

  struct OldLCPCtlFile
  {
    struct FileHeader fileHeader;
    Uint32 Checksum;
    Uint32 ValidFlag;
    Uint32 TableId;
    Uint32 FragmentId;
    Uint32 CreateTableVersion;
    Uint32 CreateGci;
    Uint32 MaxGciCompleted;
    Uint32 MaxGciWritten;
    Uint32 LcpId;
    Uint32 LocalLcpId;
    Uint32 MaxPageCount;
    Uint32 MaxNumberDataFiles;
    Uint32 LastDataFileNumber;
    Uint32 MaxPartPairs;
    Uint32 NumPartPairs;
    /**
     * Flexible sized array of partPairs, there are
     * NumPartPairs in the array here.
     */
    struct PartPair partPairs[1];
  };
  struct LCPCtlFile
  {
    struct FileHeader fileHeader;
    Uint32 Checksum;
    Uint32 ValidFlag;
    Uint32 TableId;
    Uint32 FragmentId;
    Uint32 CreateTableVersion;
    Uint32 CreateGci;
    Uint32 MaxGciCompleted;
    Uint32 MaxGciWritten;
    Uint32 LcpId;
    Uint32 LocalLcpId;
    Uint32 MaxPageCount;
    Uint32 MaxNumberDataFiles;
    Uint32 LastDataFileNumber;
    Uint32 MaxPartPairs;
    Uint32 NumPartPairs;
    Uint32 RowCountLow;
    Uint32 RowCountHigh;
    Uint32 FutureUse[16];
    /**
     * Flexible sized array of partPairs, there are
     * NumPartPairs in the array here.
     */
    struct PartPair partPairs[1];
  };
  /**
   * Data file formats
   */
  struct DataFile {

    struct FragmentHeader {
      Uint32 SectionType;
      Uint32 SectionLength;
      Uint32 TableId;
      Uint32 FragmentNo;
      Uint32 ChecksumType;
    };
    
    struct VariableData {
      Uint32 Sz;
      Uint32 Id;
      Uint32 Data[1];
    };
    
    struct Record {
      Uint32 Length;
      Uint32 NullBitmask[1];
      Uint32 DataFixedKeys[1];
      Uint32 DataFixedAttributes[1];
      VariableData DataVariableAttributes[1];
    };
    
    struct FragmentFooter {
      Uint32 SectionType;
      Uint32 SectionLength;
      Uint32 TableId;
      Uint32 FragmentNo;
      Uint32 NoOfRecords;
      Uint32 Checksum;
    };

    /* optional padding for O_DIRECT */
    struct EmptyEntry {
      Uint32 SectionType;
      Uint32 SectionLength;
      /* not used data */
    };
  };

  /**
   * CTL file formats
   */
  struct CtlFile {
    
    /**
     * Table list
     */
    struct TableList {
      Uint32 SectionType;
      Uint32 SectionLength;
      Uint32 TableIds[1];      // Length = SectionLength - 2
    };

    /**
     * Table description(s)
     */
    struct TableDescription {
      Uint32 SectionType;
      Uint32 SectionLength;
      Uint32 TableType;
      Uint32 DictTabInfo[1];   // Length = SectionLength - 3
    };

    /**
     * GCP Entry
     */
    struct GCPEntry {
      Uint32 SectionType;
      Uint32 SectionLength;
      Uint32 StartGCP;
      Uint32 StopGCP;
    };

    /**
     * Fragment Info
     */
    struct FragmentInfo {
      Uint32 SectionType;
      Uint32 SectionLength;
      Uint32 TableId;
      Uint32 FragmentNo;
      Uint32 NoOfRecordsLow;
      Uint32 NoOfRecordsHigh;
      Uint32 FilePosLow;
      Uint32 FilePosHigh;
    };
  };

  /**
   * LOG file format
   */
  struct LogFile {

    /**
     * Log Entry
     */
    struct LogEntry {
      Uint32 Length;
      Uint32 TableId;
      // If TriggerEvent & 0x10000 == true then GCI is right after data
      Uint32 TriggerEvent;
      Uint32 FragId;
      Uint32 Data[1]; // Len = Length - 3
    };

    /**
     * Log Entry pre NDBD_FRAGID_VERSION
     */
    struct LogEntry_no_fragid {
      Uint32 Length;
      Uint32 TableId;
      // If TriggerEvent & 0x10000 == true then GCI is right after data
      Uint32 TriggerEvent;
      Uint32 Data[1]; // Len = Length - 2
    };
  };

  /**
   * LCP file format
   */
  struct LcpFile {
    CtlFile::TableList TableList;
    CtlFile::TableDescription TableDescription;
    DataFile::FragmentHeader FragmentHeader;
    DataFile::Record Record;
    DataFile::FragmentFooter FragmentFooter;
  };
};


#undef JAM_FILE_ID

#endif
