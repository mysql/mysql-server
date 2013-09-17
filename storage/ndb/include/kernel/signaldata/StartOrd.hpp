/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef START_ORD_HPP
#define START_ORD_HPP

#include "SignalData.hpp"
#include "StopReq.hpp"

#define JAM_FILE_ID 184


class StartOrd {
public:
  /**
   * Senders
   */
  friend class ThreadConfig;
  friend class MgmtSrvr;
  friend class Ndbcntr;

  /**
   * Receivers
   */
  friend class SimBlockCMCtrBlck;

  /**
   * RequestInfo - See StopReq for getters/setters
   */
  Uint32 restartInfo;
  
public:
  STATIC_CONST( SignalLength = 1 );
};



#undef JAM_FILE_ID

#endif

