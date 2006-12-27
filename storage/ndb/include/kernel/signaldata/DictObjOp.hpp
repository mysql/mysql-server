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

#ifndef DICT_OBJ_OP_HPP
#define DICT_OBJ_OP_HPP

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


#endif
