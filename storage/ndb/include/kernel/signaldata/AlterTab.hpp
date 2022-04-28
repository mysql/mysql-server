/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef ALTER_TAB_HPP
#define ALTER_TAB_HPP

#include "SignalData.hpp"
#include "GlobalSignalNumbers.h"

#define JAM_FILE_ID 72


struct AlterTabReq {
  static constexpr Uint32 SignalLength = 12;

  enum RequestType {
    AlterTablePrepare = 0, // Prepare alter table
    AlterTableCommit = 1,  // Commit alter table
    AlterTableRevert = 2,  // Prepare failed, revert instead
    AlterTableComplete = 3,
    AlterTableWaitScan = 4,
    AlterTableSumaEnable = 5,
    AlterTableSumaFilter = 6
    ,AlterTableReadOnly = 7  // From TUP to LQH before mtoib
    ,AlterTableReadWrite = 8 // From TUP to LQH after mtoib
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestType;
  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 newTableVersion;
  Uint32 gci;
  Uint32 changeMask;

  /* Only used when sending to TUP. */
  Uint32 connectPtr;
  Uint32 noOfNewAttr;
  Uint32 newNoOfCharsets;
  union {
    Uint32 newNoOfKeyAttrs;
    Uint32 new_map_ptr_i;
  };

  SECTION( DICT_TAB_INFO = 0 );
  SECTION( FRAGMENTATION = 1 );
  /*
    When sent to DICT, the first section contains the new table definition.
    When sent to TUP, the first section contains the new attributes.
  */
};

struct AlterTabConf {
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderRef;
  Uint32 senderData;

  /* Only used when sent from TUP. */
  Uint32 connectPtr;
};

struct AlterTabRef {
  static constexpr Uint32 SignalLength = 7;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;
  Uint32 errorStatus;
  Uint32 connectPtr;
};

/*
  This union can be used to safely refer to a signal data part
  simultaneously as AlterTab{Req,Ref,Conf} without violating the
  strict aliasing rule.
*/
union AlterTabAll {
  AlterTabReq req;
  AlterTabRef ref;
  AlterTabConf conf;
};


#undef JAM_FILE_ID

#endif
