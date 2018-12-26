/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef COPY_TAB_HPP
#define COPY_TAB_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 493


class CopyTabReq {
  friend class Dbdih;
public:
  STATIC_CONST( SignalLengthExtra = 23 );
  STATIC_CONST( SignalLength = 21 );

  enum TabLcpStatus
  {
    LcpCompleted = 0,
    LcpActive = 1
  };

private:
  Uint32 senderRef;
  Uint32 reqinfo;
  Uint32 tableId;
  Uint32 tableSchemaVersion;
  Uint32 noOfWords;
  Uint32 tableWords[16];
  Uint32 tabLcpStatus;
  Uint32 currentLcpId;
};

class CopyTabConf {
  friend class Dbdih;
public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 nodeId;
  Uint32 tableId;
};

#undef JAM_FILE_ID

#endif
