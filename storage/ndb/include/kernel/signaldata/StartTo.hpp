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

#ifndef START_TO_HPP
#define START_TO_HPP

class StartToReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 5 );
private:
  Uint32 userPtr;
  BlockReference userRef;
  Uint32 startingNodeId;
  Uint32 nodeTakenOver;
  bool nodeRestart;
};

class StartToConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 3 );
private:
  
  Uint32 userPtr;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
};
#endif
