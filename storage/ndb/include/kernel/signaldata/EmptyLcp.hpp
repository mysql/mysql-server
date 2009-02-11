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

#ifndef EMPTY_LCPREQ_HPP
#define EMPTY_LCPREQ_HPP

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

#endif
