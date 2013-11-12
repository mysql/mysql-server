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

#ifndef GET_CONFIG_HPP
#define GET_CONFIG_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 97


/**
 * GetConfig - Get the node's current configuration
 *
 * Successfull return = GET_CONFIG_CONF -  a long signal
 */
class GetConfigReq {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Cmvmi;
  friend class MgmtSrvr;
  friend bool printGET_CONFIG_REQ(FILE *, const Uint32 *, Uint32, Uint16);

  STATIC_CONST( SignalLength = 2 );

  Uint32 nodeId; // Node id of the receiver node
  Uint32 senderRef;
};

class GetConfigRef {
  /**
   * Sender/Receiver
   */
  friend class Cmvmi;
  friend class MgmtSrvr;
  friend bool printGET_CONFIG_REF(FILE *, const Uint32 *, Uint32, Uint16);

  STATIC_CONST( SignalLength = 1 );

  Uint32 error;

  enum ErrorCode {
    WrongSender = 1,
    WrongNodeId = 2,
    NoConfig = 3
  };
};

class GetConfigConf {
  /**
   * Sender(s) / Reciver(s)
   */
  // Blocks
  friend class Cmvmi;
  friend class MgmtSrvr;
  friend bool printGET_CONFIG_CONF(FILE *, const Uint32 *, Uint32, Uint16);

  STATIC_CONST( SignalLength = 1 );

  Uint32 configLength; // config blob size
};

#undef JAM_FILE_ID

#endif
