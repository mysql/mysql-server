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

#ifndef DIHADDFRAG_HPP
#define DIHADDFRAG_HPP

#include <NodeBitmask.hpp>
#include <ndb_limits.h>

#define JAM_FILE_ID 37


/**
 * 
 */
class DihAddFragConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
    
public:
  static constexpr Uint32 SignalLength = 2;
private:  
  Uint32 senderNodeId;
  Uint32 tableId;
};
/**
 * 
 */
class DihAddFragReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
    
public:
  static constexpr Uint32 SignalLength = 10 + MAX_REPLICAS;
private:
  Uint32 masterRef;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 kValue;
  Uint32 method;
  Uint32 mask;
  Uint32 hashPointer;
  Uint32 noOfFragments;
  Uint32 noOfBackups;
  Uint32 storedTable;
  Uint32 nodes[MAX_REPLICAS];
};

#undef JAM_FILE_ID

#endif
