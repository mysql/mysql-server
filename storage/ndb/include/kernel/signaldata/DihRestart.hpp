/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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
  STATIC_CONST( SignalLength = 3 + NdbNodeBitmask::Size );
  Uint32 unused;
  Uint32 latest_gci;
  Uint32 latest_lcp_id;
  Uint32 no_nodegroup_mask[NdbNodeBitmask::Size];
};


#undef JAM_FILE_ID

#endif
