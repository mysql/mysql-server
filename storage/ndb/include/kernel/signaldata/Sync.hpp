/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SYNC_HPP
#define SYNC_HPP

struct SyncReq
{
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 prio;

  STATIC_CONST( SignalLength = 3 );
};

struct SyncRef
{
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;

  STATIC_CONST( SignalLength = 3 );

  enum ErrorCode
  {
    SR_OK = 0,
    SR_OUT_OF_MEMORY = 1
  };
};

struct SyncConf
{
  Uint32 senderRef;
  Uint32 senderData;

  STATIC_CONST( SignalLength = 3 );
};

struct SyncPathReq
{
  Uint32 senderData;
  Uint32 prio;
  Uint32 count;
  Uint32 pathlen;
  Uint32 path[1];

  STATIC_CONST( SignalLength = 4 );
};

struct SyncPathConf
{
  Uint32 senderData;
  Uint32 count;

  STATIC_CONST( SignalLength = 2 );
};

#endif
