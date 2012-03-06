/*
   Copyright (C) 2003, 2005-2007 MySQL AB
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

#ifndef CNTR_MASTERCONF_HPP
#define CNTR_MASTERCONF_HPP

#include <NodeBitmask.hpp>

/**
 * This signals is sent by NdbCntr-Master to NdbCntr
 */
class CntrMasterConf {
  /**
   * Sender(s)
   */
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  /**
   * Reciver(s)
   */
  
public:
  STATIC_CONST( SignalLength = 1 + NdbNodeBitmask::Size );
private:
  
  Uint32 noStartNodes;
  Uint32 theNodes[NdbNodeBitmask::Size];
};

#endif
