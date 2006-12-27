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

#ifndef SOCKET_SERVICE_HPP
#define SOCKET_SERVICE_HPP
#include <Parser.hpp>
#include <InputStream.hpp>
#include <Parser.hpp>
#include <NdbOut.hpp>
#include <Properties.hpp>
#include "SocketRegistry.hpp"




class SocketService {
  friend class SocketRegistry<SocketService>;
private:
  typedef Parser<SocketService> Parser_t;
  int runSession(NDB_SOCKET_TYPE socket, SocketService &);  
public:
  void getPropertyObject();
  SocketService();
  ~SocketService();

};






#endif
