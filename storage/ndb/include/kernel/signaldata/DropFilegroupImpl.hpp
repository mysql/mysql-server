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

#ifndef DROP_FILEGROUP_IMPL_HPP
#define DROP_FILEGROUP_IMPL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 149


struct DropFilegroupImplReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;

  /**
   * For printing
   */
  friend bool printDROP_FILEGROUP_IMPL_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 6;
  
  enum RequestInfo {
    Prepare = 0x1,
    Commit = 0x2,
    Abort = 0x4
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  
  Uint32 requestInfo;
  Uint32 filegroup_id;
  Uint32 filegroup_version;
  Uint32 requestType;
};

struct DropFilegroupImplRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  /**
   * For printing
   */
  friend bool printDROP_FILEGROUP_IMPL_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 3;

  enum ErrorCode {
    NoError = 0,
    NoSuchFilegroup = 767,
    InvalidFilegroupVersion = 767,
    FilegroupInUse = 768
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
};

struct DropFilegroupImplConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  /**
   * For printing
   */
  friend bool printDROP_FILEGROUP_IMPL_CONF(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 2;

  Uint32 senderData;
  Uint32 senderRef;
};

struct DropFileImplReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;

  /**
   * For printing
   */
  friend bool printDROP_FILE_IMPL_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 8;
  
  enum RequestInfo {
    Prepare = 0x1,
    Commit = 0x2,
    Abort = 0x4
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  
  Uint32 requestInfo;
  Uint32 file_id;
  Uint32 file_version;
  Uint32 filegroup_id;
  Uint32 filegroup_version;
  Uint32 requestType;
};

struct DropFileImplRef {
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  /**
   * For printing
   */
  friend bool printDROP_FILE_IMPL_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 5;

  enum ErrorCode {
    NoError = 0,
    InvalidFilegroup = 767,
    InvalidFilegroupVersion = 767,
    NoSuchFile = 766,
    FileInUse = 770
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 fsErrCode;
  Uint32 osErrCode;
};

struct DropFileImplConf {
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  
  /**
   * For printing
   */
  friend bool printDROP_FILE_IMPL_CONF(FILE*, const Uint32*, Uint32, Uint16);
  
  static constexpr Uint32 SignalLength = 2;

  Uint32 senderData;
  Uint32 senderRef;
};


#undef JAM_FILE_ID

#endif
