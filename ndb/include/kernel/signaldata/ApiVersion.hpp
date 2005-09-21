/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef API_VERSION_HPP
#define API_VERSION_HPP

class ApiVersionReq {
/**
   * Sender(s)
   */
  friend class MgmtSrv;
  
  /**
   * Reciver(s)
   */
  friend class Qmgr;  
public:
  STATIC_CONST( SignalLength = 3 );
  Uint32 senderRef; 
  Uint32 nodeId; //api node id
  Uint32 version; // Version of API node

  
};



class ApiVersionConf {
/**
   * Sender(s)
   */
  friend class Qmgr;
  
  /**
   * Reciver(s)
   */
  friend class MgmtSrv;  
public:
  STATIC_CONST( SignalLength = 4 );
  Uint32 senderRef; 
  Uint32 nodeId; //api node id
  Uint32 version; // Version of API node
  Uint32 inet_addr;
};

#endif
