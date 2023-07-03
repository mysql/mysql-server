/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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


#ifndef BUILD_FK_HPP
#define BUILD_FK_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 70


struct BuildFKReq
{
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printBUILD_FK_REQ(FILE*, const Uint32*, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 7;

  union {
    Uint32 senderData;
    Uint32 clientData;
  };
  union {
    Uint32 senderRef;
    Uint32 clientRef;
  };
  Uint32 requestInfo;
  Uint32 fkId;
  Uint32 fkVersion;
  Uint32 transId;
  Uint32 transKey;
};

struct BuildFKRef
{
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printBUILD_FK_REF(FILE*, const Uint32*, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 7;

  enum ErrorCode {
    NoError = 0,
    FKNotFound = 21060,
    InvalidFKVersion = 21061
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 masterNodeId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 transId;
};

struct BuildFKConf
{
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
  friend bool printBUILD_FK_CONF(FILE*, const Uint32*, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 5;

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 transId;
  Uint32 fkId;
  Uint32 fkVersion;
};


#undef JAM_FILE_ID

#endif
