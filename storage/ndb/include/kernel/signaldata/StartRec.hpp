/*
   Copyright (C) 2003, 2005-2008 MySQL AB
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

#ifndef START_REC_HPP
#define START_REC_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

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
  STATIC_CONST( SignalLength = 6 + NdbNodeBitmask::Size);
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
  STATIC_CONST( SignalLength = 2 );
private:
  
  Uint32 startingNodeId;
  Uint32 senderData;
};
#endif
