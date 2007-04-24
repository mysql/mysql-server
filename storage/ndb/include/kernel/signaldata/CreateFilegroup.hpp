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

#ifndef CREATE_FILEGROUP_HPP
#define CREATE_FILEGROUP_HPP

#include "SignalData.hpp"

struct CreateFilegroupReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printCREATE_FILEGROUP_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 objType;
  SECTION( FILEGROUP_INFO = 0 );
};

struct CreateFilegroupRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  
  /**
   * For printing
   */
  friend bool printCREATE_FILEGROUP_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 7 );

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    NoMoreObjectRecords = 710,
    InvalidFormat = 740,
    OutOfFilegroupRecords = 765,
    InvalidExtentSize = 764,
    InvalidUndoBufferSize = 779,
    NoSuchLogfileGroup = 767,
    InvalidFilegroupVersion = 768,
    SingleUser = 299
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 masterNodeId;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;
  Uint32 status;
};

struct CreateFilegroupConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  
  /**
   * For printing
   */
  friend bool printCREATE_FILEGROUP_CONF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 4 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 filegroupId;
  Uint32 filegroupVersion;
};

struct CreateFileReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  friend class Dbdict;
  friend class Tsman;

  /**
   * For printing
   */
  friend bool printCREATE_FILE_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 objType;
  Uint32 requestInfo;
  
  enum RequstInfo 
  {
    ForceCreateFile = 0x1
  };
  
  SECTION( FILE_INFO = 0 );
};

struct CreateFileRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  
  /**
   * For printing
   */
  friend bool printCREATE_FILE_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 7 );

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    NoMoreObjectRecords = 710,
    InvalidFormat = 752,
    NoSuchFilegroup = 753,
    InvalidFilegroupVersion = 754,
    FilenameAlreadyExists = 760,
    OutOfFileRecords = 751,
    InvalidFileType = 750,
    NotSupportedWhenDiskless = 775,
    SingleUser = 299
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 masterNodeId;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;
  Uint32 status;
};

struct CreateFileConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  friend class NdbDictInterface;
  
  /**
   * For printing
   */
  friend bool printCREATE_FILE_CONF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 4 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 fileId;
  Uint32 fileVersion;
};

#endif
