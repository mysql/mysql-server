/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
  static constexpr Uint32 SignalLength = 1;
};



#undef JAM_FILE_ID

#endif

