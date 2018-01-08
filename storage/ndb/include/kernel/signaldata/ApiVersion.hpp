/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef API_VERSION_HPP
#define API_VERSION_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 35


class ApiVersionReq {
/**
   * Sender(s)
   */
  friend class MgmtSrvr;

  /**
   * Reciver(s)
   */
  friend class Qmgr;

  friend bool printAPI_VERSION_REQ(FILE *, const Uint32 *, Uint32, Uint16);

  STATIC_CONST( SignalLength = 4 );

  Uint32 senderRef;
  Uint32 nodeId; //api node id
  Uint32 version; // Version of API node
  Uint32 mysql_version; // MySQL version
};



class ApiVersionConf {
/**
   * Sender(s)
   */
  friend class Qmgr;

  /**
   * Reciver(s)
   */
  friend class MgmtSrvr;

  friend bool printAPI_VERSION_CONF(FILE *, const Uint32 *, Uint32, Uint16);

  STATIC_CONST( SignalLength = 5 );

  Uint32 senderRef;
  Uint32 nodeId; //api node id
  Uint32 version; // Version of API node
  Uint32 m_inet_addr; // The bare name "inet_addr" can conflict with arpa/inet.h
  Uint32 mysql_version; // MySQL version
};


#undef JAM_FILE_ID

#endif
