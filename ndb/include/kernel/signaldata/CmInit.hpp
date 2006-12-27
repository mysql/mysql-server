/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CM_INIT_HPP
#define CM_INIT_HPP

#include <NodeBitmask.hpp>

/**
 * 
 */
class CmInit {
  /**
   * Sender(s)
   */
  friend class Cmvmi;
  
  /**
   * Reciver(s)
   */
  friend class Qmgr;
  
public:
  STATIC_CONST( SignalLength = 4 + NodeBitmask::Size );
private:
  
  Uint32 heartbeatDbDb;
  Uint32 heartbeatDbApi;
  Uint32 inactiveTransactionCheck;
  Uint32 arbitTimeout;
  
  Uint32 allNdbNodes[NodeBitmask::Size];
};

#endif
