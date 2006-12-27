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

#ifndef UPDATE_TO_HPP
#define UPDATE_TO_HPP

class UpdateToReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 6 );
private:
  enum UpdateState {
    TO_COPY_FRAG_COMPLETED = 0,
    TO_COPY_COMPLETED = 1
  };
  Uint32 userPtr;
  BlockReference userRef;
  UpdateState updateState;
  Uint32 startingNodeId;
  
  /**
   * Only when TO_COPY_FRAG_COMPLETED
   */
  Uint32 tableId;
  Uint32 fragmentNo;
};

class UpdateToConf {
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
