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

#ifndef INVALIDATE_NODE_LCP_REQ_HPP
#define INVALIDATE_NODE_LCP_REQ_HPP

/**
 * This signal is sent from the master DIH to all DIHs
 * when a node is starting without filesystem.
 *
 * All DIHs must then "forgett" that the starting node has 
 * performed LCP
 *
 * @see StartPermReq
 */
class InvalidateNodeLCPReq {
  
  /**
   * Sender/Receiver
   */
  friend class Dbdih;
  
  Uint32 startingNodeId;

public:
  STATIC_CONST( SignalLength = 1 );
};

#endif
