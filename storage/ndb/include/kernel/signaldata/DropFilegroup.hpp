/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

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

#ifndef DROP_FILEGROUP_HPP
#define DROP_FILEGROUP_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 135

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
  friend bool printDROP_FILEGROUP_REQ(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 7;
  static constexpr Uint32 GSN = GSN_DROP_FILEGROUP_REQ;

  union {
    Uint32 senderData;
    Uint32 clientData;
  };
  union {
    Uint32 senderRef;
    Uint32 clientRef;
  };
  Uint32 filegroup_id;
  Uint32 filegroup_version;
  Uint32 requestInfo;
  Uint32 transKey;
  Uint32 transId;
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
  friend bool printDROP_FILEGROUP_REF(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 9;
  static constexpr Uint32 GSN = GSN_DROP_FILEGROUP_REF;

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
  Uint32 errorNodeId;
  Uint32 transId;
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
  friend bool printDROP_FILEGROUP_CONF(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 5;
  static constexpr Uint32 GSN = GSN_DROP_FILEGROUP_CONF;

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 filegroupId;
  Uint32 filegroupVersion;
  Uint32 transId;
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
  friend bool printDROP_FILE_REQ(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 7;
  static constexpr Uint32 GSN = GSN_DROP_FILE_REQ;

  union {
    Uint32 senderData;
    Uint32 clientData;
  };
  union {
    Uint32 senderRef;
    Uint32 clientRef;
  };
  Uint32 file_id;
  Uint32 file_version;
  Uint32 requestInfo;
  Uint32 transKey;
  Uint32 transId;
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
  friend bool printDROP_FILE_REF(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 9;
  static constexpr Uint32 GSN = GSN_DROP_FILE_REF;

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
  Uint32 errorNodeId;
  Uint32 transId;
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
  friend bool printDROP_FILE_CONF(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 5;
  static constexpr Uint32 GSN = GSN_DROP_FILE_CONF;

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 fileId;
  Uint32 fileVersion;
  Uint32 transId;
};

#undef JAM_FILE_ID

#endif
