/*
   Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DICT_OBJ_OP_HPP
#define DICT_OBJ_OP_HPP

#define JAM_FILE_ID 143


struct DictObjOp {
  
  enum RequestType {
    Prepare = 0, // Prepare create obj
    Commit = 1,  // Commit create obj
    Abort = 2    // Prepare failed, drop instead
  };
  
  enum State {
    Defined = 0,
    Preparing = 1,
    Prepared = 2,
    Committing = 3,
    Committed = 4,
    Aborting = 5,
    Aborted = 6
  };
};

struct DictCommitReq
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 op_key;

  STATIC_CONST( SignalLength = 3 );
  STATIC_CONST( GSN = GSN_DICT_COMMIT_REQ );
};

struct DictCommitRef
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  enum ErrorCode 
  {
    NF_FakeErrorREF = 1
  };
  STATIC_CONST( SignalLength = 3 );
  STATIC_CONST( GSN = GSN_DICT_COMMIT_REF );
};

struct DictCommitConf
{
  Uint32 senderData;
  Uint32 senderRef;

  STATIC_CONST( SignalLength = 2 );
  STATIC_CONST( GSN = GSN_DICT_COMMIT_CONF );
};

struct DictAbortReq
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 op_key;

  STATIC_CONST( SignalLength = 3 );
  STATIC_CONST( GSN = GSN_DICT_ABORT_REQ );
};

struct DictAbortRef
{
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
  enum ErrorCode 
  {
    NF_FakeErrorREF = 1
  };
  STATIC_CONST( SignalLength = 3 );
  STATIC_CONST( GSN = GSN_DICT_ABORT_REF );
};

struct DictAbortConf
{
  Uint32 senderData;
  Uint32 senderRef;

  STATIC_CONST( SignalLength = 2 );
  STATIC_CONST( GSN = GSN_DICT_ABORT_CONF );
};



#undef JAM_FILE_ID

#endif
