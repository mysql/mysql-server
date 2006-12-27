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

#ifndef START_ME_HPP
#define START_ME_HPP

/**
 * This signal is sent...
 *
 * It also contains the Sysfile.
 * Since the Sysfile can be larger than on StartMeConf signal,
 *   there might be more than on of these signals sent before
 *   the entire sysfile is transfered
 *
 */
class StartMeReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 2 );
private:
  
  Uint32 startingRef;
  Uint32 startingVersion;
};

class StartMeConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 25 );
private:
  
  Uint32 startingNodeId;
  Uint32 startWord;
  
  /**
   * No of free words to carry data
   */
  STATIC_CONST( DATA_SIZE = 23 );
  
  Uint32 data[DATA_SIZE];
};
#endif
