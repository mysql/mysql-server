/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <ndb_limits.h>
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
  static const Uint32 BYTES_PER_PART_ON_DISK = 3;
  static constexpr Uint32 MAX_BACKUP_FILE_LOG_DATA_SIZE =
    MAX_ATTRIBUTES_IN_INDEX + MAX_KEY_SIZE_IN_WORDS +
    MAX_ATTRIBUTES_IN_TABLE + MAX_TUPLE_SIZE_IN_WORDS;

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
   * The convert_ctl_page_to_host is used by DBTUP and RESTORE as
   * well, these blocks need to have a buffer with size
   * LCP_CTL_FILE_DATA_SIZE to handle the conversion, this buffer
   * is a bit bigger than the file size since we decompress the
   * area.
   */
  static const Uint32 LCP_CTL_FILE_SIZE_ON_DISK =
                   (BYTES_PER_PART_ON_DISK * NDB_MAX_LCP_PARTS) +
                   sizeof(BackupFormat::LCPCtlFile);
  static const Uint32 LCP_CTL_SIZE_IN_MEMORY =
    (sizeof(struct PartPair) * NDB_MAX_LCP_PARTS) +
      sizeof(BackupFormat::LCPCtlFile);
  static const Uint32 LCP_CTL_FILE_BUFFER_SIZE_IN_WORDS =
    (MAX(NDB_LCP_CTL_FILE_SIZE_BIG,
        MAX(LCP_CTL_FILE_SIZE_ON_DISK, LCP_CTL_SIZE_IN_MEMORY))) / 4;

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
   * LOG file format (since 5.1.6 but not drop6 (5.2.x))
   */
  struct LogFile {

    /**
     * Log Entry
     */
    struct LogEntry {
      // Header length excluding leading Length word.
      static constexpr Uint32 HEADER_LENGTH_WORDS = 3;
      static constexpr Uint32 FRAGID_OFFSET = 3;
      // Add one word for leading Length word for data offset
      static constexpr Uint32 DATA_OFFSET = 1 + HEADER_LENGTH_WORDS;
      static constexpr Uint32 MAX_SIZE = 1 /* length word */ +
                                         HEADER_LENGTH_WORDS +
                                         MAX_BACKUP_FILE_LOG_DATA_SIZE +
                                         1 /* gci */ +
                                         1 /* trailing length word for undo */;

      Uint32 Length;
      Uint32 TableId;
      // If TriggerEvent & 0x10000 == true then GCI is right after data
      Uint32 TriggerEvent;
      Uint32 FragId;
      Uint32 Data[1]; // Len = Length - 3
    };
    static_assert(offsetof(LogEntry, FragId) ==
                    LogEntry::FRAGID_OFFSET * sizeof(Uint32),
                  "");
    static_assert(offsetof(LogEntry, Data) ==
                    LogEntry::DATA_OFFSET * sizeof(Uint32),
                  "");

    /**
     * Log Entry pre NDBD_FRAGID_VERSION (<5.1.6) and drop6 (5.2.x)
     */
    struct LogEntry_no_fragid {
      // Header length excluding leading Length word.
      static constexpr Uint32 HEADER_LENGTH_WORDS = 2;
      // Add one word for leadng Length word for data offset
      static constexpr Uint32 DATA_OFFSET = 1 + HEADER_LENGTH_WORDS;

      Uint32 Length;
      Uint32 TableId;
      // If TriggerEvent & 0x10000 == true then GCI is right after data
      Uint32 TriggerEvent;
      Uint32 Data[1]; // Len = Length - 2
    };
    static_assert(offsetof(LogEntry_no_fragid, Data) ==
                    LogEntry_no_fragid::DATA_OFFSET * sizeof(Uint32),
                  "");
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
