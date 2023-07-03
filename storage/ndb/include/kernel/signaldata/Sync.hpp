/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

#ifndef SYNC_HPP
#define SYNC_HPP

#define JAM_FILE_ID 31


struct SyncReq
{
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 prio;

  static constexpr Uint32 SignalLength = 3;
};

struct SyncRef
{
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;

  static constexpr Uint32 SignalLength = 3;

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

  static constexpr Uint32 SignalLength = 3;
};

struct SyncPathReq
{
  Uint32 senderData;
  Uint32 prio;
  Uint32 count;
  Uint32 pathlen;
  Uint32 path[1];

  static constexpr Uint32 SignalLength = 4;
  static constexpr Uint32 MaxPathLen = 25 - SignalLength;
};

struct SyncPathConf
{
  Uint32 senderData;
  Uint32 count;

  static constexpr Uint32 SignalLength = 2;
};

struct FreezeThreadReq
{
  Uint32 nodeId;
  Uint32 senderRef;
  static constexpr Uint32 SignalLength = 2;
};

struct FreezeThreadConf
{
  Uint32 nodeId;
  static constexpr Uint32 SignalLength = 1;
};

struct FreezeActionReq
{
  Uint32 nodeId;
  Uint32 senderRef;
  static constexpr Uint32 SignalLength = 2;
};

struct FreezeActionConf
{
  Uint32 nodeId;
  static constexpr Uint32 SignalLength = 1;
};

struct ActivateTrpReq
{
  Uint32 nodeId;
  Uint32 trpId;
  Uint32 numTrps;
  Uint32 senderRef;
  static constexpr Uint32 SignalLength = 4;
};

struct ActivateTrpConf
{
  Uint32 nodeId;
  Uint32 trpId;
  Uint32 senderRef;
  static constexpr Uint32 SignalLength = 3;
};

struct AddEpollTrpReq
{
  Uint32 nodeId;
  Uint32 trpId;
  Uint32 senderRef;
  static constexpr Uint32 SignalLength = 3;
};

struct AddEpollTrpConf
{
  Uint32 nodeId;
  Uint32 trpId;
  Uint32 senderRef;
  static constexpr Uint32 SignalLength = 3;
};

struct SwitchMultiTrpReq
{
  Uint32 nodeId;
  Uint32 senderRef;
  static constexpr Uint32 SignalLength = 2;
};

struct SwitchMultiTrpConf
{
  Uint32 nodeId;
  static constexpr Uint32 SignalLength = 1;
};

struct SwitchMultiTrpRef
{
  Uint32 nodeId;
  Uint32 errorCode;
  static constexpr Uint32 SignalLength = 2;
  enum ErrorCode
  {
    SMTR_NOT_READY_FOR_SWITCH = 1
  };
};
#undef JAM_FILE_ID

#endif
