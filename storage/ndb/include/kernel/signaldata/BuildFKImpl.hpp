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


#ifndef BUILD_FK_IMPL_HPP
#define BUILD_FK_IMPL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 207


struct BuildFKImplReq
{
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printBUILD_FK_IMPL_REQ(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 8 );
  STATIC_CONST( PARENT_COLUMNS_SEC = 0 );
  STATIC_CONST( CHILD_COLUMNS_SEC = 1 );

  enum {
    RT_PARSE    = 0x1,
    RT_PREPARE  = 0x2,
    RT_ABORT    = 0x3,
    RT_COMMIT   = 0x4,
    RT_COMPLETE = 0x5
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 transId;
  Uint32 requestType;
  Uint32 fkId;
  Uint32 fkVersion;
  Uint32 parentTableId; // could be unique index...
  Uint32 childTableId;
};

struct BuildFKImplRef
{
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printBUILD_FK_IMPL_REF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 3 );

  enum ErrorCode {
    NoError = 0
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
};

struct BuildFKImplConf
{
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printBUILD_FK_IMPL_CONF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 4 );

  Uint32 senderData;
  Uint32 senderRef;
};


#undef JAM_FILE_ID

#endif
