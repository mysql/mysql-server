/*
   Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DIH_RESTART_HPP
#define DIH_RESTART_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 84


struct DihRestartReq
{
  STATIC_CONST( SignalLength = 1 );
  Uint32 senderRef;

  /**
   * Qmgr checks if it can continue...using EXECUTE_DIRECT
   *   and fields below, setting senderRef == 0
   */
  STATIC_CONST( CheckLength = 1 + NdbNodeBitmask::Size + MAX_NDB_NODES);
  Uint32 nodemask[NdbNodeBitmask::Size];
  Uint32 node_gcis[MAX_NDB_NODES];
};

struct DihRestartRef
{
  STATIC_CONST( SignalLength = NdbNodeBitmask::Size );
  Uint32 no_nodegroup_mask[NdbNodeBitmask::Size];
};

struct DihRestartConf
{
  STATIC_CONST( SignalLength = 2 + NdbNodeBitmask::Size );
  Uint32 unused;
  Uint32 latest_gci;
  Uint32 no_nodegroup_mask[NdbNodeBitmask::Size];
};


#undef JAM_FILE_ID

#endif
