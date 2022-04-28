/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef START_REC_HPP
#define START_REC_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 105


class StartRecReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;
  /**
   * Receiver(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

  friend bool printSTART_REC_REQ(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  static constexpr Uint32 SignalLength = 6;
  static constexpr Uint32 SignalLength_v1 = 6 + NdbNodeBitmask48::Size;
private:
  
  Uint32 receivingNodeId;
  Uint32 senderRef;
  Uint32 keepGci;
  Uint32 lastCompletedGci;
  Uint32 newestGci;
  Uint32 senderData;
  Uint32 sr_nodes[NdbNodeBitmask::Size];
};

class StartRecConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;
  /**
   * Receiver(s)
   */
  friend class Dbdih;

  friend bool printSTART_REC_CONF(FILE *, const Uint32 *, Uint32, Uint16);    
public:
  static constexpr Uint32 SignalLength = 2;
private:
  
  Uint32 startingNodeId;
  Uint32 senderData;
};

#undef JAM_FILE_ID

#endif
