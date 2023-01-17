/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef GET_NUM_MULTI_TRP_HPP
#define GET_NUM_MULTI_TRP_HPP

#define JAM_FILE_ID 516

class GetNumMultiTrpReq {
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Qmgr;
  
public:
  static constexpr Uint32 SignalLength = 3;
public:
  Uint32 numMultiTrps;
  Uint32 nodeId;
  Uint32 initial_set_up_multi_trp_done;
};

class GetNumMultiTrpConf {

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Qmgr;
  
public:
  static constexpr Uint32 SignalLength = 3;
  
public:
  Uint32 numMultiTrps;
  Uint32 nodeId;
  Uint32 initial_set_up_multi_trp_done;
};

class GetNumMultiTrpRef {

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Qmgr;
  
public:
  static constexpr Uint32 SignalLength = 2;
  enum ErrorCode
  {
    NotReadyYet = 1
  };
public:
  Uint32 nodeId;
  Uint32 errorCode;
};

#undef JAM_FILE_ID

#endif
