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

#ifndef STOP_ME_HPP
#define STOP_ME_HPP

/**
 * This signal is sent by ndbcntr to local DIH
 *
 * If local DIH then sends it to all DIH's
 *
 * @see StopPermReq
 * @see StartMeReq
 * @see StartPermReq
 */
class StopMeReq {
  
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Sender
   */
  friend class Ndbcntr;

public:
  STATIC_CONST( SignalLength = 2 );
private:
  
  Uint32 senderRef;
  Uint32 senderData;
};

class StopMeConf {

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;

public:
  STATIC_CONST( SignalLength = 2 );
  
private:
  Uint32 senderRef;
  Uint32 senderData;
};


#endif
