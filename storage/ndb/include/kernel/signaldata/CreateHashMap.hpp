/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CREATE_HASHMAP_HPP
#define CREATE_HASHMAP_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 64


struct CreateHashMapReq
{

  static constexpr Uint32 SignalLength = 7;

  enum RequestType
  {
    CreateIfNotExists = 1,
    CreateDefault     = 2,
    CreateForReorg    = 4,
    CreateForOneNodegroup = 8,
  };

  Uint32 clientRef;
  Uint32 clientData;
  Uint32 transKey;
  Uint32 transId;
  Uint32 requestInfo;
  Uint32 buckets;
  Uint32 fragments;

  SECTION( INFO = 0 );
};

struct CreateHashMapConf
{

  static constexpr Uint32 SignalLength = 5;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 objectId;
  Uint32 objectVersion;
  Uint32 transId;
};

struct CreateHashMapRef
{
  static constexpr Uint32 SignalLength = 9;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 transId;
  Uint32 masterNodeId;
  Uint32 errorNodeId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorKey;
  Uint32 errorStatus;
};


#undef JAM_FILE_ID

#endif
