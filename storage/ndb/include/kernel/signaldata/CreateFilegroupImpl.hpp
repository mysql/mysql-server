/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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

#ifndef CREATE_FILEGROUP_IMPL_HPP
#define CREATE_FILEGROUP_IMPL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 53


struct CreateFilegroupImplReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;

  /**
   * For printing
   */
  friend bool printCREATE_FILEGROUP_IMPL_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 5; // DICT2DICT
  static constexpr Uint32 TablespaceLength = 7;
  static constexpr Uint32 LogfileGroupLength = 6;
  
  Uint32 senderData;
  Uint32 senderRef;  
  Uint32 filegroup_id;
  Uint32 filegroup_version;
  Uint32 requestType;
  
  union {
    struct {
      Uint32 extent_size;
      Uint32 logfile_group_id;
    } tablespace;
    struct {
      Uint32 buffer_size; // In pages
    } logfile_group;
  };
};

struct CreateFilegroupImplRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  /**
   * For printing
   */
  friend bool printCREATE_FILEGROUP_IMPL_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 3;

  enum ErrorCode {
    NoError = 0,
    FilegroupAlreadyExists = 1502,
    OutOfFilegroupRecords = 1503,
    OutOfLogBufferMemory = 1504,
    OneLogfileGroupLimit = 1514
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
};

struct CreateFilegroupImplConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  /**
   * For printing
   */
  friend bool printCREATE_FILEGROUP_IMPL_CONF(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 2;

  Uint32 senderData;
  Uint32 senderRef;
};

struct CreateFileImplReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;

  /**
   * For printing
   */
  friend bool printCREATE_FILE_IMPL_REQ(FILE*, const Uint32*, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 11; // DICT2DICT
  static constexpr Uint32 DatafileLength = 10;
  static constexpr Uint32 UndofileLength = 9;
  static constexpr Uint32 CommitLength = 7;
  static constexpr Uint32 AbortLength = 7;
  SECTION( FILENAME = 0 );
  
  enum RequestInfo {
    Create = 0x1,
    CreateForce = 0x2,
    Open = 0x4,
    Commit = 0x8,
    Abort = 0x10
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestInfo;
  Uint32 file_id;
  Uint32 file_version;
  Uint32 filegroup_id;
  Uint32 filegroup_version;
  Uint32 file_size_hi;
  Uint32 file_size_lo;

  union {
    struct {
      Uint32 extent_size;
    } tablespace;
  };
  Uint32 requestType;
};

struct CreateFileImplRef {
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  /**
   * For printing
   */
  friend bool printCREATE_FILE_IMPL_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 5;

  enum ErrorCode {
    NoError = 0,
    InvalidFilegroup = 1505,
    InvalidFilegroupVersion = 1506,
    FileNoAlreadyExists = 1507,
    OutOfFileRecords = 1508,
    FileError = 1509,
    InvalidFileMetadata = 1510,
    OutOfMemory = 1511,
    FileReadError = 1512,
    FilegroupNotOnline = 1513,
    FileSizeTooLarge = 1515,
    FileSizeTooSmall = 1516,
    OutOfDiskPageBufferMemory = 1517
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 fsErrCode;
  Uint32 osErrCode;
};

struct CreateFileImplConf {
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  
  /**
   * For printing
   */
  friend bool printCREATE_FILE_IMPL_CONF(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 4;

  Uint32 senderData;
  Uint32 senderRef;
};


#undef JAM_FILE_ID

#endif
