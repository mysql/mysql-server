/*
   Copyright (c) 2003, 2021, Oracle and/or its affiliates.

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

#ifndef EMPTY_LCPREQ_HPP
#define EMPTY_LCPREQ_HPP

#define JAM_FILE_ID 157


/**
 * This signals is sent by Dbdih-Master to Dblqh
 * as part of master take over after node crash
 */
struct EmptyLcpReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;
  
  /**
   * Sender(s) / Receiver(s)
   */
  
  /**
   * Receiver(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;
  
  STATIC_CONST( SignalLength = 1 );
  
  Uint32 senderRef;
};

/**
 * This signals is sent by Dblqh to Dbdih
 * as part of master take over after node crash
 */
struct EmptyLcpConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;
  
  /**
   * Sender(s) / Receiver(s)
   */
  
  /**
   * Receiver(s)
   */
  friend class Dbdih;
  
  STATIC_CONST( SignalLength = 6 );

  Uint32 senderNodeId;
  Uint32 tableId;
  Uint32 fragmentId;
  Uint32 lcpNo;
  Uint32 lcpId;
  Uint32 idle;
};

/**
 * This is a envelope signal
 *   sent from LQH to local DIH, that will forward it as a
 *   EMPTY_LCP_CONF to avoid race condition with LCP_FRAG_REP
 *   which is now routed via local DIH
 */
struct EmptyLcpRep
{
  STATIC_CONST( SignalLength = NdbNodeBitmask::Size );
  Uint32 receiverGroup[NdbNodeBitmask::Size];
  Uint32 conf[EmptyLcpConf::SignalLength];
};


#undef JAM_FILE_ID

#endif
