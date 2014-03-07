/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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

  STATIC_CONST( SignalLength = 7 );

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

  STATIC_CONST( SignalLength = 7 );

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

  STATIC_CONST( SignalLength = 5 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 transId;
  Uint32 fkId;
  Uint32 fkVersion;
};


#undef JAM_FILE_ID

#endif
