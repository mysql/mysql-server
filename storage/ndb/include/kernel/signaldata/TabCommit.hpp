/* Copyright (C) 2008 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef TAB_COMMIT_HPP
#define TAB_COMMIT_HPP

struct TabCommitReq {
  enum { SignalLength = 3 };
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
};

struct TabCommitConf {
  enum { SignalLength = 3 };
  Uint32 senderData;
  Uint32 nodeId;
  Uint32 tableId;
};

struct TabCommitRef {
  enum { SignalLength = 5 };
  Uint32 senderData;
  Uint32 nodeId;
  Uint32 tableId;
  Uint32 errorCode;
  Uint32 tableStatus;
};

#endif
