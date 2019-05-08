/*
   Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.

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


#ifndef CREATE_FK_HPP
#define CREATE_FK_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 173


struct CreateFKReq
{
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printCREATE_FK_REQ(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 10 );

  union {
    Uint32 senderData;
    Uint32 clientData;
  };
  union {
    Uint32 senderRef;
    Uint32 clientRef;
  };
  Uint32 requestInfo;
  Uint32 transId;
  Uint32 transKey;
};

struct CreateFKRef
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
  friend bool printCREATE_FK_REF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 7 );

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    NoMoreObjectRecords = 21020,
    InvalidFormat = 21021,
    ParentTableIsNotATable = 21022,
    InvalidParentTableVersion = 21023,
    ChildTableIsNotATable = 21024,
    InvalidChildTableVersion = 21025,
    ParentIndexIsNotAnUniqueIndex = 21026,
    InvalidParentIndexVersion = 21027,
    ChildIndexIsNotAnIndex = 21028,
    InvalidChildIndexVersion = 21029,
    NoMoreTableRecords = 707,
    ObjectAlreadyExist = 721,
    OutOfStringBuffer = 773,
    ChildTableHasBlobsAndCascadeDelete = 21034
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 masterNodeId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 transId;
};

struct CreateFKConf
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
  friend bool printCREATE_FK_CONF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 5 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 transId;
  Uint32 fkId;
  Uint32 fkVersion;
};


#undef JAM_FILE_ID

#endif
