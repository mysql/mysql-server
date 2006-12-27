/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DROP_FILEGROUP_IMPL_HPP
#define DROP_FILEGROUP_IMPL_HPP

#include "SignalData.hpp"

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
  
  STATIC_CONST( SignalLength = 5 );
  
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
  
  STATIC_CONST( SignalLength = 3 );

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
  
  STATIC_CONST( SignalLength = 2 );

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
  
  STATIC_CONST( SignalLength = 6 );
  
  enum RequestInfo {
    Prepare = 0x1,
    Commit = 0x2,
    Abort = 0x4
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  
  Uint32 requestInfo;
  Uint32 file_id;
  Uint32 filegroup_id;
  Uint32 filegroup_version;
};

struct DropFileImplRef {
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;
  
  /**
   * For printing
   */
  friend bool printDROP_FILE_IMPL_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 5 );

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
  
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderData;
  Uint32 senderRef;
};

#endif
