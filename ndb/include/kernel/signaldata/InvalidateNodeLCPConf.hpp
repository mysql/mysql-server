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

#ifndef INVALIDATE_NODE_LCP_CONF_HPP
#define INVALIDATE_NODE_LCP_CONF_HPP

/**
 * This signal is sent from the non-master DIH to master DIHs
 *
 */
class InvalidateNodeLCPConf {
  
  /**
   * Sender/Receiver
   */
  friend class Dbdih;
  
  /**
   * NodeId of sending node
   * which is "done"
   */
  Uint32 sendingNodeId;

public:
  STATIC_CONST( SignalLength = 1 );
};

#endif
