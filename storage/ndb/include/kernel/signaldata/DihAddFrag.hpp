/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef DIHADDFRAG_HPP
#define DIHADDFRAG_HPP

#include <NodeBitmask.hpp>
#include <ndb_limits.h>

/**
 * 
 */
class DihAddFragConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
    
public:
  STATIC_CONST( SignalLength = 2 );
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
  STATIC_CONST( SignalLength = 10 + MAX_REPLICAS );
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
#endif
