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

#ifndef ALTER_TAB_HPP
#define ALTER_TAB_HPP

#include "SignalData.hpp"
#include "GlobalSignalNumbers.h"

struct AlterTabReq {
  STATIC_CONST( SignalLength = 12 );

  enum RequestType {
    AlterTablePrepare = 0, // Prepare alter table
    AlterTableCommit = 1,  // Commit alter table
    AlterTableRevert = 2   // Prepare failed, revert instead
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
  Uint32 newNoOfKeyAttrs;

  SECTION( DICT_TAB_INFO = 0 );
  /*
    When sent to DICT, the first section contains the new table definition.
    When sent to TUP, the first section contains the new attributes.
  */
};

struct AlterTabConf {
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderRef;
  Uint32 senderData;

  /* Only used when sent from TUP. */
  Uint32 connectPtr;
};

struct AlterTabRef {
  STATIC_CONST( SignalLength = 6 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;
  Uint32 errorStatus;
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

#endif
