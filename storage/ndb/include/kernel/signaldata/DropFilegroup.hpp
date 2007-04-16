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

#ifndef DROP_FILEGROUP_HPP
#define DROP_FILEGROUP_HPP

#include "SignalData.hpp"

struct DropFilegroupReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  friend class Dbdict;
  friend class Tsman;

  /**
   * For printing
   */
  friend bool printDROP_FILEGROUP_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 4 );
  STATIC_CONST( GSN = GSN_DROP_FILEGROUP_REQ );
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 filegroup_id;
  Uint32 filegroup_version;
};

struct DropFilegroupRef {
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
  friend bool printDROP_FILEGROUP_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 7 );
  STATIC_CONST( GSN = GSN_DROP_FILEGROUP_REF );

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    NoSuchFilegroup = 767,
    FilegroupInUse = 768,
    InvalidSchemaObjectVersion = 774,
    SingleUser = 299
  };
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 masterNodeId;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;

};

struct DropFilegroupConf {
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
  friend bool printDROP_FILEGROUP_CONF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 4 );
  STATIC_CONST( GSN = GSN_DROP_FILEGROUP_CONF );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 filegroupId;
  Uint32 filegroupVersion;
};

struct DropFileReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  friend class Dbdict;
  friend class Tsman;

  /**
   * For printing
   */
  friend bool printDROP_FILE_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 4 );
  STATIC_CONST( GSN = GSN_DROP_FILE_REQ );
  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 file_id;
  Uint32 file_version;
};

struct DropFileRef {
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
  friend bool printDROP_FILE_REF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 7 );
  STATIC_CONST( GSN = GSN_DROP_FILE_REF );

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    NoSuchFile = 766,
    DropUndoFileNotSupported = 769,
    InvalidSchemaObjectVersion = 774,
    SingleUser = 299
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 masterNodeId;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;

};

struct DropFileConf {
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
  friend bool printDROP_FILE_CONF(FILE*, const Uint32*, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 4 );
  STATIC_CONST( GSN = GSN_DROP_FILE_CONF );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 fileId;
  Uint32 fileVersion;
};

#endif
